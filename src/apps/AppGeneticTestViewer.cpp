#include "AppGeneticTestViewer.h"
#include "imgui.h"
#include <cstdlib>
#include <filesystem>

namespace
{
ImVec2 calculateFitSize(const ImGuiTexture& texture, const ImVec2& availableSize)
{
    float scaleWidth = availableSize.x / (float)texture.width;
    float scaleHeight = availableSize.y / (float)texture.height;
    float scale = std::min(scaleWidth, scaleHeight);

    ImVec2 fitSize;
    fitSize.x = (float)texture.width * scale;
    fitSize.y = (float)texture.height * scale;
    return fitSize;
}
} // namespace

namespace TetriumApp
{

void AppGeneticTestViewer::Init(TetriumApp::InitContext& ctx) { scanPrimariesDirectories(); }

void AppGeneticTestViewer::Cleanup(TetriumApp::CleanupContext& ctx)
{
    if (_gridImage.loaded) {
        ctx.api.UnloadTexture(_gridImage.textureHandles[ColorSpace::RGB]);
        ctx.api.UnloadTexture(_gridImage.textureHandles[ColorSpace::OCV]);
        _gridImage.loaded = false;
    }
}

void AppGeneticTestViewer::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    ColorSpace colorSpace = ctx.colorSpace;

    // Check if we need to load the generated image
    if (_generating == false && !_gridImage.loaded && _selectedDirIndex >= 0) {
        // Try to load if the files exist
        std::string outputPath = getOutputDirectoryForSelection();
        std::string rgbPath = outputPath + "genetic_test_grid_RGB.png";
        if (std::filesystem::exists(rgbPath)) {
            loadGridImage(ctx);
        }
    }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar
                             | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    if (ImGui::Begin("Genetic Test Viewer", NULL, flags)) {
        drawControls();

        ImGui::Separator();

        if (_gridImage.loaded) {
            drawImage(ctx, colorSpace);
        } else {
            ImGui::Text("Generate a test grid to view it here");
        }
    }
    ImGui::End();
}

