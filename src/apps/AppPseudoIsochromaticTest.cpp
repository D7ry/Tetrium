#include <algorithm>
#include <cctype>
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

std::vector<int> ParseObserverIndexFilter(const std::string& text)
{
    std::string normalized;
    normalized.reserve(text.size());
    for (char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            normalized.push_back(c);
        } else {
            normalized.push_back(' ');
        }
    }

    std::vector<int> indices;
    std::stringstream stream(normalized);
    std::string token;
    while (stream >> token) {
        try {
            int value = std::stoi(token);
            if (value >= 0 && std::find(indices.begin(), indices.end(), value) == indices.end()) {
                indices.push_back(value);
            }
        } catch (const std::exception&) {
            // Ignore malformed fragments so partial edits in the GUI do not break settings.
        }
    }
    return indices;
}

std::string JoinObserverIndices(const std::vector<int>& indices)
{
    std::stringstream stream;
    for (size_t i = 0; i < indices.size(); i++) {
        if (i > 0) {
            stream << ",";
        }
        stream << indices[i];
    }
    return stream.str();
}

} // namespace

static const float MAX_L = 2.0f; // Maximum luminance in HERING space (cone white [1,1,1,1])

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

bool AppPseudoIsochromaticTest::isSteadyAdaptationMode() const
{
    return SETTINGS.RENDERER_MODE == RendererMode::STEADY_ADAPTATION;
}

