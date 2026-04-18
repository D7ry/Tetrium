#pragma once

#include "App.h"
#include "app_components/PR650.h"
#include "imgui.h"

namespace TetriumApp
{

class AppAutoMeasure : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;

    // Called when the app is opened i.e. becomes the primary app
    virtual void OnOpen() override{};
    // Called when the app is closed i.e. no longer the primary app
    virtual void OnClose() override{};

    // TickImGui() and TickVulkan() are only called if the app is active
    // i.e. we expect the app to render to the screen
    // TickOffScreen() is always called

    // ImGui Tick() function,
    // the function executes in imgui context
    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

    // Vulkan Tick() function,
    // record all render & compute commands within this pass,
    // push all semaphores that need to be waited on to r_waitSemaphores
    virtual void TickVulkan(TetriumApp::TickContextVulkan& ctx) override {}

    // Off-screen Tick() function,
    // function runs regardless of the window being visible
    virtual void TickOffScreen(TetriumApp::TickContextOffScreen& ctx) override {}

  private:
    void drawColorBlock(const TetriumApp::TickContextImGui& ctx, glm::ivec4 rgbo);
    void drawLandoltCStimulus(
        const TetriumApp::TickContextImGui& ctx,
        glm::ivec4 rgbo,
        float menuHeight
    );
    void drawMeasurementBoxes(ImVec2 center, float innerRadius, float outerRadius);
    bool loadPrimariesFromFile(const std::string& filename);
    void scanMeasurementFiles();

    // Daily validation methods
    void runDailyValidation(bool debugSkipPR650Measurements);
    bool measureDisplayPrimaries(const std::string& primariesDir);
    bool copyPrimariesToValidationFolder(
        const std::string& primariesSourceDir,
        const std::string& validationPrimariesDir
    );
    bool copyConfigToValidationFolder(
        const std::string& configSourcePath,
        const std::string& validationConfigPath
    );
    bool convertRYGBToRGBO(const std::string& date, const std::string& validationFolder);
    bool measureValidationTargets(const std::string& date, const std::string& validationFolder);
    bool runValidation(const std::string& date, const std::string& validationFolder);
    std::vector<glm::ivec4> parseRGBOTargets(const std::string& csvPath);
    void saveSpectrumData(
        glm::ivec4 rgbo,
        const std::string& outputDir,
        const PR650::SpectrumMeasure& result
    );
    void measureAndSaveSpectrum(glm::ivec4 rgbo, const std::string& outputDir);

    float stimulusSize = 1.0f; // Default stimulus size (1.0 = full size)

    struct
    {
        bool connecting = false;
        bool measuring = false;
        bool validating = false;
        bool validationComplete = false;
    } pr650States;

    struct
    {
        std::vector<std::string> availableFiles;
        std::vector<glm::ivec4> currentPrimaries;
        std::string selectedFile = "";
        int selectedFileIndex = 0;
        int currPrimaryIndex = 0;
    } measurementData;

    enum ValidationConfigMode
    {
        kValidationConfigCenter = 0,
        kValidationConfigFullGrid = 1,
    };

    struct
    {
        std::string currentStep = "";
        int stepNumber = 0;
        int totalSteps = 4;
        std::string statusMessage = "";
        glm::ivec4 currentRGBO = glm::ivec4(0, 0, 0, 0);
        bool displayValidationColor = false;
        bool debugSkipPR650Measurements = false;
        ValidationConfigMode configMode = kValidationConfigFullGrid;
    } validationState;
};
}; // namespace TetriumApp
