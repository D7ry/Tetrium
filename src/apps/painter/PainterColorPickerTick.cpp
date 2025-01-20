// Color picker tick time implementations

#include "apps/AppPainter.h"
#include <Pathing.h>
#include <components/ShaderUtils.h>

namespace TetriumApp
{

void AppPainter::ColorPicker::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    vk::CommandBuffer& cb = ctx.commandBuffer;
    if (_needGenerateNewCubemap) {
        { // run cubemap generation pass
            // flush UBO
            CubemapGenerateUBO* pUBO
                = reinterpret_cast<CubemapGenerateUBO*>(_cubemapGenerateContext.ubo.bufferAddress);
            pUBO->luminance = _luminance;
            pUBO->saturation = _saturation;

            // begin render pass to write into new cubemap
            vk::Extent2D extent(CUBEMAP_WIDTH, CUBEMAP_HEIGHT);
            vk::Rect2D renderArea(VkOffset2D{0, 0}, extent);
            vk::RenderPassBeginInfo renderPassBeginInfo(
                _cubemapGenerateContext.renderPass,
                _cubemapTexture.GetFrameBuffer(),
                renderArea,
                _clearValues.size(),
                _clearValues.data()
            );

            cb.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            cb.setViewport(0, vk::Viewport(0.f, 0.f, extent.width, extent.height, 0.f, 1.f));
            cb.setScissor(0, vk::Rect2D({0, 0}, extent));

            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, _cubemapGenerateContext.pipeline);

            cb.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                _cubemapGenerateContext.pipelineLayout,
                0,
                1,
                &_cubemapGenerateContext.descriptorSet,
                0,
                nullptr,
                vk::getDispatchLoaderStatic()
            );
            cb.draw(3, 1, 0, 0);
            cb.endRenderPass();
        }
        { // barrier to ensure cubmap generation pass finishes
            VkMemoryBarrier barrier = vk::MemoryBarrier(
                vk::AccessFlagBits::eColorAttachmentWrite, // write to RYGB texture
                vk::AccessFlagBits::eShaderRead // read from RYGB texture sampler
            );
            // vulkan hpp dispatch doesn't work somehow
            vkCmdPipelineBarrier(
                cb,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                1,
                &barrier,
                0,
                nullptr,
                0,
                nullptr
            );
        }
        { // run rygb transformation pass
            // we assume RYGB ubo has been flushed by the painter already.
            vk::Extent2D extent(CUBEMAP_WIDTH, CUBEMAP_HEIGHT);
            vk::Rect2D renderArea(VkOffset2D{0, 0}, extent);
            vk::RenderPassBeginInfo renderPassBeginInfo(
                _rygbToViewSpaceCtx->renderPass,
                _cubemapTextureViewSpace.GetFrameBuffer(),
                renderArea,
                _clearValues.size(),
                _clearValues.data()
            );
            cb.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
            cb.setViewport(0, vk::Viewport(0.f, 0.f, extent.width, extent.height, 0.f, 1.f));
            cb.setScissor(0, vk::Rect2D({0, 0}, extent));
            cb.bindPipeline(vk::PipelineBindPoint::eGraphics, _rygbToViewSpaceCtx->pipeline);

            cb.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                _rygbToViewSpaceCtx->pipelineLayout,
                0,
                1,
                &_rygbTransformDescriptorSets[ctx.currentFrameInFlight],
                0,
                nullptr,
                vk::getDispatchLoaderStatic()
            );
            cb.draw(3, 1, 0, 0);
            cb.endRenderPass();
        }
        { // TODO: use a memory barrier to block buffer transfer

        }
        {
            VkBufferImageCopy region = {};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {CUBEMAP_WIDTH, CUBEMAP_HEIGHT, 1};

            vk::BufferImageCopy vkRegion(region);

            cb.copyImageToBuffer(
                _cubemapTexture.GetImage(),
                vk::ImageLayout::eGeneral,
                _cubemapRYGBTextureCPU.buffer,
                1,
                &vkRegion
            );
        }

        // FIXME: currently ColorPicker uses one resources across all frames -- this 
        // isn't a huge synchronization issue unless we turn on conditional generation,
        // need to either add NUM_FRAME_IN_FLIGHT rendering resources, or just get rid of 
        // conditional generation
        //_needGenerateNewCubemap = false;
    }
    
}

void AppPainter::ColorPicker::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    // a floating window
    if (ImGui::Begin("Color Picker")) {
        ImGui::Text("RYGB Color Picker");
        // manual rygb control
        ImGui::SliderFloat("R", &_selectedColorRYGB.r, -1.0f, 1.0f);
        ImGui::SliderFloat("Y", &_selectedColorRYGB.g, -1.0f, 1.0f);
        ImGui::SliderFloat("G", &_selectedColorRYGB.b, -1.0f, 1.0f);
        ImGui::SliderFloat("B", &_selectedColorRYGB.a, -1.0f, 1.0f);

        /// render cubemap
        constexpr ImVec2 cubemapSize{CUBEMAP_WIDTH, CUBEMAP_HEIGHT};
        void* cubemapViewSpaceTextureId = _cubemapTextureViewSpace.GetImGuiTextureId();
        ImGui::Image(cubemapViewSpaceTextureId, cubemapSize);

        // cubemap selection logic
        if (ImGui::IsItemClicked()) {
            // Get the mouse position in screen coordinates
            ImVec2 mousePos = ImGui::GetMousePos();

            // Get the position of the cubemap image on the screen
            ImVec2 imagePos = ImGui::GetItemRectMin(); // The top-left corner of the cubemap image

            // Calculate the mouse position relative to the cubemap image
            ImVec2 relativePos = mousePos - imagePos;
            relativePos.x = std::clamp(relativePos.x, 0.0f, cubemapSize.x);
            relativePos.y = std::clamp(relativePos.y, 0.0f, cubemapSize.y);
            int x = relativePos.x;
            int y = relativePos.y;

            int rygbColorPixelIndex = x + y * CUBEMAP_WIDTH;


            INFO("{} {} | {}", x, y, rygbColorPixelIndex);
            char* colorBegin = (char*)_cubemapRYGBTextureCPU.bufferAddress;
            
            float* color
                = reinterpret_cast<float*>(sizeof(float) * 4 * rygbColorPixelIndex + colorBegin);

            _selectedColorRYGB.r = color[0];
            _selectedColorRYGB.g = color[1];
            _selectedColorRYGB.b = color[2];
            _selectedColorRYGB.a = color[3];

        }

        // for debug only
        //ImGui::Image(_cubemapTexture.GetImGuiTextureId(), ImVec2{CUBEMAP_WIDTH, CUBEMAP_HEIGHT});

        bool luminanceChanged = 
            ImGui::SliderFloat("Luminance", &_luminance, 0, 1);
        bool saturationChanged = 
            ImGui::SliderFloat("Saturation", &_saturation, 0, 1);

        _needGenerateNewCubemap = _needGenerateNewCubemap || luminanceChanged || saturationChanged;

    }


    ImGui::End(); // Color Picker
}

} // namespace TetriumApp