void TetriumApp::AppPseudoIsochromaticTest::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    processPendingTrialGeneration();

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
        const char* pickerOptions[] = {"Genetic", "Quest", "AEPsych"};
        int currentPickerType = static_cast<int>(SETTINGS.PICKER_TYPE);
        if (ImGui::Combo("##PickerType", &currentPickerType, pickerOptions, 3)) {
            SETTINGS.PICKER_TYPE = static_cast<ColorPickerType>(currentPickerType);
        }

        // Stimulus type dropdown
        ImGui::Text("Stimulus Type");
        const char* stimulusOptions[] = {"Pseudoisochromatic Plate", "Bipartite Circle", "Gaussian Blob"};
        int currentStimulusType = static_cast<int>(SETTINGS.STIMULUS_TYPE);
        if (ImGui::Combo("##StimulusType", &currentStimulusType, stimulusOptions, 3)) {
            SETTINGS.STIMULUS_TYPE = static_cast<StimulusType>(currentStimulusType);
        }

        ImGui::Text("Renderer Mode");
        const char* rendererOptions[] = {"Standard", "Steady Adaptation"};
        int currentRendererMode = static_cast<int>(SETTINGS.RENDERER_MODE);
        if (ImGui::Combo("##RendererMode", &currentRendererMode, rendererOptions, 2)) {
            SETTINGS.RENDERER_MODE = static_cast<RendererMode>(currentRendererMode);
            if (isSteadyAdaptationMode()) {
                SETTINGS.STIMULUS_TYPE = StimulusType::GAUSSIAN_BLOB;
            }
        }
        if (isSteadyAdaptationMode()) {
            SETTINGS.STIMULUS_TYPE = StimulusType::GAUSSIAN_BLOB;
            ImGui::TextDisabled("Uses Gaussian blobs on a continuous 0.5 display background.");
        }

        ImGui::Separator();

        // Dimension setting
        ImGui::SliderInt("Dimension", &SETTINGS.DIMENSION, 2, 3);
        ImGui::Text("2 = M/L cone testing, 3 = Full trichromat");

        ImGui::Separator();

        // Reload CDF if dimension changed (shared by both picker types)
        if (_observerCDFDimension != SETTINGS.DIMENSION) {
            try {
                _observerCDF = TetriumColor::ColorGeneratorFactory::GetObserverCDF(
                    "both", SETTINGS.DIMENSION
                );
                _observerCDFDimension = SETTINGS.DIMENSION;
                SETTINGS.NUM_OBSERVERS
                    = std::min(SETTINGS.NUM_OBSERVERS, (int)_observerCDF.size());
            } catch (const std::exception& e) {
                ERROR("Failed to load observer CDF: {}", e.what());
            }
        }

        int maxObservers = _observerCDF.empty() ? 21 : (int)_observerCDF.size();
        ImGui::SliderInt("# Observers", &SETTINGS.NUM_OBSERVERS, 1, maxObservers);
        SETTINGS.NUM_OBSERVERS = std::clamp(SETTINGS.NUM_OBSERVERS, 1, maxObservers);
        if (!_observerCDF.empty()) {
            SETTINGS.PERCENTAGE_SCREENED = _observerCDF[SETTINGS.NUM_OBSERVERS - 1];
            ImGui::SameLine();
            ImGui::TextDisabled("(%.1f%% population)", SETTINGS.PERCENTAGE_SCREENED * 100.0f);
        }
        ImGui::InputText("Observer Indices", &SETTINGS.OBSERVER_INDEX_FILTER);
        std::vector<int> selectedObserverIndices
            = ParseObserverIndexFilter(SETTINGS.OBSERVER_INDEX_FILTER);
        if (!selectedObserverIndices.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled(
                "(testing %zu explicit observers)", selectedObserverIndices.size()
            );
        }

        // Show different trial count setting based on picker type
        if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
            ImGui::SliderInt("Repetitions Per Condition", &SETTINGS.REPETITIONS_PER_AXIS, 1, 100);
            ImGui::SliderInt("MCS Conditions (K)", &SETTINGS.MCS_K, 1, 20);
            ImGui::SameLine();
            ImGui::TextDisabled("(K=1 tests max metamer only)");
            ImGui::Checkbox("Bipolar Sampling", &SETTINGS.QUEST_BIPOLAR);
            const char* colorPickingOptions[] = {"Cone contrast", "Cone"};
            int colorPickingSpace = static_cast<int>(SETTINGS.QUEST_COLOR_PICKING_SPACE);
            if (ImGui::Combo("Color Picking Space", &colorPickingSpace, colorPickingOptions, 2)) {
                SETTINGS.QUEST_COLOR_PICKING_SPACE
                    = static_cast<ColorPickingSpace>(colorPickingSpace);
            }
        } else if (SETTINGS.PICKER_TYPE == ColorPickerType::QUEST) {
            ImGui::SliderInt(
                "Quest Trials Per Direction", &SETTINGS.QUEST_TRIALS_PER_DIRECTION, 1, 40
            );
            ImGui::Checkbox("Test Only 547nm Cone (Axis 1)", &SETTINGS.QUEST_TEST_ONLY_547NM);
            ImGui::Checkbox("Bipolar Sampling", &SETTINGS.QUEST_BIPOLAR);
            const char* colorPickingOptions[] = {"Cone contrast", "Cone"};
            int colorPickingSpace = static_cast<int>(SETTINGS.QUEST_COLOR_PICKING_SPACE);
            if (ImGui::Combo("Color Picking Space", &colorPickingSpace, colorPickingOptions, 2)) {
                SETTINGS.QUEST_COLOR_PICKING_SPACE
                    = static_cast<ColorPickingSpace>(colorPickingSpace);
            }
            ImGui::Text("Bipolar: Sample in both direction and -direction");
        } else {
            SETTINGS.AEPSYCH_NUM_SOBOL_TRIALS = std::clamp(
                SETTINGS.AEPSYCH_NUM_SOBOL_TRIALS, 1, SETTINGS.AEPSYCH_NUM_TRIALS
            );
            ImGui::SliderInt("AEPsych Trials", &SETTINGS.AEPSYCH_NUM_TRIALS, 2, 1000);
            SETTINGS.AEPSYCH_NUM_SOBOL_TRIALS = std::clamp(
                SETTINGS.AEPSYCH_NUM_SOBOL_TRIALS, 1, SETTINGS.AEPSYCH_NUM_TRIALS
            );
            ImGui::SliderInt(
                "AEPsych Sobol Trials",
                &SETTINGS.AEPSYCH_NUM_SOBOL_TRIALS,
                1,
                SETTINGS.AEPSYCH_NUM_TRIALS
            );
            ImGui::SliderFloat(
                "AEPsych Threshold Level", &SETTINGS.AEPSYCH_THRESHOLD_LEVEL, 0.5f, 0.95f
            );
            ImGui::SliderInt("AEPsych CMF Samples", &SETTINGS.AEPSYCH_N_CMF_SAMPLES, 10, 2000);
            ImGui::SliderFloat(
                "AEPsych Patch Sigma Scale", &SETTINGS.AEPSYCH_PATCH_SIGMA_SCALE, 0.5f, 10.0f
            );
            ImGui::SliderFloat(
                "AEPsych Min Patch Major", &SETTINGS.AEPSYCH_MIN_PATCH_MAJOR, 0.01f, 1.0f
            );
            ImGui::SliderFloat(
                "AEPsych Min Patch Minor", &SETTINGS.AEPSYCH_MIN_PATCH_MINOR, 0.01f, 1.0f
            );
            ImGui::SliderFloat(
                "AEPsych Max Radius (<=0 = auto)", &SETTINGS.AEPSYCH_MAX_RADIUS, -1.0f, 1.5f
            );
            ImGui::SliderInt(
                "AEPsych Contour Samples A", &SETTINGS.AEPSYCH_CONTOUR_SAMPLES_A, 5, 101
            );
            ImGui::SliderInt(
                "AEPsych Contour Samples B", &SETTINGS.AEPSYCH_CONTOUR_SAMPLES_B, 5, 101
            );
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

        if (isSteadyAdaptationMode()) {
            ImGui::Separator();
            ImGui::Text("Steady Adaptation Timing");
            ImGui::SliderFloat(
                "Cue Delay (seconds)", &SETTINGS.STEADY_CUE_DELAY_SECONDS, 0.1f, 1.0f
            );
            ImGui::SliderFloat(
                "Presentation (seconds)", &SETTINGS.STEADY_PRESENTATION_SECONDS, 0.05f, 2.0f
            );
            ImGui::SliderFloat("ITI (seconds)", &SETTINGS.STEADY_ITI_SECONDS, 0.1f, 3.0f);
            ImGui::SliderFloat(
                "Adaptation Background Drive", &SETTINGS.STEADY_BACKGROUND_DRIVE, 0.0f, 1.0f
            );
            ImGui::SliderFloat(
                "Temporal Noise Amplitude",
                &SETTINGS.STEADY_TEMPORAL_NOISE_AMPLITUDE,
                0.0f,
                0.1f
            );
            ImGui::SliderFloat(
                "Temporal Noise Tile Size",
                &SETTINGS.STEADY_TEMPORAL_NOISE_TILE_SIZE,
                4.0f,
                96.0f
            );
        }

        // Lum noise slider
        ImGui::SliderFloat("Lum Noise", &SETTINGS.LUM_NOISE, 0.0f, 1.0f);

        // S-cone noise slider
        ImGui::SliderFloat("S-Cone Noise", &SETTINGS.S_CONE_NOISE, 0.0f, 1.0f);

        // Visual angle slider
        ImGui::SliderFloat("Visual Angle (degrees)", &SETTINGS.VISUAL_ANGLE, 1.0f, 10.0f);

        // Viewing distance slider
        ImGui::SliderFloat("Viewing Distance (cm)", &SETTINGS.VIEWING_DISTANCE, 30.0f, 200.0f);

        // Calculate and display stimulus size
        float radians = SETTINGS.VISUAL_ANGLE * 3.14159f / 180.0f;
        float stimulusPhysicalSize = 2.0f * SETTINGS.VIEWING_DISTANCE * std::tan(radians / 2.0f);
        ImGui::Text("Calculated stimulus size: %.2f cm", stimulusPhysicalSize);

        // Stimulus ramp-up duration slider
        ImGui::SliderFloat(
            "Stimulus Ramp-Up Duration (seconds)", &SETTINGS.STIMULUS_RAMP_UP_DURATION, 0.0f, 10.0f
        );

        // Luminance slider
        ImGui::SliderFloat("Luminance", &SETTINGS.LUMINANCE, 0.0f, float(MAX_L));

        if (SETTINGS.STIMULUS_TYPE == StimulusType::GAUSSIAN_BLOB) {
            ImGui::SliderFloat(
                "Gaussian Blob Size", &SETTINGS.GAUSSIAN_BLOB_SIZE, 0.25f, 3.0f
            );
            ImGui::TextDisabled("Changes blob spread only; field size stays fixed.");
        } else {
            ImGui::SliderFloat("Dot Size", &SETTINGS.DOT_SIZE, 0.5f, 2.0f);
        }

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
    const TetriumApp::TickContextImGui& ctx,
    float brightness
)
{
    if (isSteadyAdaptationMode()) {
        drawSteadyStimulusTexture(subject, ctx);
        return;
    }

    // Always draw the stimulus for the full presentation time
    ImGuiTexture tex = subject.prompt.currentLandoltCTexture[ctx.colorSpace];

    ImVec2 availSize = ImGui::GetContentRegionAvail();

    // Calculate stimulus size from visual angle
    // Assuming baseline: 4 degrees at size multiplier 0.5
    // This gives: size_multiplier = visual_angle / 8.0
    float calculatedSize = SETTINGS.VISUAL_ANGLE / 8.0f;

    ImVec2 textureFullscreenSize = ImVec2(tex.width * calculatedSize, tex.height * calculatedSize);

    // Apply brightness as tint color (RGB all set to brightness, alpha = 1.0)
    ImVec4 tintColor(brightness, brightness, brightness, 1.0f);

    // center the texture onto the screen using window size, independent of cursor position
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 centerPos = windowSize * 0.5f;
    ImGui::SetCursorPos(centerPos - textureFullscreenSize * 0.5f);

    ImGui::Image(tex.id, textureFullscreenSize, ImVec2(0, 0), ImVec2(1, 1), tintColor);
}

