// Tick-time function implementations

#include "Tetrium.h"

void Tetrium::Run()
{
    DEBUG("Starting run loop...");
    ASSERT(_window);
    glfwShowWindow(_window);
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        Tick();
    }
    DEBUG("Ending run loop...");
}

void Tetrium::getMainProjectionMatrix(glm::mat4& projectionMatrix)
{
    auto& extent = _swapChain.extent;
    projectionMatrix = glm::perspective(
        glm::radians(_FOV),
        extent.width / static_cast<float>(extent.height),
        DEFAULTS::ZNEAR,
        DEFAULTS::ZFAR
    );
    projectionMatrix[1][1] *= -1; // invert for vulkan coord system
}

void Tetrium::flushEngineUBOStatic(uint8_t frame)
{
    PROFILE_SCOPE(&_profiler, "flushEngineUBOStatic");
    VQBuffer& buf = _engineUBOStatic[frame];
    EngineUBOStatic ubo{
        _mainCamera.GetViewMatrix() // view
        // proj
    };
    getMainProjectionMatrix(ubo.proj);
    ubo.timeSinceStartSeconds = _timeSinceStartSeconds;
    ubo.sinWave = (sin(_timeSinceStartSeconds) + 1) / 2.f; // offset to [0, 1]
    memcpy(buf.bufferAddress, &ubo, sizeof(ubo));
}

