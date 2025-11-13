#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <sstream>
#include <variant>
#include <vector>

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h" // for string input text

#include "AppPseudoIsochromaticTest.h"
#include "TetriumColor/ColorGeneratorFactory.h"
#include "TetriumColor/ColorSpaceType.h"
#include "constants.h"

#include <Pathing.h>
#include <string>

namespace
{
ImVec2 calculateFitSize(float width, float height, const ImVec2& availableSize)
{
    float aspectRatio = (float)width / (float)height;

    float scaleWidth = availableSize.x / (float)width;
    float scaleHeight = availableSize.y / (float)height;

    float scale = std::min(scaleWidth, scaleHeight);

    ImVec2 fitSize;
    fitSize.x = (float)width * scale;
    fitSize.y = (float)height * scale;

    return fitSize;
}
} // namespace

namespace TetriumApp
{

// Landolt C orientations -- we pick from these to generate tests
static const std::vector<AppPseudoIsochromaticTest::AnswerKind> LANDOLT_C_ORIENTATIONS
    = {AppPseudoIsochromaticTest::AnswerKind::kUp,
       AppPseudoIsochromaticTest::AnswerKind::kDown,
       AppPseudoIsochromaticTest::AnswerKind::kLeft,
       AppPseudoIsochromaticTest::AnswerKind::kRight};

// Define the static map for orientation to string conversion
const std::unordered_map<AppPseudoIsochromaticTest::AnswerKind, std::string>
    AppPseudoIsochromaticTest::_orientationToStringMap
    = {{AppPseudoIsochromaticTest::AnswerKind::kUp, "up"},
       {AppPseudoIsochromaticTest::AnswerKind::kDown, "down"},
       {AppPseudoIsochromaticTest::AnswerKind::kLeft, "left"},
       {AppPseudoIsochromaticTest::AnswerKind::kRight, "right"}};

std::string AppPseudoIsochromaticTest::OrientationToString(
    AppPseudoIsochromaticTest::AnswerKind orientation
)
{
    auto it = _orientationToStringMap.find(orientation);
    return (it != _orientationToStringMap.end()) ? it->second : "unknown";
}

std::string AppPseudoIsochromaticTest::GetLandoltCAnswerTexturePath(
    AppPseudoIsochromaticTest::AnswerKind orientation
)
{
    return TETRIUM_COLOR_PATH + "TetriumColor/Assets/HiddenImages/landolt_"
           + OrientationToString(orientation) + ".png";
}

void TetriumApp::AppPseudoIsochromaticTest::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    // Generate next trial if needed (deferred from previous frame)
    // Do this at start of frame so trial is available for state transitions
    // But delay sound/logging until state transition to avoid timing issues
    if (_deferredResponse.needsTrialGeneration && _testGenerator) {
        // Store previous trial data BEFORE generating next one (for logging during state
        // transition)
        std::optional<TetriumColor::TrialData> previousTrial = _currentTrial;
        AnswerKind previousOrientation = _subject.prompt.currentOrientation;

        // Check if this is a no-answer case (buttonIndex == -1)
        bool isNoAnswer = (_deferredResponse.buttonIndex == -1);

        // Calculate correctness (needed for trial generation)
        bool correct = (_deferredResponse.buttonIndex == _subject.prompt.correctAnswerTextureIndex);

        // Store for logging during state transition
        _deferredResponse.previousTrial = previousTrial;
        _deferredResponse.orientation = previousOrientation;
        _deferredResponse.correct = correct;

        try {
            if (isNoAnswer) {
                // No answer given - re-queue the previous trial by keeping current trial
                // Don't increment trial counter, don't generate new trial
                INFO("No answer given for trial {} - re-queuing same trial", _trialCounter);
                // _currentTrial remains unchanged - we'll re-use it
            } else {
                // Normal response - generate next trial
                _trialCounter++;
                std::string filename
                    = "./temp/" + _subject.name + "_trial_" + std::to_string(_trialCounter);
                AnswerKind next_orientation
                    = LANDOLT_C_ORIENTATIONS[rand() % LANDOLT_C_ORIENTATIONS.size()];
                std::string hidden_symbol = "landolt_" + OrientationToString(next_orientation);

                ColorTestResult result
                    = correct ? ColorTestResult::Success : ColorTestResult::Failure;

                _currentTrial = _testGenerator->GetNextTrial(
                    result,
                    filename,
                    hidden_symbol,
                    GetOutputColorSpace(),
                    SETTINGS.LUM_NOISE,
                    SETTINGS.S_CONE_NOISE
                );

                if (!_currentTrial.has_value()) {
                    INFO("Test completed - no more trials");
                } else if (std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(
                               *_currentTrial
                           )) {
                    const auto& trial
                        = std::get<TetriumColor::PseudoIsochromaticTrial>(*_currentTrial);
                    INFO(
                        "Generated trial {}: rgb_path={}, ocv_path={}, genotype={}, axis={}",
                        _trialCounter,
                        trial.rgb_path,
                        trial.ocv_path,
                        trial.genotype,
                        trial.metameric_axis
                    );
                }
            }
            // Don't populatePromptContext here - let it happen during state transition
            // to avoid texture loading during render phase
        } catch (const std::exception& e) {
            ERROR("Failed to generate next trial: {}", e.what());
        }

        _deferredResponse.needsTrialGeneration = false;
    }

    // Load textures if needed (deferred from previous frame to avoid GPU sync issues)
    if (_needsTextureLoad && _currentTrial.has_value() && _state == TestState::kTesting) {
        populatePromptContext(_subject, ctx);
        _needsTextureLoad = false;
    }

    // Process deferred state transition from PREVIOUS frame (if any)
    // This happens AFTER trial generation so the trial is available
    if (_needsStateTransition && _state == TestState::kTesting) {
        transitionSubjectState(_subject, ctx);
        _needsStateTransition = false;
        // If the game ended, stop here
        if (_state != TestState::kTesting) {
            return;
        }
    }

    // Capture gamepad or keyboard input ONCE at the beginning of the frame
    _capturedGamepadInput = -1;
    bool backPressed = ImGui::IsKeyPressed(ImGuiKey_GamepadBack);

