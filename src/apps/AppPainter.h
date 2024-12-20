#pragma once

#include "imgui.h"

#include "lib/DeletionStack.h"

#include "App.h"
#include "app_components/TextureFrameBuffer.h"

namespace TetriumApp
{
// Tetrachromatic painter that functions in RYGB color space
// The painter contains:
// 1. a color picker widget that visualizes RYGB color space
// 2. an RYGB canvas which the user can paint onto
// 3. an export functionality that saves the canvas to an RYGB file
// 4. a load functionality that loads an RYGB file onto the canvas / view the file.
//
// references:
// https://github.com/OpenGL-Graphics/imgui-paint
// TODO::
// - implement serialization and deserialization of RYGB files
// - (low priority) implement canvas resizing
class AppPainter : public App
{
  private:
    // transformation matrices from RYGB to RGB and OCV color spaces
    // the project renders in RGB and OCV color space.
    std::array<glm::mat4x3, ColorSpace::ColorSpaceSize> _tranformMatrixFromRygb
        = {// RYGB -> RGB
           glm::mat4x3{
               {0.00227389, 0.02027033, 0.84088907},
               {0.09871685, 0.82513837, 0.08254044},
               {-0.0825074, 0.09203826, -0.01052114},
               {0.98151666, -0.02124976, -0.0095099}},
           // RYGB -> OCV
           glm::mat4x3{
               {-0.03549117, 0., 0.},
               {-0.30406722, 0., 0.},
               {0.95542715, 0., 0.},
               {0.06836908, 0., 0.}}};

    // Color picker widget that visualizes RYGB color space through slice of
    // tetrachromatic hue sphere.
    // The user would pick a point from the tetrachromatic cubemap,
    // and use luminance and saturation sliders to adjust the values.
    // This effecitvely allows for intuitive four-dimensional color picking.
    class ColorPicker
    {
      public:
        void Init();
        void Cleanup();

        // Draw the color picker widget
        void TickImGui(const TetriumApp::TickContextImGui& ctx);

        // Get the selected color in RYGB color space
        inline glm::vec4 GetSelectedColorRYGB() const;

        std::array<float, 4> GetSelectedColorRYGBData() const;

      private:
        // update the selected color based on cubemap texture coordinate,
        // luminance, and saturation. Must be called after any of the above changes.
        void updateSelectedColor();

        // currently we only use a fixed texture -- which kind of works for its dimension

        // selected color in RYGB color space
        glm::vec4 _selectedColorRYGB = glm::vec4(1.f);

        // Cubemap texture
        // TODO: add field
        // TODO: implement real-time cubemap that reacts to the sliders.
        // Texture _cubemapTexture;

        // slider values for luminance and saturation
        float _luminance = 1.f;
        float _saturation = 1.f;
    };

  public:
    AppPainter() {}

    ~AppPainter() {}

    virtual void Init(TetriumApp::InitContext& ctx) override;

    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;

    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

    virtual void TickVulkan(TetriumApp::TickContextVulkan& ctx) override;

  private:
    ColorPicker _colorPicker;

    bool _wantDrawColorPicker = false;

    // ---------- Paint space(RYGB) buffers ----------
    //
    // We paint onto buffer RYGB values into RGBA channels, repurposing the alpha channel.
    // so a single pixel is laid out as:
    // | R | Y | G | B | <-- pixel data
    // | R | G | B | A | <-- actual buffer memory
    // the framebuffer is in `VK_FORMAT_R32G32B32A32_SFLOAT` format,
    // as colors in RYGB space may be negative.
    // we write to a CPU-accessible staging buffer; the buffer is flushed to GPU buffer for color
    // space transformation.

    // CPU-accessible buffer to store paint space data
    VQBuffer _paintSpaceBuffer;
    void initPaintSpaceBuffer(TetriumApp::InitContext& ctx);
    void cleanupPaintSpaceBuffer(TetriumApp::CleanupContext& ctx);

    // GPU-accessible texture to sample from in paint space.
    struct PaintSpaceTexture
    {
        vk::Image image = VK_NULL_HANDLE;
        vk::ImageView imageView = VK_NULL_HANDLE;
        vk::DeviceMemory memory = VK_NULL_HANDLE;
        bool needsUpdate = true; // whether the frame buffer needs to be staged, set to `true` when
                                 // `_paintSpaceBuffer` is updated
    };

    std::array<PaintSpaceTexture, NUM_FRAME_IN_FLIGHT> _paintSpaceTexture;
    void initPaintSpaceTexture(TetriumApp::InitContext& ctx);
    void cleanupPaintSpaceTexture(TetriumApp::CleanupContext& ctx);

    // ---------- View space(RGB+OCV) frame buffers ----------

    // Frame buffers are updated by applying the transformation matrices to the RYGB canvas,
    // after which they are sampled by ImGui backend as a texture for rendering.
    std::array<TextureFrameBuffer, NUM_FRAME_IN_FLIGHT> _viewSpaceFrameBuffer;
    void initViewSpaceFrameBuffer(TetriumApp::InitContext& ctx);
    void cleanupViewSpaceFrameBuffer(TetriumApp::CleanupContext& ctx);

    // ---------- Paint to view space transformation context ----------

    enum class BindingLocation : uint32_t
    {
        ubo = 0,
        sampler = 1
    };

    struct UBO
    {
        // either RYGB -> RGB or RYGB -> OCV
        glm::mat4x3 transformMatrix;
    };

    // Render pass that samples from the paint space fb
    // and transforms the colors to RGB and OCV color spaces.
    // the pass relies on a shader that renders onto a full-screen quad.
    //
    // The shader
    // 1. samples from the paint space frame buffer as a texture
    // 2. applies 4x4 transformation matrix
    // depending on the color space,
    struct
    {
        vk::RenderPass renderPass = VK_NULL_HANDLE;

        vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
        vk::Pipeline pipeline = VK_NULL_HANDLE;

        vk::DescriptorPool descriptorPool = VK_NULL_HANDLE;
        vk::DescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        std::array<vk::DescriptorSet, NUM_FRAME_IN_FLIGHT> descriptorSets = {};

        std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> ubo = {};
        std::array<vk::Sampler, NUM_FRAME_IN_FLIGHT> samplers = {};
    } _paintToViewSpaceContext;

    void initPaintToViewSpaceContext(TetriumApp::InitContext& ctx);
    void cleanupPaintToViewSpaceContext(TetriumApp::CleanupContext& ctx);

    std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]

    uint32_t _canvasWidth = 1024;
    uint32_t _canvasHeight = 1024;
    const int PAINT_SPACE_PIXEL_SIZE = 4 * sizeof(float); // R32G32B32A32_SFLOAT

    // ---------- ImGui Runtime Logic ----------

    struct {
        std::optional<ImVec2> prevCanvasMousePos;
        uint32_t brushSize = 5;
    } _paintingState;

    void canvasInteract(const ImVec2& canvasMousePos);
    void brush(uint32_t xBegin, uint32_t yBegin, uint32_t xEnd, uint32_t yEnd);
    void fillPixel(uint32_t x, uint32_t y, const std::array<float, 4>& color);
};
} // namespace TetriumApp
