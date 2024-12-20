#include "apps/AppPainter.h"

#include "imgui.h"

namespace TetriumApp
{
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
    }

    // Pool inputs
    {
        // draw onto the paint space canvas, flagging paint space fbs for update
        // TODO: impl

    }

    // Draw canvas
    {
        // TODO: handle canvas resizing here, do we want to stall the thread?
        const TextureFrameBuffer& fb = _viewSpaceFrameBuffer[ctx.currentFrameInFlight];
        ImVec2 canvasSize = ImVec2(_canvasWidth, _canvasHeight);
        ImGui::Image(fb.GetImGuiTextureId(), canvasSize);
    }

    // Draw widgets
    {
        // TODO: impl
    }

    // Draw color picker widget
    if (_wantDrawColorPicker) {
        _colorPicker.Draw(ctx);
    }

    ImGui::End(); // Painter
}
} // namespace TetriumApp
