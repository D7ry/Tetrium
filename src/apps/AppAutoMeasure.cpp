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

    ImGui::End();
    ImGui::PopStyleColor();
}

}
