#pragma once
#include "structs/ImGuiTexture.h"
#include "ImGuiWidget.h"

// for psychophysics screening tests
class ImGuiWidgetPhychophysicsScreeningTest : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;

  private:
    static const struct
    {
        const uint32_t NUM_ATTEMPTS; // number of attempts one could try in a screening
        const struct {
            const float FIXATION = 3;
            const float IDENTIFICATION = 3;
            const float ANSWERING = 5;
        } STATE_DURATIONS_SECONDS;
    } SETTINGS;

    enum class TestState
    {
        kIdle, // no test going on, the user needs to begin the test
        kScreening
    };

    enum class SubjectState
    {
        kFixation,
        kIdentification,
        kAnswer
    };

    enum class AnswerKind
    {
        kUp,
        kDown,
        kLeft,
        kRight,
        kDontKnow
    };

    struct SubjectContext
    {
        float currStateRemainderTime; // remaining time before jumping to next state
        SubjectState state;
        uint32_t currentAttempt;     // index to the current attempt
        uint32_t numSuccessAttempts; // # of attempts where the tester identified the right pattern
        std::string currentIshiharaTexturePath[ColorSpace::ColorSpaceSize];
        void* pyObject; // python object that keeps states and generates ishihara textures
    };

    TestState _state;

    SubjectContext _subject;

  private:
    // draw subroutines
    void drawIdle(Tetrium* engine, ColorSpace colorSpace);
    void drawTestForSubject(Tetrium* engine, ColorSpace colorSpace, SubjectContext& subject);
    void drawFixGazePage();
    void drawIshihara(Tetrium* engine, SubjectContext& subject, ColorSpace cs);
    void drawAnswerPrompts(Tetrium* engine, SubjectContext& subject, ColorSpace cs);

    ImGuiTexture GetAnswerPromptTextureDigit(uint32_t digit);

    // game logic
    void newGame();

    std::pair<std::string, std::string> generateIshiharaTestTextures(SubjectContext& subject);
};
