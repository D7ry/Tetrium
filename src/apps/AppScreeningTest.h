#pragma once

#include "TetriumColor/PseudoIsochromaticPlateGenerator.h"

#include "App.h"

namespace TetriumApp
{
class AppScreeningTest : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override {};
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override {};

    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
    struct
    {
        int NUM_ATTEMPTS = 3; // number of attempts one could try in a screening

        struct
        {
            float FIXATION = 1;
            float IDENTIFICATION = 1;
            float ANSWERING = 5;
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
        kFixation,
        kIdentification,
        kAnswer,
    };

    enum class AnswerKind
    {
        kUp,
        kDown,
        kLeft,
        kRight,
        kDontKnow
    };

    struct SubjectPromptContext
    {
        std::string currentIshiharaTexturePath[ColorSpace::ColorSpaceSize];
        std::string currentAnswerTexturePath[4];
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
    // draw subroutines
    void drawIdle(const TetriumApp::TickContextImGui& ctx);

    void drawSettingsWindow(const TetriumApp::TickContextImGui& ctx);

    void drawTestForSubject(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawSubjectResult(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawIshihara(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawAnswerPrompts(SubjectContext& subject, const TetriumApp::TickContextImGui& ctx);

    void drawFixGazePage();

    ImGuiTexture GetAnswerPromptTextureDigit(uint32_t digit);

    // game logic
    void newGame();

    void transitionSubjectState(SubjectContext& subject);

    void endGame(SubjectContext& subject);

    // generate a pair of ishihara textures and store them in a subject folder, returns
    // path to generated textures(RGB, OCV) -- the textures are already loaded into GPU memory.
    // the caller is responsible for freeing generated resources.
    std::pair<std::string, std::string> generateIshiharaTestTextures(
        SubjectContext& subject,
        int number
    );

    std::string _nameInputBuffer = "tian";

    TetriumColor::PseudoIsochromaticPlateGenerator* _plateGenerator = nullptr;

    void populatePromptContext(SubjectContext& subject);
};
} // namespace TetriumApp
