#pragma once

#include "TetriumColor/ColorGenerator.h"
#include "TetriumColor/PseudoIsochromaticPlateGenerator.h"

#include "App.h"

namespace TetriumApp
{
class AppScreeningTest : public App
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
        int NUM_ATTEMPTS = 8; // number of attempts one could try in a screening
        MusicSetting MUSIC_SETTING = MusicSetting::CORRECT_WRONG;
        float LUM_NOISE = 0.0f;     // luminance noise [0.0, 1.0]
        float S_CONE_NOISE = 0.1f;  // s-cone noise [0.0, 1.0]
        float STIMULUS_SIZE = 0.5f; // stimulus texture size multiplier [0.0, 1.0]

        struct
        {
            float BLANK = 1;
            float FIXATION = 3;
            float IDENTIFICATION = 2;
            float ANSWERING = 3;
        } STATE_DURATIONS_SECONDS;
    } SETTINGS;

    enum class TestState
    {
        kIdle,     // no test going on, the user needs to begin the test
        kSettings, // settings window
        kScreening,
        kScreenResult // show the result of the screening
    };

    enum class SubjectState
    {
        kBlank,
        kFixation,
        kIdentification,
        kAnswer,
    };

    struct SubjectPromptContext
    {
        uint32_t currentLandoltCTextureHandle[ColorSpace::ColorSpaceSize] = {};
        ImGuiTexture currentLandoltCTexture[ColorSpace::ColorSpaceSize];
        uint32_t currentAnswerTextureHandle[4];
        ImGuiTexture currentAnswerTexture[4];
        int correctAnswerTextureIndex;
        int currentSelectedAnswer = -1;
    };

    struct SubjectContext
    {
        std::string name;
        float currStateRemainderTime; // remaining time before jumping to next state
        SubjectState state;
        uint32_t currentAttempt;     // index to the current attempt
        uint32_t numSuccessAttempts; // # of attempts where the tester identified the right pattern
        SubjectPromptContext prompt;
        void* pyObject; // python object that keeps states and generates ishihara textures
    };

    TestState _state;

    SubjectContext _subject;

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

    // generate a pair of Landolt C textures and store them in a subject folder, returns
    // path to generated textures(RGB, OCV) -- the textures are already loaded into GPU memory.
    // the caller is responsible for freeing generated resources.
    std::pair<std::string, std::string> generateLandoltCTextures(
        SubjectContext& subject,
        AnswerKind orientation
    );

    std::string _nameInputBuffer = "guest";

    TetriumColor::ColorGenerator* _colorGenerator = nullptr;
    TetriumColor::PseudoIsochromaticPlateGenerator* _plateGenerator = nullptr;

    void populatePromptContext(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    // Static helper functions for Landolt C orientations
    static std::string GetLandoltCAnswerTexturePath(AnswerKind orientation);
    static std::string OrientationToString(AnswerKind orientation);

    std::unordered_map<AnswerKind, uint32_t> _answerPromptTextureHandles = {};
    std::unordered_map<AnswerKind, ImGuiTexture> _answerPromptImGuiTextures = {};

    // Static map for orientation to string conversion
    static const std::unordered_map<AnswerKind, std::string> _orientationToStringMap;
};
} // namespace TetriumApp
