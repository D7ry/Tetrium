#include "AppTemporalAFC.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include "Pathing.h"

#include <algorithm>
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

    // Clean up all trial textures
    for (auto& trial : trials) {
        for (int i = 0; i < 3; ++i) {
            if (trial.stimuli[i].handleRGB)
                ctx.api.UnloadTexture(trial.stimuli[i].handleRGB);
            if (trial.stimuli[i].handleOCV)
                ctx.api.UnloadTexture(trial.stimuli[i].handleOCV);
        }
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
        case TestState::kSettings:
            drawSettings(ctx);
        case TestState::kIdle:
            drawIdle(ctx);
            break;
        case TestState::kRunning:
            drawRunning(ctx);
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
        state = TestState::kSettings;
    }

    ImGui::SetCursorPos(pos + ImVec2(0, 100 + (buttonSize.y + spacing) * 2));
    if (ImGui::Button("Exit", buttonSize)) {
        ctx.controls.wantExit = true;
    }
}

void AppTemporalAFC::drawSettings(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SliderInt("Samples per R/G ratio", &settings.numSamplesPerRatio, 1, 50);
        ImGui::SliderFloat("Orange Level", &settings.orangeLevel, 0.0f, 255.0f);
        ImGui::SliderFloat("Red Level", &settings.redLevel, 0.0f, 255.0f);
        ImGui::SliderFloat("Green Level", &settings.greenLevel, 0.0f, 255.0f);
        ImGui::SliderInt("Stimulus Duration (ms)", &settings.stimulusDurationMs, 50, 1000);
        ImGui::SliderInt("ISI Duration (ms)", &settings.isiDurationMs, 100, 2000);
        ImGui::SliderFloat("Circle Radius", &settings.circleRadius, 0.1f, 0.8f);
        ImGui::Checkbox("Noisy Boundary", &settings.hasNoisyBoundary);

        ImGui::Text("Odd Stimulus Type");
        const char* oddTypeOptions[] = {"Orange", "R+G", "Randomize"};
        int currentOddType = static_cast<int>(settings.oddType);
        if (ImGui::Combo("##OddType", &currentOddType, oddTypeOptions, 3)) {
            settings.oddType = static_cast<OddType>(currentOddType);
        }

        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            state = TestState::kIdle;
        }
        ImGui::EndPopup();
    }
}

void AppTemporalAFC::drawRunning(const TetriumApp::TickContextImGui& ctx)
{
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
        ImGuiTexture& tex = (ctx.colorSpace == ColorSpace::OCV) ? trial.stimuli[stimulusIdx].texOCV
                                                                : trial.stimuli[stimulusIdx].texRGB;

        float size = screenSize.y * settings.circleRadius;
        ImVec2 imageSize(size, size);
        ImVec2 imagePos(centerPos.x - size * 0.5f, centerPos.y - size * 0.5f);

        ImGui::SetCursorPos(imagePos);
        ImGui::Image(tex.id, imageSize);
        break;
    }

    case TrialState::kResponse: {
        // Show 3 buttons for response
        ImVec2 buttonSize(150, 60);
        float spacing = 40.0f;
        float totalWidth = 3 * buttonSize.x + 2 * spacing;
        ImVec2 startPos(centerPos.x - totalWidth * 0.5f, centerPos.y + 100);

        const char* labels[] = {"1st", "2nd", "3rd"};

        // Gamepad button keys (X, Y, B for 1st, 2nd, 3rd)
        ImGuiKey gamepadKeys[3] = {
            ImGuiKey_GamepadFaceLeft, // X button -> 1st
            ImGuiKey_GamepadFaceUp,   // Y button -> 2nd
            ImGuiKey_GamepadFaceRight // B button -> 3rd
        };

        // Check for gamepad input first
        int pressedButton = -1;
        for (int i = 0; i < 3; i++) {
            if (ImGui::IsKeyPressed(gamepadKeys[i])) {
                pressedButton = i;
                break;
            }
        }

        for (int i = 0; i < 3; ++i) {
            ImGui::SetCursorPos(ImVec2(startPos.x + i * (buttonSize.x + spacing), startPos.y));
            bool buttonClicked = ImGui::Button(labels[i], buttonSize);

            // Check if this button was activated (either by click or gamepad)
            if (buttonClicked || pressedButton == i) {
                handleResponse(i, ctx);
                // Only process one button press per frame
                break;
            }
        }

        break;
    }
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

        // Clean up trial textures
        for (auto& trial : trials) {
            for (int i = 0; i < 3; ++i) {
                if (trial.stimuli[i].handleRGB)
                    ctx.apis.UnloadTexture(trial.stimuli[i].handleRGB);
                if (trial.stimuli[i].handleOCV)
                    ctx.apis.UnloadTexture(trial.stimuli[i].handleOCV);
            }
        }
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
           "odd_type",
           "odd_position",
           "user_choice",
           "correct",
           "reaction_time_ms",
           "orange_level",
           "red_level",
           "green_level",
           "has_noisy_boundary"};

    if (logger) {
        delete logger;
    }
    logger = new TestDataLogger("AppTemporalAFC", subjectName, headers);

    // Generate all trials
    generateAllTrials(ctx);

    // Start first trial
    currentTrialIdx = 0;
    numCorrect = 0;
    trialState = TrialState::kBlank;
    stateTimeRemaining = 500.0f; // Initial blank
    state = TestState::kRunning;
}

