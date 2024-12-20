// Color picker implementation for the painter app.

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
