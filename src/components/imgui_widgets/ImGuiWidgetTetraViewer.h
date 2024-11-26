#pragma once
#include "ImGuiWidget.h"

// image file browser + viewer for RGB/OCV images
class ImGuiWidgetTetraViewer : public ImGuiWidget
{
  public:
    ImGuiWidgetTetraViewer();
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;

  private:
    struct TetraImageFile
    {
        std::string name;
        std::string paths[ColorSpaceSize];
    };

    bool _shouldRefreshFilePicker = false;

    void refreshTetraImagePicker();

    void drawControlPrompts();
    void drawTetraImagePicker(ColorSpace colorSpace);
    void drawTetraImage(Tetrium* engine, ColorSpace colorSpace, const TetraImageFile& image);

    void pollControls();

    std::vector<TetraImageFile> _tetraImages;

    int _currTetraImage = -1;

    static const char* TETRA_IMAGE_FOLDER_PATH;

    // scaling params
    bool _fullScreen = false; // full-screen the window
    float _zoom = 1.f; // zoom factor, disabled when _adaptiveImageSize = true
    bool _noUI = false; // do not draw any UI elements, only image
    bool _adaptiveImageSize = false; // automatically scale image size to window size
};
