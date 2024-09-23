#include "VulkanEngine.h"

#include "ImGuiWidget.h"

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

    // TODO: maybe render some colored quad?
    if (isEven) {
        ImGui::Text("Is even frame!");
    } else {
        ImGui::Text("Is odd frame!");
    }

    ImGui::Text("Surface Counter: %llu", engine->_surfaceCounterValue);
}
