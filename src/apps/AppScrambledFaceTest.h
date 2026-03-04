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
        kViewing,  // Show stimuli
        kResponse, // Blank screen, wait for response
        kITI,      // Inter-trial interval, fixation cross after response
        kBreak     // Break screen with continue button
    };

    enum class NormalFaceMode
    {
        kSame, // Two copies of the same normal face
        kDiff  // Two different normal faces
    };

    struct Settings
    {
        int repetitionsPerAxis = 5;
        int imagesPerTrial = 3;  // layout width (fixed at 3 for triangle)
        int trialsPerBreak = 60; // number of trials before offering a break
        float luminance = 1.0f;
        float saturation = 0.4f;
        float scrambleProb = 0.5f;
        float viewingDuration = 1.0f;  // seconds to view stimuli
        float responseDuration = 3.0f; // seconds to respond
        float itiDuration = 1.0f;      // inter-trial interval (seconds)
        NormalFaceMode normalFaceMode
            = NormalFaceMode::kSame; // SAME vs DIFF for non-scrambled faces
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
        std::string genotype;  // genotype string for this trial
        int metamericAxis = 0; // which metameric axis this trial uses
        int repetitionIdx = 0; // repetition index for this genotype/axis combination
    };

    TestState state = TestState::kIdle;
    TrialState trialState = TrialState::kViewing;
    std::string subjectName = "guest";
    int currentTrial = 0;
    int numCorrect = 0;
    std::vector<Trial> trials;
    float trialStateTimer = 0.0f;
    bool isInitialFixation = false; // True during initial fixation before first trial

    void startTest(const TetriumApp::TickContextImGui& ctx);
    void generateTrial(
        Trial& t,
        const TetriumApp::TickContextImGui& ctx,
        int trialIdx,
        const std::string& genotype,
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
    void drawBreakScreen(const TetriumApp::TickContextImGui& ctx);
    void handleTrialResponse(Trial& t, const TetriumApp::TickContextImGui& ctx, int choice);

    TetriumColor::TestGenerator* testGenerator = nullptr;
    TestDataLogger* logger = nullptr;
};
} // namespace TetriumApp
