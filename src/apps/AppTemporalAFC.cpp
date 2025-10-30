#include "AppTemporalAFC.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include "Pathing.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <random>
#include <sstream>

namespace TetriumApp
{

namespace
{
// R/G ratios to test (in percentage)
const std::vector<int> RG_RATIOS = {40, 45, 50, 55, 60, 65, 70, 75, 80};
} // namespace

void AppTemporalAFC::Init(TetriumApp::InitContext& ctx)
{
    (void)ctx;
    DEBUG("Initializing AppTemporalAFC...");
    INFO("AppTemporalAFC initialized");
}

void AppTemporalAFC::Cleanup(TetriumApp::CleanupContext& ctx)
{
    DEBUG("Cleaning up AppTemporalAFC...");

    // Clean up current trial textures
    for (int i = 0; i < 3; ++i) {
        if (currentStimuli[i].handleRGB)
            ctx.api.UnloadTexture(currentStimuli[i].handleRGB);
        if (currentStimuli[i].handleOCV)
            ctx.api.UnloadTexture(currentStimuli[i].handleOCV);
    }

    trials.clear();

    if (logger) {
        delete logger;
        logger = nullptr;
    }

    if (colorGenerator) {
        delete colorGenerator;
        colorGenerator = nullptr;
    }

    INFO("AppTemporalAFC cleaned up");
}

void AppTemporalAFC::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(0);

    if (ImGui::Begin("Temporal 3AFC Test", NULL, flags)) {
        switch (state) {
        case TestState::kIdle:
            drawIdle(ctx);
            break;
        case TestState::kRunning:
            drawRunning(ctx);
            break;
        case TestState::kBreak:
            drawBreak(ctx);
            break;
        case TestState::kResult:
            drawResult(ctx);
            break;
        }
    }
    ImGui::End();
}

void AppTemporalAFC::TickVulkan(TetriumApp::TickContextVulkan& ctx)
{
    // Not used - all rendering is done via pre-generated textures
    (void)ctx;
}

void AppTemporalAFC::drawIdle(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 buttonSize(220, 80);
    float spacing = 20.0f;

    ImVec2 pos((screenSize.x - buttonSize.x) * 0.5f, (screenSize.y - buttonSize.y) * 0.5f - 200);

    ImGui::SetCursorPos(pos);
    ImGui::Text("Subject ID:");

    ImGui::SetCursorPos(pos + ImVec2(0, 40));
    ImGui::SetNextItemWidth(buttonSize.x);
    ImGui::InputText("##Subject", &subjectName);

    bool disabled = subjectName.empty();
    if (disabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1));
    }

    ImGui::SetCursorPos(pos + ImVec2(0, 100));
    if (ImGui::Button("Start Test", buttonSize) && !disabled) {
        startTest(ctx);
    }
    if (disabled)
        ImGui::PopStyleColor(3);

    ImGui::SetCursorPos(pos + ImVec2(0, 100 + buttonSize.y + spacing));
    if (ImGui::Button("Settings", buttonSize)) {
        ImGui::OpenPopup("Settings");
    }

    ImGui::SetCursorPos(pos + ImVec2(0, 100 + (buttonSize.y + spacing) * 2));
    if (ImGui::Button("Exit", buttonSize)) {
        ctx.controls.wantExit = true;
    }

    // Draw settings popup if open
    drawSettings(ctx);
}