void AppPseudoIsochromaticTest::drawSteadyStimulusTexture(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    ImGuiTexture tex = subject.prompt.currentLandoltCTexture[ctx.colorSpace];
    if (tex.id == nullptr) {
        return;
    }

    float calculatedSize = SETTINGS.VISUAL_ANGLE / 8.0f;
    ImVec2 textureSize(tex.width * calculatedSize, tex.height * calculatedSize);

    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 center = windowPos + windowSize * 0.5f;

    ImVec2 imageBegin(
        std::round(center.x - textureSize.x * 0.5f),
        std::round(center.y - textureSize.y * 0.5f)
    );
    ImVec2 imageEnd(
        std::round(imageBegin.x + textureSize.x),
        std::round(imageBegin.y + textureSize.y)
    );

    ImGui::GetWindowDrawList()->AddImage(
        tex.id,
        imageBegin,
        imageEnd,
        ImVec2(0, 0),
        ImVec2(1, 1),
        IM_COL32_WHITE
    );
}

void AppPseudoIsochromaticTest::drawAdaptationFrame(const TetriumApp::TickContextImGui& ctx)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    float backgroundDrive = SETTINGS.STEADY_BACKGROUND_DRIVE;
    if (_currentTrial.has_value()
        && std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(*_currentTrial)) {
        const auto& trial = std::get<TetriumColor::PseudoIsochromaticTrial>(*_currentTrial);
        auto it = trial.metadata.find("display_background");
        if (it == trial.metadata.end()) {
            it = trial.metadata.find("background_luminance");
        }
        if (it != trial.metadata.end()) {
            try {
                backgroundDrive = std::stof(it->second);
            } catch (const std::exception&) {
                WARN("Invalid trial background_luminance metadata: {}", it->second);
            }
        }
    }
    int drive = static_cast<int>(std::round(std::clamp(backgroundDrive, 0.0f, 1.0f) * 255.0f));
    drawList->AddRectFilled(ImVec2(0, 0), screenSize, IM_COL32(drive, drive, drive, 255));
}

void AppPseudoIsochromaticTest::drawSteadyTemporalNoise(const TetriumApp::TickContextImGui& ctx)
{
    float amplitude = std::clamp(SETTINGS.STEADY_TEMPORAL_NOISE_AMPLITUDE, 0.0f, 0.1f);
    if (amplitude <= 0.0f) {
        return;
    }

    int frameIdx = (ImGui::GetFrameCount() / 2) % STEADY_NOISE_FRAME_COUNT;
    ImGuiTexture tex = _steadyNoiseTextures[frameIdx];
    if (tex.id == nullptr) {
        return;
    }

    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    int alphaByte = static_cast<int>(std::round(amplitude * 255.0f));
    ImGui::GetWindowDrawList()->AddImage(
        tex.id,
        ImVec2(0, 0),
        screenSize,
        ImVec2(0, 0),
        ImVec2(1, 1),
        IM_COL32(255, 255, 255, alphaByte)
    );
}