void AppTemporalAFC::generateAllTrials(const TetriumApp::TickContextImGui& ctx)
{
    trials.clear();

    // Generate trials for each R/G ratio
    for (int ratio : RG_RATIOS) {
        for (int sample = 0; sample < settings.numSamplesPerRatio; ++sample) {
            Trial trial;
            trial.rgRatio = ratio;

            // Determine odd type
            if (settings.oddType == OddType::RANDOMIZE) {
                trial.oddType = (rand() % 2 == 0) ? OddType::ORANGE : OddType::R_PLUS_G;
            } else {
                trial.oddType = settings.oddType;
            }

            // Randomize odd position
            trial.oddPosition = rand() % 3;

            // Generate stimulus textures for this trial
            generateStimulusTextures(trial, ctx);

            trials.push_back(trial);
        }
    }

    // Shuffle trials
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(trials.begin(), trials.end(), g);

    INFO("Generated {} trials", trials.size());
}

void AppTemporalAFC::generateStimulusTextures(Trial& trial, const TetriumApp::TickContextImGui& ctx)
{
    // Create temp directory
    std::filesystem::create_directories("./temp");

    // Generate 3 stimuli for this trial
    for (int i = 0; i < 3; ++i) {
        bool isOdd = (i == trial.oddPosition);
        auto [r, g, b, o] = computeRGBO(trial.rgRatio, trial.oddType, isOdd);

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
        trial.stimuli[i].handleRGB = ctx.apis.LoadTexture(rgbPath);
        trial.stimuli[i].handleOCV = ctx.apis.LoadTexture(ocvPath);
        trial.stimuli[i].texRGB = ctx.apis.InitImGuiTexture(trial.stimuli[i].handleRGB);
        trial.stimuli[i].texOCV = ctx.apis.InitImGuiTexture(trial.stimuli[i].handleOCV);
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

    // Play sound feedback for response submission
    if (choice >= 0) { // Only play sound if user actually responded (not timeout)
        ctx.apis.PlaySound(Sound::kCorrectAnswer);
    }

    // Log trial data
    logTrialData(trial);

    // Move to next trial or finish
    currentTrialIdx++;
    if (currentTrialIdx >= static_cast<int>(trials.size())) {
        state = TestState::kResult;
    } else {
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
    data["odd_type"] = (trial.oddType == OddType::ORANGE) ? "orange" : "r_plus_g";
    data["odd_position"] = std::to_string(trial.oddPosition);
    data["user_choice"] = std::to_string(trial.userChoice);
    data["correct"] = (trial.userChoice == trial.oddPosition) ? "1" : "0";
    data["reaction_time_ms"] = std::to_string(trial.reactionTimeMs);
    data["orange_level"] = std::to_string(settings.orangeLevel);
    data["red_level"] = std::to_string(settings.redLevel);
    data["green_level"] = std::to_string(settings.greenLevel);
    data["has_noisy_boundary"] = settings.hasNoisyBoundary ? "1" : "0";

    logger->LogRow(data);
}

std::tuple<float, float, float, float> AppTemporalAFC::computeRGBO(
    int rgRatio,
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

    if (isOdd) {
        // This is the ODD stimulus
        if (oddType == OddType::ORANGE) {
            // Odd = pure orange primary
            // RGBO = (0, 0, 0, orangeLevel)
            o = settings.orangeLevel;
            r = 0.0f;
            g = 0.0f;
            b = 0.0f;
        } else {
            // Odd = R+G mixture
            // RGBO = (redLevel, greenLevel, 0, 0)
            r = settings.redLevel;
            g = settings.greenLevel;
            b = 0.0f;
            o = 0.0f;
        }
    } else {
        // This is a STANDARD stimulus (not odd)
        // Standards should be the OPPOSITE type of the odd stimulus
        if (oddType == OddType::ORANGE) {
            // Odd is orange, so standards are R+G mixtures at varying ratios
            // RGBO = (ratio*total, (1-ratio)*total, 0, 0)
            float ratio = rgRatio / 100.0f;
            float totalRG = settings.redLevel + settings.greenLevel;
            r = totalRG * ratio;
            g = totalRG * (1.0f - ratio);
            b = 0.0f;
            o = 0.0f;
        } else {
            // Odd is R+G, so standards are pure orange
            // RGBO = (0, 0, 0, orangeLevel)
            r = 0.0f;
            g = 0.0f;
            b = 0.0f;
            o = settings.orangeLevel;
        }
    }

    return {r, g, b, o};
}

} // namespace TetriumApp