void AppTemporalAFC::drawSettings(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Experimental Design");
        ImGui::SliderInt("Samples per grid point", &settings.numSamplesPerPoint, 1, 10);
        ImGui::SliderInt("Luminance Levels", &settings.numLuminanceLevels, 1, 20);
        ImGui::SliderInt("Min Luminance", &settings.minLuminance, 1, 50);
        ImGui::SliderInt("Max Luminance", &settings.maxLuminance, 50, 100);
        int totalTrials = 9 * settings.numLuminanceLevels * settings.numSamplesPerPoint;
        ImGui::Text("Total trials: %d", totalTrials);

        ImGui::Separator();
        ImGui::Text("Stimulus Levels (Base)");
        ImGui::SliderFloat("Orange Level", &settings.orangeLevel, 0.0f, 255.0f);
        ImGui::SliderFloat("Red Level", &settings.redLevel, 0.0f, 255.0f);
        ImGui::SliderFloat("Green Level", &settings.greenLevel, 0.0f, 255.0f);

        ImGui::Separator();
        ImGui::Text("Timing");
        ImGui::SliderInt("Stimulus Duration (ms)", &settings.stimulusDurationMs, 50, 1000);
        ImGui::SliderInt("ISI Duration (ms)", &settings.isiDurationMs, 100, 2000);

        ImGui::Separator();
        ImGui::Text("Display");
        ImGui::SliderFloat("Circle Radius", &settings.circleRadius, 0.1f, 0.8f);
        ImGui::Checkbox("Noisy Boundary", &settings.hasNoisyBoundary);
        ImGui::Checkbox("Use RGO for OCV channel", &settings.useRGOForOCV);

        ImGui::Text("Odd Stimulus Type");
        const char* oddTypeOptions[] = {"Orange", "R+G", "Randomize"};
        int currentOddType = static_cast<int>(settings.oddType);
        if (ImGui::Combo("##OddType", &currentOddType, oddTypeOptions, 3)) {
            settings.oddType = static_cast<OddType>(currentOddType);
        }

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AppTemporalAFC::drawRunning(const TetriumApp::TickContextImGui& ctx)
{
    // Check for gamepad back button to exit test and return to menu
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadBack)) {
        INFO("Test cancelled by user via gamepad back button");

        // Clean up current trial textures
        unloadCurrentTrialTextures(ctx);
        trials.clear();

        // Return to idle state
        state = TestState::kIdle;
        return;
    }

    // Update state timer
    stateTimeRemaining -= ImGui::GetIO().DeltaTime * 1000.0f; // Convert to ms

    if (stateTimeRemaining <= 0) {
        transitionTrialState(ctx);
        if (state != TestState::kRunning)
            return;
    }

    Trial& trial = trials[currentTrialIdx];
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 centerPos(screenSize.x * 0.5f, screenSize.y * 0.5f);

    // Display trial completion counter in the top-left corner
    ImGui::SetCursorPos(ImVec2(20, 20));
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("Trial: %d / %d", currentTrialIdx + 1, static_cast<int>(trials.size()));
    ImGui::SetWindowFontScale(1.0f);

    switch (trialState) {
    case TrialState::kBlank:
    case TrialState::kISI1:
    case TrialState::kISI2:
        // Black screen - do nothing
        break;

    case TrialState::kStimulus1:
    case TrialState::kStimulus2:
    case TrialState::kStimulus3: {
        int stimulusIdx = (trialState == TrialState::kStimulus1)   ? 0
                          : (trialState == TrialState::kStimulus2) ? 1
                                                                   : 2;

        // Select appropriate texture based on color space
        ImGuiTexture& tex = (ctx.colorSpace == ColorSpace::OCV)
                                ? currentStimuli[stimulusIdx].texOCV
                                : currentStimuli[stimulusIdx].texRGB;

        float size = screenSize.y * settings.circleRadius;
        ImVec2 imageSize(size, size);
        ImVec2 imagePos(centerPos.x - size * 0.5f, centerPos.y - size * 0.5f);

        ImGui::SetCursorPos(imagePos);
        ImGui::Image(tex.id, imageSize);
        break;
    }

    case TrialState::kResponse: {
        // Show fixation cross
        ImGui::SetWindowFontScale(4.0f);
        const char* crossText = "+";
        ImVec2 textSize = ImGui::CalcTextSize(crossText);
        ImVec2 crossPos(centerPos.x - textSize.x * 0.5f, centerPos.y - textSize.y * 0.5f);
        ImGui::SetCursorPos(crossPos);
        ImGui::Text("%s", crossText);
        ImGui::SetWindowFontScale(1.0f);

        // Gamepad button keys (X, Y, B for 1st, 2nd, 3rd)
        ImGuiKey gamepadKeys[3] = {
            ImGuiKey_GamepadFaceLeft, // X button -> 1st
            ImGuiKey_GamepadFaceUp,   // Y button -> 2nd
            ImGuiKey_GamepadFaceRight // B button -> 3rd
        };

        // Check for gamepad input only
        for (int i = 0; i < 3; i++) {
            if (ImGui::IsKeyPressed(gamepadKeys[i])) {
                handleResponse(i, ctx);
                break;
            }
        }

        break;
    }
    }
}

