#include "apps/AppPainter.h"

#include "imgui.h"

namespace TetriumApp
{

void AppPainter::canvasInteract(const ImVec2& canvasMousePos) {
    uint32_t x = static_cast<uint32_t>(canvasMousePos.x);
    uint32_t y = static_cast<uint32_t>(canvasMousePos.y);
    if (x >= _canvasWidth || y >= _canvasHeight) {
        return;
    }

    // TODO: complete canvas interact logic
    DEBUG("Canvas interact at ({}, {})", x, y);

    const float RYGB_COLOR[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    uint32_t index = y * _canvasWidth + x;
    char* pBuffer = reinterpret_cast<char*>(_paintSpaceBuffer.bufferAddress); // use char for byte arithmetic

    float* pPixel = reinterpret_cast<float*>(pBuffer + index * PAINT_SPACE_PIXEL_SIZE);
    memcpy(pPixel, RYGB_COLOR, PAINT_SPACE_PIXEL_SIZE);

    for (PaintSpaceTexture& texture: _paintSpaceTexture) {
        texture.needsUpdate = true;
    }
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

    // Draw canvas
    {
        const TextureFrameBuffer& fb = _viewSpaceFrameBuffer[ctx.currentFrameInFlight];
        ImVec2 canvasSize = ImVec2(_canvasWidth, _canvasHeight);
        ImGui::Image(fb.GetImGuiTextureId(), canvasSize);

        if (ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
            // check if mouse is within canvas
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 canvasPos = ImGui::GetItemRectMin();
            if (mousePos.x >= canvasPos.x && mousePos.x < canvasPos.x + canvasSize.x
                && mousePos.y >= canvasPos.y && mousePos.y < canvasPos.y + canvasSize.y) {
                // paint
                ImVec2 canvasMousePos = ImVec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y);
                canvasInteract(canvasMousePos);
            }
        }
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
