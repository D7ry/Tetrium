#include "ImGuiWidget.h"
#include "VulkanEngine.h"

void ImGuiWidgetGraphicsPipeline::Draw(VulkanEngine* engine, ColorSpace colorSpace)
{
    ImGui::SeparatorText("Main Render Pass");
    ImGui::Text("Clear Values");
    {
        ImGui::Indent(10);
        ImGui::ColorEdit4("color", &(engine->_clearValues[0].color.float32[0]));
        ImGui::SliderFloat(
            "depth", &(engine->_clearValues[1].depthStencil.depth), 0, 1
        );
        ImGui::SliderInt(
            "stencil",
            reinterpret_cast<int*>(&(engine->_clearValues[1].depthStencil.stencil)),
            0,
            100
        );
        ImGui::Indent(-10);
    }
}
