#include <algorithm>
#include <filesystem>
#include <random>
#include <sstream>

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h" // for string input text

#include "AppPseudoIsochromaticTest.h"

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
    // Capture gamepad input ONCE at the beginning of the frame
    _capturedGamepadInput = -1;
    bool backPressed = ImGui::IsKeyPressed(ImGuiKey_GamepadBack);

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

    // Handle back button to exit test
    if (backPressed && _state == TestState::kTesting) {
        ctx.apis.PlaySound(Sound::kVineBoom);
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

        // Show different trial count setting based on picker type
        if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
            ImGui::SliderInt("Repetitions Per Axis", &SETTINGS.REPETITIONS_PER_AXIS, 1, 10);
        } else {
            ImGui::SliderInt(
                "Quest Trials Per Direction", &SETTINGS.QUEST_TRIALS_PER_DIRECTION, 5, 40
            );
            ImGui::Checkbox("Test Only 547nm Cone (Axis 1)", &SETTINGS.QUEST_TEST_ONLY_547NM);
        }

        ImGui::SliderInt("Break Interval (0 = no breaks)", &SETTINGS.BREAK_INTERVAL, 0, 100);
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
            ctx.apis.PlaySound(Sound::kVineBoom);
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
    ImGuiTexture tex = subject.prompt.currentLandoltCTexture[ctx.colorSpace]; // RGB is the default

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 textureFullscreenSize
        = ImVec2(tex.width * SETTINGS.STIMULUS_SIZE, tex.height * SETTINGS.STIMULUS_SIZE);

    // center the texture onto the screen
    ImVec2 centerPos = ImVec2(availSize.x * 0.5f, availSize.y * 0.5f);
    ImGui::SetCursorPos(centerPos - textureFullscreenSize * 0.5f);

    ImGui::Image(tex.id, textureFullscreenSize);

    // Allow answering during stimulus presentation (only if response not already given)
    if (!subject.prompt.responseGiven && _capturedGamepadInput >= 0) {
        subject.prompt.responseGiven = true; // Mark response as given
        subject.prompt.currentSelectedAnswer = _capturedGamepadInput;
        bool correct
            = (subject.prompt.currentSelectedAnswer == subject.prompt.correctAnswerTextureIndex);

        if (correct) {
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

        // Log trial data
        const Trial& trial = getCurrentTrial();
        logTrialData(
            subject,
            trial.genotype,
            trial.metameric_axis,
            subject.prompt.currentOrientation,
            _capturedGamepadInput,
            correct
        );

        // Set timer to 0 to trigger state transition on next frame (without disrupting
        // display)
        subject.currStateRemainderTime = 0.0f;
    }
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

    // Handle state transition (only for timer-based states, not break)
    if (subject.state != SubjectState::kBreak) {
        subject.currStateRemainderTime -= ImGui::GetIO().DeltaTime;
        if (subject.currStateRemainderTime <= 0) {
            transitionSubjectState(subject, ctx);
            // If the game just ended, we switched out of testing; stop drawing this frame
            if (_state != TestState::kTesting) {
                return;
            }
        }
        ASSERT(subject.currStateRemainderTime > 0);
    }

    // Draw progress indicator in upper left corner
    int totalTrials = _trials.size();
    int currentTrial = subject.currentTrialIndex + 1; // +1 to show 1-based indexing
    char progressText[64];
    snprintf(progressText, sizeof(progressText), "%d / %d", currentTrial, totalTrials);

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

    int totalTrials = _trials.size();
    int numMisses = totalTrials - subject.numSuccessAttempts;
    bool perfect = numMisses < 1;

    const char* mainMsg = perfect ? "You've finished the test" : "You've finished the test";
    const char* followMsg = perfect ? "Your results will be sent to the research team"
                                    : "Your results will be sent to the research team";

    // Vertically center text block
    float lineSpacing = ImGui::GetTextLineHeightWithSpacing();
    float yStart = (boxSize.y - (lineSpacing * 5.0f)) * 0.5f;

    // 1. "You scored x/x"
    ImVec2 textSize1 = ImGui::CalcTextSize("You scored 000/000");
    ImGui::SetCursorPos(ImVec2((boxSize.x - textSize1.x) * 0.5f, yStart));
    ImGui::Text("You scored %d/%d", subject.numSuccessAttempts, totalTrials);

    // 2. Large "Congratulations" or "Tough luck!"
    ImGui::SetWindowFontScale(2.0f);
    ImVec2 textSize2 = ImGui::CalcTextSize(mainMsg);
    ImGui::SetCursorPos(
        ImVec2((boxSize.x - textSize2.x * 2.0f * 0.5f) * 0.5f, yStart + lineSpacing * 2.0f)
    );
    ImGui::Text("%s", mainMsg);
    ImGui::SetWindowFontScale(1.0f);

    // 3. Normal text follow-up line
    ImVec2 textSize3 = ImGui::CalcTextSize(followMsg);
    ImGui::SetCursorPos(ImVec2((boxSize.x - textSize3.x) * 0.5f, yStart + lineSpacing * 4.0f));
    ImGui::Text("%s", followMsg);

    // 4. "Okay" button centered below text
    ImVec2 buttonSize(150, 60);
    ImVec2 buttonPos((boxSize.x - buttonSize.x) * 0.5f, yStart + lineSpacing * 6.0f);
    ImGui::SetCursorPos(buttonPos);
    if (ImGui::Button("Okay", buttonSize)) {
        if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
            ctx.apis.PlaySound(Sound::kVineBoom);
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
    // Just show a black screen with no visual prompts
    // Still accept gamepad input (only if response not already given)

    if (!subject.prompt.responseGiven && _capturedGamepadInput >= 0) {
        subject.prompt.responseGiven = true; // Mark response as given
        subject.prompt.currentSelectedAnswer = _capturedGamepadInput;
        bool correct
            = (subject.prompt.currentSelectedAnswer == subject.prompt.correctAnswerTextureIndex);

        if (correct) {
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

        // Log trial data
        const Trial& trial = getCurrentTrial();
        logTrialData(
            subject,
            trial.genotype,
            trial.metameric_axis,
            subject.prompt.currentOrientation,
            _capturedGamepadInput,
            correct
        );

        // Set timer to 0 to trigger state transition on next frame (without disrupting
        // display)
        subject.currStateRemainderTime = 0.0f;
    }
}

void AppPseudoIsochromaticTest::transitionSubjectState(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    switch (subject.state) {
    case SubjectState::kBlank:
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = SubjectState::kFixation;
        break;
    case SubjectState::kFixation:
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION;
        subject.state = SubjectState::kIdentification;
        break;
    case SubjectState::kIdentification:
        // If response was already given during identification phase, advance to next trial
        if (subject.prompt.responseGiven) {
            // Response was already scored and logged in the draw function
            if (subject.currentTrialIndex >= (_trials.size() - 1)) {
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
                subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.BLANK;
                subject.state = SubjectState::kBlank;
                populatePromptContext(subject, ctx);
            }
        } else {
            // No response yet, transition to answer phase
            subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING;
            subject.state = SubjectState::kAnswer;
        }
        break;
    case SubjectState::kAnswer:
        // Response was given during answer phase
        // If we've reached the last trial, end game and stop further transitions/prompts
        if (subject.currentTrialIndex >= (_trials.size() - 1)) {
            endGame(subject);
            return;
        }
        // Otherwise advance to next trial
        subject.currentTrialIndex += 1;
        subject.trialsSinceLastBreak += 1;

        // Check if we should take a break
        if (SETTINGS.BREAK_INTERVAL > 0
            && subject.trialsSinceLastBreak >= SETTINGS.BREAK_INTERVAL) {
            subject.state = SubjectState::kBreak;
            subject.trialsSinceLastBreak = 0;
        } else {
            subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.BLANK;
            subject.state = SubjectState::kBlank;
            populatePromptContext(subject, ctx);
        }
        break;
    case SubjectState::kBreak:
        // Break state is handled by button press in drawBreakWindow
        // This case shouldn't be reached by timer, but included for completeness
        break;
    }
}

void AppPseudoIsochromaticTest::buildTrialList()
{
    _trials.clear();

    if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
        // Get genotypes from the genetic color picker
        std::vector<std::string> genotypes = _geneticColorPicker->GetGenotypes();

        // Build all trials: genotype × metameric_axis × repetition
        for (const std::string& genotype : genotypes) {
            for (int axis = 0; axis < 4; axis++) {
                for (int rep = 0; rep < SETTINGS.REPETITIONS_PER_AXIS; rep++) {
                    Trial trial;
                    trial.genotype = genotype;
                    trial.metameric_axis = axis;
                    trial.repetition_idx = rep;
                    _trials.push_back(trial);
                }
            }
        }
    } else {
        // Get directions metadata from quest color picker
        auto metadata = _questColorPicker->GetDirectionsMetadata();

        // Build trials for each direction × Quest trials
        for (const auto& [dir_idx, genotype_axis] : metadata) {
            for (int rep = 0; rep < SETTINGS.QUEST_TRIALS_PER_DIRECTION; rep++) {
                Trial trial;
                trial.genotype = genotype_axis.first;
                trial.metameric_axis = genotype_axis.second;
                trial.repetition_idx = rep;
                _trials.push_back(trial);
            }
        }
    }

    // Randomize the trial order
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(_trials.begin(), _trials.end(), g);

    INFO("Built and randomized {} trials", _trials.size());
}

void AppPseudoIsochromaticTest::newGame(const TetriumApp::TickContextImGui& ctx)
{
    // Clean up old generators
    if (_geneticPlateGenerator) {
        delete _geneticPlateGenerator;
        _geneticPlateGenerator = nullptr;
    }
    if (_geneticColorPicker) {
        delete _geneticColorPicker;
        _geneticColorPicker = nullptr;
    }
    if (_questPlateGenerator) {
        delete _questPlateGenerator;
        _questPlateGenerator = nullptr;
    }
    if (_questColorPicker) {
        delete _questColorPicker;
        _questColorPicker = nullptr;
    }
    if (_logger) {
        delete _logger;
        _logger = nullptr;
    }

    std::vector<int> dimensions = {2};
    std::string display_primaries_path = TETRIUM_COLOR_PATH + "measurements/2025-10-12/primaries";

    if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
        // Create genetic color picker
        _geneticColorPicker = new TetriumColor::GeneticColorPicker(
            "female", // sex
            0.999f,   // percentage_screened
            547.0f,   // peak_to_test
            1.0f,     // luminance
            0.5f,     // saturation
            dimensions,
            42,                    // seed
            display_primaries_path // display_primaries_path
        );

        // Create plate generator with color picker
        _geneticPlateGenerator = new TetriumColor::GeneticColorPickerPlateGenerator(
            *_geneticColorPicker, 42 // seed
        );
    } else {
        // Create quest color picker
        std::vector<int> axes_to_test;
        if (SETTINGS.QUEST_TEST_ONLY_547NM) {
            axes_to_test = {2}; // Only test metameric axis 1 (547nm cone)
        }
        // If empty, will test all axes (default behavior)

        _questColorPicker = new TetriumColor::QuestColorPicker(
            "cone_shift",                        // mode
            8,                                   // num_genotypes
            SETTINGS.QUEST_TRIALS_PER_DIRECTION, // trials_per_direction
            "female",                            // sex
            0.5f,                                // background_luminance
            42,                                  // seed
            display_primaries_path,              // display_primaries_path
            axes_to_test                         // metameric_axes
        );

        // Create plate generator with quest color picker
        _questPlateGenerator = new TetriumColor::QuestColorPickerPlateGenerator(
            *_questColorPicker, 42 // seed
        );
    }

    // Build and randomize trial list
    buildTrialList();

    // Initialize logger
    std::vector<std::string> headers
        = {"subject_id",
           "session_timestamp",
           "trial_idx",
           "genotype_1",
           "genotype_2",
           "metameric_axis",
           "repetition_idx",
           "orientation",
           "user_choice",
           "correct",
           "lum_noise",
           "s_cone_noise",
           "stimulus_size"};
    _logger = new TestDataLogger("AppPseudoIsochromaticTest", _nameInputBuffer, headers);

    _subject = SubjectContext{
        .name = _nameInputBuffer,
        .currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.BLANK,
        .state = SubjectState::kBlank,
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

    // Export thresholds if using Quest color picker
    if (SETTINGS.PICKER_TYPE == ColorPickerType::QUEST && _questColorPicker) {
        std::string thresholds_filename = _logger->GetFilePath();
        // Replace .csv extension with _thresholds.csv
        size_t ext_pos = thresholds_filename.rfind(".csv");
        if (ext_pos != std::string::npos) {
            thresholds_filename.replace(ext_pos, 4, "_thresholds.csv");
        } else {
            thresholds_filename += "_thresholds.csv";
        }

        INFO("Exporting Quest thresholds to {}", thresholds_filename);
        _questColorPicker->ExportThresholds(thresholds_filename);
    }

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
    int totalTrials = _trials.size();
    int completedTrials = subject.currentTrialIndex;
    int remainingTrials = totalTrials - completedTrials;

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
    snprintf(
        progressText,
        sizeof(progressText),
        "Completed: %d / %d trials",
        completedTrials,
        totalTrials
    );
    ImVec2 progressSize = ImGui::CalcTextSize(progressText);
    ImGui::SetCursorPos(ImVec2((boxSize.x - progressSize.x) * 0.5f, yStart + lineSpacing * 3.0f));
    ImGui::Text("%s", progressText);

    char remainingText[128];
    snprintf(remainingText, sizeof(remainingText), "Remaining: %d trials", remainingTrials);
    ImVec2 remainingSize = ImGui::CalcTextSize(remainingText);
    ImGui::SetCursorPos(ImVec2((boxSize.x - remainingSize.x) * 0.5f, yStart + lineSpacing * 4.0f));
    ImGui::Text("%s", remainingText);

    // Continue button
    ImVec2 buttonSize(200, 60);
    ImVec2 buttonPos((boxSize.x - buttonSize.x) * 0.5f, yStart + lineSpacing * 6.5f);
    ImGui::SetCursorPos(buttonPos);

    bool continuePressed = ImGui::Button("Continue (A)", buttonSize)
                           || (_capturedGamepadInput == 0); // A button is index 0

    if (continuePressed) {
        // Continue to next trial
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.BLANK;
        subject.state = SubjectState::kBlank;
        populatePromptContext(subject, ctx);
    }

    ImGui::EndChild();
}

std::pair<std::string, std::string> AppPseudoIsochromaticTest::generateLandoltCTextures(
    SubjectContext& subject,
    const std::string& genotype,
    int metameric_axis,
    AnswerKind orientation
)
{
    // Ensure the temp directory exists
    std::filesystem::create_directories("./temp");

    const std::string orientationStr = OrientationToString(orientation);

    std::string baseFilename = "./temp/" + subject.name + "_" + genotype + "_axis"
                               + std::to_string(metameric_axis) + "_" + orientationStr;

    // Call GetPlate with genotype and metameric axis
    auto outputSpace = GetOutputColorSpace();

    if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
        _geneticPlateGenerator->GetPlate(
            genotype,
            metameric_axis,
            baseFilename,
            "landolt_" + orientationStr,
            outputSpace,
            SETTINGS.LUM_NOISE,
            SETTINGS.S_CONE_NOISE
        );
    } else {
        // For Quest, we need to find the direction index that matches this genotype/axis
        auto metadata = _questColorPicker->GetDirectionsMetadata();
        int direction_idx = -1;
        for (const auto& [idx, genotype_axis] : metadata) {
            if (genotype_axis.first == genotype && genotype_axis.second == metameric_axis) {
                direction_idx = idx;
                break;
            }
        }

        if (direction_idx >= 0) {
            _questPlateGenerator->GetPlate(
                direction_idx,
                baseFilename,
                "landolt_" + orientationStr,
                outputSpace,
                SETTINGS.LUM_NOISE,
                SETTINGS.S_CONE_NOISE
            );
        } else {
            ERROR("Could not find direction for genotype {} axis {}", genotype, metameric_axis);
        }
    }

    return GetTexturePaths(baseFilename, outputSpace);
}

