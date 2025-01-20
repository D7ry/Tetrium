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

void AppPainter::ColorPicker::SetPickedColor(glm::vec4 rygb)
{
    _selectedColorRYGB = rygb;
    ResetColorPickerCursor();
}

void AppPainter::ColorPicker::ResetColorPickerCursor()
{
    _colorPickerCursorPos = {-1, -1};
}

glm::vec4 AppPainter::ColorPicker::getColorFromCubemapCoord(int x, int y)
{
    if (x < 0 || y < 0 || x >= CUBEMAP_WIDTH || y >= CUBEMAP_HEIGHT) {
        return {0, 0, 0, 0};
    }

    glm::vec4 color;

    int rygbColorPixelIndex = x + y * CUBEMAP_WIDTH;
    constexpr size_t pixelColorSize = sizeof(float) * 4;
    char* colorBegin = static_cast<char*>(_cubemapRYGBTextureCPU.bufferAddress);

    float* pColor
        = reinterpret_cast<float*>(pixelColorSize * rygbColorPixelIndex + colorBegin);

    memcpy(&color.x, pColor, pixelColorSize);

    return color;
}


// update the picked color based on the current cursor position,
// by sampling the pixel from the cubemap texture.
void AppPainter::ColorPicker::updatePickedColor()
{
    int x = _colorPickerCursorPos.x;
    int y = _colorPickerCursorPos.y;
    if (x < 0 || y < 0 || x >= CUBEMAP_WIDTH || y >= CUBEMAP_HEIGHT) {
        return;
    }

    _selectedColorRYGB = getColorFromCubemapCoord(x, y);
}


void AppPainter::ColorPicker::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    updatePickedColor();

    /// render cubemap
    constexpr ImVec2 cubemapSize{CUBEMAP_WIDTH, CUBEMAP_HEIGHT};
    void* cubemapViewSpaceTextureId = _cubemapTextureViewSpace.GetImGuiTextureId();
    ImGui::Image(cubemapViewSpaceTextureId, cubemapSize);

    ImVec2 imagePos = ImGui::GetItemRectMin(); // The top-left corner of the cubemap image

    // cubemap selection logic
    if (ImGui::IsItemHovered()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        // Calculate the mouse position relative to the cubemap image
        ImVec2 relativePos = mousePos - imagePos;
        relativePos.x = std::clamp(relativePos.x, 0.0f, cubemapSize.x);
        relativePos.y = std::clamp(relativePos.y, 0.0f, cubemapSize.y);
        int x = relativePos.x;
        int y = relativePos.y;

        // calculated hovered color
        glm::vec4 hoveredColorRYGB = getColorFromCubemapCoord(x, y);
        glm::vec4 hoeveredColorViewSpace = _tranformMatrixFromRygb[ctx.colorSpace] * hoveredColorRYGB;
        for (int i = 0; i < 4; i++) {
            hoeveredColorViewSpace[i] = std::clamp(hoeveredColorViewSpace[i], 0.f, 1.f);
        }

        if (ImGui::BeginTooltip()) {
            ImGui::ColorButton(
                "hovered color",
                ImVec4{
                    hoeveredColorViewSpace.r,
                    hoeveredColorViewSpace.g,
                    hoeveredColorViewSpace.b,
                    1.f
                }
            );
            ImGui::Text(
                "%.3f %.3f %.3f %.3f",
                hoveredColorRYGB.r,
                hoveredColorRYGB.g,
                hoveredColorRYGB.b,
                hoveredColorRYGB.a
            );
            ImGui::EndTooltip();
        }


        //DEBUG(
        //    " {} {} {} {}",
        //    hoeveredColorViewSpace.r,
        //    hoeveredColorViewSpace.g,
        //    hoeveredColorViewSpace.b,
        //    hoeveredColorViewSpace.w
        //);

        // update persistent cursor position on click
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            _colorPickerCursorPos = {x, y};
            //DEBUG(
            //    "picked color: {} {} {} {}",
            //    _selectedColorRYGB.x,
            //    _selectedColorRYGB.y,
            //    _selectedColorRYGB.z,
            //    _selectedColorRYGB.w
            //);
        }
    }

    // draw picked color indicator on the cubemap
    if (_colorPickerCursorPos.x >= 0 && _colorPickerCursorPos.y >= 0) {
        ImVec2 cursorPos = imagePos + ImVec2(_colorPickerCursorPos.x, _colorPickerCursorPos.y);
        float cursorSize = 5;
        ImGui::GetWindowDrawList()->AddCircle(
            cursorPos, cursorSize, IM_COL32(255, 255, 255, 255), 0, 3.f
        );
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    bool luminanceChanged = 
        ImGui::SliderFloat("Luminance", &_luminance, 0, 1);

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    bool saturationChanged = 
        ImGui::SliderFloat("Saturation", &_saturation, 0, 1);


    _needGenerateNewCubemap = _needGenerateNewCubemap || luminanceChanged || saturationChanged;


    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    ImGui::Separator();

    ImGui::Dummy(ImVec2(0.0f, 20.0f));
    if (ImGui::BeginTable("RYGBColorPreview", 2)) {
        const uint32_t colorPreviewSize = 150;
        ImGui::TableSetupColumn("Color Preview", ImGuiTableColumnFlags_WidthFixed, colorPreviewSize);
        ImGui::TableNextColumn();
        // draw color preview
        glm::vec4 selectedColorViewSpace = _tranformMatrixFromRygb[ctx.colorSpace] * _selectedColorRYGB;
        for (int i = 0; i < 4; i++) {
            selectedColorViewSpace[i] = std::clamp(selectedColorViewSpace[i], 0.f, 1.f);
        }

        ImGui::ColorButton("Selected Color", 
            ImVec4{selectedColorViewSpace.r, selectedColorViewSpace.g, selectedColorViewSpace.b, 1.f},
            0,
            ImVec2(colorPreviewSize, colorPreviewSize)
        );

        ImGui::TableNextColumn();
        // manual rygb control
        bool manualColorOverride = false;
        manualColorOverride |= ImGui::SliderFloat("R", &_selectedColorRYGB.r, 0, 1.0f);
        manualColorOverride |= ImGui::SliderFloat("Y", &_selectedColorRYGB.g, 0, 1.0f);
        manualColorOverride |= ImGui::SliderFloat("G", &_selectedColorRYGB.b, 0, 1.0f);
        manualColorOverride |= ImGui::SliderFloat("B", &_selectedColorRYGB.a, 0, 1.0f);

        if (manualColorOverride) {
            ResetColorPickerCursor();
        }

        ImGui::EndTable();
    }
}

} // namespace TetriumApp
