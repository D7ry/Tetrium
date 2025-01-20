#include "GlobalStates.h"
#include "apps/AppPainter.h"

#include "imgui.h"

namespace TetriumApp
{

void AppPainter::canvasInteract(const ImVec2& canvasMousePos,const TetriumApp::TickContextImGui& ctx)
{
    uint32_t x = static_cast<uint32_t>(canvasMousePos.x);
    uint32_t y = static_cast<uint32_t>(canvasMousePos.y);
    if (x >= _canvasWidth || y >= _canvasHeight) {
        return;
    }

    ImVec2 prevPos = _paintingState.prevCanvasMousePos.value_or(canvasMousePos);

    if (_cursorFunction == CursorFunction::Draw) {
        if (ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
            brush(prevPos.x, prevPos.y, x, y, _colorPicker.GetSelectedColorRYGB());
        }
    } else if (_cursorFunction == CursorFunction::Erase) {
        if (ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
            glm::vec4 eraserColor = {0.0f, 0.0f, 0.0f, 0.0f};
            brush(prevPos.x, prevPos.y, x, y, eraserColor);
        }
    } else if (_cursorFunction == CursorFunction::Dropper) {
        // dropper widget
        std::array<float, 4> currentPixelColorRYGB = getPixel(x, y);
        glm::vec4 currentPixel = glm::vec4(
            currentPixelColorRYGB[0],
            currentPixelColorRYGB[1],
            currentPixelColorRYGB[2],
            currentPixelColorRYGB[3]
        );

        glm::vec4 currentPixelColorViewSpace = 
            _tranformMatrixFromRygb[ctx.colorSpace] * currentPixel;
        if (ImGui::BeginTooltip()) {
            ImGui::ColorButton(
                "Dropper Selected Color",
                ImVec4(
                    currentPixelColorViewSpace.r,
                    currentPixelColorViewSpace.g,
                    currentPixelColorViewSpace.b,
                    currentPixelColorViewSpace.a
                ),
                0,
                ImVec2(50, 50)
            );
            ImGui::EndTooltip();
        }

        if (ImGui::IsKeyReleased(ImGuiKey_MouseLeft)) {
            _colorPicker.SetPickedColor(currentPixel);
            _cursorFunction = CursorFunction::Draw; // switch back to draw
        }
    }
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

        if (ImGui::BeginTable("Painter", 2, 
                ImGuiTableFlags_BordersV
            )) {

            ImGui::TableNextColumn();

            if (ImGui::Button("Clear Canvas")) {
                clearCanvas();
            }

            ImGui::SameLine();
            if (ImGui::Button("save")) {
                saveCanvasToFile("canvas.tiff");
            }

            ImGui::SameLine();
            if (ImGui::Button("load")) {
                loadCanvasFromFile("canvas.tiff");
            }

            if (ImGui::RadioButton("Draw", _cursorFunction == CursorFunction::Draw)) {
                _cursorFunction = CursorFunction::Draw;
            }
            ImGui::SameLine();

            if (ImGui::RadioButton("Erase", _cursorFunction == CursorFunction::Erase)) {
                _cursorFunction = CursorFunction::Erase;
            }
            ImGui::SameLine();

            if (ImGui::RadioButton("Dropper", _cursorFunction == CursorFunction::Dropper)) {
                _cursorFunction = CursorFunction::Dropper;
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
                    canvasInteract(canvasMousePos, ctx);
                    _paintingState.prevCanvasMousePos = canvasMousePos;
                } else {
                    _paintingState.prevCanvasMousePos = std::nullopt;
                }
            }

            ImGui::TableNextColumn();

            // Draw color picker widget
            _colorPicker.TickImGui(ctx);

            ImGui::EndTable();
        }

    }

    ImGui::End(); // Painter
}
} // namespace TetriumApp
