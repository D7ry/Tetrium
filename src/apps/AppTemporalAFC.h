#pragma once

#include "App.h"
#include "TetriumColor/SolidColorGenerator.h"
#include "lib/TestDataLogger.h"

#include <chrono>
#include <optional>

namespace TetriumApp
{

class AppTemporalAFC : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;
    virtual void TickVulkan(TetriumApp::TickContextVulkan& ctx) override;
    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
    enum class TestState
    {
        kIdle,
        kRunning,
        kBreak,
        kResult
    };

    enum class TrialState
    {
        kBlank,
        kStimulus1,
        kISI1,
        kStimulus2,
        kISI2,
        kStimulus3,
        kResponse
    };

    enum class OddType
    {
        ORANGE,   // Odd stimulus is orange
        R_PLUS_G, // Odd stimulus is R+G
        RANDOMIZE // Randomize per trial
    };

    struct Settings
    {
        int numSamplesPerPoint = 3; // Samples per grid point
        float orangeLevel = 255.0f; // 0-255 (base level)
        float redLevel = 127.0f;    // 0-255 (base level)
        float greenLevel = 65.0f;   // 0-255 (base level)
        int minLuminance = 5;       // Minimum luminance level (0-100)
        int maxLuminance = 95;      // Maximum luminance level (0-100)
        int numLuminanceLevels = 10;
        int stimulusDurationMs = 150;
        int isiDurationMs = 350;
        float circleRadius = 0.375f; // screen proportion
        bool hasNoisyBoundary = false;
        bool useRGOForOCV = true; // If true, show RGO image in OCV channel
        OddType oddType = OddType::R_PLUS_G;
    } settings;

    struct StimulusTextures
    {
        uint32_t handleRGB = 0;
        uint32_t handleOCV = 0;
        ImGuiTexture texRGB{};
        ImGuiTexture texOCV{};
    };

    struct Trial
    {
        int rgRatio;         // R/(R+G) ratio: 40-80
        int luminance;       // Luminance level (0-100)
        int oddPosition;     // 0, 1, or 2
        OddType oddType;     // ORANGE or R_PLUS_G
        int userChoice = -1; // User's answer (0, 1, or 2)
        float reactionTimeMs = 0.0f;

        // Textures are generated on-demand, not pre-rendered
    };

    TestState state = TestState::kIdle;
    TrialState trialState = TrialState::kBlank;
    std::string subjectName = "guest";
    int currentTrialIdx = 0;
    int numCorrect = 0;
    std::vector<Trial> trials;
    float stateTimeRemaining = 0.0f;
    std::chrono::steady_clock::time_point responseStartTime;

    // Current trial textures (only for active trial)
    StimulusTextures currentStimuli[3];

    TestDataLogger* logger = nullptr;
    TetriumColor::SolidColorGenerator* colorGenerator = nullptr;

    // UI methods
    void drawIdle(const TetriumApp::TickContextImGui& ctx);
    void drawSettings(const TetriumApp::TickContextImGui& ctx);
    void drawRunning(const TetriumApp::TickContextImGui& ctx);
    void drawBreak(const TetriumApp::TickContextImGui& ctx);
    void drawResult(const TetriumApp::TickContextImGui& ctx);

    // Game logic
    void startTest(const TetriumApp::TickContextImGui& ctx);
    void generateAllTrials(const TetriumApp::TickContextImGui& ctx);
    void loadCurrentTrialTextures(const TetriumApp::TickContextImGui& ctx);
    void unloadCurrentTrialTextures(const TetriumApp::TickContextImGui& ctx);
    void transitionTrialState(const TetriumApp::TickContextImGui& ctx);
    void handleResponse(int choice, const TetriumApp::TickContextImGui& ctx);
    void logTrialData(const Trial& trial);

    // Helper to compute RGBO values for a stimulus
    std::tuple<float, float, float, float> computeRGBO(
        int rgRatio,
        int luminance,
        OddType oddType,
        bool isOdd
    );
};

} // namespace TetriumApp
