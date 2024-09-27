
#include "backends/imgui_impl_vulkan.h"

#include "Tetrium.h"

#include "ImGuiWidgetTetraViewerDemo.h"
#include "components/TextureManager.h"

void ImGuiWidgetTetraViewerDemo::Draw(Tetrium* engine, ColorSpace colorSpace)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    // image selector
    int selectedImageId = _selectedImageId;
    const char* items[NUM_IMAGES];
    for (int i = 0; i < NUM_IMAGES; i++) {
        items[i] = _images[i].name;
    }

    if (ImGui::Begin("Tetra Viewer")) {
        if (ImGui::Combo("Select Image", &selectedImageId, items, NUM_IMAGES)
            && colorSpace == ColorSpace::RGB) {
            _selectedImageId = selectedImageId;
        }

        DemoImage& image = _images[_selectedImageId];

        ImGuiTexture tex;
        switch (colorSpace) {
        case ColorSpace::RGB:
            tex = engine->_imguiManager.GetImGuiTexture(image.path_rgb);
            break;
        case ColorSpace::OCV:
            tex = engine->_imguiManager.GetImGuiTexture(image.path_ocv);
            break;
        }
        ImGui::Image(tex.ds, {(float)tex.width, (float)tex.height});
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
