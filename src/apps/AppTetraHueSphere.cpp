#include "imgui.h"

#include "AppTetraHueSphere.h"

namespace TetriumApp
{

void AppTetraHueSphere::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    if (ImGui::Begin("TetraHueSphere")) {
        ImGui::Text("Hello, TetraHueSphere!");

        if (ImGui::Button("Close")) {
            ctx.controls.wantExit = true;
        }
        
    }
    ImGui::End();
}
} // namespace TetriumApp