    // Gamepad button mapping
    ImGuiKey gamepadKeys[4] = {
        ImGuiKey_GamepadFaceDown,  // A button -> Down (index 0)
        ImGuiKey_GamepadFaceLeft,  // X button -> Left (index 1)
        ImGuiKey_GamepadFaceRight, // B button -> Right (index 2)
        ImGuiKey_GamepadFaceUp     // Y button -> Up (index 3)
    };
    for (int i = 0; i < 4; i++) {
        if (ImGui::IsKeyPressed(gamepadKeys[i])) {
            _capturedGamepadInput = i;
            break;
        }
    }

    // Keyboard controls (if no gamepad input yet)
    if (_capturedGamepadInput == -1) {
        // Arrow keys or number keys 1-4
        // Down (index 0): Down arrow or 1
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_1)) {
            _capturedGamepadInput = 0;
        }
        // Left (index 1): Left arrow or 2
        else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_2)) {
            _capturedGamepadInput = 1;
        }
        // Right (index 2): Right arrow or 3
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_3)) {
            _capturedGamepadInput = 2;
        }
        // Up (index 3): Up arrow or 4
        else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_4)) {
            _capturedGamepadInput = 3;
        }
    }

    // Handle back button to exit test
    if (backPressed && _state == TestState::kTesting) {
        ctx.apis.PlaySound(Sound::kDigitalBellNegative);
        _state = TestState::kIdle;
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(0);
    if (ImGui::Begin("PsuedoIsochromatic Test", NULL, flags)) {
        switch (_state) {
        case TestState::kSettings: // draw both settings and idle
            drawSettingsWindow(ctx);
        case TestState::kIdle:
            drawIdle(ctx);
            break;
        case TestState::kTesting:
            drawTestForSubject(_subject, ctx);
            break;
        case TestState::kTestResult:
            drawSubjectResult(_subject, ctx);
            break;
        }
    }
    ImGui::End();
}

void TetriumApp::AppPseudoIsochromaticTest::drawSettingsWindow(
    const TetriumApp::TickContextImGui& ctx
)
{
    // draw a settings pop-up window
    if (ImGui::BeginPopup("Settings", ImGuiWindowFlags_AlwaysAutoResize)) {
        // Color picker type dropdown
        ImGui::Text("Color Picker Type");
        const char* pickerOptions[] = {"Genetic", "Quest"};
        int currentPickerType = static_cast<int>(SETTINGS.PICKER_TYPE);
        if (ImGui::Combo("##PickerType", &currentPickerType, pickerOptions, 2)) {
            SETTINGS.PICKER_TYPE = static_cast<ColorPickerType>(currentPickerType);
        }

        ImGui::Separator();

        // Dimension setting
        ImGui::SliderInt("Dimension", &SETTINGS.DIMENSION, 2, 3);
        ImGui::Text("2 = M/L cone testing, 3 = Full trichromat");

        ImGui::Separator();

        // Show different trial count setting based on picker type
        if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
            ImGui::SliderInt("Repetitions Per Axis", &SETTINGS.REPETITIONS_PER_AXIS, 1, 10);
        } else {
            ImGui::SliderInt(
                "Quest Trials Per Direction", &SETTINGS.QUEST_TRIALS_PER_DIRECTION, 1, 40
            );
            ImGui::Checkbox("Test Only 547nm Cone (Axis 1)", &SETTINGS.QUEST_TEST_ONLY_547NM);
            ImGui::Checkbox("Bipolar Sampling", &SETTINGS.QUEST_BIPOLAR);
            ImGui::Text("Bipolar: Sample in both direction and -direction");
        }

        ImGui::SliderInt("Break Interval (0 = no breaks)", &SETTINGS.BREAK_INTERVAL, 0, 100);

        ImGui::Separator();

        // Trial timing mode
        ImGui::Text("Trial Timing Mode");
        const char* timingOptions[] = {"Fixed Trial Time", "Early Exit"};
        int currentTimingMode = static_cast<int>(SETTINGS.TIMING_MODE);
        if (ImGui::Combo("##TimingMode", &currentTimingMode, timingOptions, 2)) {
            SETTINGS.TIMING_MODE = static_cast<TrialTimingMode>(currentTimingMode);
        }
        ImGui::Text("Fixed: Wait full duration | Early Exit: Move on immediately after answer");

        ImGui::Separator();

        ImGui::Text("Duration of Blank Period (seconds)");
        ImGui::InputFloat("##Blank", &SETTINGS.STATE_DURATIONS_SECONDS.BLANK);
        ImGui::Text("Duration of Fixation (seconds)");
        ImGui::InputFloat("##Fixation", &SETTINGS.STATE_DURATIONS_SECONDS.FIXATION);
        ImGui::Text("Duration of Identification (seconds)");
        ImGui::InputFloat("##Identification", &SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION);
        ImGui::Text("Duration of Answering (seconds)");
        ImGui::InputFloat("##Answering", &SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING);

        // Lum noise slider
        ImGui::SliderFloat("Lum Noise", &SETTINGS.LUM_NOISE, 0.0f, 1.0f);

        // S-cone noise slider
        ImGui::SliderFloat("S-Cone Noise", &SETTINGS.S_CONE_NOISE, 0.0f, 1.0f);

        // Stimulus size slider
        ImGui::SliderFloat("Stimulus Size", &SETTINGS.STIMULUS_SIZE, 0.0f, 1.0f);

        // Music setting dropdown
        ImGui::Text("Music Setting");
        const char* musicOptions[] = {"ALL", "CORRECT_WRONG", "OFF"};
        int currentMusicSetting = static_cast<int>(SETTINGS.MUSIC_SETTING);
        if (ImGui::Combo("##Music", &currentMusicSetting, musicOptions, 3)) {
            SETTINGS.MUSIC_SETTING = static_cast<MusicSetting>(currentMusicSetting);
        }
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            _state = TestState::kIdle;
        }
        ImGui::EndPopup();
    }
}

