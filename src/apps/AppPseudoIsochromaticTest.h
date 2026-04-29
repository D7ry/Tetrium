#pragma once

#include "TetriumColor/ColorTestResult.h"
#include "TetriumColor/TestGenerator.h"
#include "TetriumColor/TrialData.h"
#include "lib/TestDataLogger.h"

#include "App.h"
#include <map>
#include <optional>
#include <random>
#include <string>

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
        QUEST,
        AEPSYCH
    };

    enum class StimulusType
    {
        PLATE,
        BIPARTITE,
        GAUSSIAN_BLOB
    };

    enum class TrialTimingMode
    {
        FIXED_TRIAL_TIME, // Wait for full duration even if answered early
        EARLY_EXIT        // Move to next state immediately after answer
    };

    struct
    {
        ColorPickerType PICKER_TYPE = ColorPickerType::QUEST;
        StimulusType STIMULUS_TYPE = StimulusType::GAUSSIAN_BLOB;
        int REPETITIONS_PER_AXIS
            = 5; // number of repetitions per (genotype × axis × intensity level) in Genetic MCS mode
        int MCS_K = 3; // number of equally-spaced intensity levels in [0,1] for Genetic MCS (1 = max only)
        int NUM_OBSERVERS = 5;            // desired number of observer genotypes to test (1,2,4,9,21)
        std::string OBSERVER_INDEX_FILTER = ""; // optional zero-based observer indices, e.g. "0,1,4,5"
        float PERCENTAGE_SCREENED = 0.995f; // derived from NUM_OBSERVERS; passed to Python generator
        int QUEST_TRIALS_PER_DIRECTION = 20; // number of Quest trials per direction (Quest mode)
        bool QUEST_TEST_ONLY_547NM
            = true; // Quest: only test the Q cone (547nm); axis index is computed dynamically per genotype
        bool QUEST_BIPOLAR
            = true;        // Quest: use bipolar sampling (sample in both direction and -direction)
        int AEPSYCH_NUM_TRIALS = 300;
        int AEPSYCH_NUM_SOBOL_TRIALS = 20;
        float AEPSYCH_THRESHOLD_LEVEL = 0.75f;
        int AEPSYCH_N_CMF_SAMPLES = 200;
        float AEPSYCH_PATCH_SIGMA_SCALE = 5.0f;
        float AEPSYCH_MIN_PATCH_MAJOR = 0.15f;
        float AEPSYCH_MIN_PATCH_MINOR = 0.06f;
        float AEPSYCH_MAX_RADIUS = 0.65f; // <= 0 passes Python None
        int AEPSYCH_CONTOUR_SAMPLES_A = 31;
        int AEPSYCH_CONTOUR_SAMPLES_B = 31;
        int DIMENSION = 3; // dimension: 2 for M/L cone testing, 3 for full trichromat
        int BREAK_INTERVAL = 50; // number of trials between breaks (0 = no breaks)
        MusicSetting MUSIC_SETTING = MusicSetting::CORRECT_WRONG;
        TrialTimingMode TIMING_MODE = TrialTimingMode::EARLY_EXIT; // Timing behavior for trials
        float LUM_NOISE = 0.0f;                                    // luminance noise [0.0, 1.0]
        float S_CONE_NOISE = 0.1f;                                 // s-cone noise [0.0, 1.0]
        float STIMULUS_SIZE = 0.5f; // stimulus texture size multiplier [0.0, 1.0]
        float STIMULUS_RAMP_UP_DURATION
            = 0.5f;                     // duration in seconds for brightness ramp-up [0.0, 10.0]
        float LUMINANCE = 0.5f;         // luminance level [0.0, 2.0]
        float DOT_SIZE = 1.0f;          // dot size multiplier for plates [0.5, 2.0]
        float VISUAL_ANGLE = 2.0f;      // stimulus visual angle in degrees [1.0, 10.0]
        float VIEWING_DISTANCE = 57.0f; // viewing distance in cm [30.0, 200.0]
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
        float currStateRemainderTime;       // remaining time before jumping to next state
        float identificationStateStartTime; // initial duration when entering identification state
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

    void drawLandoltC(
        SubjectContext& subject,
        const TetriumApp::TickContextImGui& ctx,
        float brightness
    );

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
        bool correct,
        double intensity,
        const std::map<std::string, std::string>& metadata = {}
    );

    // Static helper functions for Landolt C orientations
    static std::string GetLandoltCAnswerTexturePath(AnswerKind orientation);
    static std::string OrientationToString(AnswerKind orientation);

    std::unordered_map<AnswerKind, uint32_t> _answerPromptTextureHandles = {};
    std::unordered_map<AnswerKind, ImGuiTexture> _answerPromptImGuiTextures = {};

    std::vector<float> _observerCDF;   // cumulative probabilities per observer from Python
    int _observerCDFDimension = -1;    // dimension used when _observerCDF was last loaded

    // Static map for orientation to string conversion
    static const std::unordered_map<AnswerKind, std::string> _orientationToStringMap;
};
} // namespace TetriumApp
