// Color picker implementation for the painter app.
/**
 * real-time 4D color picker.
 * The user visually specifies the four dimensions by:
 * 1. select luminance and saturation using two sliders (2 dims)
 * 2. select a point from the tetrachromatic hue sphere, using a flattened out cubemap(2 dims)
 * The cubmap texture is computed real-time based on luminance and saturation
 *
 * Internally, the fragment shader performs all the color math and writes the RYGB cubemap
 * to a texture. The CPU then addresses the texture to get the selected RYGB color.
 * Presenting the RYGB cubemap is similar to presenting the canvas, where we
 * transform the RYGB color space into RGB/OCV using a 4x3 mat.
 */

#include "apps/AppPainter.h"

namespace TetriumApp
{

// TODO: impl
void AppPainter::ColorPicker::Init()
{
    // Load cubemap texture
}

// TODO: impl
void AppPainter::ColorPicker::Cleanup()
{
    // Cleanup cubemap texture
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
    }


    ImGui::End(); // Color Picker
}

glm::vec4 AppPainter::ColorPicker::GetSelectedColorRYGB() const { return _selectedColorRYGB; }

std::array<float, 4> AppPainter::ColorPicker::GetSelectedColorRYGBData() const
{
    return {_selectedColorRYGB.r, _selectedColorRYGB.g, _selectedColorRYGB.b, _selectedColorRYGB.a};
}

} // namespace TetriumApp
