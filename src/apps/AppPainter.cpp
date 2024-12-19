#include "imgui.h"

#include "AppPainter.h"

namespace TetriumApp
{

// ---------- Color Picker Implementation ----------
void AppPainter::ColorPicker::Init()
{
    // Load cubemap texture
}

void AppPainter::ColorPicker::Cleanup()
{
    // Cleanup cubemap texture
}

void AppPainter::ColorPicker::Draw(const TetriumApp::TickContextImGui& ctx)
{
    // a floating window
    if (ImGui::Begin("Color Picker")) {
        ImGui::Text("RYGB Color Picker");
    }
    ImGui::End(); // Color Picker
}

// ---------- Paint Space Frame Buffer Implementation ---------- 
void AppPainter::initPaintSpaceFrameBuffer(TetriumApp::InitContext& ctx) {}

void AppPainter::cleanupPaintSpaceFrameBuffer(TetriumApp::CleanupContext& ctx) {}


void AppPainter::Init(TetriumApp::InitContext& ctx)
{
    _colorPicker.Init();
    initPaintSpaceFrameBuffer(ctx);
}

void AppPainter::Cleanup(TetriumApp::CleanupContext& ctx)
{
    _colorPicker.Cleanup();
    cleanupPaintSpaceFrameBuffer(ctx);
}

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

    // Draw canvas

    // Draw widgets

    // Draw color picker widget
    if (_wantDrawColorPicker) {
        _colorPicker.Draw(ctx);
    }

    ImGui::End(); // Painter
}

void AppPainter::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    // Transform paint space to view space
}

} // namespace TetriumApp
