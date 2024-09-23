#include "VulkanEngine.h"

#include "ImGuiWidget.h"

void ImGuiWidgetEvenOdd::drawTestWindow(VulkanEngine* engine)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::SetNextWindowFocus();

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));

    ImGui::Begin(
        "Even Odd Test",
        NULL,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
    );

    if (ImGui::Button("close")) {
        _drawTestWindow = false;
    }

    bool isEven = engine->isEvenFrame();

    if (isEven) {
        ImGui::Text("Is even frame!");
    } else {
        ImGui::Text("Is odd frame!");
    }

    { // draw RGBCMY quads
        ImDrawList* dl = ImGui::GetWindowDrawList();
        // RGBCMY
        ImU32 colors[6] = {
            IM_COL32(255, 0, 0, 255), // Red
            IM_COL32(0, 255, 0, 255), // Green
            IM_COL32(0, 0, 255, 255), // Blue
                                      //
            IM_COL32(255, 0, 0, 255), // Red
            IM_COL32(0, 255, 0, 255), // Green
            IM_COL32(0, 0, 255, 255), // Blue
            // IM_COL32(255, 255, 0, 255), // Yellow
            // IM_COL32(255, 0, 255, 255), // Magenta
            // IM_COL32(0, 255, 255, 255)  // Cyan
        };

        // Get the window size
        ImVec2 windowSize = ImGui::GetWindowSize();

        // Calculate the width of each quad
        float quadWidth = windowSize.x / 6.0f;

        int iBegin; 
        int iEnd;
        if (engine->isEvenFrame()) {
            iBegin = 0;
            iEnd = 3;
        } else {
            iBegin = 3;
            iEnd = 6;
        }
        // Draw 6 evenly spaced quads
        for (int i = iBegin; i < iEnd; i++) {
            ImVec2 p0(i * quadWidth, windowSize.y * 0.2);
            ImVec2 p1((i + 1) * quadWidth, windowSize.y * 0.8);
            dl->AddRectFilled(p0, p1, colors[i]);
        }
    }

    ImGui::PopStyleColor();
    ImGui::End();
}

void ImGuiWidgetEvenOdd::Draw(VulkanEngine* engine)
{
    const char* evenOddMode = nullptr;
    switch (engine->_tetraMode) {
    case VulkanEngine::TetraMode::kEvenOddSoftwareSync:
        evenOddMode = "Software Sync";
        break;
    case VulkanEngine::TetraMode::kEvenOddHardwareSync:
        evenOddMode = "Hardware Sync";
        break;
    case VulkanEngine::TetraMode::kDualProjector:
        evenOddMode = "Dual Projector";
        break;
    }
    ImGui::Text("Even odd mode: %s", evenOddMode);
    ImGui::Checkbox("Flip Even Odd", &engine->_flipEvenOdd);
    bool isEven = engine->isEvenFrame();

    if (isEven) {
        ImGui::Text("Is even frame!");
    } else {
        ImGui::Text("Is odd frame!");
    }

    ImGui::Text("Surface Counter: %llu", engine->_surfaceCounterValue);

    if (ImGui::Button("Draw Test Window")) {
        _drawTestWindow = true;
    }

    if (_drawTestWindow) {
        drawTestWindow(engine);
    }
}
