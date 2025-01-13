// Tick-time function implementations

#include "Tetrium.h"

void Tetrium::Run()
{
    DEBUG("Starting game loop...");
#if defined(WIN32)
    MSG msg = {0};
    while (1) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (WM_QUIT == msg.message || WM_CLOSE == msg.message) {
                break;
            }
        }
        Tick();
    }
#else
    ASSERT(_window);
    glfwShowWindow(_window);
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        Tick();
    }
#endif // WIN32
    DEBUG("Ending game loop...");
}

void Tetrium::Tick()
{
    if (_paused) {
        std::this_thread::yield();
        return;
    }
    _deltaTimer.Tick();
    _soundManager.Tick();
    pollInputs();
    {
        {
            PROFILE_SCOPE(&_profiler, "Render Loop");
            //_swapChain.chainDXGI->waitForPreviousFrame();
            //_swapChain.chainDXGI->m_output->WaitForVBlank();
            updateSurfaceCounterValue();
            // CPU-exclusive workloads
            double deltaTime = _deltaTimer.GetDeltaTime();
            _timeSinceStartSeconds += deltaTime;
            ColorSpace colorSpace = getCurrentColorSpace();
            drawImGui(colorSpace, _currentFrame);
            drawFrame(colorSpace, _currentFrame);
            _currentFrame = (_currentFrame + 1) % NUM_FRAME_IN_FLIGHT;
        }
        {
            PROFILE_SCOPE(&_profiler, "GPU: Wait Idle");
            vkDeviceWaitIdle(this->_device->logicalDevice);
        }
    }
    _lastProfilerData = _profiler.NewProfile();
    _numTicks++;
}

void Tetrium::triggerClose()
{
#if defined(WIN32)
    PostMessage(_swapChain.chainDXGI->m_hWnd, WM_QUIT, 0, 0);
#else
    glfwSetWindowShouldClose(_window, GLFW_TRUE);
#endif // WIN32
}

void Tetrium::pollInputs()
{
#if defined(WIN32)
    // trigger window capture
    // note that none-win32 system uses GLFW for input capture,
    // win32 uses only one window
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        _captureCursor = !_captureCursor;
    }
#endif // WIN32
    // quit engine
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        triggerClose();
    }

    // quit application
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
        if (_primaryApp.has_value()) {
            _primaryApp.value()->OnClose();
            _primaryApp = std::nullopt;
            _soundManager.DisableMusic();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Period)) {
        _rocvPresentMode = (ROCVPresentMode)(((int)_rocvPresentMode + 1) % 3);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Comma)) {
        _flipEvenOdd = !_flipEvenOdd; 
    }
}

void Tetrium::getFullScreenViewportAndScissor(
    const SwapChainContext& swapChain,
    VkViewport& viewport,
    VkRect2D& scissor
)
{
    vk::Extent2D extend = swapChain.extent;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extend.width);
    viewport.height = static_cast<float>(extend.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    scissor.offset = {0, 0};
    scissor.extent = extend;
}

void Tetrium::resetBackbufferRenderingFence(uint8_t frameIdx)
{
    SyncPrimitives& sync = _syncProjector[frameIdx];

    VK_CHECK_RESULT(vkResetFences(this->_device->logicalDevice, 1, &sync.fenceBackbufferRendering))
}

void Tetrium::blockOnBackbufferRenderingFence(uint8_t frameIdx)
{
    SyncPrimitives& sync = _syncProjector[frameIdx];

    PROFILE_SCOPE(&_profiler, "vkWaitForFences: fenceBackbufferRendering");
    VK_CHECK_RESULT(vkWaitForFences(
        _device->logicalDevice, 1, &sync.fenceBackbufferRendering, VK_TRUE, UINT64_MAX
    ))
}

