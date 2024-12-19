#include "imgui.h"

#include "AppPainter.h"

// FIXME: they shouldn't be here
namespace HardCodedValues
{
int CANVAS_WIDTH = 1024;
int CANVAS_HEIGHT = 1024;
}; // namespace HardCodedValues

namespace TetriumApp
{

// ---------- Color Picker Implementation ----------
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

void AppPainter::ColorPicker::Draw(const TetriumApp::TickContextImGui& ctx)
{
    // a floating window
    if (ImGui::Begin("Color Picker")) {
        ImGui::Text("RYGB Color Picker");
    }
    ImGui::End(); // Color Picker
}

// ---------- Paint Space Frame Buffer Implementation ----------

// TODO: impl
void AppPainter::initPaintSpaceBuffer(TetriumApp::InitContext& ctx) {}

// TODO: impl
void AppPainter::cleanupPaintSpaceBuffer(TetriumApp::CleanupContext& ctx) {}

// TODO: impl
void AppPainter::initPaintSpaceFrameBuffer(TetriumApp::InitContext& ctx) {}

// TODO: impl
void AppPainter::cleanupPaintSpaceFrameBuffer(TetriumApp::CleanupContext& ctx) {}

// TODO: impl
void AppPainter::initPaintToViewSpaceContext(TetriumApp::InitContext& ctx)
{
    // create render pass

    // create graphics pipeline
}

// TODO: impl
void AppPainter::cleanupPaintToViewSpaceContext(TetriumApp::CleanupContext& ctx)
{
    vkDestroyRenderPass(ctx.device.logicalDevice, _paintToViewSpaceContext.renderPass, nullptr);
}

void AppPainter::Init(TetriumApp::InitContext& ctx)
{
    initPaintToViewSpaceContext(ctx);

    ASSERT(_paintToViewSpaceContext.renderPass != VK_NULL_HANDLE);

    // view space frame buffers
    for (auto& fb : _fbViewSpace) {
        fb.Init(
            ctx.device.logicalDevice,
            ctx.device.physicalDevice,
            _paintToViewSpaceContext.renderPass,
            HardCodedValues::CANVAS_WIDTH,
            HardCodedValues::CANVAS_HEIGHT,
            VK_FORMAT_R8G8B8A8_SRGB,
            ctx.device.depthFormat,
            true
        );
    }

    // paint space frame buffer
    initPaintSpaceFrameBuffer(ctx);

    _colorPicker.Init();
}

void AppPainter::Cleanup(TetriumApp::CleanupContext& ctx)
{
    _colorPicker.Cleanup();

    cleanupPaintSpaceFrameBuffer(ctx);
    for (auto& fb : _fbViewSpace) {
        fb.Cleanup();
    }

    cleanupPaintToViewSpaceContext(ctx);
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
    }

    // Pool inputs
    {
        // draw onto the paint space canvas, flagging paint space fbs for update
        


    }

    // Draw canvas
    {

        // TODO: handle canvas resizing here
    }

    // Draw widgets
    {
        // TODO: figure out what widgets to draw
    }

    // Draw color picker widget
    if (_wantDrawColorPicker) {
        _colorPicker.Draw(ctx);
    }

    ImGui::End(); // Painter
}

// TODO: impl
void AppPainter::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    // update paint space framebuffers if needed

    // Transform paint space to view space using the transform pass

}

} // namespace TetriumApp
