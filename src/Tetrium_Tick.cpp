// Tick-time function implementations
#include "implot.h"
#include "lib/Utils.h"

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
            drawImGui(RGB); // populate RGB context
            drawImGui(OCV); // populate OCV context
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

// Given the content in rgyb frame buffer,
// transform the content onto either RGB or OCV representation, and paint onto
// the swapchain's frame buffer.
//
// Internally, the remapping is done by post-processing a full-screen quad:
// the full-screen quad samples the rgyb FB's texture and performs matmul on the color
void Tetrium::transformRGYBColorSpace(
    vk::CommandBuffer CB,
    VirtualFrameBuffer& rgybFrameBuffer,
    SwapChainContext& physicalSwapChain,
    uint32_t swapChainImageIndex,
    ColorSpace colorSpace
)
{
    vk::Extent2D extend = _swapChain.extent;
    vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
    vk::RenderPassBeginInfo renderPassBeginInfo(
        _rocvTransformRenderPass,
        physicalSwapChain.frameBuffer[swapChainImageIndex],
        renderArea,
        _clearValues.size(),
        _clearValues.data(),
        nullptr
    );

    CB.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

    CB.endRenderPass();
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

void Tetrium::drawFrame(TickContext* ctx, uint8_t frame)
{
    SyncPrimitives& sync = _syncProjector[frame];
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

    std::array<VkSemaphore, 1> semaRenderFinished = {sync.semaRenderFinished};
    { // Render RGB, OCV channels onto both frame buffers
        PROFILE_SCOPE(&_profiler, "Record render commands");

        vk::CommandBuffer CB1(_device->graphicsCommandBuffers[frame]);
        //  Record a command buffer which draws the scene onto that image
        CB1.reset();

        { // begin command buffer
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;                  // Optional
            beginInfo.pInheritanceInfo = nullptr; // Optional
            CB1.begin(vk::CommandBufferBeginInfo());
        }

        // update graphics rendering context
        ctx->graphics.currentFrameInFlight = frame;
        ctx->graphics.currentSwapchainImageIndex = swapchainImageIndex;
        ctx->graphics.CB = CB1;
        ctx->graphics.currentFBextend = _swapChain.extent;
        getMainProjectionMatrix(ctx->graphics.mainProjectionMatrix);

        { // main render pass
            vk::Extent2D extend = _swapChain.extent;
            vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
            vk::RenderPassBeginInfo renderPassBeginInfo(
                {}, {}, renderArea, _clearValues.size(), _clearValues.data(), nullptr
            );

            VkViewport viewport{};
            VkRect2D scissor{};
            getFullScreenViewportAndScissor(_swapChain, viewport, scissor);

            vkCmdSetViewport(CB1, 0, 1, &viewport);
            vkCmdSetScissor(CB1, 0, 1, &scissor);
            // 1. rasterize onto RYGB FB
            if (true) {
                renderPassBeginInfo.renderPass = _renderContextRYGB.renderPass;
                renderPassBeginInfo.framebuffer
                    = _renderContextRYGB.virtualFrameBuffer.frameBuffer[swapchainImageIndex];

                CB1.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

                _rgbyRenderers.imageDisplay.Tick(ctx, "../assets/textures/spot.png");

                CB1.endRenderPass();
            }

            // at this point the RYGB FB contains RYGB channels, now we need to transform it into
            // even/odd frame representation based on the current frame's even-oddness.

            // 2. get even-odd info
            bool renderRGB = isEvenFrame();
            if (_flipEvenOdd) {
                renderRGB = !renderRGB;
            }

            // 3. depending on even-odd, transform RYGB into R000, or OCV0
            // by sampling from RYGB FB and rendering onto a full-screen quad on the FB
            transformRGYBColorSpace(
                CB1,
                _renderContextRYGB.virtualFrameBuffer,
                _swapChain,
                swapchainImageIndex,
                renderRGB ? ColorSpace::RGB : ColorSpace::OCV
            );

            // FIXME: imgui should be painted onto RGYB buffer
            // TODO: optionally create 3 imgui passes -- one that goes onto RGYB FB for user-space
            // gui frameworking, the other two goes onto RGB/OCV FB for lower-level control of
            // presented color for debugging.
            // 4. paint RGB/OCV ImGUI onto R000 / OCV0 image
            // note that imGUI contexts for both RGB/OCV are populated -- this is to provide
            // an interface to show raw colors for easy debugging & development
            {
                if (renderRGB) {
                    recordImGuiDrawCommandBuffer(_imguiCtx, RGB, CB1, extend, swapchainImageIndex);
                } else {
                    recordImGuiDrawCommandBuffer(_imguiCtx, OCV, CB1, extend, swapchainImageIndex);
                }
            }
        }

        CB1.end();

        VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
        std::array<VkCommandBuffer, 1> submitCommandBuffers = {CB1};
        submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
        submitInfo.pCommandBuffers = submitCommandBuffers.data();
        VkSemaphore signalSemaphores[0];
        submitInfo.signalSemaphoreCount = semaRenderFinished.size();
        submitInfo.pSignalSemaphores = semaRenderFinished.data();
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &sync.semaImageAvailable;
        std::array<VkPipelineStageFlags, 1> semaImageAvailableStages
            = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
        submitInfo.pWaitDstStageMask = semaImageAvailableStages.data();

        if (vkQueueSubmit(_device->graphicsQueue, 1, &submitInfo, sync.fenceInFlight)
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
#if VIRTUAL_VSYNC
        if (_softwareEvenOddCtx.mostRecentPresentFinish) {
            time = _softwareEvenOddCtx.mostRecentPresentFinish
                   + _softwareEvenOddCtx.nanoSecondsPerFrame * _softwareEvenOddCtx.vsyncFrameOffset;
        }
#endif // VIRTUAL_VSYNC

        // label each frame with the tick number
        // this is useful for calculating the virtual frame counter,
        // as we can take the max(frame.id) to get the number of presented frames.

        // VkPresentTimeGOOGLE presentTime{(uint32_t)_numTicks, time};
        //
        // VkPresentTimesInfoGOOGLE presentTimeInfo{
        //     .sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE,
        //     .pNext = VK_NULL_HANDLE,
        //     .swapchainCount = 1,
        //     .pTimes = &presentTime
        // };
        //
        // if (_tetraMode == TetraMode::kEvenOddSoftwareSync) {
        //     presentInfo.pNext = &presentTimeInfo;
        // }
        //
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