void AppPseudoIsochromaticTest::drawSteadyFixationCross()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 center(
        std::round(windowPos.x + windowSize.x * 0.5f),
        std::round(windowPos.y + windowSize.y * 0.5f)
    );
    int drive = static_cast<int>(std::round(std::clamp(
        SETTINGS.STEADY_BACKGROUND_DRIVE, 0.0f, 1.0f
    ) * 255.0f));
    int contrast = drive >= 128 ? 32 : 224;
    ImU32 crossColor = IM_COL32(contrast, contrast, contrast, 255);
    float halfLength = 18.0f;
    float thickness = 3.0f;
    drawList->AddLine(
        ImVec2(center.x - halfLength, center.y),
        ImVec2(center.x + halfLength, center.y),
        crossColor,
        thickness
    );
    drawList->AddLine(
        ImVec2(center.x, center.y - halfLength),
        ImVec2(center.x, center.y + halfLength),
        crossColor,
        thickness
    );
}

void AppPseudoIsochromaticTest::queueNextTrialGeneration(ColorTestResult result)
{
    _trialCounter++;
    AnswerKind nextOrientation = LANDOLT_C_ORIENTATIONS[rand() % LANDOLT_C_ORIENTATIONS.size()];

    _pendingTrialGeneration.active = true;
    _pendingTrialGeneration.delayFrames = 2;
    _pendingTrialGeneration.previousResult = result;
    _pendingTrialGeneration.filename
        = "./temp/" + _subject.name + "_trial_" + std::to_string(_trialCounter);
    _pendingTrialGeneration.hiddenSymbol = "landolt_" + OrientationToString(nextOrientation);

    _preparedTrial.reset();
    _preparedTrialAvailable = false;
    _waitingForPreparedTrial = true;
}

void AppPseudoIsochromaticTest::processPendingTrialGeneration()
{
    if (!_pendingTrialGeneration.active || !_testGenerator) {
        return;
    }
    if (_state == TestState::kTesting && _subject.state != SubjectState::kFixation
        && _subject.state != SubjectState::kBlank) {
        return;
    }

    if (_pendingTrialGeneration.delayFrames > 0) {
        _pendingTrialGeneration.delayFrames--;
        return;
    }

    try {
        _preparedTrial = _testGenerator->GetNextTrial(
            _pendingTrialGeneration.previousResult,
            _pendingTrialGeneration.filename,
            _pendingTrialGeneration.hiddenSymbol,
            GetOutputColorSpace(),
            SETTINGS.LUM_NOISE,
            SETTINGS.S_CONE_NOISE,
            isSteadyAdaptationMode() ? SETTINGS.STEADY_BACKGROUND_DRIVE : SETTINGS.LUMINANCE,
            SETTINGS.DOT_SIZE,
            SETTINGS.VISUAL_ANGLE
        );

        _preparedTrialAvailable = true;
        if (!_preparedTrial.has_value()) {
            INFO("Test completed - no more trials");
        } else if (std::holds_alternative<TetriumColor::PseudoIsochromaticTrial>(
                       *_preparedTrial
                   )) {
            const auto& trial = std::get<TetriumColor::PseudoIsochromaticTrial>(*_preparedTrial);
            INFO(
                "Prepared trial {}: rgb_path={}, ocv_path={}, genotype={}, axis={}",
                _trialCounter,
                trial.rgb_path,
                trial.ocv_path,
                trial.genotype,
                trial.metameric_axis
            );
        }
    } catch (const std::exception& e) {
        ERROR("Failed to prepare next trial: {}", e.what());
        _preparedTrial.reset();
        _preparedTrialAvailable = true;
    }

    _pendingTrialGeneration.active = false;
}