void TetriumApp::AppPseudoIsochromaticTest::drawIdle(const TetriumApp::TickContextImGui& ctx)
{
    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
        ctx.controls.musicOverride = Sound::kMusicGameMenu;
    } else {
        ctx.controls.musicOverride = std::nullopt;
    }
    const float buttonSpacing = 20.0f;

    // Set the button size
    ImVec2 buttonSize(200, 100);

    // Center the button on the screen
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImGuiTexture& tex = _textures.chromalabLogo;
    ImVec2 logoSize = calculateFitSize(tex.width, tex.height, availSize);
    logoSize = logoSize * 0.75f;

    ImVec2 elemPos((availSize.x - logoSize.x) * 0.5f + 25, logoSize.y / 2 - 50);
    // draw the title logo
    ImGui::SetCursorPos(elemPos);
    ImGui::Image(tex.id, logoSize);

    // now for the button
    elemPos = ImVec2(
        (screenSize.x - buttonSize.x) * 0.5f,
        (screenSize.y - logoSize.y) * 0.5f - 300 + logoSize.y + 50
    );

    // Set the cursor position for the button
    ImGui::SetCursorPos(elemPos);
    ImGui::Text("Subject ID:");
    elemPos = elemPos + ImVec2(10, 50);
    ImGui::SetCursorPos(elemPos);

    // Set the width of the input text box to be the same as the button
    ImGui::SetNextItemWidth(buttonSize.x);
    ImGui::InputText("##Input Name", &_nameInputBuffer);

    elemPos = elemPos + ImVec2(0, buttonSize.y + buttonSpacing);
    ImGui::SetCursorPos(elemPos);
    bool isNameEmpty = _nameInputBuffer.empty();
    if (isNameEmpty) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }
    if (ImGui::Button("Play", buttonSize) && !isNameEmpty) {
        newGame(ctx);
    }
    if (isNameEmpty) {
        ImGui::PopStyleColor(3);
    }

    // settings button
    elemPos = elemPos + ImVec2(0, buttonSize.y + buttonSpacing);
    ImGui::SetCursorPos(elemPos);
    if (ImGui::Button("Settings", buttonSize)) {
        ImGui::OpenPopup("Settings");
        _state = TestState::kSettings;
    }

    // exit button
    elemPos = elemPos + ImVec2(0, buttonSize.y + buttonSpacing);
    ImGui::SetCursorPos(elemPos);
    if (ImGui::Button("Exit", buttonSize)) {
        if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
            ctx.apis.PlaySound(Sound::kDigitalBellNegative);
        }
        ctx.controls.wantExit = true;
    }

    ImGui::PopStyleVar(4);
}

void AppPseudoIsochromaticTest::drawLandoltC(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // Always draw the stimulus for the full presentation time
    ImGuiTexture tex = subject.prompt.currentLandoltCTexture[ctx.colorSpace];

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 textureFullscreenSize
        = ImVec2(tex.width * SETTINGS.STIMULUS_SIZE, tex.height * SETTINGS.STIMULUS_SIZE);

    // center the texture onto the screen
    ImVec2 centerPos = ImVec2(availSize.x * 0.5f, availSize.y * 0.5f);
    ImGui::SetCursorPos(centerPos - textureFullscreenSize * 0.5f);

    ImGui::Image(tex.id, textureFullscreenSize);
}

void AppPseudoIsochromaticTest::drawTestForSubject(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
        ctx.controls.musicOverride = Sound::kMusicGamePlay;
    } else {
        ctx.controls.musicOverride = std::nullopt;
    }

    // Process gamepad input for identification and answer states
    // Do ABSOLUTE MINIMUM - just set flags, defer ALL processing to next frame
    // Any calculation or data access here delays the frame, which desyncs even-odd counter
    if ((subject.state == SubjectState::kIdentification || subject.state == SubjectState::kAnswer)
        && !subject.prompt.responseGiven && _capturedGamepadInput >= 0) {

        // Mark response as given immediately to prevent duplicate processing
        subject.prompt.responseGiven = true;
        subject.prompt.currentSelectedAnswer = _capturedGamepadInput;

        // Store ONLY the button index - defer ALL other processing to next frame
        _deferredResponse.hasResponse = true;
        _deferredResponse.buttonIndex = _capturedGamepadInput;
        // Don't calculate correct, don't access trial data, don't access orientation
        // ALL processing (correctness check, trial storage, etc.) happens in TickImGui() next frame
        _deferredResponse.needsTrialGeneration = true; // Generate next trial at start of next frame

        // If EARLY_EXIT mode is enabled, immediately trigger state transition
        if (SETTINGS.TIMING_MODE == TrialTimingMode::EARLY_EXIT) {
            subject.currStateRemainderTime = 0.0f; // Force timer to expire immediately
        }
    }

    // Handle state transition (only for timer-based states, not break)
    if (subject.state != SubjectState::kBreak) {
        subject.currStateRemainderTime -= ImGui::GetIO().DeltaTime;
        if (subject.currStateRemainderTime <= 0) {
            // Defer state transition to next frame to avoid texture loading mid-frame
            _needsStateTransition = true;
        }
    }

    // If waiting for state transition, show black screen (no rendering)
    if (_needsStateTransition) {
        return;
    }

    // Draw progress indicator in upper left corner
    int currentTrial = _trialCounter + 1; // +1 to show 1-based indexing
    int totalTrials = -1;

    // Get total trials if available (for Quest, this is predetermined)
    if (_testGenerator) {
        totalTrials = _testGenerator->GetTotalTrials();
    }

    char progressText[64];
    if (totalTrials > 0) {
        snprintf(progressText, sizeof(progressText), "Trial %d/%d", currentTrial, totalTrials);
    } else {
        snprintf(progressText, sizeof(progressText), "Trial %d", currentTrial);
    }

    ImGui::SetCursorPos(ImVec2(20, 20));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.8f)); // Semi-transparent white
    ImGui::SetWindowFontScale(1.5f);
    ImGui::Text("%s", progressText);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    switch (subject.state) {
    case SubjectState::kBlank:
        // Blank state - draw nothing (entirely black)
        break;
    case SubjectState::kFixation:
        drawFixGazePage();
        break;
    case SubjectState::kIdentification:
        drawLandoltC(subject, ctx);
        break;
    case SubjectState::kAnswer:
        drawAnswerPrompts(subject, ctx);
        break;
    case SubjectState::kBreak:
        drawBreakWindow(subject, ctx);
        break;
    }
}

