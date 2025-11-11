#pragma once

#include "TetriumColor/ColorTestResult.h"
#include "TetriumColor/TestGenerator.h"
#include "TetriumColor/TrialData.h"
#include "lib/TestDataLogger.h"

#include "App.h"
#include <optional>
#include <random>

namespace TetriumApp
{
class AppPseudoIsochromaticTest : public App
{
  public:
    enum class AnswerKind
    {
        kUp,
        kDown,
        kLeft,
        kRight,
        kDontKnow
    };

    virtual void Init(TetriumApp::InitContext& ctx) override;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;

    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
    enum class MusicSetting
    {
        ALL,
        CORRECT_WRONG,
        OFF
    };

    enum class ColorPickerType
    {
        GENETIC,
        QUEST
    };

    struct
    {
        ColorPickerType PICKER_TYPE = ColorPickerType::QUEST;
        int REPETITIONS_PER_AXIS
            = 4; // number of repetitions for each genotype × metameric axis (Genetic mode)
        int QUEST_TRIALS_PER_DIRECTION = 10; // number of Quest trials per direction (Quest mode)
        bool QUEST_TEST_ONLY_547NM
            = true;              // Quest: only test axis 1 (547nm cone) instead of all axes
        int DIMENSION = 3;       // dimension: 2 for M/L cone testing, 3 for full trichromat
        int BREAK_INTERVAL = 50; // number of trials between breaks (0 = no breaks)
        MusicSetting MUSIC_SETTING = MusicSetting::CORRECT_WRONG;
        float LUM_NOISE = 0.0f;     // luminance noise [0.0, 1.0]
        float S_CONE_NOISE = 0.1f;  // s-cone noise [0.0, 1.0]
        float STIMULUS_SIZE = 0.5f; // stimulus texture size multiplier [0.0, 1.0]

        struct
        {
            float BLANK = 1;
            float FIXATION = 1;
            float IDENTIFICATION = 1;
            float ANSWERING = 2;
        } STATE_DURATIONS_SECONDS;
    } SETTINGS;

    enum class TestState
    {
        kIdle,     // no test going on, the user needs to begin the test
        kSettings, // settings window
        kTesting,
        kTestResult // show the result of the test
    };

    enum class SubjectState
    {
        kBlank,
        kFixation,
        kIdentification,
        kAnswer,
        kBreak,
    };

    struct SubjectPromptContext
    {
        uint32_t currentLandoltCTextureHandle[ColorSpace::ColorSpaceSize] = {};
        ImGuiTexture currentLandoltCTexture[ColorSpace::ColorSpaceSize];
        uint32_t currentAnswerTextureHandle[4];
        ImGuiTexture currentAnswerTexture[4];
        int correctAnswerTextureIndex;
        int currentSelectedAnswer = -1;
        AnswerKind currentOrientation; // Track current trial orientation for logging
        bool responseGiven = false;    // Flag to prevent multiple responses per trial
    };

    struct SubjectContext
    {
        std::string name;
        float currStateRemainderTime; // remaining time before jumping to next state
        SubjectState state;
        uint32_t currentTrialIndex;  // index to the current trial in the trial list
        uint32_t numSuccessAttempts; // # of attempts where the tester identified the right pattern
        uint32_t trialsSinceLastBreak; // # of trials since last break
        SubjectPromptContext prompt;
    };

    TestState _state;

    SubjectContext _subject;

    // Store picker type to know if we should show Quest thresholds
    ColorPickerType _pickerType = ColorPickerType::QUEST;

    // Current trial data (generated on-demand)
    std::optional<TetriumColor::TrialData> _currentTrial;

    // Input state captured once per frame
    int _capturedGamepadInput = -1; // -1 = no input, 0-3 = button index

    // Deferred actions to execute after frame completes
    struct DeferredResponse
    {
        bool hasResponse = false;
        int buttonIndex;
        bool correct;
        AnswerKind orientation;
        std::optional<TetriumColor::TrialData> previousTrial; // Store trial data for logging
        bool needsTrialGeneration = false; // Flag to generate next trial at start of next frame
    };

    DeferredResponse _deferredResponse;

    // Flag to defer state transition to next frame (to avoid texture loading mid-frame)
    bool _needsStateTransition = false;

    // Flag to defer texture loading to start of next frame (to avoid GPU sync issues)
    bool _needsTextureLoad = false;

  private:
    struct
    {
        ImGuiTexture chromalabLogo;

    } _textures;

    // draw subroutines
    void drawIdle(const TetriumApp::TickContextImGui& ctx);

    void drawSettingsWindow(const TetriumApp::TickContextImGui& ctx);

    void drawTestForSubject(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawSubjectResult(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawLandoltC(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawAnswerPrompts(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawFixGazePage();

    void drawBreakWindow(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    // game logic
    void newGame(const TetriumApp::TickContextImGui& ctx);

    void transitionSubjectState(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void endGame(SubjectContext& subject);

    std::string _nameInputBuffer = "guest";

    TetriumColor::TestGenerator* _testGenerator = nullptr;
    TestDataLogger* _logger = nullptr;

    // Track trial counter for filename generation
    int _trialCounter = 0;

    void populatePromptContext(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);
    void logTrialData(
        const SubjectContext& subject,
        const std::string& genotype,
        int metameric_axis,
        AnswerKind orientation,
        int userChoice,
        bool correct
    );

    // Static helper functions for Landolt C orientations
    static std::string GetLandoltCAnswerTexturePath(AnswerKind orientation);
    static std::string OrientationToString(AnswerKind orientation);

    std::unordered_map<AnswerKind, uint32_t> _answerPromptTextureHandles = {};
    std::unordered_map<AnswerKind, ImGuiTexture> _answerPromptImGuiTextures = {};

    // Static map for orientation to string conversion
    static const std::unordered_map<AnswerKind, std::string> _orientationToStringMap;
};
} // namespace TetriumApp
