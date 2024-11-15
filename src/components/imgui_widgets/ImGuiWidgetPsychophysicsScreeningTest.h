#include "ImGuiWidget.h"


// for psychophysics screening tests
class ImGuiWidgetPhychophysicsScreeningTest : public ImGuiWidgetMut
{
  public:
    virtual void Init(const InitContext* ctx) override
    {
        // set up frame buffers


        // set up internal states
    }

    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override {
        


    }

  private:
    enum class TestState {
        kUninitialized,

    };
};
