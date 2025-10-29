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
        kSettings,
        kRunning,
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
        int numSamplesPerRatio = 10;
        float orangeLevel = 133.0f;    // 0-255
        float redLevel = 147.0 / 2.0f; // 0-255
        float greenLevel = 30.0f;      // 0-255
        int stimulusDurationMs = 150;
        int isiDurationMs = 350;
        float circleRadius = 0.375f; // screen proportion
        bool hasNoisyBoundary = false;
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
        int oddPosition;     // 0, 1, or 2
        OddType oddType;     // ORANGE or R_PLUS_G
        int userChoice = -1; // User's answer (0, 1, or 2)
        float reactionTimeMs = 0.0f;

        // Pre-rendered stimuli (3 stimuli per trial)
        StimulusTextures stimuli[3];
    };

    TestState state = TestState::kIdle;
    TrialState trialState = TrialState::kBlank;
    std::string subjectName = "guest";
    int currentTrialIdx = 0;
    int numCorrect = 0;
    std::vector<Trial> trials;
    float stateTimeRemaining = 0.0f;
    std::chrono::steady_clock::time_point responseStartTime;

    TestDataLogger* logger = nullptr;
    TetriumColor::SolidColorGenerator* colorGenerator = nullptr;

    // UI methods
    void drawIdle(const TetriumApp::TickContextImGui& ctx);
    void drawSettings(const TetriumApp::TickContextImGui& ctx);
    void drawRunning(const TetriumApp::TickContextImGui& ctx);
    void drawResult(const TetriumApp::TickContextImGui& ctx);

    // Game logic
    void startTest(const TetriumApp::TickContextImGui& ctx);
    void generateAllTrials(const TetriumApp::TickContextImGui& ctx);
    void generateStimulusTextures(Trial& trial, const TetriumApp::TickContextImGui& ctx);
    void transitionTrialState(const TetriumApp::TickContextImGui& ctx);
    void handleResponse(int choice, const TetriumApp::TickContextImGui& ctx);
    void logTrialData(const Trial& trial);

    // Helper to compute RGBO values for a stimulus
    std::tuple<float, float, float, float> computeRGBO(int rgRatio, OddType oddType, bool isOdd);
};

} // namespace TetriumApp
