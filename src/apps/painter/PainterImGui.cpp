#include "apps/AppPainter.h"

#include "imgui.h"

namespace TetriumApp
{
void AppPainter::clearCanvas()
{
    const std::array<float, 4> clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
    for (uint32_t y = 0; y < _canvasHeight; ++y) {
        for (uint32_t x = 0; x < _canvasWidth; ++x) {
            fillPixel(x, y, clearColor);
        }
    }
    flagTexturesForUpdate();
}

void AppPainter::fillPixel(uint32_t x, uint32_t y, const std::array<float, 4>& color)
{
    ASSERT(x < _canvasWidth && y < _canvasHeight);
    uint32_t index = y * _canvasWidth + x;
    char* pBuffer
        = reinterpret_cast<char*>(_paintSpaceBuffer.bufferAddress); // use char for byte arithmetic

    float* pPixel = reinterpret_cast<float*>(pBuffer + index * PAINT_SPACE_PIXEL_SIZE);
    memcpy(pPixel, color.data(), PAINT_SPACE_PIXEL_SIZE);
}

void AppPainter::brush(uint32_t xBegin, uint32_t yBegin, uint32_t xEnd, uint32_t yEnd)
{
    auto color = _colorPicker.GetSelectedColorRYGBData();

    if (_paintingState.prevCanvasMousePos.has_value()) {
        ImVec2 prevCanvasMousePos = _paintingState.prevCanvasMousePos.value();
        xBegin = static_cast<uint32_t>(prevCanvasMousePos.x);
        yBegin = static_cast<uint32_t>(prevCanvasMousePos.y);
        // DEBUG("Prev canvas interact at ({}, {})", xBegin, yBegin);
    }

    // Handle the brush size from _paintingState
    uint32_t brushSize = _paintingState.brushSize;

    // Bresenham's line algorithm to fill pixels in the line
    int dx = abs(static_cast<int>(xEnd) - static_cast<int>(xBegin));
    int dy = abs(static_cast<int>(yEnd) - static_cast<int>(yBegin));
    int sx = xBegin < xEnd ? 1 : -1;
    int sy = yBegin < yEnd ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;
    int e2;

    // not the most efficient way to paint -- brushstroke is ideally implemented through a shader.
    // CPU-based approach is performant enough for now however.
    while (true) {
        DEBUG("Painting at ({}, {})", xBegin, yBegin);
        // Apply the brush size by iterating over a circular region around the current pixel
        float radius = static_cast<float>(brushSize) / 2.0f;
        for (int dxOffset = -static_cast<int>(radius); dxOffset <= static_cast<int>(radius);
             ++dxOffset) {
            for (int dyOffset = -static_cast<int>(radius); dyOffset <= static_cast<int>(radius);
                 ++dyOffset) {
                // Check if the pixel lies within the circle of the brush
                if ((dxOffset * dxOffset + dyOffset * dyOffset) <= radius * radius) {
                    int xToPaint = xBegin + dxOffset;
                    int yToPaint = yBegin + dyOffset;
                    // Ensure the painted pixel is within bounds
                    if (xToPaint >= 0 && xToPaint < _canvasWidth && yToPaint >= 0
                        && yToPaint < _canvasHeight) {
                        // DEBUG("Painting at ({}, {})", xToPaint, yToPaint);
                        fillPixel(xToPaint, yToPaint, color);
                    }
                }
            }
        }

        if (xBegin == xEnd && yBegin == yEnd)
            break;
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            xBegin += sx;
        }
        if (e2 < dy) {
            err += dx;
            yBegin += sy;
        }
    }
    flagTexturesForUpdate();
}

void AppPainter::flagTexturesForUpdate()
{
    for (PaintSpaceTexture& texture : _paintSpaceTexture) {
        texture.needsUpdate = true;
    }
}

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
