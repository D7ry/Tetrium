#include <filesystem>

#include "Tetrium.h"

#include "ImGuiWidgetTetraViewer.h"
#include "imgui.h"

const char* ImGuiWidgetTetraViewer::TETRA_IMAGE_FOLDER_PATH = "../assets/textures/tetra_images/";

namespace
{
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
} // namespace

ImGuiWidgetTetraViewer::ImGuiWidgetTetraViewer() { refreshTetraImagePicker(); }

void ImGuiWidgetTetraViewer::drawTetraImage(
    Tetrium* engine,
    ColorSpace colorSpace,
    const TetraImageFile& image
)
{
    std::string imagePath = TETRA_IMAGE_FOLDER_PATH + image.paths[colorSpace];
    ImGuiTexture tex = engine->getOrLoadImGuiTexture(engine->_imguiCtx, imagePath);

    ImVec2 size = {(float)tex.width * _zoom, (float)tex.height * _zoom};

    if (_adaptiveImageSize) {
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        size = calculateFitSize(tex, availableSize);
    }
    ImGui::Image(tex.id, size);
}

void ImGuiWidgetTetraViewer::Draw(Tetrium* engine, ColorSpace colorSpace)
{
    pollControls();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGuiWindowFlags flags = 0;
    if (_fullScreen) {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize;
    }

    if (ImGui::Begin("Tetra Viewer", NULL, flags)) {
        if (!_noUI) {
            drawControlPrompts();
            drawTetraImagePicker(colorSpace);
        }

        if (_currTetraImage != -1) {
            drawTetraImage(engine, colorSpace, _tetraImages.at(_currTetraImage));
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

// reload all tetra images through IO
void ImGuiWidgetTetraViewer::refreshTetraImagePicker()
{
    _currTetraImage = -1;
    _tetraImages.clear();

    std::unordered_set<std::string> images;
    // iterate over all files, each pairs of files that are in xxx_RGB.png and xxx_OCV.png format
    // make up one tetra image
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(TETRA_IMAGE_FOLDER_PATH)) {
        if (entry.is_directory() || entry.is_symlink()) {
            continue;
        }
        std::string fileName = entry.path().filename().string();
        bool isRGB = fileName.ends_with("_RGB.png");
        bool isOCV = fileName.ends_with("_OCV.png");
        if (!isRGB && !isOCV) {
            continue;
        }
        // look for counterpart, if found, we have found a tetra image pair
        std::string tetraImageName = fileName.substr(
            0,
            fileName.size() - (sizeof("_RGB.png") / sizeof(char)) + 1
        ); // pure name of tetra image
        std::string counterPart = tetraImageName;
        if (isRGB) {
            counterPart += "_OCV.png";
        } else {
            counterPart += "_RGB.png";
        }
        auto it = images.find(counterPart);
        if (it != images.end()) { // found both part of RGB/OCV image pair -- create tetra image
            std::string rgbFileName = fileName;
            std::string ocvFileName = counterPart;
            if (!isRGB) { // fileName is actually ocv
                rgbFileName.swap(ocvFileName);
            }
            TetraImageFile image{.name = tetraImageName, .paths = {rgbFileName, ocvFileName}};
            _tetraImages.emplace_back(image);
        } else {
            images.insert(fileName);
        }
    }
}

void ImGuiWidgetTetraViewer::drawTetraImagePicker(ColorSpace colorSpace)
{
    if (_shouldRefreshFilePicker) {
        refreshTetraImagePicker();
        _shouldRefreshFilePicker = false;
    }
    if (ImGui::Button("Load Images")) {
        _shouldRefreshFilePicker = true;
    }

    ImGui::SameLine();

    const char* currentTetraImageName
        = _currTetraImage != -1 ? _tetraImages[_currTetraImage].name.c_str() : "Select Tetra Image";

    if (ImGui::BeginCombo("Tetra Images", currentTetraImageName)) {
        for (int i = 0; i < _tetraImages.size(); i++) {
            TetraImageFile& image = _tetraImages.at(i);
            bool imageSelected = _currTetraImage == i;
            if (ImGui::Selectable(image.name.c_str(), imageSelected) && colorSpace == RGB) {
                _currTetraImage = i;
            }
        }

        ImGui::EndCombo();
    }
}

void ImGuiWidgetTetraViewer::drawControlPrompts()
{
    ImGui::Text("J/K : zoom in/out. H/L: cycle images. F: toggle full screen mode. B: toggle UI. "
                "A: Toggle Adaptive Image Sizes");
}

void ImGuiWidgetTetraViewer::pollControls()
{

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
    if (ImGui::IsKeyPressed(ImGuiKey_A)) {
        _adaptiveImageSize = !_adaptiveImageSize;
    }

    int numImages = _tetraImages.size();
    if (numImages != 0) {
        if (ImGui::IsKeyPressed(ImGuiKey_L)) {
            _currTetraImage++;
            _currTetraImage = _currTetraImage % numImages;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_H)) {
            _currTetraImage--;
            if (_currTetraImage == -1) { // wrap
                _currTetraImage = numImages - 1;
            }
        }
    }
}