void Tetrium::Tick()
{
    if (_paused) {
        std::this_thread::yield();
        return;
    }
    _deltaTimer.Tick();
    _soundManager.Tick();
    {
        {
            PROFILE_SCOPE(&_profiler, "Render Loop");
            // CPU-exclusive workloads
            double deltaTime = _deltaTimer.GetDeltaTime();
            _timeSinceStartSeconds += deltaTime;
            _timeSinceStartNanoSeconds += _deltaTimer.GetDeltaTimeNanoSeconds();
            _inputManager.Tick(deltaTime);
            TickContext tickData{&_mainCamera, deltaTime};
            tickData.profiler = &_profiler;
            flushEngineUBOStatic(_currentFrame);
            drawFrame(&tickData, _currentFrame);
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

void Tetrium::drawFrame(TickContext* ctx, uint8_t frameIdx)
{
    SyncPrimitives& sync = _syncProjector[frameIdx];
    VkResult result;
    uint32_t swapchainImageIndex;

    { // wait for previous render
        PROFILE_SCOPE(&_profiler, "vkWaitForFences: fenceInFlight");
        VK_CHECK_RESULT(
            vkWaitForFences(_device->logicalDevice, 1, &sync.fenceInFlight, VK_TRUE, UINT64_MAX)
        );
        VK_CHECK_RESULT(vkResetFences(this->_device->logicalDevice, 1, &sync.fenceInFlight));
    }

    { // Asynchronously acquire an image from the swap chain,
        result = vkAcquireNextImageKHR(
            this->_device->logicalDevice,
            _swapChain.chain,
            UINT64_MAX,
            sync.semaImageAvailable,
            VK_NULL_HANDLE,
            &swapchainImageIndex
        );
        [[unlikely]] if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            _framebufferResized = true;
        } else [[unlikely]] if (result != VK_SUCCESS) {
            const char* res = string_VkResult(result);
            PANIC("Failed to acquire swap chain image: {}", res);
        }
    }

    ColorSpace colorSpace = getCurrentColorSpace();
    bool isEven = isEvenFrame();

    std::array<VkSemaphore, 1> semaRenderFinished = {sync.semaRenderFinished};
    std::array<VkSemaphore, 1> semaAppVulkanFinished = {sync.semaAppVulkanFinished};
    { // Render RGB, OCV channels onto both frame buffers
        PROFILE_SCOPE(&_profiler, "Record render commands");

        vk::CommandBuffer CBApp(_device->appCommandBuffers[frameIdx]);
        CBApp.reset();

        CBApp.begin(vk::CommandBufferBeginInfo());
        if (_primaryApp.has_value()) {
            TetriumApp::TickContextVulkan tickCtx{
                .currentFrameInFlight = frameIdx,
                .colorSpace = colorSpace,
                .commandBuffer = CBApp,
            };
            _primaryApp.value()->TickVulkan(tickCtx);
        }
        CBApp.end();

        // TODO: should this be placed here?
        drawImGui(colorSpace, frameIdx);

        vk::CommandBuffer CB1(_device->graphicsCommandBuffers[frameIdx]);
        //  Record a command buffer which draws the scene onto that image
        CB1.reset();

        { // begin command buffer
            CB1.begin(vk::CommandBufferBeginInfo());
        }

        // update graphics rendering context
        ctx->graphics.currentFrameInFlight = frameIdx;
        ctx->graphics.currentSwapchainImageIndex = swapchainImageIndex;
        ctx->graphics.CB = CB1;
        ctx->graphics.currentFBextend = _swapChain.extent;
        getMainProjectionMatrix(ctx->graphics.mainProjectionMatrix);

        { // main render pass
            vk::Extent2D extend = _swapChain.extent;
            vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
            VkViewport viewport{};
            VkRect2D scissor{};
            getFullScreenViewportAndScissor(_swapChain, viewport, scissor);

            vkCmdSetViewport(CB1, 0, 1, &viewport);
            vkCmdSetScissor(CB1, 0, 1, &scissor);
#if 0 // legacy code, to be referenced when migrating to ROCV in app
            vk::RenderPassBeginInfo renderPassBeginInfo(
                {}, {}, renderArea, _clearValues.size(), _clearValues.data(), nullptr
            );

            // 1. rasterize onto RYGB FB
            if (true) {
                renderPassBeginInfo.renderPass = _renderContextRYGB.renderPass;
                renderPassBeginInfo.framebuffer
                    = _renderContextRYGB.virtualFrameBuffer.frameBuffer[swapchainImageIndex];

                CB1.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

                // _rgbyRenderers.imageDisplay.Tick(ctx, "../assets/textures/spot.png");

                CB1.endRenderPass();
            }

            // at this point the RYGB FB contains RYGB channels, now we need to transform it into
            // even/odd frame representation based on the current frame's even-oddness.

            // 3. depending on even-odd, transform RYGB into R000, or OCV0
            // by sampling from RYGB FB and rendering onto a full-screen quad on the FB
            transformToROCVframeBuffer(
                _renderContextRYGB.virtualFrameBuffer,
                _swapChain,
                frameIdx,
                swapchainImageIndex,
                colorSpace,
                CB1,
                (isEven && _evenOddRenderingSettings.blackOutEven)
                    || (!isEven && _evenOddRenderingSettings.blackOutOdd)
            );
#endif

            recordImGuiDrawCommandBuffer(_imguiCtx, CB1, extend, swapchainImageIndex);
        }

        CB1.end();

        // submit app command buffer, that paints the apps,
        // that may render onto textures sampled onto imguis
        std::array<VkCommandBuffer, 1> appCommandBuffers = {CBApp};
        VkSubmitInfo submitInfoApp{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = 0,
            .commandBufferCount = 1,
            .pCommandBuffers = appCommandBuffers.data(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = semaAppVulkanFinished.data()
        };

        // submit engine rendering command buffer, that paints the imgui backend
        VkSubmitInfo submitInfoEngineRendering{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
        std::array<VkCommandBuffer, 1> submitCommandBuffers = {CB1};

        submitInfoEngineRendering.commandBufferCount
            = static_cast<uint32_t>(submitCommandBuffers.size());
        submitInfoEngineRendering.pCommandBuffers = submitCommandBuffers.data();
        submitInfoEngineRendering.signalSemaphoreCount = semaRenderFinished.size();
        submitInfoEngineRendering.pSignalSemaphores = semaRenderFinished.data();

        // set up sync primitives
        // want to wait for:
        // 1. fb to be available to render onto
        // 2. all apps to finish rendering, so we can safely paint imgui
        std::array<VkSemaphore, 2> submitInfoEngineRenderingWaitSemaphores
            = {sync.semaImageAvailable, sync.semaAppVulkanFinished};
        submitInfoEngineRendering.waitSemaphoreCount
            = submitInfoEngineRenderingWaitSemaphores.size();
        submitInfoEngineRendering.pWaitSemaphores = submitInfoEngineRenderingWaitSemaphores.data();
        std::array<VkPipelineStageFlags, 2> submitInfoEngineRenderingWaitStages
            = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfoEngineRendering.pWaitDstStageMask = submitInfoEngineRenderingWaitStages.data();

        std::array<VkSubmitInfo, 2> submitInfos = {
            submitInfoApp, // user-space app rendering
            submitInfoEngineRendering // engine rendering, would wait for app rendering to finish
        };

        if (vkQueueSubmit(
                _device->graphicsQueue, submitInfos.size(), submitInfos.data(), sync.fenceInFlight
            )
            != VK_SUCCESS) {
            FATAL("Failed to submit draw command buffer!");
        }
    }

    { // Presented the swapchain, which at this point contains a rendered RGB/OCV image
        PROFILE_SCOPE(&_profiler, "Queue Present");
        //  Present the swap chain image
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = semaRenderFinished.data();

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
    }
}
