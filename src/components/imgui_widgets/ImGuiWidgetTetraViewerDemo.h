#pragma once
#include "ImGuiWidget.h"

// demo class to view a tetrachromatic image
class ImGuiWidgetTetraViewerDemo : public ImGuiWidgetMut
{
  private:
    const char* TETRA_IMAGE_PATH_RGB = "../assets/textures/metamer_pair__even.png";
    const char* TETRA_IMAGE_PATH_CMY = "../assets/textures/metamer_pair__odd.png";

    struct TetraImage
    {
        VkDescriptorSet ds = VK_NULL_HANDLE;
        int width;
        int height;
    };

    TetraImage rgb;
    TetraImage cmy;
    bool rgbLoaded = false;
    bool cmyLoaded = false;

  public:
    virtual void Draw(Quarkolor* engine, ColorSpace colorSpace) override;
};