void AppTemporalAFC::drawBreak(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;

    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 centerPos(screenSize.x * 0.5f, screenSize.y * 0.5f);

    // Show break message
    ImGui::SetWindowFontScale(2.0f);
    const char* breakText = "Take a Break!";
    ImVec2 textSize = ImGui::CalcTextSize(breakText);
    ImGui::SetCursorPos(ImVec2(centerPos.x - textSize.x * 0.5f, centerPos.y - 150));
    ImGui::Text("%s", breakText);
    ImGui::SetWindowFontScale(1.0f);

    // Show progress
    ImGui::SetCursorPos(ImVec2(centerPos.x - 150, centerPos.y - 50));
    ImGui::Text("Completed: %d / %d trials", currentTrialIdx, static_cast<int>(trials.size()));

    int remaining = static_cast<int>(trials.size()) - currentTrialIdx;
    ImGui::SetCursorPos(ImVec2(centerPos.x - 150, centerPos.y - 20));
    ImGui::Text("Remaining: %d trials", remaining);

    // Continue button
    ImVec2 buttonSize(200, 60);
    ImGui::SetCursorPos(ImVec2(centerPos.x - buttonSize.x * 0.5f, centerPos.y + 50));
    if (ImGui::Button("Continue", buttonSize)) {
        // Load next trial's textures
        loadCurrentTrialTextures(ctx);
        // Resume test
        trialState = TrialState::kBlank;
        stateTimeRemaining = 500.0f;
        state = TestState::kRunning;
    }
}

void AppTemporalAFC::drawResult(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;

    ImVec2 boxSize(800, 600);
    ImVec2 win = ImGui::GetWindowSize();
    ImVec2 pos((win.x - boxSize.x) * 0.5f, (win.y - boxSize.y) * 0.5f);

    ImGui::SetCursorPos(pos);
    ImGui::BeginChild("ResultBox", boxSize, true);

    ImGui::Text("Subject: %s", subjectName.c_str());
    ImGui::Text("Total Trials: %d", static_cast<int>(trials.size()));
    ImGui::Text("Correct: %d", numCorrect);
    float accuracy = trials.empty() ? 0.0f : (100.0f * numCorrect / trials.size());
    ImGui::Text("Accuracy: %.1f%%", accuracy);

    ImGui::Separator();
    ImGui::Text("Accuracy by R/G Ratio:");

    // Calculate accuracy per ratio
    for (int ratio : RG_RATIOS) {
        int totalForRatio = 0;
        int correctForRatio = 0;

        for (const auto& trial : trials) {
            if (trial.rgRatio == ratio) {
                totalForRatio++;
                if (trial.userChoice == trial.oddPosition)
                    correctForRatio++;
            }
        }

        if (totalForRatio > 0) {
            float ratioAccuracy = 100.0f * correctForRatio / totalForRatio;
            ImGui::Text(
                "  R/G = %d%%: %.1f%% (%d/%d)", ratio, ratioAccuracy, correctForRatio, totalForRatio
            );
        }
    }

    ImGui::Separator();
    if (ImGui::Button("Back to Menu", ImVec2(200, 60))) {
        state = TestState::kIdle;

        // Clean up current trial textures
        unloadCurrentTrialTextures(ctx);
        trials.clear();
    }

    ImGui::EndChild();
}

void AppTemporalAFC::startTest(const TetriumApp::TickContextImGui& ctx)
{
    INFO("Starting Temporal 3AFC test for subject: {}", subjectName);

    // Initialize color generator
    if (colorGenerator) {
        delete colorGenerator;
    }
    colorGenerator = new TetriumColor::SolidColorGenerator(
        TETRIUM_COLOR_PATH + "measurements/2025-10-12/primaries"
    );

    // Initialize logger
    std::vector<std::string> headers
        = {"subject_id",
           "session_timestamp",
           "trial_idx",
           "rg_ratio",
           "luminance",
           "odd_type",
           "odd_position",
           "user_choice",
           "correct",
           "reaction_time_ms",
           "orange_level",
           "red_level",
           "green_level",
           "has_noisy_boundary",
           "use_rgo_for_ocv"};

    if (logger) {
        delete logger;
    }
    logger = new TestDataLogger("AppTemporalAFC", subjectName, headers);

    // Generate all trials
    generateAllTrials(ctx);

    // Start first trial
    currentTrialIdx = 0;
    numCorrect = 0;
    loadCurrentTrialTextures(ctx);
    trialState = TrialState::kBlank;
    stateTimeRemaining = 500.0f; // Initial blank
    state = TestState::kRunning;
}

