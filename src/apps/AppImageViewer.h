#include "App.h"

namespace TetriumApp
{
// image file browser + viewer for RGB/OCV images
class AppImageViewer : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override {};
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;

    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
    struct TetraImageFile
    {
        std::string name;
        std::string fileNames[ColorSpaceSize];
        uint32_t textureHandles[ColorSpaceSize];
        ImGuiTexture textures[ColorSpaceSize];
    };

    bool _wantReloadImages = true;

    void refreshTetraImagePicker(const TickContextImGui& ctx);

    void drawControlPrompts();
    void drawTetraImagePicker(const TickContextImGui& ctx, ColorSpace colorSpace);
    void drawTetraImage(const TickContextImGui& ctx, ColorSpace colorSpace, const TetraImageFile& image);

    void pollControls();

    std::vector<TetraImageFile> _tetraImages;

    int _currTetraImage = -1;

    inline static const char* TETRA_IMAGE_FOLDER_PATH = "../assets/apps/AppImageViewer/tetra_images/";

    // scaling params
    float _zoom = 1.f;               // zoom factor, disabled when _adaptiveImageSize = true
    bool _noUI = false;              // do not draw any UI elements, only image
    bool _adaptiveImageSize = false; // automatically scale image size to window size
};
} // namespace TetriumApp
