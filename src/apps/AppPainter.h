#pragma once

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
class AppPainter : public App
{
  private:
    // transformation matrices from RYGB to RGB and OCV color spaces
    // the project renders in RGB and OCV color space.
    // TODO: use actual matrices
    glm::mat4 _tranformMatrixFromRygb[ColorSpace::ColorSpaceSize] = {
        glm::mat4(1.f), // RYGB -> RGB
        glm::mat4(1.f), // RYGB -> OCV
    };

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
        void Draw(const TetriumApp::TickContextImGui& ctx);

        // Get the selected color in RYGB color space
        glm::vec4 GetSelectedColorRYGB() const;

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
    AppPainter();
    ~AppPainter();

    virtual void Init(TetriumApp::InitContext& ctx) override;

    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;

    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

    virtual void TickVulkan(TetriumApp::TickContextVulkan& ctx) override;

  private:
    void Draw(const TetriumApp::TickContextImGui& ctx);

    void DrawColorPickerWidget(const TetriumApp::TickContextImGui& ctx);

    // TODO::
    // - implement serialization and deserialization of RYGB files
    ColorPicker _colorPicker;

    // Paint space(RYGB) frame buffer.
    // We paint onto this texture RYGB values into RGBA channels, repurposing the alpha channel.
    // so a single pixel is laid out as:
    // | R | Y | G | B | -> | R | G | B | A |
    // the framebuffer is in `VK_FORMAT_R32G32B32A32_SFLOAT` format,
    // as colors in RYGB space may be negative.
    TextureFrameBuffer _fbPaintSpace;

    // View space(RGB+OCV) frame buffers.
    // Frame buffers are updated by applying the transformation matrices to the RYGB canvas,
    // after which they are sampled by ImGui backend as a texture for rendering.
    std::array<TextureFrameBuffer, NUM_FRAME_IN_FLIGHT> _fbViewSpace;
};
} // namespace TetriumApp
