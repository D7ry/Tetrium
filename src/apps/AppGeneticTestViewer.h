#pragma once

#include "App.h"
#include "Pathing.h"
#include "imgui.h"

namespace TetriumApp
{
// Simple viewer for genetic color picker test grids
class AppGeneticTestViewer : public App
{
  public:
    enum class GeneratorType
    {
        Plate,
        Bipartite
    };

    virtual void Init(TetriumApp::InitContext& ctx) override;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;
    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
    struct GridImage
    {
        std::string name;
        uint32_t textureHandles[ColorSpaceSize];
        ImGuiTexture textures[ColorSpaceSize];
        bool loaded = false;
    };

    void drawControls();
    void drawImage(const TickContextImGui& ctx, ColorSpace colorSpace);
    void scanPrimariesDirectories();
    void generateGrid();
    void loadGridImage(const TickContextImGui& ctx);
    void unloadGridImage(const TickContextImGui& ctx);

    // UI state
    std::vector<std::string> _primariesDirs;
    int _selectedDirIndex = -1;
    int _testingDim = 3;
    GeneratorType _generatorType = GeneratorType::Plate;
    bool _generating = false;
    bool _adaptiveImageSize = true;
    float _zoom = 0.5f;

    // Drag state
    ImVec2 _imageOffset = {0.0f, 0.0f};
    bool _isDragging = false;
    ImVec2 _dragStartPos = {0.0f, 0.0f};

    // Image state
    GridImage _gridImage;

    // Paths
    inline static const std::string MEASUREMENTS_BASE_PATH
        = std::string(TETRIUM_COLOR_PATH + "measurements/");
    inline static const std::string OUTPUT_PATH
        = std::string(TETRIUM_COLOR_PATH + "measurements/genetic_test_output/");
};
} // namespace TetriumApp
