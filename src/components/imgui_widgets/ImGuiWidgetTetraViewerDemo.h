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

    std::vector<DemoImage> _images = {
        DemoImage {
            .name = "Gradients",
            .path_rgb = "../assets/textures/gradient_RGB.png",
            .path_ocv = "../assets/textures/gradient_OCV.png"
        },
        DemoImage {
            .name = "Gamut",
            .path_rgb = "../assets/textures/gamut_RGB.png",
            .path_ocv = "../assets/textures/gamut_OCV.png"
        },
        DemoImage {
            .name = "Cubemap",
            .path_rgb = "../assets/textures/cubemap_RGB.png",
            .path_ocv = "../assets/textures/cubemap_OCV.png"
        }
        // DemoImage{
        //     .name = "observer vs noise",
        //     .path_rgb = "../assets/textures/observers_vs_noise-even.png",
        //     .path_ocv = "../assets/textures/observers_vs_noise-odd.png"
        // },
        // DemoImage{
        //     .name = "metamer pair",
        //     .path_rgb = "../assets/textures/metamer_pair__even.png",
        //     .path_ocv = "../assets/textures/metamer_pair__odd.png"
        // },
        // DemoImage{
        //     .name = "govardovskii_common_genes",
        //     .path_rgb = "../assets/textures/govardovskii_common_genes-rgb.png",
        //     .path_ocv = "../assets/textures/govardovskii_common_genes-ocv.png"
        // },
        // DemoImage{
        //     .name = "govardovskii_common_genes rdmclr",
        //     .path_rgb = "../assets/textures/govardovskii_common_genes_rdmclr-rgb.png",
        //     .path_ocv = "../assets/textures/govardovskii_common_genes_rdmclr-ocv.png"
        // },
        // DemoImage{
        //     .name = "neitz_common_genes",
        //     .path_rgb = "../assets/textures/neitz_common_genes-rgb.png",
        //     .path_ocv = "../assets/textures/neitz_common_genes-ocv.png"
        // },
        // DemoImage{
        //     .name = "neitz_common_genes rdmclr",
        //     .path_rgb = "../assets/textures/neitz_common_genes_rdmclr-rgb.png",
        //     .path_ocv = "../assets/textures/neitz_common_genes_rdmclr-ocv.png"
        // },
        // DemoImage{
        //     .name = "stockman_vs_neitz_vs_govardovskii",
        //     .path_rgb = "../assets/textures/stockman_vs_neitz_vs_govardovskii-rgb.png",
        //     .path_ocv = "../assets/textures/stockman_vs_neitz_vs_govardovskii-ocv.png"
        // },
        // DemoImage{
        //     .name = "stockman_vs_neitz_vs_govardovskii rdmclr",
        //     .path_rgb = "../assets/textures/stockman_vs_neitz_vs_govardovskii_rdmclr-rgb.png",
        //     .path_ocv = "../assets/textures/stockman_vs_neitz_vs_govardovskii_rdmclr-ocv.png"
        // },
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
