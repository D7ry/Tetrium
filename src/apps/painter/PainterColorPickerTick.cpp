// Color picker tick time implementations

#include "apps/AppPainter.h"
#include <Pathing.h>
#include <components/ShaderUtils.h>

#include "lib/ImGuiUtils.h"

namespace
{
struct ColorPickerInput
{
    float u;
    float v;
    float luminance;
    float saturation;
};

using namespace glm;

// same as the ones in cubemap_rygb_gen.frag
// TODO: pass matrices into UBO instead.
const mat4 g_heringToRYGB = mat4(
    vec4(0.5, 0.5, 0.5, 0.5),
    vec4(-0.28867513459481287, -0.28867513459481287, -0.28867513459481287, 0.866025403784439),
    vec4(-0.408248290463863, -0.408248290463863, 0.8164965809277261, -3.183243964787847e-17),
    vec4(-0.7071067811865477, 0.7071067811865476, -9.80883324026333e-17, -7.571128974804755e-19)
);
const mat3 g_invMetamericDirMat = mat3(
    vec3(0.9865474651933757, 0.009838790494736325, -0.16317872754169252),
    vec3(0.009838790494736328, 0.9928041963993547, 0.11934414863508082),
    vec3(0.16317872754169252, -0.11934414863508083, 0.9793516615927303)
);

glm::vec3 cartesianToSpherical(const glm::vec3& cartesian)
{
    float r = glm::length(cartesian);                // The radius
    float theta = glm::acos(cartesian.z / r);        // Polar angle (colatitude)
    float phi = glm::atan(cartesian.y, cartesian.x); // Azimuthal angle (longitude)

    return glm::vec3(r, theta, phi);
}

glm::vec2 convertCartesianToCubemapUV(glm::vec3 xyz)
{
    // Constants for the 4x3 grid layout
    const float GRID_COLS = 4.0f;
    const float GRID_ROWS = 3.0f;
    const float FACE_WIDTH = 1.0f / GRID_COLS;
    const float FACE_HEIGHT = 1.0f / GRID_ROWS;

    // Find the dominant axis to determine which face we're on
    glm::vec3 absXYZ = glm::abs(xyz);
    float maxAxis = glm::max(glm::max(absXYZ.x, absXYZ.y), absXYZ.z);

    // Initialize UV coordinates
    glm::vec2 uv(0.0f);

    // Convert to local face coordinates and determine grid position
    if (absXYZ.x > absXYZ.y && absXYZ.x > absXYZ.z) {
        // X axis faces (left/right)
        if (xyz.x > 0.0f) {
            // Right face (+X)
            uv = glm::vec2(-xyz.z, xyz.y) / absXYZ.x;
            uv = (uv + glm::vec2(1.0f)) * 0.5f;
            uv.x = uv.x * FACE_WIDTH + (2.0f * FACE_WIDTH); // Third column
        } else {
            // Left face (-X)
            uv = glm::vec2(xyz.z, xyz.y) / absXYZ.x;
            uv = (uv + glm::vec2(1.0f)) * 0.5f;
            uv.x = uv.x * FACE_WIDTH + (0.0f * FACE_WIDTH); // First column
        }
        uv.y = uv.y * FACE_HEIGHT + FACE_HEIGHT; // Middle row
    } else if (absXYZ.y > absXYZ.x && absXYZ.y > absXYZ.z) {
        // Y axis faces (top/bottom)
        if (xyz.y > 0.0f) {
            // Top face (+Y)
            uv = glm::vec2(xyz.x, -xyz.z) / absXYZ.y;
            uv = (uv + glm::vec2(1.0f)) * 0.5f;
            uv.y = uv.y * FACE_HEIGHT + (2.0f * FACE_HEIGHT); // Top row
        } else {
            // Bottom face (-Y)
            uv = glm::vec2(xyz.x, xyz.z) / absXYZ.y;
            uv = (uv + glm::vec2(1.0f)) * 0.5f;
            uv.y = uv.y * FACE_HEIGHT + (0.0f * FACE_HEIGHT); // Bottom row
        }
        uv.x = uv.x * FACE_WIDTH + FACE_WIDTH; // Second column
    } else {
        // Z axis faces (front/back)
        if (xyz.z > 0.0f) {
            // Front face (+Z)
            uv = glm::vec2(xyz.x, xyz.y) / absXYZ.z;
            uv = (uv + glm::vec2(1.0f)) * 0.5f;
            uv.x = uv.x * FACE_WIDTH + FACE_WIDTH; // Second column
        } else {
            // Back face (-Z)
            uv = glm::vec2(-xyz.x, xyz.y) / absXYZ.z;
            uv = (uv + glm::vec2(1.0f)) * 0.5f;
            uv.x = uv.x * FACE_WIDTH + (3.0f * FACE_WIDTH); // Fourth column
        }
        uv.y = uv.y * FACE_HEIGHT + FACE_HEIGHT; // Middle row
    }

    return uv;
}

// get x, y, luminance and saturation value, given RYGB value.
// useful for adjusting color picker input states as user directly changes RYGB values.
ColorPickerInput getColorPickerInputFromRYGB(glm::vec4 rygb) 
{
    // convert RYGB to cartesian
    mat4 rygbToHering = glm::inverse(g_heringToRYGB);
    vec4 hering = rygbToHering * rygb;

    float luminance = hering[0];

    vec3 xyz = vec3(hering[1], hering[2], hering[3]);

    // convert hering to vshh
    vec3 spherical = cartesianToSpherical(vec3(hering.y, hering.z, hering.w));
    vec4 vshh = vec4(luminance, spherical.x, spherical.y, spherical.z);

    float saturation = vshh[1];

    // invert xyz
    xyz = glm::inverse(g_invMetamericDirMat) * xyz;

    // get cubemap UV's
    vec2 uv = convertCartesianToCubemapUV(xyz);

    return ColorPickerInput{
        .u = uv[0], .v = uv[1], .luminance = luminance, .saturation = saturation
    };
}

} // namespace

