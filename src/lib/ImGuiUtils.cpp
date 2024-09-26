#include "ImGuiUtils.h"

void ImGuiU::DrawCenteredText(const char* text, const ImVec4& windowBackground)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleColor(ImGuiCol_WindowBg, windowBackground);
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
    ImGui::PopStyleColor();
    ImGui::End();
}
