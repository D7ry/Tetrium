#include "ImGuiWidget.h"

// demo class to view a tetrachromatic image
class ImGuiWidgetTetraViewerDemo : public ImGuiWidget
{
  public:
    virtual void Draw(const Quarkolor* engine, ColorSpace colorSpace) override {}
};
