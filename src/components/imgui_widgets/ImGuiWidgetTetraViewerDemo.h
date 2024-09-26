#include "ImGuiWidget.h"
#include "lib/VQDevice.h"

// demo class to view a tetrachromatic image
class ImGuiWidgetTetraViewerDemo : public ImGuiWidget
{
  private:
    const char* TETRA_IMAGE_PATH = "../assets/images/";


  public:
    virtual void Draw(const Quarkolor* engine, ColorSpace colorSpace) override;

    void TempSetup(VQDevice* device);
};
