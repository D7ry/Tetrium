#include "Tetrium.h"

#include "ImGuiWidget.h"

// current setup assumes single logical device
void ImGuiWidgetDeviceInfo::Draw(const Tetrium* engine, ColorSpace colorSpace)
{
    const int INDENT = 20;
    { // GPU
        ImGui::SeparatorText("GPU");
        VQDevice* device = engine->_device.get();
        auto properties = device->properties;
        ImGui::Text("Device: %s", device->properties.deviceName);
        ImGui::Text("Device Extensions: ");
        ImGui::Indent(INDENT);
        for (const char* extension : engine->getRequiredDeviceExtensions()) {
            ImGui::Text("%s", extension);
        }
        ImGui::Indent(-INDENT);
    }
    { // Display
        ImGui::SeparatorText("Display");
        if (engine->_tetraMode == Tetrium::TetraMode::kEvenOddHardwareSync) {
            Tetrium::DisplayContext display = engine->_mainProjectorDisplay;
            ASSERT(display.display);
            ImGui::Text("Using Exclusive Display");
            ImGui::Text("Size: %i x %i", display.extent.width, display.extent.height);
            ImGui::Text("Refresh Rate: %i hz", static_cast<int>(display.refreshrate / 1000.0f));
        }
        GLFWmonitor* monitor = glfwGetWindowMonitor(engine->_window);
        if (monitor) {
            ImGui::Text("Device: %s", glfwGetMonitorName(monitor));
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                ImGui::Text("Size: %i x %i", mode->width, mode->height);
                ImGui::Text("Refresh Rate: %i hz", mode->refreshRate);
            }
        } else {
            ImGui::Text("Application running in windowed mode, no dedicated "
                        "display is used.");
        }
    }
}
