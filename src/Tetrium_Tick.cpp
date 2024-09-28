// Tick-time function implementations
#include "lib/Utils.h"

#include "Tetrium.h"

void Tetrium::Run()
{
    ASSERT(_window);
    glfwShowWindow(_window);
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        Tick();
    }
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
        PROFILE_SCOPE(&_profiler, "Main Tick");
        {
            PROFILE_SCOPE(&_profiler, "CPU");
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
            PROFILE_SCOPE(&_profiler, "GPU");
            vkDeviceWaitIdle(this->_device->logicalDevice);
        }
    }
    _lastProfilerData = _profiler.NewProfile();
    _numTicks++;
}

void Tetrium::drawFrame(TickContext* ctx, uint8_t frame)
{
    SyncPrimitives& sync = _syncProjector[frame];
    //  Wait for the previous frame to finish
    PROFILE_SCOPE(&_profiler, "Render Tick");
    vkWaitForFences(_device->logicalDevice, 1, &sync.fenceInFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(this->_device->logicalDevice, 1, &sync.fenceInFlight);

    //  Asynchronously acquire an image from the swap chain
    uint32_t swapchainImageIndex;
    VkResult result = vkAcquireNextImageKHR(
        this->_device->logicalDevice,
        _swapChain.chain,
        UINT64_MAX,
        sync.semaImageAvailable,
        VK_NULL_HANDLE,
        &swapchainImageIndex
    );
    [[unlikely]] if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        this->recreateSwapChain(_swapChain);
        recreateVirtualFrameBuffers();
        return;
    } else [[unlikely]] if (result != VK_SUCCESS) {
        const char* res = string_VkResult(result);
        PANIC("Failed to acquire swap chain image: {}", res);
    }

    vk::CommandBuffer CB1(_device->graphicsCommandBuffers[frame]);
    //  Record a command buffer which draws the scene onto that image
    CB1.reset();
    { // update ctx->graphics field
        ctx->graphics.currentFrameInFlight = frame;
        ctx->graphics.currentSwapchainImageIndex = swapchainImageIndex;
        ctx->graphics.CB = CB1;
        // ctx->graphics.currentFB = FB;
        ctx->graphics.currentFBextend = _swapChain.extent;
        getMainProjectionMatrix(ctx->graphics.mainProjectionMatrix);
    }

    { // begin command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;                  // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional
        CB1.begin(vk::CommandBufferBeginInfo());
    }

    { // main render pass
        vk::Extent2D extend = _swapChain.extent;
        vk::Rect2D renderArea(VkOffset2D{0, 0}, extend);
        vk::RenderPassBeginInfo renderPassBeginInfo(
            {}, {}, renderArea, _clearValues.size(), _clearValues.data(), nullptr
        );

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extend.width);
        viewport.height = static_cast<float>(extend.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = extend;

        // two-pass rendering: render RGB and OCV colors onto two virtual FBs
        {
            renderPassBeginInfo.renderPass = _renderContexts.RGB.renderPass;
            renderPassBeginInfo.framebuffer
                = _renderContexts.RGB.virtualFrameBuffer
                      .frameBuffer[swapchainImageIndex]; // which frame buffer in the
                                                         // swapchain do the pass i.e.
                                                         // all draw calls render to?
            CB1.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            vkCmdSetViewport(CB1, 0, 1, &viewport);
            vkCmdSetScissor(CB1, 0, 1, &scissor);
            _renderer.TickRGB(ctx);
            CB1.endRenderPass();

            drawImGui(ColorSpace::RGB);
            _imguiManager.RecordCommandBufferRGB(CB1, extend, swapchainImageIndex);
        }
        {
            renderPassBeginInfo.renderPass = _renderContexts.OCV.renderPass;
            renderPassBeginInfo.framebuffer
                = _renderContexts.OCV.virtualFrameBuffer
                      .frameBuffer[swapchainImageIndex]; // which frame buffer in the
                                                         // swapchain do the pass i.e.
                                                         // all draw calls render to?
            CB1.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            vkCmdSetViewport(CB1, 0, 1, &viewport);
            vkCmdSetScissor(CB1, 0, 1, &scissor);
            _renderer.TickOCV(ctx);
            CB1.endRenderPass();

            drawImGui(ColorSpace::OCV);
            _imguiManager.RecordCommandBufferOCV(CB1, extend, swapchainImageIndex);
        }
    }

    CB1.end();

    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    std::array<VkCommandBuffer, 1> submitCommandBuffers = {CB1};
    submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
    submitInfo.pCommandBuffers = submitCommandBuffers.data();
    VkSemaphore signalSemaphores[0];
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(_device->Get(), 1, &sync.fenceRenderFinished);
    if (vkQueueSubmit(_device->graphicsQueue, 1, &submitInfo, sync.fenceRenderFinished)
        != VK_SUCCESS) {
        FATAL("Failed to submit draw command buffer!");
    }

    // for the image transfer to finish, two semas need to be uppsed:
    // 1. the render from the previous CB has to finish for 2 virtual FBs to be available
    // 2. the actual FB needs to be available for copying
    //
    // for (1) : we use `fenceRenderFinished` -- CPU decides which virtual frame buffer to
    // commit right before committing
    // for (2) : we use `semaImageAvailable` -- the GPU waits til the actual swapchain is available

    vk::CommandBuffer CB2(_device->graphicsCommandBuffers2[frame]);
    CB2.reset();
    CB2.begin(vk::CommandBufferBeginInfo());

    // important: here we wait for the rendering to finish before copying over image
    vkWaitForFences(_device->logicalDevice, 1, &sync.fenceRenderFinished, VK_TRUE, UINT64_MAX);
    // choose whether to render the even/odd frame buffer, discarding the other
    bool isEven = isEvenFrame();
    VkImage virtualFramebufferImage
        = isEven ? _renderContexts.RGB.virtualFrameBuffer.image[swapchainImageIndex]
                 : _renderContexts.OCV.virtualFrameBuffer.image[swapchainImageIndex];

    VkImage swapchainFramebufferImage = _swapChain.image[swapchainImageIndex];
    Utils::ImageTransfer::CmdCopyToFB(
        CB2, virtualFramebufferImage, swapchainFramebufferImage, _swapChain.extent
    );

    if (isEven != _evenOddDebugCtx.currShouldBeEven) {
        _evenOddDebugCtx.numDroppedFrames++;
    }
    _evenOddDebugCtx.currShouldBeEven = !isEven; // advance to next frame
    CB2.end();

    // submit CB2
    VkSubmitInfo submitInfo2{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO};
    std::array<VkCommandBuffer, 1> submitCommandBuffers2 = {CB2};
    std::array<VkSemaphore, 1> semaImageAvailable = {sync.semaImageAvailable};
    std::array<VkPipelineStageFlags, 1> semaImageAvailableStages
        = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
    std::array<VkSemaphore, 1> semaImageCopyFinished = {sync.semaImageCopyFinished};

    submitInfo2.waitSemaphoreCount = semaImageAvailable.size();
    submitInfo2.pWaitSemaphores = semaImageAvailable.data();
    submitInfo2.pWaitDstStageMask = semaImageAvailableStages.data();
    submitInfo2.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers2.size());
    submitInfo2.pCommandBuffers = submitCommandBuffers2.data();
    submitInfo2.signalSemaphoreCount = semaImageCopyFinished.size();
    submitInfo2.pSignalSemaphores = semaImageCopyFinished.data();

    if (vkQueueSubmit(_device->graphicsQueue, 1, &submitInfo2, sync.fenceInFlight) != VK_SUCCESS) {
        FATAL("Failed to submit draw command buffer!");
    }

    //  Present the swap chain image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = semaImageCopyFinished.data();

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

    VkPresentTimeGOOGLE presentTime{(uint32_t)_numTicks, time};

    VkPresentTimesInfoGOOGLE presentTimeInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE,
        .pNext = VK_NULL_HANDLE,
        .swapchainCount = 1,
        .pTimes = &presentTime
    };

    if (_tetraMode == TetraMode::kEvenOddSoftwareSync) {
        presentInfo.pNext = &presentTimeInfo;
    }

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
