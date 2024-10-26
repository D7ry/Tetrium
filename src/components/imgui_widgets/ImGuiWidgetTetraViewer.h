#pragma once
#include "ImGuiWidget.h"

// image file browser + viewer for RGB/OCV images
class ImGuiWidgetTetraViewer : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;

  private:
    struct TetraImageFile
    {
        std::string name;
        std::string paths[ColorSpaceSize];
    };

    bool _shouldRefreshFilePicker = false;

    void refreshTetraImagePicker();

    void drawTetraImagePicker(ColorSpace colorSpace);
    void drawTetraImage(Tetrium* engine, ColorSpace colorSpace, const TetraImageFile& image);

    std::vector<TetraImageFile> _tetraImages;

    int _currTetraImage = -1;

    static const char* TETRA_IMAGE_FOLDER_PATH;

    // scaling params
    bool _fullScreen = false;
    float _zoom = 1.f;
};
