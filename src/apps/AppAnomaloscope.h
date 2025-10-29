#pragma once

#include "App.h"
#include "TetriumColor/SolidColorGenerator.h"

#include <optional>

namespace TetriumApp
{

class AppAnomaloscope : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;
    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
    enum class TestState
    {
        kIdle,
        kRunning
    };

    struct Settings
    {
        // R+G side parameters (top half)
        float rgRatio = 50.0f;       // R/(R+G) ratio as percentage (0-100)
        float rgTotalLevel = 255.0f; // Total luminance of R+G mixture (0-255)
        float redLevel = 74.0f;      // Red primary level (0-255)
        float greenLevel = 30.0f;    // Green primary level (0-255)

        // Orange side parameters (bottom half)
        float orangeLevel = 130.0f; // Orange primary level (0-255)

        // Display parameters
        float splitRatio = 0.5f;     // Vertical split position (0-1, 0.5=center)
        float circleRadius = 0.375f; // Circle size as screen proportion
        bool hasNoisyBoundary = false;
        bool useRGOForOCV = true; // If true, show RGO image in OCV channel

        // Joystick adjustment speeds
        float rgRatioSpeed = 10.0f;      // Units per second for R/G ratio
        float rgTotalLevelSpeed = 50.0f; // Units per second for R+G total level
        float redLevelSpeed = 50.0f;     // Units per second for red level
        float greenLevelSpeed = 50.0f;   // Units per second for green level
        float orangeLevelSpeed = 50.0f;  // Units per second for orange level
    } settings;

    struct BipartiteStimulus
    {
        // Top half (R+G mixture)
        uint32_t topHandleRGB = 0;
        uint32_t topHandleOCV = 0;
        ImGuiTexture topTexRGB{};
        ImGuiTexture topTexOCV{};

        // Bottom half (Orange)
        uint32_t bottomHandleRGB = 0;
        uint32_t bottomHandleOCV = 0;
        ImGuiTexture bottomTexRGB{};
        ImGuiTexture bottomTexOCV{};
    };

    TestState state = TestState::kIdle;
    BipartiteStimulus stimulus;
    bool stimulusNeedsUpdate = true;

    TetriumColor::SolidColorGenerator* colorGenerator = nullptr;

    // UI methods
    void drawIdle(const TetriumApp::TickContextImGui& ctx);
    void drawRunning(const TetriumApp::TickContextImGui& ctx);

    // Stimulus generation
    void updateStimulus(const TetriumApp::TickContextImGui& ctx);

    // Helper to compute RGBO values
    std::tuple<float, float, float, float> computeRGSide();
    std::tuple<float, float, float, float> computeOrangeSide();
};

} // namespace TetriumApp
