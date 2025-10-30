#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <cmath>
#include <filesystem>

#include "AppScrambledFaceTest.h"
#include "Pathing.h"

namespace TetriumApp
{

void AppScrambledFaceTest::Init(TetriumApp::InitContext& ctx) { (void)ctx; }

void AppScrambledFaceTest::Cleanup(TetriumApp::CleanupContext& ctx)
{
    for (auto& t : trials) {
        for (auto& c : t.choices) {
            if (c.handleRGB)
                ctx.api.UnloadTexture(c.handleRGB);
            if (c.handleOCV)
                ctx.api.UnloadTexture(c.handleOCV);
        }
    }
    trials.clear();
    if (generator) {
        delete generator;
        generator = nullptr;
    }
    if (logger) {
        delete logger;
        logger = nullptr;
    }
}

void AppScrambledFaceTest::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(0);
    if (ImGui::Begin("Scrambled Face Test", NULL, flags)) {
        switch (state) {
        case TestState::kSettings:
            drawSettings(ctx);
        case TestState::kIdle:
            drawIdle(ctx);
            break;
        case TestState::kRunning:
            drawRunning(ctx);
            break;
        case TestState::kResult:
            drawResult(ctx);
            break;
        }
    }
    ImGui::End();
}

void AppScrambledFaceTest::drawIdle(const TetriumApp::TickContextImGui& ctx)
{
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 buttonSize(220, 80);
    ImVec2 pos((screenSize.x - buttonSize.x) * 0.5f, (screenSize.y - buttonSize.y) * 0.5f - 100);
    ImGui::SetCursorPos(pos);
    ImGui::Text("Subject ID:");
    ImGui::SetCursorPos(pos + ImVec2(0, 40));
    ImGui::SetNextItemWidth(buttonSize.x);
    ImGui::InputText("##Subject", &subjectName);

    bool disabled = subjectName.empty();
    if (disabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1));
    }
    ImGui::SetCursorPos(pos + ImVec2(0, 100));
    bool playClicked = ImGui::Button("Play", buttonSize) && !disabled;
    if (disabled)
        ImGui::PopStyleColor(3);
    
    // Gamepad: A button to play
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown) && !disabled) {
        playClicked = true;
    }
    
    if (playClicked) {
        startTest(ctx);
        state = TestState::kRunning;
    }

    ImGui::SetCursorPos(pos + ImVec2(0, 200));
    bool settingsClicked = ImGui::Button("Settings", buttonSize);
    
    // Gamepad: Y button for settings
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp)) {
        settingsClicked = true;
    }
    
    if (settingsClicked)
        state = TestState::kSettings;
}

void AppScrambledFaceTest::drawSettings(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SliderInt("Metameric Axes", &settings.numMetamericAxes, 1, 4);
        ImGui::SliderInt("Repetitions per axis", &settings.repetitionsPerAxis, 1, 10);
        ImGui::Text("Total trials: %d", settings.numMetamericAxes * settings.repetitionsPerAxis);
        ImGui::Separator();
        ImGui::SliderFloat("Luminance", &settings.luminance, 0.1f, 2.0f);
        ImGui::SliderFloat("Saturation", &settings.saturation, 0.0f, 1.0f);
        ImGui::SliderFloat("Scramble prob", &settings.scrambleProb, 0.0f, 1.0f);
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            state = TestState::kIdle;
        }
        ImGui::EndPopup();
    } else {
        ImGui::OpenPopup("Settings");
    }
}

void AppScrambledFaceTest::startTest(const TetriumApp::TickContextImGui& ctx)
{
    if (generator) {
        delete generator;
        generator = nullptr;
    }
    if (logger) {
        delete logger;
        logger = nullptr;
    }

    std::filesystem::create_directories("./temp");

    int totalTrials = settings.numMetamericAxes * settings.repetitionsPerAxis;
    generator = new TetriumColor::CircleGridGenerator(
        std::string(TETRIUM_COLOR_PATH) + "measurements/2025-10-12/primaries",
        totalTrials,
        settings.scrambleProb
    );

    // Initialize logger
    std::vector<std::string> headers
        = {"subject_id",
           "session_timestamp",
           "trial_idx",
           "metameric_axis",
           "scrambled_original_idx",
           "user_choice",
           "correct",
           "luminance",
           "saturation",
           "scramble_prob"};
    logger = new TestDataLogger("AppScrambledFaceTest", subjectName, headers);

    // Generate all trial combinations (metameric_axis × repetitions)
    trials.clear();
    trials.resize(totalTrials);
    int trialIdx = 0;
    for (int axis = 0; axis < settings.numMetamericAxes; ++axis) {
        for (int rep = 0; rep < settings.repetitionsPerAxis; ++rep) {
            trials[trialIdx].metamericAxis = axis;
            trialIdx++;
        }
    }

    // Shuffle trials to randomize order
    for (int i = totalTrials - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        std::swap(trials[i], trials[j]);
    }

    currentTrial = 0;
    numCorrect = 0;
    generateTrial(trials[currentTrial], ctx, currentTrial, trials[currentTrial].metamericAxis);
}