void AppTemporalAFC::generateAllTrials(const TetriumApp::TickContextImGui& ctx)
{
    trials.clear();

    // Generate luminance levels (evenly spaced with proper rounding)
    std::vector<int> luminanceLevels;
    for (int i = 0; i < settings.numLuminanceLevels; ++i) {
        float luminance = settings.minLuminance
                          + i * (settings.maxLuminance - settings.minLuminance)
                                / static_cast<float>(settings.numLuminanceLevels - 1);
        luminanceLevels.push_back(static_cast<int>(std::round(luminance)));
    }

    // Generate trials for each grid point (ratio × luminance)
    int trialIdx = 0;
    for (int ratio : RG_RATIOS) {
        for (int luminance : luminanceLevels) {
            for (int sample = 0; sample < settings.numSamplesPerPoint; ++sample) {
                Trial trial;
                trial.rgRatio = ratio;
                trial.luminance = luminance;

                // Determine odd type
                if (settings.oddType == OddType::RANDOMIZE) {
                    trial.oddType = (rand() % 2 == 0) ? OddType::ORANGE : OddType::R_PLUS_G;
                } else {
                    trial.oddType = settings.oddType;
                }

                // Randomize odd position
                trial.oddPosition = rand() % 3;

                trials.push_back(trial);
                trialIdx++;
            }
        }
    }

    // Shuffle trials
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(trials.begin(), trials.end(), g);

    INFO("Generated {} trials", trials.size());
}

void AppTemporalAFC::loadCurrentTrialTextures(const TetriumApp::TickContextImGui& ctx)
{
    Trial& trial = trials[currentTrialIdx];

    // Create temp directory
    std::filesystem::create_directories("./temp");

    // Generate 3 stimuli for this trial
    for (int i = 0; i < 3; ++i) {
        bool isOdd = (i == trial.oddPosition);
        auto [r, g, b, o] = computeRGBO(trial.rgRatio, trial.luminance, trial.oddType, isOdd);

        std::string baseFilename = "./temp/" + subjectName + "_trial"
                                   + std::to_string(currentTrialIdx) + "_stim" + std::to_string(i);

        auto [rgbPath, ocvPath] = colorGenerator->GenerateCircle(
            baseFilename,
            r,
            g,
            b,
            o,
            512, // image size
            settings.circleRadius,
            settings.hasNoisyBoundary,
            TetriumColor::ColorSpaceType::DISP_6P
        );

        // Load textures
        currentStimuli[i].handleRGB = ctx.apis.LoadTexture(rgbPath);
        // If useRGOForOCV is enabled, use RGB path for OCV channel as well
        std::string ocvTexturePath = settings.useRGOForOCV ? rgbPath : ocvPath;
        currentStimuli[i].handleOCV = ctx.apis.LoadTexture(ocvTexturePath);
        currentStimuli[i].texRGB = ctx.apis.InitImGuiTexture(currentStimuli[i].handleRGB);
        currentStimuli[i].texOCV = ctx.apis.InitImGuiTexture(currentStimuli[i].handleOCV);
    }
}

void AppTemporalAFC::unloadCurrentTrialTextures(const TetriumApp::TickContextImGui& ctx)
{
    for (int i = 0; i < 3; ++i) {
        if (currentStimuli[i].handleRGB) {
            ctx.apis.UnloadTexture(currentStimuli[i].handleRGB);
            currentStimuli[i].handleRGB = 0;
        }
        if (currentStimuli[i].handleOCV) {
            ctx.apis.UnloadTexture(currentStimuli[i].handleOCV);
            currentStimuli[i].handleOCV = 0;
        }
    }
}

void AppTemporalAFC::transitionTrialState(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    switch (trialState) {
    case TrialState::kBlank:
        trialState = TrialState::kStimulus1;
        stateTimeRemaining = settings.stimulusDurationMs;
        break;
    case TrialState::kStimulus1:
        trialState = TrialState::kISI1;
        stateTimeRemaining = settings.isiDurationMs;
        break;
    case TrialState::kISI1:
        trialState = TrialState::kStimulus2;
        stateTimeRemaining = settings.stimulusDurationMs;
        break;
    case TrialState::kStimulus2:
        trialState = TrialState::kISI2;
        stateTimeRemaining = settings.isiDurationMs;
        break;
    case TrialState::kISI2:
        trialState = TrialState::kStimulus3;
        stateTimeRemaining = settings.stimulusDurationMs;
        break;
    case TrialState::kStimulus3:
        trialState = TrialState::kResponse;
        stateTimeRemaining = 5000.0f; // 5 second response window
        responseStartTime = std::chrono::steady_clock::now();
        break;
    case TrialState::kResponse:
        // Timeout - no response
        handleResponse(-1, ctx);
        break;
    }
}

