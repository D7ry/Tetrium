#pragma once
#include "ImGuiWidget.h"
#include "structs/ColorSpace.h"

class ImGuiWidgetColorTile : public ImGuiWidget
{
  public:
    virtual void Draw(const Tetrium* engine, ColorSpace colorSpace) override;

  private:
    
    // initial color, all black
    float _leftTileColor[ColorSpaceSize][4] = {
        {0.f, 0.f, 0.f, 1.f},
        {0.f, 0.f, 0.f, 1.f},
    };
    float _rightTileColor[ColorSpaceSize][4] = {
        {0.f, 0.f, 0.f, 1.f},
        {0.f, 0.f, 0.f, 1.f},
    };
};
