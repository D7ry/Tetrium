#include "ImGuiWidgetTetraViewer.h"
#include "imgui.h"

const char* ImGuiWidgetTetraViewer::TETRA_IMAGE_FOLDER_PATH = "../assets/textures/tetra_images/";

void ImGuiWidgetTetraViewer::Draw(Tetrium* engine, ColorSpace colorSpace)
{
    drawTetraImagePicker(colorSpace);
}

// reload all tetra images through IO
void ImGuiWidgetTetraViewer::refreshTetraImagePicker() {
    _currTetraImage = -1;
    _tetraImages.clear();
    // iterate over all files, each pairs of files that are in xxx_RGB and xxx_OCV format make up one tetra image


}

void ImGuiWidgetTetraViewer::drawTetraImagePicker(ColorSpace colorSpace)
{
    if (_shouldRefreshFilePicker) {
        refreshTetraImagePicker();
        _shouldRefreshFilePicker = false;
    }
    if (ImGui::Button("Refresh") && colorSpace == RGB) {
        _shouldRefreshFilePicker = true;
    }

    const char* currentTetraImageName
        = _currTetraImage != -1 ? _tetraImages[_currTetraImage].name.c_str() : "Select Tetra Image";

    if (ImGui::BeginCombo("Tetra Images", currentTetraImageName)) {
        for (int i = 0; i < _tetraImages.size(); i++) {
            TetraImageFile& image = _tetraImages.at(i);
            bool imageSelected = _currTetraImage == i;
            if (ImGui::Selectable(image.name.c_str(), imageSelected) && colorSpace == RGB) {
                _currTetraImage = i;
                INFO("Setting current tetra image to {}", _currTetraImage);
            }
        }

        ImGui::EndCombo();
    }
}
