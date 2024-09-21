#include "VulkanEngine.h"

#include "ImGuiWidget.h"

void ImGuiWidgetEvenOdd::Draw(VulkanEngine* engine)
{
    ImGui::Text("Surface Counter: %lu", engine->_surfaceCounterValue);
    ImGui::Checkbox("Flip Even Odd", &engine->_flipEvenOdd);
    bool isEven = engine->isEvenFrame();

    // TODO: maybe render some colored quad?
    if (isEven) {
        ImGui::Text("Is even frame!");
    } else {
        ImGui::Text("Is odd frame!");
    }
}