void AppPseudoIsochromaticTest::drawSubjectResult(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // Calculate the size and position of the box
    ImVec2 boxSize(1200, 900);
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 boxPos((windowSize.x - boxSize.x) * 0.5f, (windowSize.y - boxSize.y) * 0.5f);

    // Draw centered box
    ImGui::SetCursorPos(boxPos);
    ImGui::BeginChild(
        "CenteredBox", boxSize, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
    );

    int completedTrials = _trialCounter; // Trials completed
    int totalTrials = -1;

    // Get total trials from TestGenerator if available (for Quest, this is predetermined)
    if (_testGenerator) {
        totalTrials = _testGenerator->GetTotalTrials();
    }

    // If total trials not available, use completed trials
    if (totalTrials < 0) {
        totalTrials = completedTrials;
    }

    int numMisses = completedTrials - subject.numSuccessAttempts;
    bool perfect = numMisses < 1;

    const char* mainMsg = perfect ? "You've finished the test" : "You've finished the test";
    const char* followMsg = perfect ? "Your results will be sent to the research team"
                                    : "Your results will be sent to the research team";

    // Vertically center text block
    float lineSpacing = ImGui::GetTextLineHeightWithSpacing();
    float yStart = (boxSize.y - (lineSpacing * 8.0f)) * 0.5f; // Increased for threshold display

    // 1. "You scored x/x" or "Trial x/x"
    ImVec2 textSize1 = ImGui::CalcTextSize("You scored 000/000");
    ImGui::SetCursorPos(ImVec2((boxSize.x - textSize1.x) * 0.5f, yStart));
    ImGui::Text("You scored %d/%d", subject.numSuccessAttempts, completedTrials);

    // Show total trials if different from completed (for Quest)
    if (totalTrials > completedTrials) {
        ImVec2 textSizeTotal = ImGui::CalcTextSize("Total trials: 000");
        ImGui::SetCursorPos(ImVec2((boxSize.x - textSizeTotal.x) * 0.5f, yStart + lineSpacing));
        ImGui::Text("Total trials: %d", totalTrials);
    }

    // 2. Large "Congratulations" or "Tough luck!"
    ImGui::SetWindowFontScale(2.0f);
    ImVec2 textSize2 = ImGui::CalcTextSize(mainMsg);
    float offsetY = (totalTrials > completedTrials) ? lineSpacing * 3.0f : lineSpacing * 2.0f;
    ImGui::SetCursorPos(ImVec2((boxSize.x - textSize2.x * 2.0f * 0.5f) * 0.5f, yStart + offsetY));
    ImGui::Text("%s", mainMsg);
    ImGui::SetWindowFontScale(1.0f);

    // 3. Quest thresholds summary (if Quest mode)
    float currentY = yStart + offsetY + lineSpacing * 2.0f;
    if (_pickerType == ColorPickerType::QUEST && _testGenerator) {
        auto thresholds = _testGenerator->GetThresholds();
        if (!thresholds.empty()) {
            ImGui::Text("Quest Thresholds Summary:");
            currentY += lineSpacing;

            // Show first few directions as summary
            int count = 0;
            for (const auto& [dirIdx, data] : thresholds) {
                if (count >= 5)
                    break; // Show max 5 directions

                auto itThreshold = data.find("threshold_proportion");
                auto itGenotype = data.find("genotype");
                auto itAxis = data.find("metameric_axis");

                if (itThreshold != data.end()) {
                    std::string summary = "Direction " + std::to_string(dirIdx) + ": ";
                    if (itGenotype != data.end() && !itGenotype->second.empty()
                        && itGenotype->second != "None") {
                        summary += "Genotype " + itGenotype->second;
                    }
                    if (itAxis != data.end() && !itAxis->second.empty()) {
                        summary += ", Axis " + itAxis->second;
                    }
                    summary += ", Threshold: " + itThreshold->second;

                    ImGui::SetCursorPos(ImVec2(20, currentY));
                    ImGui::Text("%s", summary.c_str());
                    currentY += lineSpacing * 0.8f;
                    count++;
                }
            }
            if (thresholds.size() > 5) {
                ImGui::SetCursorPos(ImVec2(20, currentY));
                ImGui::Text("... and %zu more directions (see CSV file)", thresholds.size() - 5);
                currentY += lineSpacing;
            }
            currentY += lineSpacing * 0.5f;
        }
    }

    // 4. Normal text follow-up line
    ImVec2 textSize3 = ImGui::CalcTextSize(followMsg);
    ImGui::SetCursorPos(ImVec2((boxSize.x - textSize3.x) * 0.5f, currentY));
    ImGui::Text("%s", followMsg);

    // 5. "Okay" button centered below text
    ImVec2 buttonSize(150, 60);
    ImVec2 buttonPos((boxSize.x - buttonSize.x) * 0.5f, currentY + lineSpacing * 2.0f);
    ImGui::SetCursorPos(buttonPos);
    if (ImGui::Button("Okay", buttonSize)) {
        if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
            ctx.apis.PlaySound(Sound::kDigitalBellNegative);
        }
        _state = TestState::kIdle;
    }

    ImGui::EndChild();
}

void AppPseudoIsochromaticTest::drawAnswerPrompts(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // Screen remains black throughout this state
    // Input processing is handled in drawTestForSubject()
}

