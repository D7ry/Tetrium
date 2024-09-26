#pragma once
#include "ImGuiWidget.h"

// demo class to view a tetrachromatic image
class ImGuiWidgetTetraViewerDemo : public ImGuiWidgetMut
{
  private:
    const char* TETRA_IMAGE_PATH = "../assets/images/";


  public:
    virtual void Draw(Quarkolor* engine, ColorSpace colorSpace) override;
};
