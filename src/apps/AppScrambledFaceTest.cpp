#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <cmath>
#include <ctime>
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
                 | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoNavInputs
                 | ImGuiWindowFlags_NoNavFocus;
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
        ImGui::Text("Trial Settings");
        ImGui::SliderInt("Repetitions per axis", &settings.repetitionsPerAxis, 1, 10);
        ImGui::Text("(Genotypes and axes determined by color picker)");
        ImGui::SliderInt("Trials per break", &settings.trialsPerBreak, 10, 120);
        ImGui::Text("(0 = no breaks)");
        ImGui::Separator();

        ImGui::Text("Timing Settings");
        ImGui::SliderFloat("Viewing Duration (s)", &settings.viewingDuration, 0.5f, 10.0f, "%.1f");
        ImGui::SliderFloat(
            "Response Duration (s)", &settings.responseDuration, 1.0f, 10.0f, "%.1f"
        );
        ImGui::SliderFloat("Inter-Trial Interval (s)", &settings.itiDuration, 0.0f, 5.0f, "%.1f");
        ImGui::Separator();

        ImGui::Text("Stimulus Settings");
        ImGui::SliderFloat("Luminance", &settings.luminance, 0.1f, 2.0f);
        ImGui::SliderFloat("Saturation", &settings.saturation, 0.0f, 1.0f);
        ImGui::SliderFloat("Scramble prob", &settings.scrambleProb, 0.0f, 1.0f);

        // Normal face mode selector
        ImGui::Text("Normal Face Mode");
        const char* faceModeName
            = (settings.normalFaceMode == NormalFaceMode::kSame) ? "SAME" : "DIFF";
        if (ImGui::BeginCombo("##NormalFaceMode", faceModeName)) {
            if (ImGui::Selectable("SAME", settings.normalFaceMode == NormalFaceMode::kSame)) {
                settings.normalFaceMode = NormalFaceMode::kSame;
            }
            if (ImGui::Selectable("DIFF", settings.normalFaceMode == NormalFaceMode::kDiff)) {
                settings.normalFaceMode = NormalFaceMode::kDiff;
            }
            ImGui::EndCombo();
        }
        ImGui::Text("(SAME: two copies of one face, DIFF: two different faces)");
        ImGui::Separator();

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

    // Create CircleGridGenerator with genetic parameters
    std::vector<int> dimensions = {2};
    generator = new TetriumColor::CircleGridGenerator(
        settings.scrambleProb,
        "female", // sex
        0.999f,   // percentage_screened
        547.0f,   // peak_to_test
        settings.luminance,
        settings.saturation,
        dimensions,
        42,    // seed
        "led", // cst_display_type
        std::string(TETRIUM_COLOR_PATH) + "measurements/2025-10-12/primaries"
    );

    // Get genotypes from generator
    std::vector<std::string> genotypes = generator->GetGenotypes();

    // Build all trials: genotype × metameric_axis × repetition
    trials.clear();
    for (const std::string& genotype : genotypes) {
        for (int axis = 0; axis < 4; axis++) {
            for (int rep = 0; rep < settings.repetitionsPerAxis; ++rep) {
                Trial trial;
                trial.genotype = genotype;
                trial.metamericAxis = axis;
                trial.repetitionIdx = rep;
                trials.push_back(trial);
            }
        }
    }

    int totalTrials = trials.size();

    // Shuffle trials to randomize order
    for (int i = totalTrials - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        std::swap(trials[i], trials[j]);
    }

    // Initialize logger
    std::vector<std::string> headers
        = {"subject_id",
           "session_timestamp",
           "trial_idx",
           "genotype",
           "metameric_axis",
           "repetition_idx",
           "scrambled_original_idx",
           "user_choice",
           "correct",
           "response_time",
           "timeout",
           "luminance",
           "saturation",
           "scramble_prob",
           "normal_face_mode"};
    logger = new TestDataLogger("AppScrambledFaceTest", subjectName, headers);

    currentTrial = 0;
    numCorrect = 0;
    generateTrial(
        trials[currentTrial],
        ctx,
        currentTrial,
        trials[currentTrial].genotype,
        trials[currentTrial].metamericAxis
    );

    // Start with fixation cross (ITI state) before showing first trial
    // This prevents the flash of stimuli appearing immediately
    trialState = TrialState::kITI;
    trialStateTimer = 0.0f;
    isInitialFixation = true; // Mark this as the initial fixation period
}