void AppPseudoIsochromaticTest::transitionSubjectState(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    switch (subject.state) {
    case SubjectState::kFixation:
        // Generate next trial if we don't have one yet (shouldn't happen, but safety check)
        if (!_currentTrial.has_value() && _testGenerator && _trialCounter > 0) {
            // This shouldn't happen - next trial should have been generated in deferred response
            // handler
            ERROR("No current trial available when transitioning to fixation - generating one now");
            try {
                _trialCounter++;
                std::string filename
                    = "./temp/" + subject.name + "_trial_" + std::to_string(_trialCounter);
                AnswerKind next_orientation
                    = LANDOLT_C_ORIENTATIONS[rand() % LANDOLT_C_ORIENTATIONS.size()];
                std::string hidden_symbol = "landolt_" + OrientationToString(next_orientation);

                // Use Success as default (shouldn't matter for first trial after break)
                _currentTrial = _testGenerator->GetNextTrial(
                    ColorTestResult::Success,
                    filename,
                    hidden_symbol,
                    GetOutputColorSpace(),
                    SETTINGS.LUM_NOISE,
                    SETTINGS.S_CONE_NOISE
                );
            } catch (const std::exception& e) {
                ERROR("Failed to generate trial in transition: {}", e.what());
            }
        }
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION;
        subject.state = SubjectState::kIdentification;
        break;
    case SubjectState::kIdentification:
        // Timer expired - check if response was given during presentation
        if (subject.prompt.responseGiven) {
            // Process sound and logging NOW (during state transition, not during rendering)
            // This happens when transitioning from Identification to Fixation
            if (_deferredResponse.hasResponse && _deferredResponse.previousTrial.has_value()
                && std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(
                    *_deferredResponse.previousTrial
                )) {
                const auto& trial = std::get<TetriumColor::PseudoIsochromaticTrial>(
                    *_deferredResponse.previousTrial
                );

                if (_deferredResponse.correct) {
                    subject.numSuccessAttempts += 1;
                    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL
                        || SETTINGS.MUSIC_SETTING == MusicSetting::CORRECT_WRONG) {
                        ctx.apis.PlaySound(Sound::kCorrectAnswer);
                    }
                } else {
                    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL
                        || SETTINGS.MUSIC_SETTING == MusicSetting::CORRECT_WRONG) {
                        ctx.apis.PlaySound(Sound::kWrongAnswer);
                    }
                }

                logTrialData(
                    subject,
                    trial.genotype,
                    trial.metameric_axis,
                    _deferredResponse.orientation,
                    _deferredResponse.buttonIndex,
                    _deferredResponse.correct
                );

                _deferredResponse.hasResponse = false;
            }

            // Response was given during identification - skip answer phase
            // Check if there's a next trial
            if (!_currentTrial.has_value()) {
                endGame(subject);
                return;
            }
            subject.currentTrialIndex += 1;
            subject.trialsSinceLastBreak += 1;

            // Check if we should take a break
            if (SETTINGS.BREAK_INTERVAL > 0
                && subject.trialsSinceLastBreak >= SETTINGS.BREAK_INTERVAL) {
                subject.state = SubjectState::kBreak;
                subject.trialsSinceLastBreak = 0;
            } else {
                // Skip blank period, go directly to fixation
                subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
                subject.state = SubjectState::kFixation;
                _needsTextureLoad = true; // Defer texture loading to avoid GPU sync issues
            }
        } else {
            // No response yet, transition to answer phase
            subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING;
            subject.state = SubjectState::kAnswer;
        }
        break;
    case SubjectState::kAnswer:
        // Timer expired - check if response was given
        if (subject.prompt.responseGiven) {
            // Process sound and logging NOW (during state transition, not during rendering)
            // This happens when transitioning from Answer to Fixation
            if (_deferredResponse.hasResponse && _deferredResponse.previousTrial.has_value()
                && std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(
                    *_deferredResponse.previousTrial
                )) {
                const auto& trial = std::get<TetriumColor::PseudoIsochromaticTrial>(
                    *_deferredResponse.previousTrial
                );

                if (_deferredResponse.correct) {
                    subject.numSuccessAttempts += 1;
                    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL
                        || SETTINGS.MUSIC_SETTING == MusicSetting::CORRECT_WRONG) {
                        ctx.apis.PlaySound(Sound::kCorrectAnswer);
                    }
                } else {
                    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL
                        || SETTINGS.MUSIC_SETTING == MusicSetting::CORRECT_WRONG) {
                        ctx.apis.PlaySound(Sound::kWrongAnswer);
                    }
                }

                logTrialData(
                    subject,
                    trial.genotype,
                    trial.metameric_axis,
                    _deferredResponse.orientation,
                    _deferredResponse.buttonIndex,
                    _deferredResponse.correct
                );

                _deferredResponse.hasResponse = false;
            }

            // Response was given during answer phase - skip blank, go to fixation
            if (!_currentTrial.has_value()) {
                endGame(subject);
                return;
            }
            subject.currentTrialIndex += 1;
            subject.trialsSinceLastBreak += 1;

            // Check if we should take a break
            if (SETTINGS.BREAK_INTERVAL > 0
                && subject.trialsSinceLastBreak >= SETTINGS.BREAK_INTERVAL) {
                subject.state = SubjectState::kBreak;
                subject.trialsSinceLastBreak = 0;
            } else {
                // Skip blank period, go directly to fixation
                subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
                subject.state = SubjectState::kFixation;
                _needsTextureLoad = true; // Defer texture loading to avoid GPU sync issues
            }
        } else {
            // No response given - re-queue the same trial
            INFO("No response given for trial {} - re-queuing same trial", _trialCounter);

            // Store current trial data for logging
            std::optional<TetriumColor::TrialData> previousTrial = _currentTrial;

            // Log the no-answer response immediately
            if (previousTrial.has_value()
                && std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(*previousTrial)) {
                const auto& trial = std::get<TetriumColor::PseudoIsochromaticTrial>(*previousTrial);
                logTrialData(
                    subject,
                    trial.genotype,
                    trial.metameric_axis,
                    subject.prompt.currentOrientation,
                    -1,   // No response
                    false // No response = incorrect
                );
            }

            // Defer trial handling until next frame (re-queue same trial)
            _deferredResponse.hasResponse = true;
            _deferredResponse.buttonIndex = -1; // No response
            _deferredResponse.correct = false;  // No response = incorrect
            _deferredResponse.orientation = subject.prompt.currentOrientation;
            _deferredResponse.previousTrial = previousTrial;
            _deferredResponse.needsTrialGeneration
                = true; // Handle trial re-queuing at start of next frame

            // Don't increment trial counters - we're re-queuing the same trial
            // Just transition back to fixation to show the trial again
            subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
            subject.state = SubjectState::kFixation;
            _needsTextureLoad = true; // Reload textures (same trial, but reset state)
        }
        break;
    case SubjectState::kBreak:
        // Continuing from break - transition to fixation and load new trial
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = SubjectState::kFixation;
        _needsTextureLoad = true; // Defer texture loading to avoid GPU sync issues
        break;
    case SubjectState::kBlank:
        // This state is no longer used but kept for safety
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = SubjectState::kFixation;
        break;
    }
}

// buildTrialList removed - trials now generated on-demand via TestGenerator

