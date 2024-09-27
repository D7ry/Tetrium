#pragma once
#include "ImGuiWidget.h"

// prototype demo class to view a tetrachromatic image
class ImGuiWidgetTetraViewerDemo : public ImGuiWidgetMut
{
  private:
    struct DemoImage
    {
        const char* name;
        const char* path_rgb;
        const char* path_ocv;
    };

    DemoImage images[5] = {
        DemoImage{
            .name = "observer vs noise",
            .path_rgb = "../assets/textures/observer_vs_noise_even.png",
            .path_ocv = "../assets/textures/observer_vs_noise_odd.png"
        },
        DemoImage{
            "metamer pair"

        },
    };
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
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;
};