void AppScrambledFaceTest::generateTrial(
    Trial& t,
    const TetriumApp::TickContextImGui& ctx,
    int trialIdx,
    int metamericAxis
)
{
    t.choices.clear();
    t.choices.resize(settings.imagesPerTrial);

    std::vector<std::string> names;
    names.reserve(settings.imagesPerTrial);
    for (int i = 0; i < settings.imagesPerTrial; ++i) {
        names.emplace_back(
            "./temp/" + subjectName + "_trial" + std::to_string(trialIdx) + "_axis"
            + std::to_string(metamericAxis) + "_" + std::to_string(i)
        );
    }
    auto idxs = generator->GetImages(
        metamericAxis,
        settings.luminance,
        settings.saturation,
        names,
        TetriumColor::ColorSpaceType::DISP_6P
    );
    (void)idxs;

    // Load both RGB and OCV variants saved by Python: _RGB.png and _OCV.png
    for (int i = 0; i < settings.imagesPerTrial; ++i) {
        std::string rgbPath = names[i] + "_RGB.png";
        std::string ocvPath = names[i] + "_OCV.png";
        t.choices[i].handleRGB = ctx.apis.LoadTexture(rgbPath);
        t.choices[i].handleOCV = ctx.apis.LoadTexture(ocvPath);
        t.choices[i].texRGB = ctx.apis.InitImGuiTexture(t.choices[i].handleRGB);
        t.choices[i].texOCV = ctx.apis.InitImGuiTexture(t.choices[i].handleOCV);
    }

    // Build display shuffle mapping 0..N-1 and randomize
    t.displayToOriginal.resize(settings.imagesPerTrial);
    for (int i = 0; i < settings.imagesPerTrial; ++i)
        t.displayToOriginal[i] = i;
    // simple Fisher-Yates
    for (int i = settings.imagesPerTrial - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        std::swap(t.displayToOriginal[i], t.displayToOriginal[j]);
    }
}