namespace TetriumApp
{

void AppPainter::ColorPicker::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    vk::CommandBuffer& cb = ctx.commandBuffer;

    auto barrierWaitForFragmentWriteFinishBeforeRead= [cb] () {
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
    };
    { // run cubemap generation pass
        // flush UBO
        CubemapGenerateUBO* pUBO
            = reinterpret_cast<CubemapGenerateUBO*>(_cubemapGenerateContext.ubo.bufferAddress);
        // NOTE: new cubemap uses constant luminance and saturation.
        pUBO->luminance = 1; //_luminance;
        pUBO->saturation = 1; //_saturation;

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
            vk::detail::getDispatchLoaderStatic()
        );
        cb.draw(3, 1, 0, 0);
        cb.endRenderPass();
    }
    
    barrierWaitForFragmentWriteFinishBeforeRead();
    { // run rygb transformation pass
        // we assume RYGB ubo has been flushed by the painter already.
        vk::Extent2D extent(CUBEMAP_WIDTH, CUBEMAP_HEIGHT);
        vk::RenderPassBeginInfo renderPassBeginInfo(
            _rygbToViewSpaceCtx->renderPass,
            _cubemapTextureViewSpace.GetFrameBuffer(),
            vk::Rect2D(VkOffset2D{0, 0}, extent),
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
            vk::detail::getDispatchLoaderStatic()
        );


        // struct
        // {
        //     glm::vec2 cursorMarkUV;
        //     float aspectRatio = (float)CUBEMAP_WIDTH / (float)CUBEMAP_HEIGHT;
        //     float markRadius = 0.005f;
        // } pushConstants;
        //
        // pushConstants.cursorMarkUV = glm::vec2(
        //     (float)_colorPickerCursorPos.x / CUBEMAP_WIDTH,
        //     (float)_colorPickerCursorPos.y / CUBEMAP_HEIGHT
        // );
        //
        // cb.pushConstants(_rygbToViewSpaceCtx->pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(pushConstants),
        //     &pushConstants
        // );
        cb.draw(3, 1, 0, 0);
        cb.endRenderPass();
    }
    // color square generation
    {
        // flush UBO
        struct ColorSquareGenerationUBO* pUBO
            = reinterpret_cast<ColorSquareGenerationUBO*>(_colorSquareContext.ubo.bufferAddress);
        pUBO->cubemap_u =(float)_colorPickerCursorPos.x / CUBEMAP_WIDTH;
        pUBO->cubemap_v =(float)_colorPickerCursorPos.y / CUBEMAP_HEIGHT;

        // begin render pass to write into new cubemap
        vk::Extent2D extent(COLORSQUARE_SIZE, COLORSQUARE_SIZE);
        vk::Rect2D renderArea(VkOffset2D{0, 0}, extent);
        vk::RenderPassBeginInfo renderPassBeginInfo(
            _colorSquareContext.renderPass,
            _colorSquareTexture.GetFrameBuffer(),
            renderArea,
            _clearValues.size(),
            _clearValues.data()
        );

        cb.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        cb.setViewport(0, vk::Viewport(0.f, 0.f, extent.width, extent.height, 0.f, 1.f));
        cb.setScissor(0, vk::Rect2D({0, 0}, extent));

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, _colorSquareContext.pipeline);

        cb.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            _colorSquareContext.pipelineLayout,
            0,
            1,
            &_colorSquareContext.descriptorSet,
            0,
            nullptr,
            vk::detail::getDispatchLoaderStatic()
        );
        cb.draw(3, 1, 0, 0);
        cb.endRenderPass();
    }

    barrierWaitForFragmentWriteFinishBeforeRead();
    { // run rygb transformation pass but for color square
        // we assume RYGB ubo has been flushed by the painter already.
        vk::Extent2D extent(COLORSQUARE_SIZE, COLORSQUARE_SIZE);
        vk::RenderPassBeginInfo renderPassBeginInfo(
            _rygbToViewSpaceCtx->renderPass,
            _colorSquareTextureViewSpace.GetFrameBuffer(),
            vk::Rect2D(VkOffset2D{0, 0}, extent),
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
            &_rygbTransformDescriptorSetsColorSquare[ctx.currentFrameInFlight],
            0,
            nullptr,
            vk::detail::getDispatchLoaderStatic()
        );

        cb.draw(3, 1, 0, 0);
        cb.endRenderPass();
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
        region.imageExtent = {COLORSQUARE_SIZE, COLORSQUARE_SIZE, 1};

        vk::BufferImageCopy vkRegion(region);

        cb.copyImageToBuffer(
            _colorSquareTexture.GetImage(),
            vk::ImageLayout::eGeneral,
            _colorSquareRYGBTextureCPU.buffer,
            1,
            &vkRegion
        );
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
    barrierWaitForFragmentWriteFinishBeforeRead();
    _hueSphere.TickVulkan(ctx);

    // FIXME: currently ColorPicker uses one resources across all frames -- this 
    // isn't a huge synchronization issue unless we turn on conditional generation,
    // need to either add NUM_FRAME_IN_FLIGHT rendering resources, or just get rid of 
    // conditional generation
    //_needGenerateNewCubemap = false;
    
}

