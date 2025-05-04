#include <thread>
#include "AppAutoMeasure.h"
#include "imgui.h"

#include "app_components/PR650.h"


namespace {
    PR650* IPR650;
    
    bool IPR650AttempetedInit();
}

namespace TetriumApp {

void AppAutoMeasure::Init(TetriumApp::InitContext& ctx) {
    std::string portName = "COM3";
    IPR650 = new PR650(portName);
};

void AppAutoMeasure::Cleanup(TetriumApp::CleanupContext& ctx) {
    delete IPR650;
};

void AppAutoMeasure::TickImGui(const TetriumApp::TickContextImGui& ctx) {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGuiWindowFlags flags = 0;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize;

    if (ImGui::Begin("measure", NULL, flags)) {
        if (!IPR650->isConnected()) {
            ImGui::Text("PR650 not connected");
            ImGui::SameLine();
            if (pr650States.connecting == false) {
                if (ImGui::Button("connect to PR650")) {
                    std::thread t([] () {IPR650->Init();});
                    t.detach();
                    pr650States.connecting = true;
                }
            } else {
                ImGui::Text("Spinning thread to attempt connection...");
            }
        } else {
            ImGui::Text("PR650 connected!");
        }
    }

    if (IPR650->isConnected()) {
        if (!pr650States.measuring) {
            // if not measuring, prompt user to start measuring
            if (ImGui::Button("Measure")) {
                std::thread t([]() {
                    IPR650->StartMeasuring();
                });
                t.detach();
                pr650States.measuring = true;
            }
        } else {
            // query pr650 to see if data is ready
            if (IPR650->MeasureResult.ready) {
                pr650States.measuring = false;
                // read back pr650 states
                DEBUG("PR650 results:");
                auto& result = IPR650->MeasureResult;
                DEBUG("luminance: {}", result.luminance);
                for (int i = 0; i < result.power.size(); i++) {
                    double power = result.power[i];
                    double wavelength = result.wavelength[i];
                    DEBUG("{} : {} {}", i, power, wavelength);
                }
            }
            ImGui::Text("PR650 measuring");
        }
    }
    static glm::ivec4 rgbo = {255, 0, 0, 255};
    constexpr std::array<const char*, 4> labels = {
        "r", "g", "b", "o"
    };

    for (int i = 0; i < 4; i++) {
        const char* label = labels.at(i);
        ImGui::SliderInt(label, &rgbo[i], 0, 255);
    }
    
    drawColorBlock(ctx, rgbo);
    ImGui::End();
    ImGui::PopStyleColor();
}

void AppAutoMeasure::drawColorBlock(const TetriumApp::TickContextImGui& ctx, glm::ivec4 rgbo) {
    ImVec2 start_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    // Define the rectangle from current draw line (cursor Y) to the bottom of the window
    ImVec2 rect_min = start_pos;
    ImVec2 rect_max = ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y);

    // Choose your color (RGBA)
    ImU32 color = ctx.colorSpace == RGB ? IM_COL32(rgbo.x, rgbo.y, rgbo.z, 255) : IM_COL32(rgbo.w, 0, 0, 255);

    // Draw the filled rectangle
    ImGui::GetWindowDrawList()->AddRectFilled(rect_min, rect_max, color);
}
}
