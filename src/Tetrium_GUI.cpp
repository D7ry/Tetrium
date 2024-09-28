// ImGUI subroutine implementations

#include "lib/ImGuiUtils.h"

#include "Tetrium.h"

namespace Tetrium_GUI
{
void drawCursor(ImGuiManager* imguiManager)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    ImVec2 mousePos = io.MousePos;
    // TODO: store all engine imgui assets in a table
    const ImGuiTexture& cursorTexture
        = imguiManager->GetImGuiTexture("../assets/textures/engine/cursor.png");
    ImVec2 cursorSize(cursorTexture.width * 2, cursorTexture.height * 2);
    ImVec2 cursorPos(mousePos.x, mousePos.y);
    drawList->AddImage(
        (ImTextureID)cursorTexture.ds,
        cursorPos,
        ImVec2(cursorPos.x + cursorSize.x, cursorPos.y + cursorSize.y)
    );
}

void drawFootNote()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize = io.DisplaySize;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    const char* footnoteText = (const char*)u8"🧩 Tetrium 0.3a";
    ImVec2 textSize = ImGui::CalcTextSize(footnoteText);
    ImVec2 padding(10.0f, 5.0f);
    ImVec2 pos(
        windowSize.x - textSize.x - padding.x * 2, windowSize.y - textSize.y - padding.y * 2
    );

    drawList->AddRectFilled(pos, ImVec2(windowSize.x, windowSize.y), IM_COL32(0, 0, 0, 200));
    drawList->AddText(
        ImVec2(pos.x + padding.x, pos.y + padding.y), IM_COL32(255, 255, 255, 255), footnoteText
    );
}

}; // namespace Tetrium_GUI

// parent function to draw imgui; sets up all contexts and performs drawing.
void Tetrium::drawImGui(ColorSpace colorSpace)
{
    if (!_wantToDrawImGui) {
        return;
    }

    // ---------- Prologue ----------
    // note that drawImGui is called twice per tick for RGB and OCV space,
    // so we need different profiler ID for them.
    const char* profileId = colorSpace == ColorSpace::RGB ? "ImGui Draw RGB" : "ImGui Draw OCV";
    PROFILE_SCOPE(&_profiler, profileId);
    _imguiManager.BeginImGuiContext();

    // imgui is associated with the glfw window to handle inputs,
    // but its actual fb is associated with the projector display;
    // so we need to manually re-adjust the display size for the scissors/
    // viewports/clipping to be consistent
    bool imguiDisplaySizeOverride = _tetraMode == TetraMode::kEvenOddHardwareSync;
    if (imguiDisplaySizeOverride) {
        ImVec2 projectorDisplaySize{
            static_cast<float>(_mainProjectorDisplay.extent.width),
            static_cast<float>(_mainProjectorDisplay.extent.height)
        };
        _imguiManager.forceDisplaySize(projectorDisplaySize);
    }

    if (!_windowFocused) {
        ImGuiU::DrawCenteredText("Press Tab to enable input", ImVec4(0, 0, 0, 0.8));
    } else if (_uiMode) { // window focused and in ui mode, draw cursor
        Tetrium_GUI::drawCursor(&_imguiManager);
    }
    Tetrium_GUI::drawFootNote();

    if (ImGui::Begin(DEFAULTS::Engine::APPLICATION_NAME)) {
        if (ImGui::BeginTabBar("Engine Tab")) {
            if (ImGui::BeginTabItem("General")) {
                ImGui::ShowDemoWindow();
                ImGui::SeparatorText("Camera");
                {
                    ImGui::Text(
                        "Position: (%f, %f, %f)",
                        _mainCamera.GetPosition().x,
                        _mainCamera.GetPosition().y,
                        _mainCamera.GetPosition().z
                    );
                    ImGui::Text(
                        "Yaw: %f Pitch: %f Roll: %f",
                        _mainCamera.GetRotation().y,
                        _mainCamera.GetRotation().x,
                        _mainCamera.GetRotation().z
                    );
                    ImGui::SliderFloat("FOV", &_FOV, 30, 120, "%.f");
                }
                if (ImGui::Button("Reset")) {
                    _mainCamera.SetPosition(0, 0, 0);
                }
                ImGui::SeparatorText("Cursor Lock(tab)");
                if (_windowFocused) {
                    ImGui::Text("Cursor Lock: Active");
                } else {
                    ImGui::Text("Cursor Lock: Deactive");
                }
                if (_uiMode) {
                    ImGui::Text("UI Mode: Active");
                } else {
                    ImGui::Text("UI Mode: Deactive");
                }
                ImGui::SeparatorText("Engine UBO");
                _widgetUBOViewer.Draw(this, colorSpace);
                ImGui::SeparatorText("Graphics Pipeline");
                _widgetGraphicsPipeline.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Performance")) {
                _widgetPerfPlot.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Device")) {
                _widgetDeviceInfo.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Even-Odd")) {
                _widgetEvenOdd.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Tetra Viewer")) {
                _widgetTetraViewerDemo.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar(); // Engine Tab
        }
    }

    ImGui::End();
    _imguiManager.EndImGuiContext();
}