void AppPseudoIsochromaticTest::populatePromptContext(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // Get the current trial
    const Trial& trial = getCurrentTrial();

    // Pick a random orientation for the correct answer
    AnswerKind answerOrientation = LANDOLT_C_ORIENTATIONS[rand() % LANDOLT_C_ORIENTATIONS.size()];

    // Generate Landolt C textures for this trial's genotype and metameric axis
    auto [rgbTexturePath, ocvTexturePath] = generateLandoltCTextures(
        _subject, trial.genotype, trial.metameric_axis, answerOrientation
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

    const Trial& trial = getCurrentTrial();

    // Parse genotype into separate components
    std::string genotypeCleaned = genotype;
    // Remove parentheses
    genotypeCleaned.erase(
        std::remove(genotypeCleaned.begin(), genotypeCleaned.end(), '('), genotypeCleaned.end()
    );
    genotypeCleaned.erase(
        std::remove(genotypeCleaned.begin(), genotypeCleaned.end(), ')'), genotypeCleaned.end()
    );

    // Split by comma
    std::vector<std::string> genotypeComponents;
    std::stringstream ss(genotypeCleaned);
    std::string component;
    while (std::getline(ss, component, ',')) {
        genotypeComponents.push_back(component);
    }

    std::map<std::string, std::string> data;
    data["subject_id"] = subject.name;
    data["session_timestamp"] = ""; // Empty for now, could add session start time if needed
    data["trial_idx"] = std::to_string(subject.currentTrialIndex);

    // Save genotype components as separate columns (up to 4)
    for (int i = 0; i < 2; i++) {
        data["genotype_" + std::to_string(i + 1)]
            = (i < (int)genotypeComponents.size()) ? genotypeComponents[i] : "";
    }
    data["metameric_axis"] = std::to_string(metameric_axis);
    data["repetition_idx"] = std::to_string(trial.repetition_idx);
    data["orientation"] = OrientationToString(orientation);
    data["user_choice"] = std::to_string(userChoice);
    data["correct"] = correct ? "1" : "0";
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

    // Clean up generators
    if (_geneticPlateGenerator) {
        delete _geneticPlateGenerator;
        _geneticPlateGenerator = nullptr;
    }
    if (_geneticColorPicker) {
        delete _geneticColorPicker;
        _geneticColorPicker = nullptr;
    }
    if (_questPlateGenerator) {
        delete _questPlateGenerator;
        _questPlateGenerator = nullptr;
    }
    if (_questColorPicker) {
        delete _questColorPicker;
        _questColorPicker = nullptr;
    }
    if (_logger) {
        delete _logger;
        _logger = nullptr;
    }
}
} // namespace TetriumApp