void AppTemporalAFC::handleResponse(int choice, const TetriumApp::TickContextImGui& ctx)
{
    Trial& trial = trials[currentTrialIdx];
    trial.userChoice = choice;

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - responseStartTime);
    trial.reactionTimeMs = duration.count();

    bool correct = (choice == trial.oddPosition);
    if (correct)
        numCorrect++;

    // Play sound feedback based on correctness
    if (choice >= 0) { // Only play sound if user actually responded (not timeout)
        if (correct) {
            ctx.apis.PlaySound(Sound::kCorrectAnswer);
        } else {
            ctx.apis.PlaySound(Sound::kWrongAnswer);
        }
    }

    // Log trial data
    logTrialData(trial);

    // Move to next trial or finish
    currentTrialIdx++;

    // Unload previous trial's textures
    unloadCurrentTrialTextures(ctx);

    if (currentTrialIdx >= static_cast<int>(trials.size())) {
        state = TestState::kResult;
    } else if (currentTrialIdx % 90 == 0) {
        // Take a break every 90 trials
        state = TestState::kBreak;
    } else {
        // Load next trial's textures
        loadCurrentTrialTextures(ctx);
        trialState = TrialState::kBlank;
        stateTimeRemaining = 500.0f;
    }
}

void AppTemporalAFC::logTrialData(const Trial& trial)
{
    if (!logger)
        return;

    std::map<std::string, std::string> data;
    data["subject_id"] = subjectName;
    data["session_timestamp"] = ""; // Will be auto-filled by logger
    data["trial_idx"] = std::to_string(currentTrialIdx);
    data["rg_ratio"] = std::to_string(trial.rgRatio);
    data["luminance"] = std::to_string(trial.luminance);
    data["odd_type"] = (trial.oddType == OddType::ORANGE) ? "orange" : "r_plus_g";
    data["odd_position"] = std::to_string(trial.oddPosition);
    data["user_choice"] = std::to_string(trial.userChoice);
    data["correct"] = (trial.userChoice == trial.oddPosition) ? "1" : "0";
    data["reaction_time_ms"] = std::to_string(trial.reactionTimeMs);
    data["orange_level"] = std::to_string(settings.orangeLevel);
    data["red_level"] = std::to_string(settings.redLevel);
    data["green_level"] = std::to_string(settings.greenLevel);
    data["has_noisy_boundary"] = settings.hasNoisyBoundary ? "1" : "0";
    data["use_rgo_for_ocv"] = settings.useRGOForOCV ? "1" : "0";

    logger->LogRow(data);
}

std::tuple<float, float, float, float> AppTemporalAFC::computeRGBO(
    int rgRatio,
    int luminance,
    OddType oddType,
    bool isOdd
)
{
    // TASK DESIGN:
    // - Each trial has 3 stimuli
    // - 1 stimulus is the "odd one out"
    // - 2 stimuli are the "standard" (should look similar to each other)
    //
    // Two conditions:
    // 1. If oddType == ORANGE: odd=pure orange (O), standards=R+G mixtures
    // 2. If oddType == R_PLUS_G: odd=R+G mixture, standards=pure orange (O)
    //
    // This way we always have 2 of one type and 1 of the other type

    float r = 0.0f, g = 0.0f, b = 0.0f, o = 0.0f;
    float luminanceScale = luminance / 100.0f;

    // Determine whether this stimulus should be orange or R+G
    // If oddType == ORANGE: odd=orange, standards=R+G
    // If oddType == R_PLUS_G: odd=R+G, standards=orange
    bool isOrangeStimulus
        = (oddType == OddType::ORANGE && isOdd) || (oddType == OddType::R_PLUS_G && !isOdd);

    if (isOrangeStimulus) {
        // Pure orange primary
        o = settings.orangeLevel * luminanceScale;
        r = 0.0f;
        g = 0.0f;
        b = 0.0f;
    } else {
        // R+G mixture at varying ratio
        float ratio = rgRatio / 100.0f;
        r = ratio * settings.redLevel;
        g = (1.0f - ratio) * settings.greenLevel;
        b = 0.0f;
        o = 0.0f;
    }

    return {r, g, b, o};
}

} // namespace TetriumApp
