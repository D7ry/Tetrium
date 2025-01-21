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
    // NOTE: the 4th row is not used but required for std140 layout padding
    static inline std::array<glm::mat4x4, ColorSpace::ColorSpaceSize> _tranformMatrixFromRygb = {
        glm::mat4x4{
            {0.9815166593137846, -0.021249756545876134, -0.009509897322450037, 0.0},
            {-0.08250740137899303, 0.0920382578273052, -0.010521143643565989, 0.0},
            {0.09871685006945341, 0.8251383685884338, 0.08254044197374301, 0.0},
            {0.0022738919957916515, 0.02027032667801149, 0.8408890714703111, 0.0},
        },
        glm::mat4x4{
            {0.06836907784191643, 0.0, 0.0, 0.0},
            {0.9554271494416933, 0.0, 0.0, 0.0},
            {-0.3040672192082057, 0.0, 0.0, 0.0},
            {-0.035491169221697684, 0.0, 0.0, 0.0},
        },
    };

    // Render pass that samples from the paint space fb
    // and transforms the colors to RGB and OCV color spaces.
    // the pass relies on a shader that renders onto a full-screen quad.
    //
    // The shader
    // 1. samples from the paint space frame buffer as a texture
    // 2. applies 4x4 transformation matrix
    // depending on the color space,
    struct RYGBToViewSpaceContext
    {
        vk::RenderPass renderPass = VK_NULL_HANDLE;

        vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
        vk::Pipeline pipeline = VK_NULL_HANDLE;

        vk::DescriptorPool descriptorPool = VK_NULL_HANDLE;
        vk::DescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

        std::array<VQBuffer, NUM_FRAME_IN_FLIGHT> ubo = {};
        std::array<vk::Sampler, NUM_FRAME_IN_FLIGHT> samplers = {};
    };

    enum class RYGBToViewSpaceBindingLocation : uint32_t
    {
        ubo = 0,
        textureSampler = 1
    };

    struct RYGBToViewSpaceUBO
    {
        // either RYGB -> RGB or RYGB -> OCV
        // note the 4th row is unused in shader and is only used for std140 padding.
        glm::mat4x4 transformMatrix;
    };

    // GPU-accessible texture to sample from in paint space.
    struct SharedTexture
    {
        vk::Image image = VK_NULL_HANDLE;
        vk::ImageView imageView = VK_NULL_HANDLE;
        vk::DeviceMemory memory = VK_NULL_HANDLE;
        bool needsUpdate = true; // whether the frame buffer needs to be staged, set to `true` when
                                 // `_paintSpaceBuffer` is updated
    };

    // Color picker widget that visualizes RYGB color space through slice of
    // tetrachromatic hue sphere.
    // The user would pick a point from the tetrachromatic cubemap,
    // and use luminance and saturation sliders to adjust the values.
    // This effecitvely allows for intuitive four-dimensional color picking.
    class ColorPicker
    {
      public:
        void Init(TetriumApp::InitContext& ctx, RYGBToViewSpaceContext* rygbToViewspaceCtx);
        void Cleanup(TetriumApp::CleanupContext& ctx);

        // Draw the color picker widget
        void TickImGui(const TetriumApp::TickContextImGui& ctx);

        void TickVulkan(TetriumApp::TickContextVulkan& ctx);

        // Get the selected color in RYGB color space
        glm::vec4 GetSelectedColorRYGB() const;

        std::array<float, 4> GetSelectedColorRYGBData() const;

        // reset the cursor position of the color picker, removing the selection
        void ResetColorPickerCursor();

        // override the picked color, at the same time resetting the cursor position
        // to be off the cubemap.
        void SetPickedColor(glm::vec4 rygb);

      private:
        enum class CubemapGenerationBindingLocation : uint32_t
        {
            ubo = 0,
            vshMaxSaturationLUT = 1
        };

        // currently we only use a fixed texture -- which kind of works for its dimension

        // selected color in RYGB color space
        glm::vec4 _selectedColorRYGB = glm::vec4(1.f);

        // Cubemap texture that we `_cubemapGenerateContext` render into
        // the texture is used for:
        // 1. RYGB color space texture that is sampled by `RYGBToViewSpaceContext`'s
        // pass to generate view space cubemap texture -- _cubemapTextureViewSpace
        // 2.[TODO] being copied to CPU-accessible staging buffer for color-picking
        TextureFrameBuffer _cubemapTexture;

        // view space cubemap texture in RGB/OCV color space
        TextureFrameBuffer _cubemapTextureViewSpace;

        // slider values for luminance and saturation
        float _luminance = 1.f;
        float _saturation = 1.f;

        bool _needGenerateNewCubemap = true;

        std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]

        static const uint32_t CUBEMAP_CUBE_SIZE = 256;
        static const uint32_t CUBEMAP_WIDTH = 4 * CUBEMAP_CUBE_SIZE;
        static const uint32_t CUBEMAP_HEIGHT = 3 * CUBEMAP_CUBE_SIZE;

        RYGBToViewSpaceContext* _rygbToViewSpaceCtx; // points to painter's transform context TODO: make it better

        // render context to generate an RYGB cubemap texture,
        // using luminance, saturation, and cubemap texture coordinate.
        // writes to `_cubemapTexture`
        struct
        {
            vk::RenderPass renderPass = VK_NULL_HANDLE;

            vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
            vk::Pipeline pipeline = VK_NULL_HANDLE;

            vk::DescriptorPool descriptorPool = VK_NULL_HANDLE;
            vk::DescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

            vk::DescriptorSet descriptorSet = VK_NULL_HANDLE;

            VQBuffer ubo = {};

            uint32_t vshMaxSaturationLUTTextureHandle = 0;

        } _cubemapGenerateContext;

        // descriptor sets for transforming from rygb fb to imgui fb
        std::array<vk::DescriptorSet, NUM_FRAME_IN_FLIGHT> _rygbTransformDescriptorSets{};

        struct CubemapGenerateUBO
        {
            float luminance;
            float saturation;
        };

        void initCubemapGenerateContext(TetriumApp::InitContext& ctx);
        void cleanupCubemapGenerateContext(TetriumApp::CleanupContext& ctx);

        void initRYGBTransform(TetriumApp::InitContext& ctx);

        // cursor position of the color picker
        // negative values indicate no seletion
        struct
        {
            int x = -1;
            int y = -1;
        } _colorPickerCursorPos;

        glm::vec4 getColorFromCubemapCoord(int x, int y);
        

        void updatePickedColor();
        void updatePickedColorFromRYGB();

        // CPU-accessible RYGB buffer
        VQBuffer _cubemapRYGBTextureCPU{};
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


    std::array<SharedTexture, NUM_FRAME_IN_FLIGHT> _paintSpaceTexture;
    void initPaintSpaceTexture(TetriumApp::InitContext& ctx);
    void cleanupPaintSpaceTexture(TetriumApp::CleanupContext& ctx);

    // ---------- View space(RGB+OCV) frame buffers ----------

    // Frame buffers are updated by applying the transformation matrices to the RYGB canvas,
    // after which they are sampled by ImGui backend as a texture for rendering.
    std::array<TextureFrameBuffer, NUM_FRAME_IN_FLIGHT> _viewSpaceFrameBuffer;
    void initViewSpaceFrameBuffer(TetriumApp::InitContext& ctx);
    void cleanupViewSpaceFrameBuffer(TetriumApp::CleanupContext& ctx);

    // ---------- Paint to view space transformation context ----------

    // shared between painter and color picker
    RYGBToViewSpaceContext _paintToViewSpaceContext;
    // descriptor sets 
    std::array<vk::DescriptorSet, NUM_FRAME_IN_FLIGHT> _canvasToViewSpaceDescriptorSets = {};

    void initPaintToViewSpaceContext(TetriumApp::InitContext& ctx);
    void cleanupPaintToViewSpaceContext(TetriumApp::CleanupContext& ctx);

    void initDescriptorSets(TetriumApp::InitContext& ctx);

    std::array<vk::ClearValue, 2> _clearValues; // [color, depthStencil]

    uint32_t _canvasWidth = 1024;
    uint32_t _canvasHeight = 1024;
    const int PAINT_SPACE_PIXEL_SIZE = 4 * sizeof(float); // R32G32B32A32_SFLOAT

    // ---------- ImGui Runtime Logic ----------

    enum class BrushStrokeType : uint32_t
    {
        Circle = 0,
        Square,
        Diamond,
        SoftCircle,
        BrushStrokeCount
    };

    struct BrushStrokeArgs
    {
        uint32_t x;
        uint32_t y;
        std::array<float, 4> color;
        uint32_t brushSize;
        uint32_t canvasWidth;
        uint32_t canvasHeight;
        std::function<void(uint32_t, uint32_t, const std::array<float,4>&)> fillPixel;
        std::function<std::array<float,4>(uint32_t, uint32_t)> getPixel;
    };

    static const std::array<std::function<void(const BrushStrokeArgs&)>,
                       static_cast<size_t>(BrushStrokeType::BrushStrokeCount)>
        brushStrokeArray;

    struct
    {
        std::optional<ImVec2> prevCanvasMousePos;
        uint32_t brushSize = 5;
        BrushStrokeType brushType = BrushStrokeType::Circle;
    } _paintingState;

    enum CursorFunction
    {
        Draw,
        Erase,
        Dropper
    };

    CursorFunction _cursorFunction = CursorFunction::Draw;

    void clearCanvas();

    void flagTexturesForUpdate();

    // Drawing and brushstrokes
    void canvasInteract(const ImVec2& canvasMousePos,const TetriumApp::TickContextImGui& ctx);
    void brush(uint32_t xBegin, uint32_t yBegin, uint32_t xEnd, uint32_t yEnd, const glm::vec4& color);
    void fillPixel(uint32_t x, uint32_t y, const std::array<float, 4>& color);
    std::array<float, 4> getPixel(uint32_t x, uint32_t y) const;

    // ---------- Serialization ----------
    // We serialize and de-serialize the canvas using the TIFF format,
    // the format supports 32-bit floating point values for up to 4 channels.
    void saveCanvasToFile(const std::string& filename);
    void loadCanvasFromFile(const std::string& filename);
};
} // namespace TetriumApp