void AppScrambledFaceTest::drawRunning(const TetriumApp::TickContextImGui& ctx)
{
    // Check for gamepad back button to return to menu
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadBack)) {
        state = TestState::kResult;
        return;
    }
    
    Trial& t = trials[currentTrial];
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 center(avail.x * 0.5f, avail.y * 0.5f);
 
    // Gamepad button mapping: Y (top), B (bottom-right), X (bottom-left)
    ImGuiKey gamepadKeys[3] = {
        ImGuiKey_GamepadFaceUp,    // Y button -> top stimulus (index 0)
        ImGuiKey_GamepadFaceRight, // B button -> bottom-right stimulus (index 1)
        ImGuiKey_GamepadFaceLeft   // X button -> bottom-left stimulus (index 2)
    };
    
    // Check for gamepad input first
    for (int i = 0; i < 3; i++) {
        if (ImGui::IsKeyPressed(gamepadKeys[i])) {
            t.userChoice = i;
            bool correct = (t.displayToOriginal[t.userChoice] == t.scrambledOriginalIndex);
            if (correct)
                ++numCorrect;

            // Log trial data
            if (logger) {
                std::map<std::string, std::string> data;
                data["subject_id"] = subjectName;
                data["trial_idx"] = std::to_string(currentTrial);
                data["metameric_axis"] = std::to_string(t.metamericAxis);
                data["scrambled_original_idx"] = std::to_string(t.scrambledOriginalIndex);
                data["user_choice"] = std::to_string(t.userChoice);
                data["correct"] = correct ? "1" : "0";
                data["luminance"] = std::to_string(settings.luminance);
                data["saturation"] = std::to_string(settings.saturation);
                data["scramble_prob"] = std::to_string(settings.scrambleProb);
                logger->LogRow(data);
            }

            int totalTrials = settings.numMetamericAxes * settings.repetitionsPerAxis;
            if (currentTrial + 1 >= totalTrials) {
                state = TestState::kResult;
            } else {
                currentTrial++;
                generateTrial(
                    trials[currentTrial], ctx, currentTrial, trials[currentTrial].metamericAxis
                );
            }
            return;
        }
    }
 
    // Draw fixation cross in center
    float crossSize = 20.0f;
    float crossThickness = 3.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 crossCenter(windowPos.x + center.x, windowPos.y + center.y);
    ImU32 crossColor = IM_COL32(255, 255, 255, 255);
    drawList->AddLine(
        ImVec2(crossCenter.x - crossSize, crossCenter.y),
        ImVec2(crossCenter.x + crossSize, crossCenter.y),
        crossColor,
        crossThickness
    );
    drawList->AddLine(
        ImVec2(crossCenter.x, crossCenter.y - crossSize),
        ImVec2(crossCenter.x, crossCenter.y + crossSize),
        crossColor,
        crossThickness
    );

    // Arrange 3 stimuli in equidistant triangle around center
    float radius = avail.y * 0.30f;            // distance from center to each stimulus
    float angles[3] = {-90.0f, 30.0f, 150.0f}; // degrees, top, bottom-right, bottom-left

    for (int i = 0; i < 3; ++i) {
        float angleRad = angles[i] * 3.14159265f / 180.0f;
        float stimX = center.x + radius * cosf(angleRad);
        float stimY = center.y + radius * sinf(angleRad);

        // Choose which variant to show based on current color space
        ImGuiTexture shown = (ctx.colorSpace == ColorSpace::OCV)
                                 ? t.choices[t.displayToOriginal[i]].texOCV
                                 : t.choices[t.displayToOriginal[i]].texRGB;

        // Compute fitted size preserving aspect ratio
        float maxSize = avail.y * 0.25f;
        float scaleW = maxSize / (float)shown.width;
        float scaleH = maxSize / (float)shown.height;
        float scale = std::min(scaleW, scaleH);
        ImVec2 fitted((float)shown.width * scale, (float)shown.height * scale);

        // Position image centered at calculated position
        ImVec2 imagePos(stimX - fitted.x * 0.5f, stimY - fitted.y * 0.5f);
        ImGui::SetCursorPos(imagePos);

        // Borderless button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.10f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        std::string btnId
            = std::string("##img_") + std::to_string(currentTrial) + "_" + std::to_string(i);
        bool clicked = ImGui::ImageButton(btnId.c_str(), (void*)(intptr_t)shown.id, fitted);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        if (clicked) {
            t.userChoice = i;
            // correctness: original scrambled is last in original (index 2)
            bool correct = (t.displayToOriginal[t.userChoice] == t.scrambledOriginalIndex);
            if (correct)
                ++numCorrect;

            // Log trial data
            if (logger) {
                std::map<std::string, std::string> data;
                data["subject_id"] = subjectName;
                data["trial_idx"] = std::to_string(currentTrial);
                data["metameric_axis"] = std::to_string(t.metamericAxis);
                data["scrambled_original_idx"] = std::to_string(t.scrambledOriginalIndex);
                data["user_choice"] = std::to_string(t.userChoice);
                data["correct"] = correct ? "1" : "0";
                data["luminance"] = std::to_string(settings.luminance);
                data["saturation"] = std::to_string(settings.saturation);
                data["scramble_prob"] = std::to_string(settings.scrambleProb);
                logger->LogRow(data);
            }

            int totalTrials = settings.numMetamericAxes * settings.repetitionsPerAxis;
            if (currentTrial + 1 >= totalTrials) {
                state = TestState::kResult;
            } else {
                currentTrial++;
                generateTrial(
                    trials[currentTrial], ctx, currentTrial, trials[currentTrial].metamericAxis
                );
            }
            return;
        }
    }
}

void AppScrambledFaceTest::drawResult(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    ImVec2 boxSize(800, 400);
    ImVec2 win = ImGui::GetWindowSize();
    ImVec2 pos((win.x - boxSize.x) * 0.5f, (win.y - boxSize.y) * 0.5f);
    ImGui::SetCursorPos(pos);
    ImGui::BeginChild("ResultBox", boxSize, true);
    int totalTrials = settings.numMetamericAxes * settings.repetitionsPerAxis;
    ImGui::Text("Subject: %s", subjectName.c_str());
    ImGui::Text("Trials: %d", totalTrials);
    ImGui::Text("Correct: %d", numCorrect);
    ImGui::Text(
        "Accuracy: %.2f%%", totalTrials ? (100.f * (float)numCorrect / (float)totalTrials) : 0.f
    );
    bool backClicked = ImGui::Button("Back to Menu", ImVec2(200, 60));
    
    // Gamepad: A or Back button to return to menu
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadBack)) {
        backClicked = true;
    }
    
    if (backClicked) {
        state = TestState::kIdle;
        for (auto& t : trials) {
            for (auto& c : t.choices) {
                if (c.handleRGB)
                    ctx.apis.UnloadTexture(c.handleRGB);
                if (c.handleOCV)
                    ctx.apis.UnloadTexture(c.handleOCV);
            }
        }
        trials.clear();
    }
    ImGui::EndChild();
}

} // namespace TetriumApp
