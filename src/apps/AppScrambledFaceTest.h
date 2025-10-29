#pragma once

#include "App.h"
#include "TetriumColor/TetriumColor.h"
#include "lib/TestDataLogger.h"

namespace TetriumApp
{
class AppScrambledFaceTest : public App
{
  public:
    virtual void Init(TetriumApp::InitContext& ctx) override;
    virtual void Cleanup(TetriumApp::CleanupContext& ctx) override;
    virtual void TickImGui(const TetriumApp::TickContextImGui& ctx) override;

  private:
    enum class TestState
    {
        kIdle,
        kSettings,
        kRunning,
        kResult
    };

    struct Settings
    {
        int numTrials = 5;
        int imagesPerTrial = 3; // layout width
        float luminance = 1.0f;
        float saturation = 0.4f;
        float scrambleProb = 0.5f;
    } settings;

    struct Trial
    {
        // For each choice, we load both RGB and OCV variants
        struct ChoiceTextures
        {
            uint32_t handleRGB = 0;
            uint32_t handleOCV = 0;
            ImGuiTexture texRGB{};
            ImGuiTexture texOCV{};
        };

        std::vector<ChoiceTextures> choices; // size imagesPerTrial

        // Shuffled mapping from displayed index -> original index
        // (0:unscramble1,1:unscramble2,2:scramble)
        std::vector<int> displayToOriginal;
        int scrambledOriginalIndex = 2; // last in original list
        int userChoice = -1;
    };

    TestState state = TestState::kIdle;
    std::string subjectName = "guest";
    int currentTrial = 0;
    int numCorrect = 0;
    std::vector<Trial> trials;

    void startTest(const TetriumApp::TickContextImGui& ctx);
    void generateTrial(Trial& t, const TetriumApp::TickContextImGui& ctx, int trialIdx);
    void drawIdle(const TetriumApp::TickContextImGui& ctx);
    void drawSettings(const TetriumApp::TickContextImGui& ctx);
    void drawRunning(const TetriumApp::TickContextImGui& ctx);
    void drawResult(const TetriumApp::TickContextImGui& ctx);

    TetriumColor::CircleGridGenerator* generator = nullptr;
    TestDataLogger* logger = nullptr;
};
} // namespace TetriumApp
