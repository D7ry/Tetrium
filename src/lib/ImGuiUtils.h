#pragma once
#include "imgui.h"
#include "structs/ImGuiTexture.h"

// ImGui Utils library
namespace ImGuiU
{

void DrawCenteredText(const char* text, const ImVec4& windowBackground);

void DrawCursor(const ImGuiTexture& cursorTexture);

void DrawFootNote(const char* footnoteText);


} // namespace ImGuiU
