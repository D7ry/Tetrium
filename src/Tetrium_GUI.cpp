// ImGUI subroutine implementations

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "implot.h"
#include "lib/ImGuiUtils.h"

#include "Tetrium.h"

#include "components/imgui_widgets/ImGuiWidgetColorTile.h"

namespace Tetrium_GUI
{
void drawCursor(const ImGuiTexture& cursorTexture)
{
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    ImVec2 mousePos = io.MousePos;
    ImVec2 cursorSize(cursorTexture.width * 2, cursorTexture.height * 2);
    ImVec2 cursorPos(mousePos.x, mousePos.y);
    drawList->AddImage(
        (ImTextureID)cursorTexture.id,
        cursorPos,
        ImVec2(cursorPos.x + cursorSize.x, cursorPos.y + cursorSize.y)
    );
}

void drawFootNote()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize = io.DisplaySize;
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    const char* footnoteText = (const char*)u8"🧩 Tetrium 0.5a";
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

void Tetrium::drawAppsImGui(ColorSpace colorSpace, int currentFrameInFlight)
{
    if (_primaryApp.has_value()) {

        TetriumApp::App* app = _primaryApp.value();
        TetriumApp::TickContextImGui ctxImGui{
            .currentFrameInFlight = currentFrameInFlight,
            .colorSpace = colorSpace,
            .apis = {
                .PlaySound = [this](Sound sound) { _soundManager.PlaySound(sound); },
                .LoadTexture = [this](const std::string& path) { return _textureManager.LoadTexture(path); },
                .InitImGuiTexture = [this](uint32_t textureHandle) {
                    _textureManager.LoadImGuiTexture(textureHandle);
                    return _textureManager.GetImGuiTexture(textureHandle);
                },
                .UnloadTexture = [this](uint32_t textureHandle) { _textureManager.UnLoadTexture(textureHandle); }
            },
            .controls = {.wantExit = false, .musicOverride = std::nullopt}
        };

        app->TickImGui(ctxImGui);

        if (ctxImGui.controls.wantExit) {
            _primaryApp.value()->OnClose();
            _primaryApp = std::nullopt;
            _soundManager.DisableMusic();
        } else {
            if (ctxImGui.controls.musicOverride.has_value()) {
                _soundManager.SetMusic(ctxImGui.controls.musicOverride.value());
            }
        }
    }
}

void Tetrium::drawMainMenu(ColorSpace colorSpace)
{
    int fullScreenFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                          | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    if (ImGui::Begin(DEFAULTS::Engine::APPLICATION_NAME, NULL, fullScreenFlags)) {
        if (ImGui::BeginTabBar("Engine Tab")) {
            if (ImGui::BeginTabItem("🎨Apps")) {
                // show all apps
                for (auto& [appName, app] : _appMap) {
                    if (ImGui::Button(appName.c_str())) {
                        if (_primaryApp.has_value() && _primaryApp.value() != app) {
                            _primaryApp.value()->OnClose();
                        }
                        _primaryApp = app;
                        app->OnOpen();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🚀Performance")) {
                _widgetPerfPlot.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("💻Device")) {
                _widgetDeviceInfo.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("🛸Even-Odd")) {
                _widgetEvenOdd.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Color Tile")) {
                _widgetColorTile.Draw(this, colorSpace);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar(); // Engine Tab
        }
    }

    ImGui::End();
}

// parent function to draw imgui; sets up all contexts and performs drawing.
// note that the actual "painting" onto the frame buffer doesn't happen.
// ImGui internally constructs a draw list, that gets painted onto the fb
// with `recordImGuiDrawCommandBuffer`
void Tetrium::drawImGui(ColorSpace colorSpace, int currentFrameInFlight)
{

    // ---------- Prologue ----------
    PROFILE_SCOPE(&_profiler, "ImGui Draw");

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

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
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = projectorDisplaySize;
        io.DisplayFramebufferScale = {1, 1};
        ImGui::GetMainViewport()->Size = projectorDisplaySize;
    }

    if (!_windowFocused) {
        ImGuiU::DrawCenteredText("Press Tab to enable input", ImVec4(0, 0, 0, 0.8));
    } else if (_uiMode) { // window focused and in ui mode, draw cursor
        ImGuiTexture imguiTexture = _engineTextures[(int)EngineTexture::kCursor].second;
        Tetrium_GUI::drawCursor(imguiTexture);
    }
    Tetrium_GUI::drawFootNote();

    if (_primaryApp.has_value()) {
        drawAppsImGui(colorSpace, currentFrameInFlight);
    } else {
        drawMainMenu(colorSpace);
    }

    ImGui::Render();
}