void AppGeneticTestViewer::drawControls()
{
    ImGui::Text("Genetic Color Picker Test Generator");

    // Primaries directory selector
    if (ImGui::Button("Refresh Directories")) {
        scanPrimariesDirectories();
    }

    ImGui::SameLine();

    const char* currentDir = (_selectedDirIndex >= 0 && _selectedDirIndex < _primariesDirs.size())
                                 ? _primariesDirs[_selectedDirIndex].c_str()
                                 : "Select Primaries Directory";

    if (ImGui::BeginCombo("Primaries", currentDir)) {
        for (int i = 0; i < _primariesDirs.size(); i++) {
            bool isSelected = _selectedDirIndex == i;
            if (ImGui::Selectable(_primariesDirs[i].c_str(), isSelected)) {
                _selectedDirIndex = i;
            }
        }
        ImGui::EndCombo();
    }

    // Generator type selector
    const char* currentGeneratorType;
    switch (_generatorType) {
    case GeneratorType::TetraPicker: currentGeneratorType = "TetraColorPicker Gaussian Blob"; break;
    case GeneratorType::Plate: currentGeneratorType = "Pseudoisochromatic Plate"; break;
    case GeneratorType::Bipartite: currentGeneratorType = "Bipartite Circle"; break;
    case GeneratorType::GaussianBlob: currentGeneratorType = "Gaussian Blob"; break;
    }

    if (ImGui::BeginCombo("Generator Type", currentGeneratorType)) {
        if (ImGui::Selectable(
                "TetraColorPicker Gaussian Blob", _generatorType == GeneratorType::TetraPicker
            )) {
            _generatorType = GeneratorType::TetraPicker;
        }
        if (ImGui::Selectable(
                "Pseudoisochromatic Plate", _generatorType == GeneratorType::Plate
            )) {
            _generatorType = GeneratorType::Plate;
        }
        if (ImGui::Selectable("Bipartite Circle", _generatorType == GeneratorType::Bipartite)) {
            _generatorType = GeneratorType::Bipartite;
        }
        if (ImGui::Selectable("Gaussian Blob", _generatorType == GeneratorType::GaussianBlob)) {
            _generatorType = GeneratorType::GaussianBlob;
        }

        ImGui::EndCombo();
    }

    if (_generatorType == GeneratorType::GaussianBlob) {
        ImGui::Checkbox("Cone Contrast", &_gaussianBlobConeContrast);
    }

    // Testing dimension
    ImGui::InputInt("Testing Dimension", &_testingDim);
    if (_testingDim < 1)
        _testingDim = 1;
    if (_testingDim > 4)
        _testingDim = 4;

    // Generate button
    if (_generating) {
        ImGui::Text("Generating... (check console)");
    } else {
        if (ImGui::Button("Generate Test Grid")) {
            if (_selectedDirIndex >= 0) {
                generateGrid();
            } else {
                INFO("Please select a primaries directory first");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Existing Grid")) {
            // Will be picked up in next tick
            if (_gridImage.loaded) {
                _gridImage.loaded = false; // Force reload
            }
        }
    }

    // Zoom controls
    ImGui::Checkbox("Adaptive Size", &_adaptiveImageSize);
    if (!_adaptiveImageSize) {
        ImGui::SliderFloat("Zoom", &_zoom, 0.1f, 5.0f);
        ImGui::SameLine();
        if (ImGui::Button("Reset View")) {
            _imageOffset = {0.0f, 0.0f};
            _zoom = 0.5f;
        }
        ImGui::Text("Controls: Drag to pan | +/- to zoom | R to reset");
    }
}

void AppGeneticTestViewer::drawImage(const TickContextImGui& ctx, ColorSpace colorSpace)
{
    if (!_gridImage.loaded)
        return;

    ImGuiTexture tex = _gridImage.textures[colorSpace];
    ImVec2 size = {(float)tex.width * _zoom, (float)tex.height * _zoom};

    if (_adaptiveImageSize) {
        ImVec2 availableSize = ImGui::GetContentRegionAvail();
        size = calculateFitSize(tex, availableSize);
        _imageOffset = {0.0f, 0.0f}; // Reset offset when in adaptive mode
    }

    // Keyboard controls for zoom (when not in adaptive mode)
    if (!_adaptiveImageSize) {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
            _zoom = std::min(_zoom + 0.1f, 5.0f);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
            _zoom = std::max(_zoom - 0.1f, 0.1f);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            _imageOffset = {0.0f, 0.0f};
            _zoom = 0.5f;
        }
    }

    // Get cursor position before drawing
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Apply offset to cursor position
    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + _imageOffset.x, cursorPos.y + _imageOffset.y));

    // Draw the image
    ImGui::Image(tex.id, size);

    // Handle dragging
    if (!_adaptiveImageSize) {
        // Check if mouse is hovering over the image area
        ImVec2 imageMin = ImVec2(cursorPos.x + _imageOffset.x, cursorPos.y + _imageOffset.y);
        ImVec2 imageMax = ImVec2(imageMin.x + size.x, imageMin.y + size.y);

        if (ImGui::IsMouseHoveringRect(imageMin, imageMax) || _isDragging) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                _isDragging = true;
                _dragStartPos = ImGui::GetMousePos();
            }
        }

        if (_isDragging) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                ImVec2 currentMousePos = ImGui::GetMousePos();
                ImVec2 delta = ImVec2(
                    currentMousePos.x - _dragStartPos.x, currentMousePos.y - _dragStartPos.y
                );
                _imageOffset.x += delta.x;
                _imageOffset.y += delta.y;
                _dragStartPos = currentMousePos;
            } else {
                _isDragging = false;
            }
        }
    }
}

void AppGeneticTestViewer::scanPrimariesDirectories()
{
    _primariesDirs.clear();

    try {
        // Iterate through all date directories in measurements
        for (const auto& dateEntry : std::filesystem::directory_iterator(MEASUREMENTS_BASE_PATH)) {
            if (!dateEntry.is_directory())
                continue;

            // Check if it has a primaries subdirectory
            std::string primariesPath = dateEntry.path().string() + "/primaries";
            if (std::filesystem::exists(primariesPath)
                && std::filesystem::is_directory(primariesPath)) {
                // Check if there are CSV files in the primaries directory
                bool hasCsvFiles = false;
                for (const auto& file : std::filesystem::directory_iterator(primariesPath)) {
                    if (file.path().extension() == ".csv") {
                        hasCsvFiles = true;
                        break;
                    }
                }

                if (hasCsvFiles) {
                    // Add just the date folder name for display
                    _primariesDirs.push_back(dateEntry.path().filename().string());
                }
            }
        }

        // Sort in reverse chronological order (newest first)
        std::sort(_primariesDirs.rbegin(), _primariesDirs.rend());

        INFO("Found {} primaries directories", _primariesDirs.size());
    } catch (const std::filesystem::filesystem_error& e) {
        ERROR("Error scanning primaries directories: {}", e.what());
    }
}

std::string AppGeneticTestViewer::getOutputDirectoryForSelection() const
{
    if (_selectedDirIndex < 0 || _selectedDirIndex >= _primariesDirs.size()) {
        return "";
    }

    const std::string dateDir = _primariesDirs[_selectedDirIndex];
    if (_generatorType == GeneratorType::TetraPicker) {
        return OUTPUT_PATH + dateDir + "/tetra_picker/";
    }
    if (_generatorType == GeneratorType::GaussianBlob && _gaussianBlobConeContrast) {
        return OUTPUT_PATH + dateDir + "/gaussian_blob_cone_contrast/";
    }

    switch (_generatorType) {
    case GeneratorType::TetraPicker: return OUTPUT_PATH + dateDir + "/tetra_picker/";
    case GeneratorType::Plate: return OUTPUT_PATH + dateDir + "/plate/";
    case GeneratorType::Bipartite: return OUTPUT_PATH + dateDir + "/bipartite/";
    case GeneratorType::GaussianBlob: return OUTPUT_PATH + dateDir + "/gaussian_blob/";
    }

    return OUTPUT_PATH + dateDir + "/";
}

