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
    } SETTINGS;

    enum class TestState
    {
        kIdle, // no test going on, the user needs to begin the test
        kScreening
    };

    enum class SubjectState
    {
        kPreparing,
        kIdentifying,
        kAnswering
    };

    struct SubjectContext
    {
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

    // game logic
    void newGame();

    std::pair<std::string, std::string> generateIshiharaTestTextures(SubjectContext& subject);
};