void AppScrambledFaceTest::generateTrial(
    Trial& t,
    const TetriumApp::TickContextImGui& ctx,
    int trialIdx,
    const std::string& genotype,
    int metamericAxis
)
{
    t.choices.clear();
    t.choices.resize(settings.imagesPerTrial);

    // GetImages returns 3 images - generate names for all 3
    std::vector<std::string> names;
    names.reserve(3);
    for (int i = 0; i < 3; ++i) {
        names.emplace_back(
            "./temp/" + subjectName + "_trial" + std::to_string(trialIdx) + "_genotype" + genotype
            + "_axis" + std::to_string(metamericAxis) + "_" + std::to_string(i)
        );
    }

    auto idxs = generator->GetImages(
        genotype, metamericAxis, names, TetriumColor::ColorSpaceType::DISP_6P
    );
    (void)idxs;

    if (settings.normalFaceMode == NormalFaceMode::kSame) {
        // SAME mode: Randomly pick one of the first two images as the "normal" face
        int pickedIdx = rand() % 2;

        // Load the picked image for positions 0 and 1 (two copies of the normal face)
        std::string normalRgbPath = names[pickedIdx] + "_RGB.png";
        std::string normalOcvPath = names[pickedIdx] + "_OCV.png";

        for (int i = 0; i < 2; ++i) {
            t.choices[i].handleRGB = ctx.apis.LoadTexture(normalRgbPath);
            t.choices[i].handleOCV = ctx.apis.LoadTexture(normalOcvPath);
            t.choices[i].texRGB = ctx.apis.InitImGuiTexture(t.choices[i].handleRGB);
            t.choices[i].texOCV = ctx.apis.InitImGuiTexture(t.choices[i].handleOCV);
        }
    } else {
        // DIFF mode: Load both of the first two images as different normal faces
        for (int i = 0; i < 2; ++i) {
            std::string normalRgbPath = names[i] + "_RGB.png";
            std::string normalOcvPath = names[i] + "_OCV.png";
            t.choices[i].handleRGB = ctx.apis.LoadTexture(normalRgbPath);
            t.choices[i].handleOCV = ctx.apis.LoadTexture(normalOcvPath);
            t.choices[i].texRGB = ctx.apis.InitImGuiTexture(t.choices[i].handleRGB);
            t.choices[i].texOCV = ctx.apis.InitImGuiTexture(t.choices[i].handleOCV);
        }
    }

    // Load the third image (index 2) as the scrambled/odd one out
    std::string scrambledRgbPath = names[2] + "_RGB.png";
    std::string scrambledOcvPath = names[2] + "_OCV.png";
    t.choices[2].handleRGB = ctx.apis.LoadTexture(scrambledRgbPath);
    t.choices[2].handleOCV = ctx.apis.LoadTexture(scrambledOcvPath);
    t.choices[2].texRGB = ctx.apis.InitImGuiTexture(t.choices[2].handleRGB);
    t.choices[2].texOCV = ctx.apis.InitImGuiTexture(t.choices[2].handleOCV);

    // Mark that the scrambled image is at original index 2
    t.scrambledOriginalIndex = 2;

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

    // Update timer
    ImGuiIO& io = ImGui::GetIO();
    trialStateTimer += io.DeltaTime;

    // Draw progress counter in upper left corner
    int totalTrials = trials.size();
    // Add 1 to currentTrial for display since it's 0-indexed
    std::string progressText = std::to_string(currentTrial + 1) + "/" + std::to_string(totalTrials);
    ImGui::SetCursorPos(ImVec2(20, 20));
    ImGui::Text("%s", progressText.c_str());

    // State machine for trial phases
    switch (trialState) {
    case TrialState::kViewing:
        // Show stimuli for viewing duration
        if (trialStateTimer >= settings.viewingDuration) {
            trialState = TrialState::kResponse;
            trialStateTimer = 0.0f;
        }
        drawStimuli(t, ctx, avail, center);
        break;

    case TrialState::kResponse: {
        // Show blank screen with fixation cross, accept responses
        drawFixationCross(center);

        // Check for timeout
        if (trialStateTimer >= settings.responseDuration) {
            // Timeout - no response, mark as incorrect
            handleTrialResponse(t, ctx, -1);
            return;
        }

        // Gamepad button mapping: Y (top), B (bottom-right), X (bottom-left)
        ImGuiKey gamepadKeys[3] = {
            ImGuiKey_GamepadFaceUp,    // Y button -> top stimulus (index 0)
            ImGuiKey_GamepadFaceRight, // B button -> bottom-right stimulus (index 1)
            ImGuiKey_GamepadFaceLeft   // X button -> bottom-left stimulus (index 2)
        };

        // Check for gamepad input
        for (int i = 0; i < 3; i++) {
            if (ImGui::IsKeyPressed(gamepadKeys[i])) {
                handleTrialResponse(t, ctx, i);
                return;
            }
        }
        break;
    }

    case TrialState::kITI: {
        // Inter-trial interval: show fixation cross, no input
        drawFixationCross(center);

        // Check if ITI is complete
        if (trialStateTimer >= settings.itiDuration) {
            // For the initial fixation before first trial, just transition to viewing
            // The trial was already generated in startTest()
            if (isInitialFixation) {
                isInitialFixation = false;
                trialState = TrialState::kViewing;
                trialStateTimer = 0.0f;
            } else {
                // Between trials: check if we need a break or move to next trial
                int totalTrials = trials.size();
                if (currentTrial + 1 >= totalTrials) {
                    state = TestState::kResult;
                } else {
                    currentTrial++;

                    // Check if it's time for a break (not on the first trial, not on last trial)
                    bool needsBreak = settings.trialsPerBreak > 0
                                      && (currentTrial % settings.trialsPerBreak) == 0
                                      && currentTrial < totalTrials - 1;

                    if (needsBreak) {
                        // Generate trial but show break screen first
                        generateTrial(
                            trials[currentTrial],
                            ctx,
                            currentTrial,
                            trials[currentTrial].genotype,
                            trials[currentTrial].metamericAxis
                        );
                        trialState = TrialState::kBreak;
                        trialStateTimer = 0.0f;
                    } else {
                        // Normal progression: generate trial and start viewing
                        generateTrial(
                            trials[currentTrial],
                            ctx,
                            currentTrial,
                            trials[currentTrial].genotype,
                            trials[currentTrial].metamericAxis
                        );
                        trialState = TrialState::kViewing;
                        trialStateTimer = 0.0f;
                    }
                }
            }
        }
        break;
    }

    case TrialState::kBreak: {
        // Show break screen with continue button
        drawBreakScreen(ctx);
        break;
    }
    }
}