void AppPainter::ColorPicker::SetPickedColor(glm::vec4 rygb)
{
    _selectedColorRYGB = rygb;
    updatePickedColorFromRYGB();
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

glm::vec4 AppPainter::ColorPicker::getColorFromColorSquareCoord(int x, int y)
{
    if (x < 0 || y < 0 || x >= COLORSQUARE_SIZE || y >= COLORSQUARE_SIZE) {
        return {0, 0, 0, 0};
    }

    glm::vec4 color;

    int rygbColorPixelIndex = x + y * COLORSQUARE_SIZE;
    constexpr size_t pixelColorSize = sizeof(float) * 4;
    char* colorBegin = static_cast<char*>(_colorSquareRYGBTextureCPU.bufferAddress);

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

void AppPainter::ColorPicker::updatePickedColorColorSquare()
{
    int x = _colorSquareCursorPos.x;
    int y = _colorSquareCursorPos.y;
    if (x < 0 || y < 0 || x >= COLORSQUARE_SIZE || y >= COLORSQUARE_SIZE) {
        return;
    }

    _selectedColorRYGB = getColorFromColorSquareCoord(x, y);
}

void AppPainter::ColorPicker::updatePickedColorFromRYGB()
{
    ColorPickerInput input = getColorPickerInputFromRYGB(_selectedColorRYGB);
    _luminance = input.luminance;
    _saturation = input.saturation;
    _colorPickerCursorPos.x = input.u * CUBEMAP_WIDTH;
    _colorPickerCursorPos.y = input.v * CUBEMAP_HEIGHT;
}

void AppPainter::ColorPicker::TickImGuiHueSphere(const TetriumApp::TickContextImGui& ctx)
{
    _hueSphere.DrawHuesphereInImGui(ctx, _saturation,
        -((float)_colorPickerCursorPos.x  + CUBEMAP_CUBE_SIZE * 3.5)/ CUBEMAP_WIDTH,
        (float)_colorPickerCursorPos.y / CUBEMAP_WIDTH
    );
}

void AppPainter::ColorPicker::TickImGui(const TetriumApp::TickContextImGui& ctx)
{

    /// render cubemap
    const uint32_t cubemapImageColumnSize = CUBEMAP_WIDTH + 10;

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

        ImVec2 rectBegin = {mousePos.x - 30, mousePos.y - 30};
        ImVec2 rectEnd = mousePos;
        ImGui::GetWindowDrawList()->AddRectFilled(
            rectBegin, rectEnd, 
            ImColor(
                hoeveredColorViewSpace.r, hoeveredColorViewSpace.g, hoeveredColorViewSpace.b, 1.f
            )
        );
        ImGui::GetWindowDrawList()->AddRect(
            rectBegin,
            rectEnd,
            IM_COL32_WHITE
        );

        // update persistent cursor position on click
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            _colorPickerCursorPos = {x, y};
            updatePickedColor();
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
    bool luminanceChanged = false;
        //ImGui::SliderFloat("Luminance", &_luminance, 0, 2);

    ImGui::Dummy(ImVec2(0.0f, 5.0f));
    bool saturationChanged = false;
        //ImGui::SliderFloat("Saturation", &_saturation, 0, 1);

    ImGui::Dummy(ImVec2(0.0f, 20.0f));

    

    if (ImGui::BeginTable("RYGB Slider + preview", 2)) {
        ImGui::TableSetupColumn(
            "color preview", ImGuiTableColumnFlags_WidthFixed, COLORSQUARE_SIZE
        );

        ImGui::TableNextColumn();
{ // draw color square
        constexpr ImVec2 colorSquareSize{COLORSQUARE_SIZE, COLORSQUARE_SIZE};
        ImVec2 size{COLORSQUARE_SIZE, COLORSQUARE_SIZE};
        void* textureId = _colorSquareTextureViewSpace.GetImGuiTextureId();
        //textureId = _colorSquareTexture.GetImGuiTextureId();
        //float cubemap_u =(float)_colorPickerCursorPos.x / CUBEMAP_WIDTH;
        //float cubemap_v =(float)_colorPickerCursorPos.y / CUBEMAP_HEIGHT;
        //ImGui::Text("DEBUG: Cubemap UV: %f, %f", cubemap_u, cubemap_v);
        //size = ImVec2{50, 50};
        ImGui::Image(textureId, size);

        imagePos = ImGui::GetItemRectMin();

        if (ImGui::IsItemHovered()){
            ImVec2 mousePos = ImGui::GetMousePos();
            // Calculate the mouse position relative to the cubemap image
            ImVec2 relativePos = mousePos - imagePos;

            //relativePos.x = std::clamp(relativePos.x, 0.0f, colorSquareSize.x);
            //relativePos.y = std::clamp(relativePos.y, 0.0f, colorSquareSize.y);
            int x = relativePos.x;
            int y = relativePos.y;

            // calculated hovered color
            glm::vec4 hoveredColorRYGB = getColorFromColorSquareCoord(x, y);
            glm::vec4 hoeveredColorViewSpace = _tranformMatrixFromRygb[ctx.colorSpace] * hoveredColorRYGB;
            for (int i = 0; i < 4; i++) {
                hoeveredColorViewSpace[i] = std::clamp(hoeveredColorViewSpace[i], 0.f, 1.f);
            }

            ImVec2 rectBegin = {mousePos.x - 30, mousePos.y - 30};
            ImVec2 rectEnd = mousePos;
            ImGui::GetWindowDrawList()->AddRectFilled(
                rectBegin, rectEnd, 
                ImColor(
                    hoeveredColorViewSpace.r, hoeveredColorViewSpace.g, hoeveredColorViewSpace.b, 1.f
                )
            );
            ImGui::GetWindowDrawList()->AddRect(
                rectBegin,
                rectEnd,
                IM_COL32_WHITE
            );

            // update persistent cursor position on click
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                _colorSquareCursorPos = {x, y};
                updatePickedColorColorSquare();
            }

        }

        bool xChanged = ImGui::SliderInt("Cursor X", &_colorSquareCursorPos.x, 0, COLORSQUARE_SIZE - 1);
        bool yChanged = ImGui::SliderInt("Cursor Y", &_colorSquareCursorPos.y, 0, COLORSQUARE_SIZE - 1);
        if (xChanged || yChanged) {
            updatePickedColorColorSquare();
        }

        if (_colorSquareCursorPos.x >= 0 && _colorSquareCursorPos.y >= 0) {
            ImVec2 cursorPos = imagePos + ImVec2(_colorSquareCursorPos.x, _colorSquareCursorPos.y);
            float cursorSize = 5;
            ImGui::GetWindowDrawList()->AddCircle(
                cursorPos, cursorSize, IM_COL32(255, 255, 255, 255), 0, 3.f
            );
        }
    }


        // manual rygb control
        glm::vec4 rDisplaySpace = _tranformMatrixFromRygb[ctx.colorSpace] * glm::vec4(1, 0, 0, 0);
        glm::vec4 yDisplaySpace = _tranformMatrixFromRygb[ctx.colorSpace] * glm::vec4(0, 1, 0, 0);
        glm::vec4 gDisplaySpace = _tranformMatrixFromRygb[ctx.colorSpace] * glm::vec4(0, 0, 1, 0);
        glm::vec4 bDisplaySpace = _tranformMatrixFromRygb[ctx.colorSpace] * glm::vec4(0, 0, 0, 1);

        auto convertToImVec4 = [](const glm::vec4& color) -> ImVec4 {
            return ImVec4{color.r, color.g, color.b, 1.f};
        };
        // Apply alpha = 1 to each color and convert to ImVec4
        ImVec4 r = convertToImVec4(rDisplaySpace);
        ImVec4 y = convertToImVec4(yDisplaySpace);
        ImVec4 g = convertToImVec4(gDisplaySpace);
        ImVec4 b = convertToImVec4(bDisplaySpace);

        ImGui::TableNextColumn();
        bool manualColorOverride = false;
        ImVec4 black = ImVec4{0, 0, 0, 1};
        manualColorOverride
            |= ImGuiU::GradientSliderFloat("R", &_selectedColorRYGB.r, 0, 1.0f, black, r);
        manualColorOverride
            |= ImGuiU::GradientSliderFloat("Y", &_selectedColorRYGB.g, 0, 1.0f, black, y);
        manualColorOverride
            |= ImGuiU::GradientSliderFloat("G", &_selectedColorRYGB.b, 0, 1.0f, black, g);
        manualColorOverride
            |= ImGuiU::GradientSliderFloat("B", &_selectedColorRYGB.a, 0, 1.0f, black, b);

        // picked color widget
        if (1) {
            const uint32_t colorPreviewSize = COLORSQUARE_SIZE / 2;
            glm::vec4 selectedColorViewSpace = _tranformMatrixFromRygb[ctx.colorSpace] * _selectedColorRYGB;
            for (int i = 0; i < 4; i++) {
                selectedColorViewSpace[i] = std::clamp(selectedColorViewSpace[i], 0.f, 1.f);
            }

            ImGui::ColorButton("Selected Color", 
                ImVec4{selectedColorViewSpace.r, selectedColorViewSpace.g, selectedColorViewSpace.b, 1.f},
                0,
                ImVec2(colorPreviewSize, colorPreviewSize)
            );
        }


        if (manualColorOverride) {
            updatePickedColorFromRYGB();
        }
        ImGui::EndTable();
    }

}

} // namespace TetriumApp
