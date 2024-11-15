#include "ImGuiWidget.h"


// for psychophysics screening tests
class ImGuiWidgetPhychophysicsScreeningTest : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override {

    }

  private:
    static const struct {
        const uint32_t NUM_ATTEMPTS; // number of attempts one could try in a screening
    } SETTINGS;

    enum class TestState {
        kIdle, // no test going on, the user needs to begin the test
        kScreening
    };

    struct ScreeningState {
        uint32_t currentAttempt; // index to the current attempt
        uint32_t numSuccessAttempts;
    };
};