void AppScrambledFaceTest::handleTrialResponse(
    Trial& t,
    const TetriumApp::TickContextImGui& ctx,
    int choice
)
{
    t.userChoice = choice;
    bool correct = (choice >= 0) && (t.displayToOriginal[t.userChoice] == t.scrambledOriginalIndex);
    if (correct)
        ++numCorrect;

    // Play sound feedback (only if user actually responded, not timeout)
    if (choice >= 0) {
        if (correct) {
            ctx.apis.PlaySound(Sound::kCorrectAnswer);
        } else {
            ctx.apis.PlaySound(Sound::kWrongAnswer);
        }
    }

    // Log trial data
    if (logger) {
        std::map<std::string, std::string> data;
        data["subject_id"] = subjectName;
        data["trial_idx"] = std::to_string(currentTrial);
        data["genotype"] = t.genotype;
        data["metameric_axis"] = std::to_string(t.metamericAxis);
        data["repetition_idx"] = std::to_string(t.repetitionIdx);
        data["scrambled_original_idx"] = std::to_string(t.scrambledOriginalIndex);
        data["user_choice"] = std::to_string(t.userChoice);
        data["correct"] = correct ? "1" : "0";
        data["response_time"] = std::to_string(trialStateTimer);
        data["timeout"] = (choice < 0) ? "1" : "0";
        data["luminance"] = std::to_string(settings.luminance);
        data["saturation"] = std::to_string(settings.saturation);
        data["scramble_prob"] = std::to_string(settings.scrambleProb);
        data["normal_face_mode"]
            = (settings.normalFaceMode == NormalFaceMode::kSame) ? "SAME" : "DIFF";
        logger->LogRow(data);
    }

    // Transition to inter-trial interval
    trialState = TrialState::kITI;
    trialStateTimer = 0.0f;
}

