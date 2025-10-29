#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
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
    if (ImGui::Button("Play", buttonSize) && !disabled) {
        startTest(ctx);
        state = TestState::kRunning;
    }
    if (disabled)
        ImGui::PopStyleColor(3);

    ImGui::SetCursorPos(pos + ImVec2(0, 200));
    if (ImGui::Button("Settings", buttonSize))
        state = TestState::kSettings;
}

void AppScrambledFaceTest::drawSettings(const TetriumApp::TickContextImGui& ctx)
{
    (void)ctx;
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SliderInt("Trials", &settings.numTrials, 1, 100);
        ImGui::SliderInt("Images per trial", &settings.imagesPerTrial, 3, 6);
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
    std::filesystem::create_directories("./temp");
    generator = new TetriumColor::CircleGridGenerator(
        std::string(TETRIUM_COLOR_PATH) + "measurements/2025-10-12/primaries",
        settings.numTrials,
        settings.scrambleProb
    );
    trials.clear();
    trials.resize(settings.numTrials);
    currentTrial = 0;
    numCorrect = 0;
    generateTrial(trials[currentTrial], ctx, currentTrial);
}

void AppScrambledFaceTest::generateTrial(
    Trial& t,
    const TetriumApp::TickContextImGui& ctx,
    int trialIdx
)
{
    t.choices.clear();
    t.choices.resize(settings.imagesPerTrial);

    std::vector<std::string> names;
    names.reserve(settings.imagesPerTrial);
    for (int i = 0; i < settings.imagesPerTrial; ++i) {
        names.emplace_back(
            "./temp/" + subjectName + "_trial" + std::to_string(trialIdx) + "_" + std::to_string(i)
        );
    }
    auto idxs = generator->GetImages(
        0, settings.luminance, settings.saturation, names, TetriumColor::ColorSpaceType::DISP_6P
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
    Trial& t = trials[currentTrial];
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float spacing = 30.0f;
    float widthPer = (avail.x - spacing * (settings.imagesPerTrial - 1)) / settings.imagesPerTrial;
    float maxHeight = avail.y * 0.6f;
    float totalRowWidth
        = (widthPer * settings.imagesPerTrial) + spacing * (settings.imagesPerTrial - 1);
    ImVec2 start = ImVec2((avail.x - totalRowWidth) * 0.5f, (avail.y - maxHeight) * 0.5f);
    // const char* buttonLabels[3] = {"A", "B", "C"};
    for (int i = 0; i < settings.imagesPerTrial; ++i) {
        ImVec2 pos = ImVec2(start.x + i * (widthPer + spacing), start.y);
        // Choose which variant to show based on current color space
        ImGuiTexture shown = (ctx.colorSpace == ColorSpace::OCV)
                                 ? t.choices[t.displayToOriginal[i]].texOCV
                                 : t.choices[t.displayToOriginal[i]].texRGB;
        // Compute fitted size preserving aspect ratio
        float scaleW = widthPer / (float)shown.width;
        float scaleH = maxHeight / (float)shown.height;
        float scale = std::min(scaleW, scaleH);
        ImVec2 fitted((float)shown.width * scale, (float)shown.height * scale);
        // Center in cell
        ImVec2 imagePos
            = ImVec2(pos.x + (widthPer - fitted.x) * 0.5f, pos.y + (maxHeight - fitted.y) * 0.5f);
        ImGui::SetCursorPos(imagePos);
        // Borderless button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.06f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.10f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        // bool clicked = ImGui::ImageButton(buttonLabels[i], (void*)(intptr_t)shown.id, fitted);
        std::string btnId
            = std::string("##img_") + std::to_string(currentTrial) + "_" + std::to_string(i);
        bool clicked = ImGui::ImageButton(btnId.c_str(), (void*)(intptr_t)shown.id, fitted);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        if (clicked) {
            t.userChoice = i;
            // correctness: original scrambled is last in original (index 2)
            if (t.displayToOriginal[t.userChoice] == t.scrambledOriginalIndex)
                ++numCorrect;
            // for (auto& c : t.choices) {
            //     if (c.handleRGB)
            //         ctx.apis.UnloadTexture(c.handleRGB);
            //     if (c.handleOCV)
            //         ctx.apis.UnloadTexture(c.handleOCV);
            // }
            if (currentTrial + 1 >= settings.numTrials) {
                state = TestState::kResult;
            } else {
                currentTrial++;
                generateTrial(trials[currentTrial], ctx, currentTrial);
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
    ImGui::Text("Subject: %s", subjectName.c_str());
    ImGui::Text("Trials: %d", settings.numTrials);
    ImGui::Text("Correct: %d", numCorrect);
    ImGui::Text(
        "Accuracy: %.2f%%",
        settings.numTrials ? (100.f * (float)numCorrect / (float)settings.numTrials) : 0.f
    );
    if (ImGui::Button("Back to Menu", ImVec2(200, 60))) {
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