void Tetrium::drawFrame(ColorSpace colorSpace, uint8_t frameIdx)
{
    SyncPrimitives& sync = _syncProjector[frameIdx];
    VkResult result;
    uint8_t swapchainImageIndex = 0;
    
    { // wait for the rendering resources
#if defined(WIN32)
        // when using DXGI swapchain, backbuffer rendering resources are always ready at this point;
        // because we block on them right before presenting
#else
        // when using Vulkan swapchain, we can be simultaneously rendering to multiple backbuffers.
        blockOnBackbufferRenderingFence(frameIdx);
        resetBackbufferRenderingFence(frameIdx);
#endif
    }
    
    { // acquire image from swap chain
#if defined(WIN32)
        swapchainImageIndex = _swapChain.chainDXGI->m_pSwapChain->GetCurrentBackBufferIndex();
#else
        result = vkAcquireNextImageKHR(
            this->_device->logicalDevice,
            _swapChain.chain,
            UINT64_MAX,
            sync.semaImageAvailable,
            VK_NULL_HANDLE,
            &swapchainImageIndex
        );
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            _framebufferResized = true;
        } else if (result != VK_SUCCESS) {
            const char* res = string_VkResult(result);
            PANIC("Failed to acquire swap chain image: {}", res);
        }
#endif
    }

    vk::CommandBuffer appCB(_device->appCommandBuffers[frameIdx]);
    vk::CommandBuffer engineCB(_device->graphicsCommandBuffers[frameIdx]);

    { // record render commands
        PROFILE_SCOPE(&_profiler, "Record render commands");

        // record app rendering commands
        appCB.reset();
        appCB.begin(vk::CommandBufferBeginInfo());
        if (_primaryApp.has_value()) {
            TetriumApp::TickContextVulkan tickCtx{
                .currentFrameInFlight = frameIdx,
                .colorSpace = colorSpace,
                .commandBuffer = appCB,
            };
            _primaryApp.value()->TickVulkan(tickCtx);
        }
        appCB.end();

        // record engine rendering commands
        engineCB.reset();
        engineCB.begin(vk::CommandBufferBeginInfo());
        {
            vk::Extent2D extend = _swapChain.extent;
            vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
            VkViewport viewport{};
            VkRect2D scissor{};
            getFullScreenViewportAndScissor(_swapChain, viewport, scissor);

            vkCmdSetViewport(engineCB, 0, 1, &viewport);
            vkCmdSetScissor(engineCB, 0, 1, &scissor);

            recordImGuiDrawCommandBuffer(
                _imguiCtx, engineCB, extend, swapchainImageIndex, colorSpace
            );
        }
        engineCB.end();
    }

    { // submit command buffers for both app and engine rendering,
        // submit command buffers for both app and engine rendering,
        // engine rendering waits for app rendering to finish
        std::array<vk::CommandBuffer, 1> appCBs = {appCB};
        std::array<vk::CommandBuffer, 1> engineCBs = {engineCB};

#if defined(WIN32)
        std::array<vk::Semaphore, 1> engineWaits = {
            sync.semaAppVulkanFinished, // app rendering finished
        };
#else
        std::array<vk::Semaphore, 2> engineWaits = {
            sync.semaAppVulkanFinished, // app rendering finished
            sync.semaImageAvailable,    // screen fb availability
        };
#endif // WIN32

        std::array<vk::PipelineStageFlags, 2> engineWaitStages = {
            vk::PipelineStageFlagBits::eTopOfPipe, // conservatively wait for all app rendering to
                                                   // finish
            vk::PipelineStageFlagBits::eColorAttachmentOutput // screen fb needs to be available
        };

        std::array<vk::Semaphore, 1> appSignals = {sync.semaAppVulkanFinished};

#if defined(WIN32)
        std::array<vk::Semaphore, 0> engineSignals = {};
#else
        std::array<vk::Semaphore, 1> engineSignals = {sync.semaRenderFinished};
#endif // WIN32

        std::array<vk::SubmitInfo, 2> submitInfos = {
            vk::SubmitInfo(
                0, nullptr, {}, appCBs.size(), appCBs.data(), appSignals.size(), appSignals.data()
            ),
            vk::SubmitInfo(
                engineWaits.size(),
                engineWaits.data(),
                engineWaitStages.data(),
                engineCBs.size(),
                engineCBs.data(),
                engineSignals.size(),
                engineSignals.data()
            ),
        };

        vk::Queue queue = _device->graphicsQueue;
#if defined(WIN32)
        resetBackbufferRenderingFence(frameIdx);
#endif // WIN32
        VkResult result = static_cast<VkResult>(
            queue.submit(submitInfos.size(), submitInfos.data(), sync.fenceBackbufferRendering)
        );
        VK_CHECK_RESULT(result);
    }

    { // Presented the swap chain
        PROFILE_SCOPE(&_profiler, "Queue Present")
#if defined(WIN32)
        // since the Present() call immediately shoves in the FB, the CPU has to wait
        // here for the back buffer rendering to finish.
        blockOnBackbufferRenderingFence(frameIdx);
        resetBackbufferRenderingFence(frameIdx);
        _swapChain.chainDXGI->Present();
#else
        //  Present the swap chain image
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &sync.semaRenderFinished;

        VkSwapchainKHR swapChains[] = {_swapChain.chain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &swapchainImageIndex; // specify which image to present
        presentInfo.pResults = nullptr;                   // Optional: can be used to check if
                                                          // presentation was successful

        uint64_t time = 0;
        result = vkQueuePresentKHR(_device->presentationQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
            || this->_framebufferResized) {
            ASSERT(
                _tetraMode != TetraMode::kEvenOddHardwareSync
            ); // exclusive window does not resize its swapchain
            this->recreateSwapChain(_swapChain);
            recreateVirtualFrameBuffers();
            this->_framebufferResized = false;
        }
#endif
    }
}
