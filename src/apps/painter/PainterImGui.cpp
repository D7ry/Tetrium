#include "apps/AppPainter.h"

#include "imgui.h"

namespace TetriumApp
{

void AppPainter::canvasInteract(const ImVec2& canvasMousePos)
{
    uint32_t x = static_cast<uint32_t>(canvasMousePos.x);
    uint32_t y = static_cast<uint32_t>(canvasMousePos.y);
    if (x >= _canvasWidth || y >= _canvasHeight) {
        return;
    }

    brush(_paintingState.prevCanvasMousePos->x, _paintingState.prevCanvasMousePos->y, x, y);
}

// TODO: impl
void AppPainter::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    if (ImGui::Begin(
            "Painter",
            NULL,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoScrollWithMouse
        )) {
        // Draw color picker widget
        if (ImGui::Button("Color Picker")) {
            _wantDrawColorPicker = true;
        }

        if (ImGui::Button("Clear Canvas")) {
            clearCanvas();
        }

        int brushSize = _paintingState.brushSize;
        if (ImGui::SliderInt("Brush Size", &brushSize, 1, 100)) {
            _paintingState.brushSize = brushSize;
        }

        static const char* brushStrokeNames[] = {"Circle", "Square", "Diamond", "SoftCircle"};
        int currentBrush = static_cast<int>(_paintingState.brushType);
        if (ImGui::Combo("Brush Stroke", &currentBrush, brushStrokeNames,
                         IM_ARRAYSIZE(brushStrokeNames))) {
            _paintingState.brushType = static_cast<BrushStrokeType>(currentBrush);
        }

        // Draw canvas
        //
        ImVec2 canvasSize = ImVec2(_canvasWidth, _canvasHeight);
        {
            const TextureFrameBuffer& fb = _viewSpaceFrameBuffer[ctx.currentFrameInFlight];
            ImGui::Image(fb.GetImGuiTextureId(), canvasSize);
        }
        ImVec2 canvasPos = ImGui::GetItemRectMin();
        {
            // check if mouse is within canvas
            ImVec2 mousePos = ImGui::GetMousePos();
            if (mousePos.x >= canvasPos.x && mousePos.x < canvasPos.x + canvasSize.x
                && mousePos.y >= canvasPos.y && mousePos.y < canvasPos.y + canvasSize.y) {
                ImVec2 canvasMousePos = ImVec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y);
                if (ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
                    canvasInteract(canvasMousePos);
                }
                _paintingState.prevCanvasMousePos = canvasMousePos;
            } else {
                _paintingState.prevCanvasMousePos = std::nullopt;
            }
        }

        // Draw widgets
        {
            // TODO: impl
        }

        // Draw color picker widget
        if (_wantDrawColorPicker) {
            _colorPicker.TickImGui(ctx);
        }
    }

    ImGui::End(); // Painter
}
} // namespace TetriumApp
