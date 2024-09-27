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

    int _selectedImageId = 0;

    float _zoom = 1.f;

    bool _imageFitWindow = false;

    bool _fullScreen = false;

    bool _noUI = false;

    static const int NUM_IMAGES = 2;
    std::array<DemoImage, NUM_IMAGES> _images = {
        DemoImage{
            .name = "observer vs noise",
            .path_rgb = "../assets/textures/observers_vs_noise-even.png",
            .path_ocv = "../assets/textures/observers_vs_noise-odd.png"
        },
        DemoImage{
            .name = "metamer pair",
            .path_rgb = "../assets/textures/metamer_pair__even.png",
            .path_ocv = "../assets/textures/metamer_pair__odd.png"
        },
    };

    struct TetraImage
    {
        VkDescriptorSet ds = VK_NULL_HANDLE;
        int width;
        int height;
    };

  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;
};
