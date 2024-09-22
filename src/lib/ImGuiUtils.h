#pragma once
#include "imgui.h"

// ImGui Utils library
namespace ImGuiU
{
void DrawCenteredText(const char* text)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowFocus();
    if (ImGui::Begin(
            "Fullscreen",
            nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoSavedSettings
        )) {
        ImVec2 textSize = ImGui::CalcTextSize(text);
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        ImVec2 centerPos((screenSize.x - textSize.x) * 0.5f, (screenSize.y - textSize.y) * 0.5f);

        ImGui::SetCursorPos(centerPos);
        ImGui::Text("%s", text);
    }
    ImGui::End();
}

} // namespace ImGui
