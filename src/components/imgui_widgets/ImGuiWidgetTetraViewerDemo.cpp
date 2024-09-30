
#include "backends/imgui_impl_vulkan.h"

#include "Tetrium.h"

#include "ImGuiWidgetTetraViewerDemo.h"
#include "components/TextureManager.h"

ImVec2 calculateFitSize(const ImGuiTexture& texture, const ImVec2& availableSize)
{
    float aspectRatio = (float)texture.width / (float)texture.height;

    float scaleWidth = availableSize.x / (float)texture.width;
    float scaleHeight = availableSize.y / (float)texture.height;

    float scale = std::min(scaleWidth, scaleHeight);

    ImVec2 fitSize;
    fitSize.x = (float)texture.width * scale;
    fitSize.y = (float)texture.height * scale;

    return fitSize;
}

void ImGuiWidgetTetraViewerDemo::Draw(Tetrium* engine, ColorSpace colorSpace)
{
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    // image selector
    int selectedImageId = _selectedImageId;
    const char* items[NUM_IMAGES];
    for (int i = 0; i < NUM_IMAGES; i++) {
        items[i] = _images[i].name;
    }

    if (colorSpace == ColorSpace::RGB) {
        if (ImGui::IsKeyPressed(ImGuiKey_K)) {
            _zoom += 0.1;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_J)) {
            _zoom -= 0.1;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F)) {
            _fullScreen = !_fullScreen;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_B)) {
            _noUI = !_noUI;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_L)) {
            _selectedImageId++;
            _selectedImageId = _selectedImageId % NUM_IMAGES;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_H)) {
            _selectedImageId--;
            if (_selectedImageId == -1) { // wrap
                _selectedImageId = NUM_IMAGES - 1;
            }
        }
    }

    ImGuiWindowFlags flags = 0;
    if (_fullScreen) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize;
    }

    if (ImGui::Begin("Tetra Viewer", NULL, flags)) {
        if (!_noUI) {
            ImGui::Text("J/K : zoom in/out. H/L: cycle images. F: toggle full screen mode. B: toggle UI");
            bool fitWindow = _imageFitWindow;
            if (ImGui::Checkbox("Fit Window", &fitWindow) && colorSpace == RGB) {
                _imageFitWindow = fitWindow;
            }

            if (ImGui::Combo("Select Image", &selectedImageId, items, NUM_IMAGES)
                && colorSpace == ColorSpace::RGB) {
                _selectedImageId = selectedImageId;
            }

            float zoom = _zoom;
            if (ImGui::SliderFloat("zoom", &zoom, 0.1, 5) && colorSpace == ColorSpace::RGB) {
                _zoom = zoom;
            }
        }

        DemoImage& image = _images[_selectedImageId];

        ImGuiTexture tex;
        switch (colorSpace) {
        case ColorSpace::RGB:
            tex = engine->getOrLoadImGuiTexture(engine->_imguiCtx, image.path_rgb);
            break;
        case ColorSpace::OCV:
            tex = engine->getOrLoadImGuiTexture(engine->_imguiCtx, image.path_ocv);
            break;
        default:
            break;
        }

        ImVec2 size = {(float)tex.width * _zoom, (float)tex.height * _zoom};
        if (_imageFitWindow) {
            ImVec2 availableSize = ImGui::GetContentRegionAvail();
            size = calculateFitSize(tex, availableSize);
        }
        ImGui::Image(tex.id, size);
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
