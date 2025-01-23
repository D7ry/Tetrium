#include "GlobalStates.h"
#include "apps/AppPainter.h"

#include "imgui.h"

namespace TetriumApp
{

void AppPainter::canvasInteract(
    const ImVec2& canvasMousePos,
    const TetriumApp::TickContextImGui& ctx
)
{
    uint32_t x = static_cast<uint32_t>(canvasMousePos.x);
    uint32_t y = static_cast<uint32_t>(canvasMousePos.y);
    if (x >= _canvasWidth || y >= _canvasHeight) {
        return;
    }

    ImVec2 prevPos = _paintingState.prevCanvasMousePos.value_or(canvasMousePos);

    if (_cursorFunction != CursorFunction::Dropper) {
        glm::vec4 selectedColorViewSpace;

        if (_cursorFunction == CursorFunction::Draw) {
            selectedColorViewSpace
                = _tranformMatrixFromRygb[ctx.colorSpace] * _colorPicker.GetSelectedColorRYGB();
            selectedColorViewSpace *= 255.0f;
            selectedColorViewSpace = glm::clamp(selectedColorViewSpace, 0.0f, 255.0f);
            selectedColorViewSpace.a = 255.0f; // override alpha value which is always 0
        } else {
            selectedColorViewSpace = glm::vec4(0, 0, 0, 0);
        }

        // draw brush size preview
        ImGui::GetWindowDrawList()->AddCircleFilled(
            ImGui::GetMousePos(),
            static_cast<float>(_paintingState.brushSize) / 2.0f,
            IM_COL32(
                static_cast<int>(selectedColorViewSpace.r),
                static_cast<int>(selectedColorViewSpace.g),
                static_cast<int>(selectedColorViewSpace.b),
                static_cast<int>(selectedColorViewSpace.a)
            )
        );

        ImGui::GetWindowDrawList()->AddCircle(
            ImGui::GetMousePos(),
            static_cast<float>(_paintingState.brushSize) / 2.0f,
            IM_COL32(255, 255, 255, 255),
            0,
            2.f
        );
        ctx.controls.wantDrawCursor = false;
    }

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

        glm::vec4 currentPixelColorViewSpace
            = _tranformMatrixFromRygb[ctx.colorSpace] * currentPixel;
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


std::string getCurrentTimeForFileName()
{
    // Get the current time as a time_point
    auto now = std::chrono::system_clock::now();

    // Convert to time_t to obtain the time in seconds
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    // Convert to tm struct for formatting
    std::tm tm = *std::localtime(&t);

    // Create a string stream to format the date/time
    std::stringstream ss;

    // Format as YYYY-MM-DD_HH-MM-SS
    ss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");

    return ss.str();
}


void AppPainter::drawImGuiCanvas(const TetriumApp::TickContextImGui& ctx)
{

    if (ImGui::Button("Clear Canvas")) {
        clearCanvas();
    }

    ImGui::SameLine();
    static std::string snapShotDefaultName = "snapshot";
    auto snapShotFileName = snapShotDefaultName + getCurrentTimeForFileName() + ".tiff";
    if (ImGui::Button("save")) {
        saveCanvasToFile("canvas.tiff");
        saveCanvasToFile(snapShotFileName);
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
    if (ImGui::Combo(
            "Brush Stroke", &currentBrush, brushStrokeNames, IM_ARRAYSIZE(brushStrokeNames)
        )) {
        _paintingState.brushType = static_cast<BrushStrokeType>(currentBrush);
    }

    // Draw canvas
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
            ImVec2 canvasMousePos
                = ImVec2(mousePos.x - canvasPos.x, mousePos.y - canvasPos.y);
            canvasInteract(canvasMousePos, ctx);
            _paintingState.prevCanvasMousePos = canvasMousePos;
        } else {
            _paintingState.prevCanvasMousePos = std::nullopt;
        }
    }

    // quick switch between eraser normal and dropper
    if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
        _cursorFunction = CursorFunction::Draw;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_X)) {
        _cursorFunction = CursorFunction::Erase;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_C)) {
        _cursorFunction = CursorFunction::Dropper;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_A)) {
        _paintingState.brushSize += 5;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (_paintingState.brushSize >= 5) {
            _paintingState.brushSize -= 5;
        }
    }

    ImGui::Text("Z : Draw Mode | X : Erase Mode | C: Dropper Mode");
    ImGui::Text("A: Increase Brush Size | S: Decrease Brush Size");
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
        ImGui::Checkbox("Draw Huesphere", &_drawHueSphereInstead);


        if (ImGui::BeginTable("Painter", 2, ImGuiTableFlags_BordersV)) {
            ImGui::TableNextColumn();
            // FIXME: this is a super workaround to render hue sphere, straighten
            // dependencies!
            if (_drawHueSphereInstead) {
                _colorPicker.TickImGuiHueSphere(ctx);
            } else {
                drawImGuiCanvas(ctx);
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
