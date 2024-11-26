#include "ImGuiWidgetColorTile.h"
#include "imgui.h"

namespace
{
ImU32 ConvertFloatToImU32(float* rgba)
{
    // Assuming rgba is a float array with 4 elements: {R, G, B, A}
    ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]));
    return color;
}
} // namespace

void ImGuiWidgetColorTile::Draw(Tetrium* engine, ColorSpace colorSpace)
{
    if (ImGui::Begin("Color Tile")) {
        // color picker widget
        // left RGB, left OCV, right RGB, right OCV
        ImGui::SeparatorText("Left");
        ImGui::ColorEdit4("RGBLeft", &_leftTileColor[RGB][0]);
        ImGui::ColorEdit4("OCVLeft", &_leftTileColor[OCV][0]);

        ImGui::SeparatorText("Right");
        ImGui::ColorEdit4("RGBRight", &_rightTileColor[RGB][0]);
        ImGui::ColorEdit4("OCVRight", &_rightTileColor[OCV][0]);

        // draw four color tiles side-by-side
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 windowSize = ImGui::GetWindowSize();

        // 2 quads, l and r
        float quadWidth = windowSize.x / 2.f;
        float quadHeight = windowSize.y * 0.8f;

        // draw two tiles
        float* pLeftColor = _leftTileColor[colorSpace];
        ImU32 leftColor = ConvertFloatToImU32(pLeftColor);

        float* pRightColor = _rightTileColor[colorSpace];
        ImU32 rightColor = ConvertFloatToImU32(pRightColor);

        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 cursorPos = ImGui::GetCursorPos();

        ImVec2 origin = ImVec2{winPos.x, winPos.y + cursorPos.y};

        // black background

        dl->AddRectFilled(
            origin,
            origin + ImVec2{windowSize.x, quadHeight},
            IM_COL32(0, 0, 0, 255)
        );

        ImVec2 leftOrigin = origin;
        ImVec2 leftMax{leftOrigin.x + quadWidth, leftOrigin.y + quadHeight};
        dl->AddRectFilled(leftOrigin, leftMax, leftColor);

        ImVec2 rightOrigin{leftOrigin.x + quadWidth, leftOrigin.y};
        ImVec2 rightMax{leftMax.x + quadWidth, leftMax.y};
        
        dl->AddRectFilled(rightOrigin, rightMax, rightColor);
    }
    ImGui::End(); // Color Tile
}
