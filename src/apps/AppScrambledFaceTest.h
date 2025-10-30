#pragma once

#include "App.h"
#include "TetriumColor/TetriumColor.h"
#include "lib/TestDataLogger.h"

struct ImVec2;

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

    enum class TrialState
    {
        kViewing, // Show stimuli
        kResponse // Blank screen, wait for response
    };

    struct Settings
    {
        int numMetamericAxes = 4; // 0-3 for tetrachromats
        int repetitionsPerAxis = 3;
        int imagesPerTrial = 3; // layout width (fixed at 3 for triangle)
        float luminance = 1.0f;
        float saturation = 0.4f;
        float scrambleProb = 0.5f;
        float viewingDuration = 2.0f;  // seconds to view stimuli
        float responseDuration = 3.0f; // seconds to respond
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
        int metamericAxis = 0; // which metameric axis this trial uses
    };

    TestState state = TestState::kIdle;
    TrialState trialState = TrialState::kViewing;
    std::string subjectName = "guest";
    int currentTrial = 0;
    int numCorrect = 0;
    std::vector<Trial> trials;
    float trialStateTimer = 0.0f;

    void startTest(const TetriumApp::TickContextImGui& ctx);
    void generateTrial(
        Trial& t,
        const TetriumApp::TickContextImGui& ctx,
        int trialIdx,
        int metamericAxis
    );
    void drawIdle(const TetriumApp::TickContextImGui& ctx);
    void drawSettings(const TetriumApp::TickContextImGui& ctx);
    void drawRunning(const TetriumApp::TickContextImGui& ctx);
    void drawResult(const TetriumApp::TickContextImGui& ctx);
    void drawStimuli(
        Trial& t,
        const TetriumApp::TickContextImGui& ctx,
        ImVec2 avail,
        ImVec2 center
    );
    void drawFixationCross(ImVec2 center);
    void handleTrialResponse(Trial& t, const TetriumApp::TickContextImGui& ctx, int choice);

    TetriumColor::CircleGridGenerator* generator = nullptr;
    TestDataLogger* logger = nullptr;
};
} // namespace TetriumApp