void AppGeneticTestViewer::generateGrid()
{
    if (_selectedDirIndex < 0 || _selectedDirIndex >= _primariesDirs.size()) {
        ERROR("Invalid directory selection");
        return;
    }

    _generating = true;

    std::string dateDir = _primariesDirs[_selectedDirIndex];
    std::string primariesPath = MEASUREMENTS_BASE_PATH + dateDir + "/primaries";
    std::string outputPath = getOutputDirectoryForSelection();

    INFO("Generating test grid from: {}", primariesPath);
    INFO("Output path: {}", outputPath);

    // Build Python command
    std::string pythonScript = "../generate_genetic_test.py";
    std::string generatorTypeStr;
    switch (_generatorType) {
    case GeneratorType::TetraPicker: generatorTypeStr = "tetra_picker"; break;
    case GeneratorType::Plate: generatorTypeStr = "plate"; break;
    case GeneratorType::Bipartite: generatorTypeStr = "bipartite"; break;
    case GeneratorType::GaussianBlob: generatorTypeStr = "gaussian_blob"; break;
    }
    std::stringstream cmd;
    cmd << "conda run -n tetrium python " << pythonScript << " \"" << primariesPath << "\" \""
        << outputPath << "\" " << _testingDim << " " << generatorTypeStr;
    if (_generatorType == GeneratorType::GaussianBlob) {
        cmd << " " << (_gaussianBlobConeContrast ? "cone_contrast" : "raw");
    }

    INFO("Running: {}", cmd.str());

    // Run Python script
    int result = std::system(cmd.str().c_str());

    _generating = false;

    if (result == 0) {
        INFO("Grid generation complete");

        // Load the generated images
        _gridImage.name = dateDir + "_dim" + std::to_string(_testingDim);

        // Store context for loading textures
        // Note: We need to load textures in the next frame, not here
        // For simplicity, we'll just set a flag and load on next tick
        // But for this barebones version, we'll try to load immediately
        // This is a hack - proper implementation would defer to next frame

    } else {
        ERROR("Grid generation failed with code: {}", result);
    }
}

void AppGeneticTestViewer::loadGridImage(const TickContextImGui& ctx)
{
    if (_selectedDirIndex < 0)
        return;

    std::string dateDir = _primariesDirs[_selectedDirIndex];
    std::string outputPath = getOutputDirectoryForSelection();
    std::string rgbPath = outputPath + "genetic_test_grid_RGB.png";
    std::string ocvPath = outputPath + "genetic_test_grid_OCV.png";

    // Check if files exist
    if (!std::filesystem::exists(rgbPath) || !std::filesystem::exists(ocvPath)) {
        std::string fallbackPath = OUTPUT_PATH + dateDir + "/";
        rgbPath = fallbackPath + "genetic_test_grid_RGB.png";
        ocvPath = fallbackPath + "genetic_test_grid_OCV.png";
        outputPath = fallbackPath;
    }

    if (!std::filesystem::exists(rgbPath) || !std::filesystem::exists(ocvPath)) {
        INFO("Grid images not found, please generate first");
        return;
    }

    // Unload previous if any
    unloadGridImage(ctx);

    // Load new images
    _gridImage.textureHandles[ColorSpace::RGB] = ctx.apis.LoadTexture(rgbPath);
    _gridImage.textureHandles[ColorSpace::OCV] = ctx.apis.LoadTexture(ocvPath);
    _gridImage.textures[ColorSpace::RGB]
        = ctx.apis.InitImGuiTexture(_gridImage.textureHandles[ColorSpace::RGB]);
    _gridImage.textures[ColorSpace::OCV]
        = ctx.apis.InitImGuiTexture(_gridImage.textureHandles[ColorSpace::OCV]);
    _gridImage.name = dateDir;
    _gridImage.loaded = true;

    INFO("Loaded grid images for: {}", dateDir);
}

void AppGeneticTestViewer::unloadGridImage(const TickContextImGui& ctx)
{
    if (_gridImage.loaded) {
        ctx.apis.UnloadTexture(_gridImage.textureHandles[ColorSpace::RGB]);
        ctx.apis.UnloadTexture(_gridImage.textureHandles[ColorSpace::OCV]);
        _gridImage.loaded = false;
    }
}

} // namespace TetriumApp
