#pragma once
#include "ImGuiWidget.h"
#include "structs/ImGuiTexture.h"

// for psychophysics screening tests
class ImGuiWidgetPhychophysicsScreeningTest : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;

  private:
    const struct
    {
        const uint32_t NUM_ATTEMPTS = 3; // number of attempts one could try in a screening
        const struct
        {
            const float FIXATION = 1;
            const float IDENTIFICATION = 1;
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
        std::string name;
        float currStateRemainderTime; // remaining time before jumping to next state
        SubjectState state;
        uint32_t currentAttempt;     // index to the current attempt
        uint32_t numSuccessAttempts; // # of attempts where the tester identified the right pattern
        std::string currentIshiharaTexturePath[ColorSpace::ColorSpaceSize];
        std::string currentAnswerTexturePath[4];
        uint8_t correctAnswerTextureIndex;
        int currentSelectedAnswer = -1;
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
    void transitionSubjectState(SubjectContext& subject);
    void endGame(SubjectContext& subject);

    // generate a pair of ishihara textures and store them in a subject folder, returns
    // path to generated textures(RGB, OCV) -- the textures are already loaded into GPU memory.
    // the caller is responsible for freeing generated resources.
    std::pair<std::string, std::string> generateIshiharaTestTextures(SubjectContext& subject);


    std::string _nameInputBuffer;
};