bool AppPseudoIsochromaticTest::commitPreparedTrial()
{
    if (!_preparedTrialAvailable) {
        return false;
    }

    if (!_preparedTrial.has_value()) {
        _currentTrial.reset();
        _waitingForPreparedTrial = false;
        _preparedTrialAvailable = false;
        return false;
    }

    _currentTrial = _preparedTrial;
    _preparedTrial.reset();
    _preparedTrialAvailable = false;
    _waitingForPreparedTrial = false;
    _needsTextureLoad = true;
    return true;
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

    // Process gamepad input for identification and answer states.
    // Keep this path limited to response bookkeeping; adaptive generation and texture swaps happen
    // later during the inter-trial fixation/blank path.
    if ((subject.state == SubjectState::kIdentification || subject.state == SubjectState::kAnswer)
        && !subject.prompt.responseGiven && _capturedGamepadInput >= 0) {

        // Mark response as given immediately to prevent duplicate processing
        subject.prompt.responseGiven = true;
        subject.prompt.currentSelectedAnswer = _capturedGamepadInput;

        bool correct = _capturedGamepadInput == subject.prompt.correctAnswerTextureIndex;

        _deferredResponse.hasResponse = true;
        _deferredResponse.buttonIndex = _capturedGamepadInput;
        _deferredResponse.correct = correct;
        _deferredResponse.orientation = subject.prompt.currentOrientation;
        _deferredResponse.previousTrial = _currentTrial;
        _deferredResponse.needsTrialGeneration = false;

        queueNextTrialGeneration(correct ? ColorTestResult::Success : ColorTestResult::Failure);

        // If EARLY_EXIT mode is enabled, immediately trigger state transition
        if (!isSteadyAdaptationMode() && SETTINGS.TIMING_MODE == TrialTimingMode::EARLY_EXIT) {
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

    // If waiting for state transition, keep the adaptation field stable in steady mode.
    if (_needsStateTransition) {
        if (isSteadyAdaptationMode()) {
            drawAdaptationFrame(ctx);
            drawSteadyTemporalNoise(ctx);
            drawSteadyFixationCross();
        }
        return;
    }

    // Calculate brightness ramp for stimulus
    float brightness = 0.0f; // Default to black for states that don't show stimuli
    if (subject.state == SubjectState::kIdentification) {
        if (isSteadyAdaptationMode()) {
            if (SETTINGS.STIMULUS_RAMP_UP_DURATION > 0.0f) {
                float elapsedTime
                    = subject.identificationStateStartTime - subject.currStateRemainderTime;
                brightness = std::min(1.0f, elapsedTime / SETTINGS.STIMULUS_RAMP_UP_DURATION);
                brightness = std::max(0.0f, brightness);
            } else {
                brightness = 1.0f;
            }
        } else if (SETTINGS.STIMULUS_RAMP_UP_DURATION > 0.0f) {
            float elapsedTime
                = subject.identificationStateStartTime - subject.currStateRemainderTime;
            brightness = std::min(1.0f, elapsedTime / SETTINGS.STIMULUS_RAMP_UP_DURATION);
            brightness = std::max(0.0f, brightness); // Clamp to [0, 1]
        } else {
            brightness = 1.0f; // No ramp, full brightness immediately
        }
    }

    if (isSteadyAdaptationMode()) {
        drawAdaptationFrame(ctx);
        switch (subject.state) {
        case SubjectState::kIdentification:
            drawLandoltC(subject, ctx, brightness);
            drawSteadyTemporalNoise(ctx);
            drawSteadyFixationCross();
            break;
        case SubjectState::kBreak:
            drawBreakWindow(subject, ctx);
            break;
        case SubjectState::kBlank:
        case SubjectState::kFixation:
        case SubjectState::kAnswer:
            drawSteadyTemporalNoise(ctx);
            drawSteadyFixationCross();
            break;
        }
        return;
    }

    // Draw background
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImU32 bgColor = IM_COL32(0, 0, 0, 255);
    if (isSteadyAdaptationMode()) {
        int drive = static_cast<int>(std::round(std::clamp(
            SETTINGS.STEADY_BACKGROUND_DRIVE, 0.0f, 1.0f
        ) * 255.0f));
        bgColor = IM_COL32(drive, drive, drive, 255);
    }

    // Draw fullscreen rectangle
    drawList->AddRectFilled(ImVec2(0, 0), screenSize, bgColor);

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

    if (!isSteadyAdaptationMode()) {
        ImGui::SetCursorPos(ImVec2(20, 20));
        ImGui::PushStyleColor(
            ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.8f)
        ); // Semi-transparent white
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("%s", progressText);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();
    }

    switch (subject.state) {
    case SubjectState::kBlank:
        break;
    case SubjectState::kFixation:
        drawFixGazePage();
        break;
    case SubjectState::kIdentification:
        drawLandoltC(subject, ctx, brightness);
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
        if (_waitingForPreparedTrial) {
            if (!_preparedTrialAvailable) {
                subject.currStateRemainderTime = 0.016f;
                return;
            }
            if (!commitPreparedTrial()) {
                endGame(subject);
                return;
            }
            subject.currStateRemainderTime = 0.016f;
            return;
        }
        subject.currStateRemainderTime = isSteadyAdaptationMode()
                                             ? SETTINGS.STEADY_PRESENTATION_SECONDS
                                             : SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION;
        subject.identificationStateStartTime = subject.currStateRemainderTime;
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
                    _deferredResponse.correct,
                    trial.intensity,
                    trial.metadata
                );

                _deferredResponse.hasResponse = false;
            }

            // Response was given during identification - skip answer phase.
            if (_preparedTrialAvailable && !commitPreparedTrial()) {
                endGame(subject);
                return;
            }
            if (_preparedTrialAvailable && !_currentTrial.has_value()) {
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
                subject.currStateRemainderTime = isSteadyAdaptationMode()
                                                     ? SETTINGS.STEADY_ITI_SECONDS
                                                     : SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
                subject.state = isSteadyAdaptationMode() ? SubjectState::kBlank
                                                          : SubjectState::kFixation;
                if (!_waitingForPreparedTrial) {
                    _needsTextureLoad = true; // Defer texture loading to avoid GPU sync issues
                }
            }
        } else {
            // No response yet, transition to answer phase
            subject.currStateRemainderTime = isSteadyAdaptationMode()
                                                 ? SETTINGS.STEADY_ITI_SECONDS
                                                 : SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING;
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
                    _deferredResponse.correct,
                    trial.intensity,
                    trial.metadata
                );

                _deferredResponse.hasResponse = false;
            }

            // Response was given during answer phase - skip blank, go to fixation.
            if (_preparedTrialAvailable && !commitPreparedTrial()) {
                endGame(subject);
                return;
            }
            if (_preparedTrialAvailable && !_currentTrial.has_value()) {
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
                subject.currStateRemainderTime = isSteadyAdaptationMode()
                                                     ? SETTINGS.STEADY_ITI_SECONDS
                                                     : SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
                subject.state = isSteadyAdaptationMode() ? SubjectState::kBlank
                                                          : SubjectState::kFixation;
                if (!_waitingForPreparedTrial) {
                    _needsTextureLoad = true; // Defer texture loading to avoid GPU sync issues
                }
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
                    -1,    // No response
                    false, // No response = incorrect
                    trial.intensity,
                    trial.metadata
                );
            }

            // Defer trial handling until next frame (re-queue same trial)
            _deferredResponse.hasResponse = true;
            _deferredResponse.buttonIndex = -1; // No response
            _deferredResponse.correct = false;  // No response = incorrect
            _deferredResponse.orientation = subject.prompt.currentOrientation;
            _deferredResponse.previousTrial = previousTrial;
            _deferredResponse.needsTrialGeneration = false;

            // Don't increment trial counters - we're re-queuing the same trial
            // Just transition back to fixation to show the trial again
            subject.currStateRemainderTime = isSteadyAdaptationMode()
                                                 ? SETTINGS.STEADY_ITI_SECONDS
                                                 : SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
            subject.state = isSteadyAdaptationMode() ? SubjectState::kBlank
                                                      : SubjectState::kFixation;
            _needsTextureLoad = true; // Reload textures (same trial, but reset state)
        }
        break;
    case SubjectState::kBreak:
        // Continuing from break - transition to fixation and load new trial
        subject.currStateRemainderTime = isSteadyAdaptationMode()
                                             ? SETTINGS.STEADY_ITI_SECONDS
                                             : SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = isSteadyAdaptationMode() ? SubjectState::kBlank : SubjectState::kFixation;
        _needsTextureLoad = true; // Defer texture loading to avoid GPU sync issues
        break;
    case SubjectState::kBlank:
        subject.currStateRemainderTime = isSteadyAdaptationMode()
                                             ? SETTINGS.STEADY_CUE_DELAY_SECONDS
                                             : SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = SubjectState::kFixation;
        if (isSteadyAdaptationMode()) {
            ctx.apis.PlaySound(Sound::kGlassCue);
        }
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
    _preparedTrial = std::nullopt;
    _preparedTrialAvailable = false;
    _waitingForPreparedTrial = false;
    _pendingTrialGeneration = PendingTrialGeneration{};
    _deferredResponse = DeferredResponse{};

    std::string display_primaries_path = getTodayPrimariesPath();

    // Ensure PERCENTAGE_SCREENED is in sync with NUM_OBSERVERS via the real CDF
    if (_observerCDFDimension != SETTINGS.DIMENSION || _observerCDF.empty()) {
        try {
            _observerCDF = TetriumColor::ColorGeneratorFactory::GetObserverCDF(
                "both", SETTINGS.DIMENSION
            );
            _observerCDFDimension = SETTINGS.DIMENSION;
        } catch (const std::exception& e) {
            ERROR("Failed to load observer CDF: {}", e.what());
        }
    }
    if (!_observerCDF.empty()) {
        int idx = std::clamp(SETTINGS.NUM_OBSERVERS, 1, (int)_observerCDF.size()) - 1;
        SETTINGS.PERCENTAGE_SCREENED = _observerCDF[idx];
    }

    // Store picker type for later use
    _pickerType = SETTINGS.PICKER_TYPE;
    std::vector<int> observerIndices = ParseObserverIndexFilter(SETTINGS.OBSERVER_INDEX_FILTER);
    if (!observerIndices.empty()) {
        INFO("Using explicit observer indices: {}", JoinObserverIndices(observerIndices));
    }
    const std::string colorPickingSpace
        = SETTINGS.QUEST_COLOR_PICKING_SPACE == ColorPickingSpace::CONE_CONTRAST
              ? "cone_contrast"
              : "cone";
    const float trialBackgroundDrive
        = isSteadyAdaptationMode() ? SETTINGS.STEADY_BACKGROUND_DRIVE : SETTINGS.LUMINANCE;

    // Create Python ColorGenerator using factory
    PyObject* pColorGenerator = nullptr;
    try {
        std::vector<int> metameric_axes;
        std::vector<int> dimensions;
        if (SETTINGS.DIMENSION == 2) {
            dimensions = {2};
            if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
                metameric_axes = {2}; // Genetic MCS: single axis for dim-2
            } else {
                metameric_axes = {1, 2}; // Quest: test both M/L axes
            }
        } else {
            dimensions = {3};
            if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
                metameric_axes = {2}; // remapped dynamically to 547nm Q cone index per genotype
            } else if (SETTINGS.QUEST_TEST_ONLY_547NM) {
                metameric_axes = {2}; // remapped dynamically to 547nm Q cone index per genotype
            } else {
                metameric_axes = {1, 2, 3};
            }
        }

        if (SETTINGS.PICKER_TYPE == ColorPickerType::GENETIC) {
            // MCS mode: use Quest generator with K equally-spaced intensity levels
            pColorGenerator = TetriumColor::ColorGeneratorFactory::CreateQuestColorGenerator(
                "both",                        // sex
                SETTINGS.PERCENTAGE_SCREENED,  // percentage_screened
                trialBackgroundDrive,          // background_luminance
                SETTINGS.REPETITIONS_PER_AXIS, // trials_per_direction
                metameric_axes,                // metameric_axes
                dimensions,                    // dimensions
                display_primaries_path,        // display_primaries_path
                SETTINGS.QUEST_BIPOLAR,        // bipolar
                SETTINGS.VISUAL_ANGLE,         // degree (visual angle)
                SETTINGS.MCS_K,                // mcs_k: K intervals instead of Quest adaptive
                observerIndices,               // observer_indices: explicit population-sorted subset
                colorPickingSpace              // color_picking_space
            );
        } else if (SETTINGS.PICKER_TYPE == ColorPickerType::QUEST) {
            // Quest adaptive mode

            // Empty vector = test all axes
            pColorGenerator = TetriumColor::ColorGeneratorFactory::CreateQuestColorGenerator(
                "both",                              // sex
                SETTINGS.PERCENTAGE_SCREENED,        // percentage_screened
                trialBackgroundDrive,                // background_luminance
                SETTINGS.QUEST_TRIALS_PER_DIRECTION, // trials_per_direction
                metameric_axes,                      // metameric_axes
                dimensions,                          // dimensions
                display_primaries_path,              // display_primaries_path
                SETTINGS.QUEST_BIPOLAR,              // bipolar
                SETTINGS.VISUAL_ANGLE,               // degree (visual angle)
                0,                                   // mcs_k=0: use Quest adaptive
                observerIndices,                     // observer_indices: explicit population-sorted subset
                colorPickingSpace                    // color_picking_space
            );
        } else {
            pColorGenerator
                = TetriumColor::ColorGeneratorFactory::CreateAEPsychThresholdContourGenerator(
                    SETTINGS.AEPSYCH_NUM_TRIALS,
                    SETTINGS.AEPSYCH_NUM_SOBOL_TRIALS,
                    SETTINGS.AEPSYCH_THRESHOLD_LEVEL,
                    "both",
                    trialBackgroundDrive,
                    dimensions,
                    display_primaries_path,
                    42,
                    SETTINGS.AEPSYCH_N_CMF_SAMPLES,
                    SETTINGS.AEPSYCH_PATCH_SIGMA_SCALE,
                    SETTINGS.AEPSYCH_MIN_PATCH_MAJOR,
                    SETTINGS.AEPSYCH_MIN_PATCH_MINOR,
                    SETTINGS.AEPSYCH_MAX_RADIUS
                );
        }
    } catch (const std::exception& e) {
        ERROR("Failed to create ColorGenerator: {}", e.what());
        throw;
    }

    // Create test generator based on stimulus type
    PyObject* pTestGenerator = nullptr;
    try {
        switch (SETTINGS.STIMULUS_TYPE) {
        case StimulusType::PLATE:
            pTestGenerator
                = TetriumColor::ColorGeneratorFactory::CreatePseudoIsochromaticPlateGenerator(
                    pColorGenerator, 42
                );
            break;
        case StimulusType::BIPARTITE:
            pTestGenerator
                = TetriumColor::ColorGeneratorFactory::CreateBipartiteFieldGenerator(
                    pColorGenerator, 42, 512
                );
            break;
        case StimulusType::GAUSSIAN_BLOB:
            pTestGenerator
                = TetriumColor::ColorGeneratorFactory::CreateGaussianBlobGenerator(
                    pColorGenerator,
                    42,
                    1024,
                    SETTINGS.GAUSSIAN_BLOB_SIZE,
                    isSteadyAdaptationMode()
                );
            break;
        }
        Py_DECREF(pColorGenerator);
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
            SETTINGS.S_CONE_NOISE,
            trialBackgroundDrive,
            SETTINGS.DOT_SIZE,
            SETTINGS.VISUAL_ANGLE
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
           "stimulus_size",
           "gaussian_blob_size",
           "picker_type",
           "stimulus_type",
           "renderer_mode",
           "steady_background_drive",
           "steady_cue_delay_seconds",
           "steady_presentation_seconds",
           "steady_iti_seconds",
           "dimension",
           "color_picking_space",
           "quest_direction_idx",
           "quest_proportion",
           "quest_bipolar",
           "quest_color_picking_space",
           "quest_background_disp",
           "quest_adapting_background",
           "quest_adapting_background_space",
           "quest_inside_disp",
           "quest_outside_disp",
           "quest_genotype",
           "quest_metameric_axis",
           "quest_raw_cone_delta",
           "quest_cone_contrast_delta",
           "aepsych_sample_index",
           "aepsych_completed_trials",
           "aepsych_phase",
           "aepsych_a",
           "aepsych_b",
           "aepsych_u",
           "aepsych_v",
           "aepsych_r",
           "aepsych_theta",
           "aepsych_phi",
           "aepsych_direction",
           "aepsych_disp",
           "aepsych_q_axis",
           "aepsych_actual_max_radius",
           "aepsych_threshold_level"};

    // Generate additional info string with picker type, stimulus type, and dimension
    std::string pickerTypeStr = "Quest";
    if (_pickerType == ColorPickerType::GENETIC) {
        pickerTypeStr = "Genetic";
    } else if (_pickerType == ColorPickerType::AEPSYCH) {
        pickerTypeStr = "AEPsych";
    }
    const char* stimTypeStrs[] = {"Plate", "Bipartite", "GaussianBlob"};
    std::string stimTypeStr = stimTypeStrs[static_cast<int>(SETTINGS.STIMULUS_TYPE)];
    std::string additionalInfo
        = pickerTypeStr + "_" + stimTypeStr + "_dim" + std::to_string(SETTINGS.DIMENSION)
          + "_" + colorPickingSpace;
    if (isSteadyAdaptationMode()) {
        additionalInfo += "_steady_adaptation";
    }
    if (!observerIndices.empty()) {
        additionalInfo += "_obs" + JoinObserverIndices(observerIndices);
    }

    _logger = new TestDataLogger(
        "AppPseudoIsochromaticTest", _nameInputBuffer, headers, additionalInfo
    );

    _subject = SubjectContext{
        .name = _nameInputBuffer,
        .currStateRemainderTime = isSteadyAdaptationMode()
                                       ? SETTINGS.STEADY_ITI_SECONDS
                                       : SETTINGS.STATE_DURATIONS_SECONDS.FIXATION,
        .identificationStateStartTime = isSteadyAdaptationMode()
                                            ? SETTINGS.STEADY_PRESENTATION_SECONDS
                                            : SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION,
        .state = isSteadyAdaptationMode() ? SubjectState::kBlank : SubjectState::kFixation,
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

    if (_testGenerator) {
        std::filesystem::create_directories("../data/AppPseudoIsochromaticTest");
    }

    // Export Quest thresholds if using Quest color generator
    if (_pickerType == ColorPickerType::QUEST && _testGenerator) {
        std::string pickerTypeStr = "Quest";
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
    } else if (_pickerType == ColorPickerType::AEPSYCH && _testGenerator) {
        std::string filenameBase
            = subject.name + "_AEPsych_dim" + std::to_string(SETTINGS.DIMENSION);
        std::string timestamp = TestDataLogger::getCurrentTimestamp();
        std::string basePath
            = "../data/AppPseudoIsochromaticTest/" + filenameBase + "_" + timestamp;

        std::string modelPath = basePath + "_aepsych_model.pkl";
        if (_testGenerator->SaveModelState(modelPath)) {
            INFO("AEPsych model exported to {}", modelPath);
        } else {
            WARN("Failed to export AEPsych model");
        }

        std::string contourPath = basePath + "_aepsych_threshold_contour.npz";
        if (_testGenerator->ExportThresholdPatch(
                contourPath,
                SETTINGS.AEPSYCH_CONTOUR_SAMPLES_A,
                SETTINGS.AEPSYCH_CONTOUR_SAMPLES_B,
                SETTINGS.AEPSYCH_THRESHOLD_LEVEL
            )) {
            INFO("AEPsych threshold contour exported to {}", contourPath);
        } else {
            WARN("Failed to export AEPsych threshold contour");
        }

        std::string trialLogPath = basePath + "_aepsych_trials.csv";
        if (_testGenerator->ExportColorGeneratorTrialLog(trialLogPath)) {
            INFO("AEPsych trial log exported to {}", trialLogPath);
        } else {
            WARN("Failed to export AEPsych trial log");
        }
    }

    // The logged data is already saved via TestDataLogger
    _state = TestState::kTestResult;
}

