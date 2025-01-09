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

void ImGuiU::DrawCursor(const ImGuiTexture& cursorTexture)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    ImVec2 mousePos = io.MousePos;
    ImVec2 cursorSize(cursorTexture.width * 2, cursorTexture.height * 2);
    ImVec2 cursorPos(mousePos.x, mousePos.y);
    drawList->AddImage(
        (ImTextureID)cursorTexture.id,
        cursorPos,
        ImVec2(cursorPos.x + cursorSize.x, cursorPos.y + cursorSize.y)
    );
}

void ImGuiU::DrawFootNote(const char* footnoteText)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize = io.DisplaySize;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    ImVec2 textSize = ImGui::CalcTextSize(footnoteText);
    ImVec2 padding(10.0f, 5.0f);
    ImVec2 pos(
        windowSize.x - textSize.x - padding.x * 2, windowSize.y - textSize.y - padding.y * 2
    );

    drawList->AddRectFilled(pos, ImVec2(windowSize.x, windowSize.y), IM_COL32(0, 0, 0, 200));
    drawList->AddText(
        ImVec2(pos.x + padding.x, pos.y + padding.y), IM_COL32(255, 255, 255, 255), footnoteText
    );
}