void AppPseudoIsochromaticTest::newGame(const TetriumApp::TickContextImGui& ctx)
{
    // Clean up old test generator
    if (_testGenerator) {
        delete _testGenerator;
        _testGenerator = nullptr;
    }
    if (_logger) {
        delete _logger;
        _logger = nullptr;
    }

    // Reset trial counter and current trial
    _trialCounter = 0;
    _currentTrial = std::nullopt;

    std::string display_primaries_path = TETRIUM_COLOR_PATH + "measurements/2025-10-12/primaries";

    // Store picker type for later use
    _pickerType = SETTINGS.PICKER_TYPE;

    // Create Python ColorGenerator using factory
    PyObject* pColorGenerator = nullptr;
    try {
        std::vector<int> metameric_axes;
        std::vector<int> dimensions;
        // Handle dimension == 2 case: test M/L cones
        if (SETTINGS.DIMENSION == 2) {
            metameric_axes = {1, 2}; // Test axes 1 and 2 for M/L cone function
            dimensions = {2};        // Use 2D dimensions
        } else {
            // Default: dimension 3, use axis 2, peak 547
            if (SETTINGS.QUEST_TEST_ONLY_547NM) {
                metameric_axes = {2}; // Only test extra cone ( usually the 547nm cone)
            } else {
                metameric_axes = {1, 2, 3}; // Test all axes
            }
            dimensions = {3};
        }

        if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
            // Create GeneticColorGenerator
            float peak_to_test = 547.0f; // dummy basically for now
            pColorGenerator = TetriumColor::ColorGeneratorFactory::CreateGeneticColorGenerator(
                "both",                        // sex
                0.999f,                        // percentage_screened
                peak_to_test,                  // peak_to_test (547, 530, or 559)
                1.0f,                          // luminance
                0.5f,                          // saturation
                dimensions,                    // dimensions (2 or 3)
                42,                            // seed
                SETTINGS.REPETITIONS_PER_AXIS, // trials_per_direction
                metameric_axes,                // metameric_axes
                display_primaries_path         // display_primaries_path
            );
        } else {
            // Create QuestColorGenerator

            // Empty vector = test all axes
            pColorGenerator = TetriumColor::ColorGeneratorFactory::CreateQuestColorGenerator(
                "both",                              // sex
                0.99f,                               // percentage_screened
                0.5f,                                // background_luminance
                SETTINGS.QUEST_TRIALS_PER_DIRECTION, // trials_per_direction
                metameric_axes,                      // metameric_axes
                dimensions,                          // dimensions
                display_primaries_path,              // display_primaries_path
                SETTINGS.QUEST_BIPOLAR               // bipolar
            );
        }
    } catch (const std::exception& e) {
        ERROR("Failed to create ColorGenerator: {}", e.what());
        throw;
    }

    // Create PseudoIsochromaticPlateGenerator using factory
    PyObject* pTestGenerator = nullptr;
    try {
        pTestGenerator
            = TetriumColor::ColorGeneratorFactory::CreatePseudoIsochromaticPlateGenerator(
                pColorGenerator,
                42 // seed
            );
        Py_DECREF(pColorGenerator
        ); // Factory returns new reference, we're done with color generator
        pColorGenerator = nullptr;
    } catch (const std::exception& e) {
        if (pColorGenerator) {
            Py_DECREF(pColorGenerator);
        }
        ERROR("Failed to create TestGenerator: {}", e.what());
        throw;
    }

    // Create C++ TestGenerator wrapper
    _testGenerator = new TetriumColor::TestGenerator(pTestGenerator);
    Py_DECREF(pTestGenerator); // TestGenerator constructor does Py_INCREF

    // Generate first trial
    try {
        std::string filename = "./temp/" + _nameInputBuffer + "_trial_0";
        AnswerKind first_orientation
            = LANDOLT_C_ORIENTATIONS[rand() % LANDOLT_C_ORIENTATIONS.size()];
        std::string hidden_symbol = "landolt_" + OrientationToString(first_orientation);

        _currentTrial = _testGenerator->NewTrial(
            filename,
            hidden_symbol,
            GetOutputColorSpace(),
            SETTINGS.LUM_NOISE,
            SETTINGS.S_CONE_NOISE
        );

        if (!_currentTrial.has_value()) {
            throw std::runtime_error("Failed to generate first trial");
        }
    } catch (const std::exception& e) {
        ERROR("Failed to generate first trial: {}", e.what());
        delete _testGenerator;
        _testGenerator = nullptr;
        throw;
    }

    // Initialize logger
    std::vector<std::string> headers
        = {"subject_id",
           "session_timestamp",
           "trial_idx",
           "genotype_1",
           "genotype_2",
           "genotype_3",
           "metameric_axis",
           "orientation",
           "user_choice",
           "correct",
           "intensity",
           "lum_noise",
           "s_cone_noise",
           "stimulus_size"};

    // Generate additional info string with picker type and dimension
    std::string pickerTypeStr = (_pickerType == ColorPickerType::GENETIC) ? "Genetic" : "Quest";
    std::string additionalInfo = pickerTypeStr + "_dim" + std::to_string(SETTINGS.DIMENSION);

    _logger = new TestDataLogger(
        "AppPseudoIsochromaticTest", _nameInputBuffer, headers, additionalInfo
    );

    _subject = SubjectContext{
        .name = _nameInputBuffer,
        .currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION,
        .state = SubjectState::kFixation,
        .currentTrialIndex = 0,
        .numSuccessAttempts = 0,
        .trialsSinceLastBreak = 0,
    };
    populatePromptContext(_subject, ctx);
    _state = TestState::kTesting;
}

void AppPseudoIsochromaticTest::endGame(SubjectContext& subject)
{
    DEBUG("ending game for subject {}", subject.name);

    // Export Quest thresholds if using Quest color generator
    if (_pickerType == ColorPickerType::QUEST && _testGenerator) {
        std::string pickerTypeStr = (_pickerType == ColorPickerType::GENETIC) ? "Genetic" : "Quest";
        std::string additionalInfo = pickerTypeStr + "_dim" + std::to_string(SETTINGS.DIMENSION);
        std::string filenameBase = subject.name;
        if (!additionalInfo.empty()) {
            filenameBase += "_" + additionalInfo;
        }
        std::string thresholdPath = "../data/AppPseudoIsochromaticTest/" + filenameBase + "_"
                                    + TestDataLogger::getCurrentTimestamp() + "_thresholds.csv";
        if (_testGenerator->ExportThresholds(thresholdPath)) {
            INFO("Quest thresholds exported to {}", thresholdPath);
        } else {
            WARN("Failed to export Quest thresholds");
        }
    }

    // The logged data is already saved via TestDataLogger
    _state = TestState::kTestResult;
}