void AppPseudoIsochromaticTest::drawFixGazePage()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 screenCenter = ImVec2(windowPos.x + windowSize.x * 0.5f, windowPos.y + windowSize.y * 0.5f);

    float crossHairSize = 70.f;
    float crossHairThickness = 10.f;
    ImU32 crossHairColor = IM_COL32(128, 128, 128, 128);

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
    bool correct,
    double intensity,
    const std::map<std::string, std::string>& metadata
)
{
    if (!_logger)
        return;

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
    data["gaussian_blob_size"] = std::to_string(SETTINGS.GAUSSIAN_BLOB_SIZE);
    if (_pickerType == ColorPickerType::GENETIC) {
        data["picker_type"] = "Genetic";
    } else if (_pickerType == ColorPickerType::AEPSYCH) {
        data["picker_type"] = "AEPsych";
    } else {
        data["picker_type"] = "Quest";
    }
    const char* stimTypeStrs[] = {"Plate", "Bipartite", "GaussianBlob"};
    data["stimulus_type"] = stimTypeStrs[static_cast<int>(SETTINGS.STIMULUS_TYPE)];
    data["renderer_mode"] = isSteadyAdaptationMode() ? "steady_adaptation" : "standard";
    data["steady_background_drive"] = std::to_string(SETTINGS.STEADY_BACKGROUND_DRIVE);
    data["steady_cue_delay_seconds"] = std::to_string(SETTINGS.STEADY_CUE_DELAY_SECONDS);
    data["steady_presentation_seconds"] = std::to_string(SETTINGS.STEADY_PRESENTATION_SECONDS);
    data["steady_iti_seconds"] = std::to_string(SETTINGS.STEADY_ITI_SECONDS);
    data["steady_temporal_noise_amplitude"]
        = std::to_string(SETTINGS.STEADY_TEMPORAL_NOISE_AMPLITUDE);
    data["steady_temporal_noise_tile_size"]
        = std::to_string(SETTINGS.STEADY_TEMPORAL_NOISE_TILE_SIZE);
    data["dimension"] = std::to_string(SETTINGS.DIMENSION);
    data["color_picking_space"]
        = SETTINGS.QUEST_COLOR_PICKING_SPACE == ColorPickingSpace::CONE_CONTRAST
              ? "cone_contrast"
              : "cone";

    for (const auto& [key, value] : metadata) {
        if (key.rfind("aepsych_", 0) == 0 || key.rfind("quest_", 0) == 0) {
            data[key] = value;
        }
    }

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
    initializeSteadyTemporalNoiseTextures(ctx);
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
    cleanupSteadyTemporalNoiseTextures(ctx);

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

void AppPseudoIsochromaticTest::initializeSteadyTemporalNoiseTextures(
    const TetriumApp::InitContext& ctx
)
{
    float tileSize = std::max(SETTINGS.STEADY_TEMPORAL_NOISE_TILE_SIZE, 1.0f);
    int width = std::max(1, static_cast<int>(std::ceil(ctx.swapchain.extent.width / tileSize)));
    int height = std::max(1, static_cast<int>(std::ceil(ctx.swapchain.extent.height / tileSize)));

    std::mt19937 rng(0x51ead123);
    std::uniform_int_distribution<int> bitDist(0, 1);
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);

    for (int frame = 0; frame < STEADY_NOISE_FRAME_COUNT; frame++) {
        for (int i = 0; i < width * height; i++) {
            uint8_t value = bitDist(rng) == 0 ? 0 : 255;
            pixels[static_cast<size_t>(i) * 4 + 0] = value;
            pixels[static_cast<size_t>(i) * 4 + 1] = value;
            pixels[static_cast<size_t>(i) * 4 + 2] = value;
            pixels[static_cast<size_t>(i) * 4 + 3] = 255;
        }

        _steadyNoiseTextureHandles[frame] = ctx.api.LoadTextureRGBA(pixels.data(), width, height);
        _steadyNoiseTextures[frame] = ctx.api.InitImGuiTexture(_steadyNoiseTextureHandles[frame]);
    }
}

void AppPseudoIsochromaticTest::cleanupSteadyTemporalNoiseTextures(
    const TetriumApp::CleanupContext& ctx
)
{
    for (uint32_t& handle : _steadyNoiseTextureHandles) {
        if (handle != 0) {
            ctx.api.UnloadTexture(handle);
            handle = 0;
        }
    }
    _steadyNoiseTextures = {};
}
} // namespace TetriumApp
