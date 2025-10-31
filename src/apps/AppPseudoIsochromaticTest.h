#pragma once

#include "TetriumColor/GeneticColorPicker.h"
#include "TetriumColor/GeneticColorPickerPlateGenerator.h"
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

    struct
    {
        int REPETITIONS_PER_AXIS = 3; // number of repetitions for each genotype × metameric axis
        MusicSetting MUSIC_SETTING = MusicSetting::CORRECT_WRONG;
        float LUM_NOISE = 0.0f;     // luminance noise [0.0, 1.0]
        float S_CONE_NOISE = 0.1f;  // s-cone noise [0.0, 1.0]
        float STIMULUS_SIZE = 0.5f; // stimulus texture size multiplier [0.0, 1.0]

        struct
        {
            float BLANK = 1;
            float FIXATION = 2;
            float IDENTIFICATION = 1;
            float ANSWERING = 3;
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
    };

    // Structure to represent a single trial
    struct Trial
    {
        std::string genotype; // The genotype string
        int metameric_axis;   // 0, 1, 2, or 3
        int repetition_idx;   // Which repetition this is (0 to REPETITIONS_PER_AXIS-1)
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
    };

    struct SubjectContext
    {
        std::string name;
        float currStateRemainderTime; // remaining time before jumping to next state
        SubjectState state;
        uint32_t currentTrialIndex;  // index to the current trial in the trial list
        uint32_t numSuccessAttempts; // # of attempts where the tester identified the right pattern
        SubjectPromptContext prompt;
    };

    TestState _state;

    SubjectContext _subject;

    // List of all trials (randomized)
    std::vector<Trial> _trials;

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

    // game logic
    void newGame(const TetriumApp::TickContextImGui& ctx);

    void transitionSubjectState(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void endGame(SubjectContext& subject);

    // generate a pair of Landolt C textures for a specific genotype and metameric axis
    std::pair<std::string, std::string> generateLandoltCTextures(
        SubjectContext& subject,
        const std::string& genotype,
        int metameric_axis,
        AnswerKind orientation
    );

    std::string _nameInputBuffer = "guest";

    TetriumColor::GeneticColorPicker* _colorPicker = nullptr;
    TetriumColor::GeneticColorPickerPlateGenerator* _plateGenerator = nullptr;
    TestDataLogger* _logger = nullptr;

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

    // Build the randomized trial list
    void buildTrialList();

    // Get the current trial
    const Trial& getCurrentTrial() const { return _trials[_subject.currentTrialIndex]; }
};
} // namespace TetriumApp
