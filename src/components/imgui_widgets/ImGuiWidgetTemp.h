#include "ImGuiWidget.h"
#include "structs/ImGuiTexture.h"

// temporary widget that contains all the dirty hacks

class ImGuiWidgetTemp : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;

  private:
    // ---------- Fake video ---------- //
    bool _fakeVideoInitialized = false; // ugh
    void initFakeVideo(Tetrium* engine);
    void drawFakeVideo(Tetrium* engine, ColorSpace colorSpace);
    struct FakeVideoFrame {
        ImGuiTexture textures[ColorSpaceSize];
    };
};
