#include "AppAnomaloscope.h"
#include "imgui.h"

#include "Pathing.h"

#include <filesystem>

namespace TetriumApp
{

void AppAnomaloscope::Init(TetriumApp::InitContext& ctx)
{
    (void)ctx;
    INFO("AppAnomaloscope initialized");
}

void AppAnomaloscope::Cleanup(TetriumApp::CleanupContext& ctx)
{
    // Clean up textures
    if (stimulus.topHandleRGB)
        ctx.api.UnloadTexture(stimulus.topHandleRGB);
    if (stimulus.topHandleOCV)
        ctx.api.UnloadTexture(stimulus.topHandleOCV);
    if (stimulus.bottomHandleRGB)
        ctx.api.UnloadTexture(stimulus.bottomHandleRGB);
    if (stimulus.bottomHandleOCV)
        ctx.api.UnloadTexture(stimulus.bottomHandleOCV);

    stimulus = BipartiteStimulus{};

    if (colorGenerator) {
        delete colorGenerator;
        colorGenerator = nullptr;
    }

    INFO("AppAnomaloscope cleaned up");
}

void AppAnomaloscope::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(0);

    if (ImGui::Begin("Anomaloscope Test", NULL, flags)) {
        switch (state) {
        case TestState::kIdle:
            drawIdle(ctx);
            break;
        case TestState::kRunning:
            drawRunning(ctx);
            break;
        }
    }
    ImGui::End();
}

void AppAnomaloscope::drawIdle(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 buttonSize(220, 80);

    ImVec2 pos((screenSize.x - buttonSize.x) * 0.5f, (screenSize.y - buttonSize.y) * 0.5f - 150);

    ImGui::SetCursorPos(pos);
    ImGui::Text("Anomaloscope Color Matching");

    ImGui::SetCursorPos(pos + ImVec2(0, 50));
    ImGui::Text("Adjust R+G mixture and Orange level");

    ImGui::SetCursorPos(pos + ImVec2(0, 80));
    ImGui::Text("to find a metameric match");

    ImGui::SetCursorPos(pos + ImVec2(0, 120));
    ImGui::Text("Controls:");

    ImGui::SetCursorPos(pos + ImVec2(0, 140));
    ImGui::Text("  Left Stick Y: R/G Ratio");

    ImGui::SetCursorPos(pos + ImVec2(0, 160));
    ImGui::Text("  D-Pad: Red, Green & Orange Levels");

    ImGui::SetCursorPos(pos + ImVec2(0, 180));
    ImGui::Text("  Shoulder + D-Pad Y: Green Level");

    ImGui::SetCursorPos(pos + ImVec2(0, 230));
    if (ImGui::Button("Start", buttonSize)) {
        // Initialize color generator
        if (!colorGenerator) {
            colorGenerator = new TetriumColor::SolidColorGenerator(
                TETRIUM_COLOR_PATH + "measurements/2025-10-12/primaries"
            );
        }
        state = TestState::kRunning;
        stimulusNeedsUpdate = true;
    }

    ImGui::SetCursorPos(pos + ImVec2(0, 230 + buttonSize.y + 20));
    if (ImGui::Button("Exit", buttonSize)) {
        ctx.controls.wantExit = true;
    }
}