void AppScrambledFaceTest::drawFixationCross(ImVec2 center)
{
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
}

void AppScrambledFaceTest::drawBreakScreen(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 boxSize(600, 400);
    ImVec2 pos((screenSize.x - boxSize.x) * 0.5f, (screenSize.y - boxSize.y) * 0.5f);

    ImGui::SetCursorPos(pos);
    ImGui::BeginChild("BreakBox", boxSize, true);

    int totalTrials = trials.size();
    int trialsRemaining = totalTrials - currentTrial;
    int trialsCompleted = currentTrial;

    // Center the text vertically
    float lineSpacing = ImGui::GetTextLineHeightWithSpacing();
    float yStart = (boxSize.y - (lineSpacing * 6.0f)) * 0.5f;

    // Title
    ImGui::SetWindowFontScale(1.5f);
    const char* titleText = "Take a Break";
    ImVec2 titleSize = ImGui::CalcTextSize(titleText);
    ImGui::SetCursorPos(ImVec2((boxSize.x - titleSize.x * 1.5f) * 0.5f, yStart));
    ImGui::Text("%s", titleText);
    ImGui::SetWindowFontScale(1.0f);

    // Progress info
    ImGui::SetCursorPos(ImVec2(50, yStart + lineSpacing * 3.0f));
    ImGui::Text("Trials completed: %d / %d", trialsCompleted, totalTrials);

    ImGui::SetCursorPos(ImVec2(50, yStart + lineSpacing * 4.5f));
    ImGui::Text("Trials remaining: %d", trialsRemaining);

    // Continue button
    ImVec2 buttonSize(200, 60);
    ImVec2 buttonPos((boxSize.x - buttonSize.x) * 0.5f, yStart + lineSpacing * 7.0f);
    ImGui::SetCursorPos(buttonPos);
    bool continueClicked = ImGui::Button("Continue", buttonSize);

    // Gamepad: A button to continue
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)) {
        continueClicked = true;
    }

    if (continueClicked) {
        trialState = TrialState::kViewing;
        trialStateTimer = 0.0f;
    }

    ImGui::EndChild();
}

void AppScrambledFaceTest::drawStimuli(
    Trial& t,
    const TetriumApp::TickContextImGui& ctx,
    ImVec2 avail,
    ImVec2 center
)
{
    // Draw fixation cross in center
    drawFixationCross(center);

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

        // Just display the image, no interaction during viewing phase
        ImGui::Image((void*)(intptr_t)shown.id, fitted);
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
    int totalTrials = trials.size();
    ImGui::Text("Subject: %s", subjectName.c_str());
    ImGui::Text("Trials: %d", totalTrials);
    ImGui::Text("Correct: %d", numCorrect);
    ImGui::Text(
        "Accuracy: %.2f%%", totalTrials ? (100.f * (float)numCorrect / (float)totalTrials) : 0.f
    );
    bool backClicked = ImGui::Button("Back to Menu", ImVec2(200, 60));

    // Gamepad: A or Back button to return to menu
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown)
        || ImGui::IsKeyPressed(ImGuiKey_GamepadBack)) {
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
