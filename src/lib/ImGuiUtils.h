#pragma once
#include "imgui.h"
#include "structs/ImGuiTexture.h"

// ImGui Utils library
namespace ImGuiU
{

void DrawCenteredText(const char* text, const ImVec4& windowBackground);

void DrawCursor(const ImGuiTexture& cursorTexture);

void DrawFootNote(const char* footnoteText);

bool GradientSliderFloat(
    const char* label,
    float* v,
    float v_min,
    float v_max,
    const ImVec4& left_color,
    const ImVec4& right_color,
    const char* format = "%.3f",
    ImGuiSliderFlags flags = 0
);

} // namespace ImGuiU