void AppAnomaloscope::drawRunning(const TetriumApp::TickContextImGui& ctx)
{
    ImGuiIO& io = ImGui::GetIO();
    float deltaTime = io.DeltaTime;

    // Check for gamepad back button to return to menu
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadBack)) {
        // Clean up textures
        if (stimulus.topHandleRGB)
            ctx.apis.UnloadTexture(stimulus.topHandleRGB);
        if (stimulus.topHandleOCV)
            ctx.apis.UnloadTexture(stimulus.topHandleOCV);
        if (stimulus.bottomHandleRGB)
            ctx.apis.UnloadTexture(stimulus.bottomHandleRGB);
        if (stimulus.bottomHandleOCV)
            ctx.apis.UnloadTexture(stimulus.bottomHandleOCV);

        stimulus = BipartiteStimulus{};
        state = TestState::kIdle;
        return;
    }

    // Gamepad controls
    // Left joystick Y-axis controls R/G ratio (up = more red, down = more green)
    if (io.NavInputs[ImGuiNavInput_LStickUp] != 0.0f
        || io.NavInputs[ImGuiNavInput_LStickDown] != 0.0f) {
        float adjust
            = (io.NavInputs[ImGuiNavInput_LStickUp] - io.NavInputs[ImGuiNavInput_LStickDown])
              * settings.rgRatioSpeed * deltaTime;
        settings.rgRatio = std::clamp(settings.rgRatio + adjust, 0.0f, 100.0f);

        stimulusNeedsUpdate = true;
    }

    // Note: Total R+G level is now read-only and computed from red/green levels
    // Use D-pad to adjust red and green levels independently

    // D-pad X-axis controls red level (right = more red, left = less red)
    if (io.NavInputs[ImGuiNavInput_DpadRight] != 0.0f
        || io.NavInputs[ImGuiNavInput_DpadLeft] != 0.0f) {
        float adjust
            = (io.NavInputs[ImGuiNavInput_DpadRight] - io.NavInputs[ImGuiNavInput_DpadLeft])
              * settings.redLevelSpeed * deltaTime;
        settings.redLevel = std::clamp(settings.redLevel + adjust, 0.0f, 255.0f);
        stimulusNeedsUpdate = true;
    }

    // D-pad Y-axis controls green level when combined with shoulder button, otherwise orange
    bool shoulderPressed
        = (io.NavInputs[ImGuiNavInput_TweakSlow] != 0.0f
           || io.NavInputs[ImGuiNavInput_TweakFast] != 0.0f);

    if (io.NavInputs[ImGuiNavInput_DpadUp] != 0.0f
        || io.NavInputs[ImGuiNavInput_DpadDown] != 0.0f) {
        float adjust = (io.NavInputs[ImGuiNavInput_DpadUp] - io.NavInputs[ImGuiNavInput_DpadDown])
                       * deltaTime;

        if (shoulderPressed) {
            // With shoulder button: adjust green level
            adjust *= settings.greenLevelSpeed;
            settings.greenLevel = std::clamp(settings.greenLevel + adjust, 0.0f, 255.0f);
        } else {
            // Without shoulder button: adjust orange level
            adjust *= settings.orangeLevelSpeed;
            settings.orangeLevel = std::clamp(settings.orangeLevel + adjust, 0.0f, 255.0f);
        }
        stimulusNeedsUpdate = true;
    }

    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 centerPos(screenSize.x * 0.5f, screenSize.y * 0.5f);

    // Left side: Control panel (wider to accommodate all controls)
    float controlPanelWidth = 1000.0f; // 2.5x wider (was 400)
    ImGui::SetNextWindowPos(ImVec2(20, 20));
    ImGui::SetNextWindowSize(ImVec2(controlPanelWidth, screenSize.y - 40));
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::SeparatorText("Top Half: R+G Mixture");
    ImGui::Text("Ratio Control (Left Stick Y):");
    ImGui::Text("  Output = ratio * redLevel + (1-ratio) * greenLevel");
    ImGui::Spacing();

    // R/G Ratio slider
    if (ImGui::SliderFloat("R/G Ratio (%)", &settings.rgRatio, 0.0f, 100.0f, "%.1f")) {
        stimulusNeedsUpdate = true;
    }
    ImGui::Text("  Red: %.1f%%", settings.rgRatio);
    ImGui::Text("  Green: %.1f%%", 100.0f - settings.rgRatio);

    // Fine adjustment buttons for R/G ratio
    ImGui::Text("Fine Adjust:");
    ImGui::SameLine();
    if (ImGui::Button("-1##rg")) {
        settings.rgRatio = std::max(0.0f, settings.rgRatio - 1.0f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("-0.1##rg")) {
        settings.rgRatio = std::max(0.0f, settings.rgRatio - 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+0.1##rg")) {
        settings.rgRatio = std::min(100.0f, settings.rgRatio + 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+1##rg")) {
        settings.rgRatio = std::min(100.0f, settings.rgRatio + 1.0f);
        stimulusNeedsUpdate = true;
    }

    ImGui::Spacing();

    // Total luminance (read-only display)
    settings.rgTotalLevel = settings.redLevel + settings.greenLevel;
    ImGui::Text("Total R+G Level: %.1f", settings.rgTotalLevel);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Red & Green Level Control (D-Pad):");
    ImGui::Text("  These set the base levels used in the ratio formula");
    ImGui::Spacing();

    // Red level slider
    if (ImGui::SliderFloat("Red Level", &settings.redLevel, 0.0f, 255.0f, "%.1f")) {
        stimulusNeedsUpdate = true;
    }

    // Fine adjustment buttons for red
    ImGui::Text("Fine Adjust:");
    ImGui::SameLine();
    if (ImGui::Button("-1##red")) {
        settings.redLevel = std::max(0.0f, settings.redLevel - 1.0f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("-0.1##red")) {
        settings.redLevel = std::max(0.0f, settings.redLevel - 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+0.1##red")) {
        settings.redLevel = std::min(255.0f, settings.redLevel + 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+1##red")) {
        settings.redLevel = std::min(255.0f, settings.redLevel + 1.0f);
        stimulusNeedsUpdate = true;
    }

    ImGui::Spacing();

    // Green level slider
    if (ImGui::SliderFloat("Green Level", &settings.greenLevel, 0.0f, 255.0f, "%.1f")) {
        stimulusNeedsUpdate = true;
    }

    // Fine adjustment buttons for green
    ImGui::Text("Fine Adjust:");
    ImGui::SameLine();
    if (ImGui::Button("-1##green")) {
        settings.greenLevel = std::max(0.0f, settings.greenLevel - 1.0f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("-0.1##green")) {
        settings.greenLevel = std::max(0.0f, settings.greenLevel - 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+0.1##green")) {
        settings.greenLevel = std::min(255.0f, settings.greenLevel + 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+1##green")) {
        settings.greenLevel = std::min(255.0f, settings.greenLevel + 1.0f);
        stimulusNeedsUpdate = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SeparatorText("Bottom Half: Orange Primary");
    ImGui::Text("(D-Pad Up/Down)");
    ImGui::Spacing();

    // Orange level slider
    if (ImGui::SliderFloat("Orange Level", &settings.orangeLevel, 0.0f, 255.0f, "%.1f")) {
        stimulusNeedsUpdate = true;
    }

    // Fine adjustment buttons for orange
    ImGui::Text("Fine Adjust:");
    ImGui::SameLine();
    if (ImGui::Button("-1##o")) {
        settings.orangeLevel = std::max(0.0f, settings.orangeLevel - 1.0f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("-0.1##o")) {
        settings.orangeLevel = std::max(0.0f, settings.orangeLevel - 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+0.1##o")) {
        settings.orangeLevel = std::min(255.0f, settings.orangeLevel + 0.1f);
        stimulusNeedsUpdate = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+1##o")) {
        settings.orangeLevel = std::min(255.0f, settings.orangeLevel + 1.0f);
        stimulusNeedsUpdate = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SeparatorText("Display Settings");

    if (ImGui::SliderFloat("Split Position", &settings.splitRatio, 0.0f, 1.0f)) {
        // No need to regenerate textures for this
    }

    if (ImGui::SliderFloat("Circle Size", &settings.circleRadius, 0.1f, 0.8f)) {
        stimulusNeedsUpdate = true;
    }

    if (ImGui::Checkbox("Noisy Boundary", &settings.hasNoisyBoundary)) {
        stimulusNeedsUpdate = true;
    }

    if (ImGui::Checkbox("Use RGO for OCV channel", &settings.useRGOForOCV)) {
        stimulusNeedsUpdate = true;
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Current RGBO values display
    auto [r_top, g_top, b_top, o_top] = computeRGSide();
    auto [r_bottom, g_bottom, b_bottom, o_bottom] = computeOrangeSide();

    ImGui::SeparatorText("Current RGBO Values");
    ImGui::Text("Top (R+G):    R=%.1f, G=%.1f, B=%.1f, O=%.1f", r_top, g_top, b_top, o_top);
    ImGui::Text(
        "Bottom (O):   R=%.1f, G=%.1f, B=%.1f, O=%.1f", r_bottom, g_bottom, b_bottom, o_bottom
    );

    ImGui::Spacing();
    ImGui::Separator();

    if (ImGui::Button("Back to Menu", ImVec2(200, 50))) {
        // Clean up textures
        if (stimulus.topHandleRGB)
            ctx.apis.UnloadTexture(stimulus.topHandleRGB);
        if (stimulus.topHandleOCV)
            ctx.apis.UnloadTexture(stimulus.topHandleOCV);
        if (stimulus.bottomHandleRGB)
            ctx.apis.UnloadTexture(stimulus.bottomHandleRGB);
        if (stimulus.bottomHandleOCV)
            ctx.apis.UnloadTexture(stimulus.bottomHandleOCV);

        stimulus = BipartiteStimulus{};
        state = TestState::kIdle;
    }

    ImGui::End();

    // Update stimulus if parameters changed
    if (stimulusNeedsUpdate) {
        updateStimulus(ctx);
        stimulusNeedsUpdate = false;
    }

    // Right side: Display bipartite stimulus
    // Calculate the display area (right half of screen, minus control panel)
    float displayLeft = controlPanelWidth + 40; // Control panel width + margin
    float displayWidth = screenSize.x - displayLeft - 20;
    float displayHeight = screenSize.y;

    ImVec2 displayCenter(displayLeft + displayWidth * 0.5f, displayHeight * 0.5f);

    // Circle size
    float circleSize = std::min(displayWidth, displayHeight) * settings.circleRadius;

    // Select appropriate textures based on color space
    ImGuiTexture& topTex
        = (ctx.colorSpace == ColorSpace::OCV) ? stimulus.topTexOCV : stimulus.topTexRGB;
    ImGuiTexture& bottomTex
        = (ctx.colorSpace == ColorSpace::OCV) ? stimulus.bottomTexOCV : stimulus.bottomTexRGB;

    if (topTex.id && bottomTex.id) {
        // Draw bipartite circle using two halves (top and bottom)
        ImVec2 circlePos(displayCenter.x - circleSize * 0.5f, displayCenter.y - circleSize * 0.5f);

        // Split position in UV coordinates (V-axis for horizontal split)
        float splitV = settings.splitRatio;

        // Top half (R+G)
        ImGui::SetCursorPos(circlePos);
        ImGui::Image(
            topTex.id,
            ImVec2(circleSize, circleSize * splitV),
            ImVec2(0, 0),     // UV top-left
            ImVec2(1, splitV) // UV bottom-right (crop to split in V)
        );

        // Bottom half (Orange) - position it directly below the top half
        ImGui::SetCursorPos(ImVec2(circlePos.x, circlePos.y + circleSize * splitV));
        ImGui::Image(
            bottomTex.id,
            ImVec2(circleSize, circleSize * (1.0f - splitV)),
            ImVec2(0, splitV), // UV top-left (start from split in V)
            ImVec2(1, 1)       // UV bottom-right
        );

        // Draw a horizontal line at the split to make it more visible
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 lineLeft(circlePos.x, displayCenter.y + circleSize * (splitV - 0.5f));
        ImVec2 lineRight(circlePos.x + circleSize, displayCenter.y + circleSize * (splitV - 0.5f));
        drawList->AddLine(lineLeft, lineRight, IM_COL32(128, 128, 128, 255), 2.0f);
    } else {
        // Show loading message
        ImGui::SetCursorPos(displayCenter);
        ImGui::Text("Generating stimulus...");
    }
}

void AppAnomaloscope::updateStimulus(const TetriumApp::TickContextImGui& ctx)
{
    if (!colorGenerator)
        return;

    // Clean up old textures
    if (stimulus.topHandleRGB)
        ctx.apis.UnloadTexture(stimulus.topHandleRGB);
    if (stimulus.topHandleOCV)
        ctx.apis.UnloadTexture(stimulus.topHandleOCV);
    if (stimulus.bottomHandleRGB)
        ctx.apis.UnloadTexture(stimulus.bottomHandleRGB);
    if (stimulus.bottomHandleOCV)
        ctx.apis.UnloadTexture(stimulus.bottomHandleOCV);

    stimulus = BipartiteStimulus{};

    // Create temp directory
    std::filesystem::create_directories("./temp");

    // Generate top half (R+G mixture)
    auto [r_top, g_top, b_top, o_top] = computeRGSide();
    std::string topBase = "./temp/anomaloscope_top";

    auto [topRGBPath, topOCVPath] = colorGenerator->GenerateCircle(
        topBase,
        r_top,
        g_top,
        b_top,
        o_top,
        512, // image size
        settings.circleRadius,
        settings.hasNoisyBoundary,
        TetriumColor::ColorSpaceType::DISP_6P
    );

    stimulus.topHandleRGB = ctx.apis.LoadTexture(topRGBPath);
    // If useRGOForOCV is enabled, use RGB path for OCV channel as well
    std::string topOCVTexturePath = settings.useRGOForOCV ? topRGBPath : topOCVPath;
    stimulus.topHandleOCV = ctx.apis.LoadTexture(topOCVTexturePath);
    stimulus.topTexRGB = ctx.apis.InitImGuiTexture(stimulus.topHandleRGB);
    stimulus.topTexOCV = ctx.apis.InitImGuiTexture(stimulus.topHandleOCV);

    // Generate bottom half (Orange)
    auto [r_bottom, g_bottom, b_bottom, o_bottom] = computeOrangeSide();
    std::string bottomBase = "./temp/anomaloscope_bottom";

    auto [bottomRGBPath, bottomOCVPath] = colorGenerator->GenerateCircle(
        bottomBase,
        r_bottom,
        g_bottom,
        b_bottom,
        o_bottom,
        512, // image size
        settings.circleRadius,
        settings.hasNoisyBoundary,
        TetriumColor::ColorSpaceType::DISP_6P
    );

    stimulus.bottomHandleRGB = ctx.apis.LoadTexture(bottomRGBPath);
    // If useRGOForOCV is enabled, use RGB path for OCV channel as well
    std::string bottomOCVTexturePath = settings.useRGOForOCV ? bottomRGBPath : bottomOCVPath;
    stimulus.bottomHandleOCV = ctx.apis.LoadTexture(bottomOCVTexturePath);
    stimulus.bottomTexRGB = ctx.apis.InitImGuiTexture(stimulus.bottomHandleRGB);
    stimulus.bottomTexOCV = ctx.apis.InitImGuiTexture(stimulus.bottomHandleOCV);
}

std::tuple<float, float, float, float> AppAnomaloscope::computeRGSide()
{
    // Top half: R+G mixture
    // Use ratio * redLevel and (1 - ratio) * greenLevel
    float ratio = settings.rgRatio / 100.0f;
    float r = ratio * settings.redLevel;
    float g = (1.0f - ratio) * settings.greenLevel;
    float b = 0.0f;
    float o = 0.0f;

    return {r, g, b, o};
}

std::tuple<float, float, float, float> AppAnomaloscope::computeOrangeSide()
{
    // Bottom half: Pure orange primary
    // RGBO = (0, 0, 0, orangeLevel)
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float o = settings.orangeLevel;

    return {r, g, b, o};
}

} // namespace TetriumApp
