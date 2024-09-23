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