void AppPseudoIsochromaticTest::drawFixGazePage()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 screenCenter = ImGui::GetIO().DisplaySize;
    screenCenter.x *= 0.5f;
    screenCenter.y *= 0.5f;

    float crossHairSize = 70.f;
    float crossHairThickness = 10.f;
    ImU32 crossHairColor = IM_COL32(255, 255, 255, 255); // White color

    drawList->AddLine(
        ImVec2(screenCenter.x - crossHairSize, screenCenter.y),
        ImVec2(screenCenter.x + crossHairSize, screenCenter.y),
        crossHairColor,
        crossHairThickness
    );
    drawList->AddLine(
        ImVec2(screenCenter.x, screenCenter.y - crossHairSize),
        ImVec2(screenCenter.x, screenCenter.y + crossHairSize),
        crossHairColor,
        crossHairThickness
    );
}

void AppPseudoIsochromaticTest::drawBreakWindow(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // Calculate the size and position of the box
    ImVec2 boxSize(800, 400);
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 boxPos((windowSize.x - boxSize.x) * 0.5f, (windowSize.y - boxSize.y) * 0.5f);

    // Draw centered box
    ImGui::SetCursorPos(boxPos);
    ImGui::BeginChild(
        "BreakBox", boxSize, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
    );

    // Calculate progress
    int completedTrials = _trialCounter;
    // Note: We don't know total trials ahead of time with adaptive methods

    // Vertically center content
    float lineSpacing = ImGui::GetTextLineHeightWithSpacing();
    float yStart = (boxSize.y - (lineSpacing * 6.0f)) * 0.5f;

    // Title
    ImGui::SetWindowFontScale(2.0f);
    const char* titleText = "Take a Break";
    ImVec2 titleSize = ImGui::CalcTextSize(titleText);
    ImGui::SetCursorPos(ImVec2((boxSize.x - titleSize.x * 2.0f * 0.5f) * 0.5f, yStart));
    ImGui::Text("%s", titleText);
    ImGui::SetWindowFontScale(1.0f);

    // Progress info
    char progressText[128];
    snprintf(progressText, sizeof(progressText), "Completed: %d trials", completedTrials);
    ImVec2 progressSize = ImGui::CalcTextSize(progressText);
    ImGui::SetCursorPos(ImVec2((boxSize.x - progressSize.x) * 0.5f, yStart + lineSpacing * 3.0f));
    ImGui::Text("%s", progressText);

    char infoText[128];
    snprintf(infoText, sizeof(infoText), "Take your time to rest");
    ImVec2 infoSize = ImGui::CalcTextSize(infoText);
    ImGui::SetCursorPos(ImVec2((boxSize.x - infoSize.x) * 0.5f, yStart + lineSpacing * 4.0f));
    ImGui::Text("%s", infoText);

    // Continue button
    ImVec2 buttonSize(200, 60);
    ImVec2 buttonPos((boxSize.x - buttonSize.x) * 0.5f, yStart + lineSpacing * 6.5f);
    ImGui::SetCursorPos(buttonPos);

    bool continuePressed = ImGui::Button("Continue (A)", buttonSize)
                           || (_capturedGamepadInput == 0); // A button is index 0

    if (continuePressed) {
        // Defer transition to next frame to avoid texture loading mid-frame
        // transitionSubjectState will see kBreak and handle the transition to kBlank + load
        // textures
        _needsStateTransition = true;
    }

    ImGui::EndChild();
}

// generateLandoltCTextures removed - textures now generated by Python TestGenerator

void AppPseudoIsochromaticTest::populatePromptContext(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // Check if we have a current trial
    if (!_currentTrial.has_value()) {
        ERROR("Cannot populate prompt: No current trial available");
        return;
    }

    // Get trial data from variant (currently only PseudoIsochromaticTrial supported)
    if (!std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(*_currentTrial)) {
        ERROR("Cannot populate prompt: Unsupported trial type");
        return;
    }

    const auto& trial = std::get<TetriumColor::PseudoIsochromaticTrial>(*_currentTrial);

    // Get texture paths from trial data (already generated by Python)
    std::string rgbTexturePath = trial.rgb_path;
    std::string ocvTexturePath = trial.ocv_path;

    INFO(
        "Loading textures for trial: rgb={}, ocv={}, genotype={}, axis={}",
        rgbTexturePath,
        ocvTexturePath,
        trial.genotype,
        trial.metameric_axis
    );

    // unload previous textures
    if (_subject.prompt.currentLandoltCTextureHandle[ColorSpace::RGB] != 0) {
        ctx.apis.UnloadTexture(_subject.prompt.currentLandoltCTextureHandle[ColorSpace::RGB]);
    }
    if (_subject.prompt.currentLandoltCTextureHandle[ColorSpace::OCV] != 0) {
        ctx.apis.UnloadTexture(_subject.prompt.currentLandoltCTextureHandle[ColorSpace::OCV]);
    }

    _subject.prompt.currentLandoltCTextureHandle[ColorSpace::RGB]
        = ctx.apis.LoadTexture(rgbTexturePath);
    _subject.prompt.currentLandoltCTextureHandle[ColorSpace::OCV]
        = ctx.apis.LoadTexture(ocvTexturePath);

    _subject.prompt.currentLandoltCTexture[ColorSpace::RGB]
        = ctx.apis.InitImGuiTexture(_subject.prompt.currentLandoltCTextureHandle[ColorSpace::RGB]);
    _subject.prompt.currentLandoltCTexture[ColorSpace::OCV]
        = ctx.apis.InitImGuiTexture(_subject.prompt.currentLandoltCTextureHandle[ColorSpace::OCV]);

    // Extract orientation from hidden_symbol (e.g., "landolt_up" -> "up")
    std::string hidden_symbol = trial.hidden_symbol;
    std::string orientation_str = hidden_symbol.substr(hidden_symbol.find("_") + 1);

    // Convert string to AnswerKind
    AnswerKind answerOrientation = AnswerKind::kUp;
    if (orientation_str == "up")
        answerOrientation = AnswerKind::kUp;
    else if (orientation_str == "down")
        answerOrientation = AnswerKind::kDown;
    else if (orientation_str == "left")
        answerOrientation = AnswerKind::kLeft;
    else if (orientation_str == "right")
        answerOrientation = AnswerKind::kRight;

    // Create a fixed mapping between button positions and orientations
    std::array<AnswerKind, 4> buttonOrientationMap = {
        AnswerKind::kDown,  // Index 0: bottomPos → "↓"
        AnswerKind::kLeft,  // Index 1: leftPos → "←"
        AnswerKind::kRight, // Index 2: rightPos → "→"
        AnswerKind::kUp     // Index 3: topPos → "↑"
    };

    // populate answer textures -- they're pre-generated
    for (int i = 0; i < 4; i++) {
        _subject.prompt.currentAnswerTextureHandle[i]
            = _answerPromptTextureHandles[buttonOrientationMap[i]];
        _subject.prompt.currentAnswerTexture[i]
            = _answerPromptImGuiTextures[buttonOrientationMap[i]];
    }

    // Find which button index corresponds to the correct answer
    int correctButtonIndex = -1;
    for (int i = 0; i < 4; i++) {
        if (buttonOrientationMap[i] == answerOrientation) {
            correctButtonIndex = i;
            break;
        }
    }

    // Set the correct answer index
    _subject.prompt.correctAnswerTextureIndex = correctButtonIndex;

    // Store current orientation for logging
    _subject.prompt.currentOrientation = answerOrientation;

    // Reset response flag for new trial
    _subject.prompt.responseGiven = false;
}

