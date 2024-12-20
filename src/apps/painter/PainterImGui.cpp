#include "apps/AppPainter.h"

#include "imgui.h"

namespace TetriumApp
{

void AppPainter::fillPixel(uint32_t x, uint32_t y, const std::array<float, 4>& color)
{
    ASSERT(x < _canvasWidth && y < _canvasHeight);
    uint32_t index = y * _canvasWidth + x;
    char* pBuffer
        = reinterpret_cast<char*>(_paintSpaceBuffer.bufferAddress); // use char for byte arithmetic

    float* pPixel = reinterpret_cast<float*>(pBuffer + index * PAINT_SPACE_PIXEL_SIZE);
    memcpy(pPixel, color.data(), PAINT_SPACE_PIXEL_SIZE);
}

void AppPainter::canvasInteract(const ImVec2& canvasMousePos)
{
    uint32_t x = static_cast<uint32_t>(canvasMousePos.x);
    uint32_t y = static_cast<uint32_t>(canvasMousePos.y);
    if (x >= _canvasWidth || y >= _canvasHeight) {
        return;
    }

    // TODO: complete canvas interact logic
    DEBUG("Canvas interact at ({}, {})", x, y);

    auto color = _colorPicker.GetSelectedColorRYGBData();

    uint32_t prevX = x;
    uint32_t prevY = y;
    if (_paintingState.prevCanvasMousePos.has_value()) {
        ImVec2 prevCanvasMousePos = _paintingState.prevCanvasMousePos.value();
        prevX = static_cast<uint32_t>(prevCanvasMousePos.x);
        prevY = static_cast<uint32_t>(prevCanvasMousePos.y);
        DEBUG("Prev canvas interact at ({}, {})", prevX, prevY);
    }
    // fill pixels between prev and current mouse pos
    // Bresenham's line algorithm https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
    int dx = abs(static_cast<int>(x) - static_cast<int>(prevX));
    int dy = abs(static_cast<int>(y) - static_cast<int>(prevY));
    int sx = prevX < x ? 1 : -1;
    int sy = prevY < y ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    int e2;

    while (true) {
        fillPixel(prevX, prevY, color);
        if (prevX == x && prevY == y) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; prevX += sx; }
        if (e2 < dy) { err += dx; prevY += sy; }
    }

    // flag textures for update
    for (PaintSpaceTexture& texture : _paintSpaceTexture) {
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

    ImGui::End(); // Painter
}
} // namespace TetriumApp
