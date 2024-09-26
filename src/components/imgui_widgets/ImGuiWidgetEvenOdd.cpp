#include <iostream>

#include "ImGuiWidget.h"
#include "VulkanEngine.h"

void ImGuiWidgetEvenOdd::drawColorQuadTest()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    // ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGui::Begin("Color Quad Test");

    if (ImGui::Button("close")) {
        _drawQuadColorTest = false;
    }
    static int currentColor = 0;
    static int currentIntensity = 7; // Start at 128 (2^7)

    const char* colorNames[] = {"Red", "Green", "Blue", "Cyan", "Magenta", "Yellow"};
    ImGui::Combo("Primary Color", &currentColor, colorNames, IM_ARRAYSIZE(colorNames));

    ImGui::SliderInt("Intensity (Power of 2)", &currentIntensity, 0, 8);

    ImVec2 windowSize = ImGui::GetContentRegionAvail();
    ImVec2 quadSize(windowSize.x, windowSize.y);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 quadMin = ImGui::GetCursorScreenPos();
    ImVec2 quadMax(quadMin.x + quadSize.x, quadMin.y + quadSize.y);

    // Calculate color based on selection
    int intensity = 1 << currentIntensity;
    ImU32 quadColor = 0;
    switch (currentColor) {
    case 0:
        quadColor = IM_COL32(intensity, 0, 0, 255);
        break; // Red
    case 1:
        quadColor = IM_COL32(0, intensity, 0, 255);
        break; // Green
    case 2:
        quadColor = IM_COL32(0, 0, intensity, 255);
        break; // Blue
    case 3:
        quadColor = IM_COL32(0, intensity, intensity, 255);
        break; // Cyan
    case 4:
        quadColor = IM_COL32(intensity, 0, intensity, 255);
        break; // Magenta
    case 5:
        quadColor = IM_COL32(intensity, intensity, 0, 255);
        break; // Yellow
    }

    drawList->AddRectFilled(quadMin, quadMax, quadColor);

    ImGui::PopStyleColor();
    ImGui::End();
}

void ImGuiWidgetEvenOdd::drawCalibrationWindow(VulkanEngine* engine, ColorSpace colorSpace)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    // ImGui::SetNextWindowFocus();
    //
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

    if (colorSpace == ColorSpace::RGB) {
        ImGui::Text("Color Space: RGB");
    } else {
        ImGui::Text("Color Space: CMY");
    }

    ImGui::Text("Num Dropped Frame: %u", engine->_evenOddDebugCtx.numDroppedFrames);

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
        };

        // Get the window size
        ImVec2 windowSize = ImGui::GetWindowSize();

        // Calculate the width of each quad
        float quadWidth = windowSize.x / 6.0f;

        int iBegin;
        int iEnd;
        if (colorSpace == ColorSpace::RGB) {
            iBegin = 0;
            iEnd = 3;
        } else { // CMY
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

    ImGui::Checkbox("Flip RGB/CMY", &engine->_flipEvenOdd);
    if (engine->_tetraMode == VulkanEngine::TetraMode::kEvenOddSoftwareSync) {
        ImGui::SliderInt(
            "Software Sync Timing Offset (ns)",
            &engine->_softwareEvenOddCtx.timeOffset,
            0,
            engine->_softwareEvenOddCtx.nanoSecondsPerFrame
        );
        ImGui::Text(
            "Software Sync Frame Time (ns): %lu", engine->_softwareEvenOddCtx.nanoSecondsPerFrame
        );
    }

    ImGui::PopStyleColor();
    ImGui::End();
}

void ImGuiWidgetEvenOdd::Draw(VulkanEngine* engine, ColorSpace colorSpace)
{
    const char* evenOddMode = nullptr;

    uint64_t numFrames = engine->getSurfaceCounterValue();
    switch (engine->_tetraMode) {
    case VulkanEngine::TetraMode::kEvenOddSoftwareSync: {
        evenOddMode = "Software Sync";
    } break;
    case VulkanEngine::TetraMode::kEvenOddHardwareSync:
        evenOddMode = "Hardware Sync";
        break;
    case VulkanEngine::TetraMode::kDualProjector:
        evenOddMode = "Dual Projector Does Not Use Even-Odd rendering";
        break;
    }
    ImGui::Text("Even odd mode: %s", evenOddMode);
    bool isEven = engine->isEvenFrame();

    ImGui::Text("Num Frame: %llu", engine->getSurfaceCounterValue());

    ImGui::Text("Num Dropped Frame: %u", engine->_evenOddDebugCtx.numDroppedFrames);

    if (engine->_tetraMode == VulkanEngine::TetraMode::kEvenOddSoftwareSync) {
        int buf = engine->_softwareEvenOddCtx.vsyncFrameOffset;
        if (ImGui::SliderInt("VSync frame offset", &buf, -10, 10)) {
            engine->_softwareEvenOddCtx.vsyncFrameOffset = buf;
        }
    }

    if (ImGui::Button("Draw Calibration Window")) {
        _drawTestWindow = true;
    }

    if (ImGui::Button("Draw Quad Color Test")) {
        _drawQuadColorTest = true;
    }

    if (_drawTestWindow) {
        drawCalibrationWindow(engine, colorSpace);
    }
    if (_drawQuadColorTest) {
        drawColorQuadTest();
    }

    ImGui::Text("Stress Test");

    if (_stressTesting) {
        ImGui::Text("Stress testing with %i threads...", NUM_STRESS_THREADS);
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            _stressTesting = false;
        }
    } else {
        if (ImGui::Button("Start")) {
            for (int i = 0; i < NUM_STRESS_THREADS; i++) {
                auto stressFunc = [this, i]() {
                    while (_stressTesting) {
                        std::cout << "here" << i << std::endl;
                    }
                    std::cout << i << "done" << std::endl;
                };
                _stressThreads[i] = std::thread(stressFunc);
                _stressThreads[i].detach();
            }
            _stressTesting = true;
        }
    }
}
