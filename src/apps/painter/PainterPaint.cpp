#include "apps/AppPainter.h"

#include "imgui.h"
#include <unordered_map>
#include <functional>

namespace TetriumApp
{

const std::unordered_map<AppPainter::BrushStrokeType, std::function<void(const AppPainter::BrushStrokeArgs&)>>
    AppPainter::brushStrokeMap{
        {BrushStrokeType::Circle,
         [](const BrushStrokeArgs& args) {
             float radius = static_cast<float>(args.brushSize) / 2.0f;
             for (int dxOffset = -static_cast<int>(radius); dxOffset <= static_cast<int>(radius);
                  ++dxOffset) {
                 for (int dyOffset = -static_cast<int>(radius);
                      dyOffset <= static_cast<int>(radius);
                      ++dyOffset) {
                     if ((dxOffset * dxOffset + dyOffset * dyOffset) <= radius * radius) {
                         int xToPaint = args.x + dxOffset;
                         int yToPaint = args.y + dyOffset;
                         if (xToPaint >= 0 && xToPaint < static_cast<int>(args.canvasWidth)
                             && yToPaint >= 0 && yToPaint < static_cast<int>(args.canvasHeight)) {
                             args.fillPixel(xToPaint, yToPaint, args.color);
                         }
                     }
                 }
             }
         }},
        {BrushStrokeType::Square,
         [](const BrushStrokeArgs& args) {
             for (int dxOffset = -static_cast<int>(args.brushSize);
                  dxOffset <= static_cast<int>(args.brushSize);
                  ++dxOffset) {
                 for (int dyOffset = -static_cast<int>(args.brushSize);
                      dyOffset <= static_cast<int>(args.brushSize);
                      ++dyOffset) {
                     int xToPaint = args.x + dxOffset;
                     int yToPaint = args.y + dyOffset;
                     if (xToPaint >= 0 && xToPaint < static_cast<int>(args.canvasWidth)
                         && yToPaint >= 0 && yToPaint < static_cast<int>(args.canvasHeight)) {
                         args.fillPixel(xToPaint, yToPaint, args.color);
                     }
                 }
             }
         }},
        {BrushStrokeType::Diamond,
         [](const BrushStrokeArgs& args) {
             for (int i = -static_cast<int>(args.brushSize); i <= static_cast<int>(args.brushSize);
                  ++i) {
                 int span = static_cast<int>(args.brushSize) - abs(i);
                 for (int j = -span; j <= span; ++j) {
                     int xToPaint = args.x + j;
                     int yToPaint = args.y + i;
                     if (xToPaint >= 0 && xToPaint < static_cast<int>(args.canvasWidth)
                         && yToPaint >= 0 && yToPaint < static_cast<int>(args.canvasHeight)) {
                         args.fillPixel(xToPaint, yToPaint, args.color);
                     }
                 }
             }
         }},
        {BrushStrokeType::SoftCircle,
         [](const BrushStrokeArgs& args) {
             float alpha = 0.3f; // example alpha
             float radius = static_cast<float>(args.brushSize) / 2.0f;
             for (int dxOffset = -static_cast<int>(radius); dxOffset <= static_cast<int>(radius);
                  ++dxOffset) {
                 for (int dyOffset = -static_cast<int>(radius); dyOffset <= static_cast<int>(radius);
                      ++dyOffset) {
                     if ((dxOffset * dxOffset + dyOffset * dyOffset) <= radius * radius) {
                         int xToPaint = args.x + dxOffset;
                         int yToPaint = args.y + dyOffset;
                         if (xToPaint >= 0 && xToPaint < static_cast<int>(args.canvasWidth)
                             && yToPaint >= 0 && yToPaint < static_cast<int>(args.canvasHeight)) {
                             auto oldColor = args.fillPixel ? std::array<float,4>{}
                                                         : std::array<float,4>{}; 
                             // We'll retrieve the existing color from getPixel
                             oldColor = args.getPixel(xToPaint, yToPaint);

                             // Blend: old*(1-alpha) + new*(alpha)
                             std::array<float,4> blended = {
                                 oldColor[0]*(1.f-alpha) + args.color[0]*alpha,
                                 oldColor[1]*(1.f-alpha) + args.color[1]*alpha,
                                 oldColor[2]*(1.f-alpha) + args.color[2]*alpha,
                                 // store alpha in 4th channel if needed
                                 oldColor[3]*(1.f-alpha) + args.color[3]*alpha
                             };
                             args.fillPixel(xToPaint, yToPaint, blended);
                         }
                     }
                 }
             }
         }},
    };

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

std::array<float, 4> AppPainter::getPixel(uint32_t x, uint32_t y) const
{
    uint32_t index = y * _canvasWidth + x;
    const char* pBuffer = reinterpret_cast<const char*>(_paintSpaceBuffer.bufferAddress);
    const float* pPixel = reinterpret_cast<const float*>(pBuffer + index * PAINT_SPACE_PIXEL_SIZE);
    return {pPixel[0], pPixel[1], pPixel[2], pPixel[3]};
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

        auto it = brushStrokeMap.find(_paintingState.brushType);
        if (it != brushStrokeMap.end()) {
            BrushStrokeArgs args{
                xBegin, yBegin,
                _colorPicker.GetSelectedColorRYGBData(),
                _paintingState.brushSize,
                _canvasWidth, _canvasHeight,
                [this](uint32_t x, uint32_t y, const std::array<float,4>& c) {
                    fillPixel(x, y, c);
                },
                [this](uint32_t x, uint32_t y) {
                    return getPixel(x, y);
                }
            };
            it->second(args);
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
} // namespace TetriumApp