namespace
{
// Helper function to parse genotype string (e.g., "(547, 530, 559)" or "(558.9, 530.3)")
// Returns vector of peak values, empty if parsing fails
std::vector<double> ParseGenotypeString(const std::string& genotypeStr)
{
    std::vector<double> peaks;

    // Handle empty or "unknown" genotype
    if (genotypeStr.empty() || genotypeStr == "unknown" || genotypeStr == "None") {
        return peaks;
    }

    // Remove whitespace
    std::string trimmed = genotypeStr;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    // Check if it starts with '(' and ends with ')'
    if (trimmed.empty() || trimmed.front() != '(' || trimmed.back() != ')') {
        return peaks; // Return empty if not a tuple format
    }

    // Remove parentheses
    trimmed = trimmed.substr(1, trimmed.length() - 2);

    // Split by comma
    std::istringstream iss(trimmed);
    std::string token;
    while (std::getline(iss, token, ',')) {
        // Trim whitespace from token
        token.erase(0, token.find_first_not_of(" \t\n\r"));
        token.erase(token.find_last_not_of(" \t\n\r") + 1);

        if (!token.empty()) {
            try {
                double peak = std::stod(token);
                peaks.push_back(peak);
            } catch (const std::exception&) {
                // If conversion fails, skip this peak
                continue;
            }
        }
    }

    return peaks;
}
} // namespace

void AppPseudoIsochromaticTest::logTrialData(
    const SubjectContext& subject,
    const std::string& genotype,
    int metameric_axis,
    AnswerKind orientation,
    int userChoice,
    bool correct
)
{
    if (!_logger)
        return;

    // Get intensity from current trial if available
    double intensity = 1.0;
    if (_currentTrial.has_value()
        && std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(*_currentTrial)) {
        const auto& trial = std::get<TetriumColor::PseudoIsochromaticTrial>(*_currentTrial);
        intensity = trial.intensity;
    }

    // Parse genotype string to extract individual peaks
    std::vector<double> peaks = ParseGenotypeString(genotype);

    // Determine how many peaks to populate based on dimension
    int numPeaksToPopulate = SETTINGS.DIMENSION;

    std::map<std::string, std::string> data;
    data["subject_id"] = subject.name;
    data["session_timestamp"] = ""; // Empty for now, could add session start time if needed
    data["trial_idx"] = std::to_string(_trialCounter);

    // Save individual peaks as genotype_1, genotype_2, genotype_3
    // Always save all 3 columns, but only populate up to dimension
    for (int i = 0; i < 3; i++) {
        std::string key = "genotype_" + std::to_string(i + 1);
        if (i < numPeaksToPopulate && i < static_cast<int>(peaks.size())) {
            data[key] = std::to_string(peaks[i]);
        } else {
            data[key] = ""; // Empty if peak not available or beyond dimension
        }
    }

    data["metameric_axis"] = std::to_string(metameric_axis);
    data["orientation"] = OrientationToString(orientation);
    data["user_choice"] = std::to_string(userChoice);
    data["correct"] = correct ? "1" : "0";
    data["intensity"] = std::to_string(intensity);
    data["lum_noise"] = std::to_string(SETTINGS.LUM_NOISE);
    data["s_cone_noise"] = std::to_string(SETTINGS.S_CONE_NOISE);
    data["stimulus_size"] = std::to_string(SETTINGS.STIMULUS_SIZE);

    _logger->LogRow(data);
}

void AppPseudoIsochromaticTest::Init(TetriumApp::InitContext& ctx)
{
    for (AnswerKind orientation : LANDOLT_C_ORIENTATIONS) {
        std::string path = AppPseudoIsochromaticTest::GetLandoltCAnswerTexturePath(orientation);
        uint32_t textureHandle = ctx.api.LoadTexture(path);
        _answerPromptTextureHandles[orientation] = textureHandle;
        _answerPromptImGuiTextures[orientation] = ctx.api.InitImGuiTexture(textureHandle);
    }
    // load chromalab logo
    _textures.chromalabLogo
        = ctx.api.InitImGuiTexture(ctx.api.LoadTexture(ASSETS_PATH + "textures/chromalab-logo.png")
        );
};

void AppPseudoIsochromaticTest::Cleanup(TetriumApp::CleanupContext& ctx)
{
    for (AnswerKind orientation : LANDOLT_C_ORIENTATIONS) {
        ctx.api.UnloadTexture(_answerPromptTextureHandles[orientation]);
    }

    // Clean up test generator
    if (_testGenerator) {
        delete _testGenerator;
        _testGenerator = nullptr;
    }
    if (_logger) {
        delete _logger;
        _logger = nullptr;
    }
}
} // namespace TetriumApp
