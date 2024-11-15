#include "ImGuiWidgetPsychophysicsScreeningTest.h"

void ImGuiWidgetPhychophysicsScreeningTest::Draw(Tetrium* engine, ColorSpace colorSpace)
{

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(1);
    if (ImGui::Begin("PsydoIsochromatic Test", NULL, flags)) {
        switch (_state) {
        case TestState::kIdle:
            drawIdle(engine, colorSpace);
            break;
        case TestState::kScreening:
            drawTestForSubject(engine, colorSpace, _subject);
            break;
        }
    }
    ImGui::End();
}

void ImGuiWidgetPhychophysicsScreeningTest::drawIdle(Tetrium* engine, ColorSpace colorSpace)
{
    // Set the button size
    ImVec2 buttonSize(200, 100);

    // Center the button on the screen
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;
    ImVec2 buttonPos((screenSize.x - buttonSize.x) * 0.5f, (screenSize.y - buttonSize.y) * 0.5f);

    // Set the cursor position for the button
    ImGui::SetCursorPos(buttonPos);

    // Draw the button
    if (ImGui::Button("Yuan Test", buttonSize)) {
        newGame();
    }
}

void ImGuiWidgetPhychophysicsScreeningTest::newGame()
{
    _subject = SubjectContext{
        .state = SubjectState::kPreparing, .currentAttempt = 0, .numSuccessAttempts = 0
    };
    _state = TestState::kScreening;
}

void ImGuiWidgetPhychophysicsScreeningTest::drawTestForSubject(
    Tetrium* engine,
    ColorSpace colorSpace,
    SubjectContext& subject
)
{
    switch (subject.state) {
    case SubjectState::kPreparing:
        drawFixGazePage();
        break;
    case SubjectState::kAnswering:
        break;
    case SubjectState::kIdentifying:

        break;
    }
}

void ImGuiWidgetPhychophysicsScreeningTest::drawFixGazePage()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 screenCenter = ImGui::GetIO().DisplaySize;
    screenCenter.x *= 0.5f;
    screenCenter.y *= 0.5f;

    float crossHairSize = 70.f;
    float crossHairThickness = 10.f;
    ImU32 crossHairColor = IM_COL32(255, 255, 255, 255); // White color

    drawList->AddLine(
        ImVec2(screenCenter.x - crossHairSize, screenCenter.y),
        ImVec2(screenCenter.x + crossHairSize, screenCenter.y),
        crossHairColor,
        crossHairThickness
    );
    drawList->AddLine(
        ImVec2(screenCenter.x, screenCenter.y - crossHairSize),
        ImVec2(screenCenter.x, screenCenter.y + crossHairSize),
        crossHairColor,
        crossHairThickness
    );

    // Add text below crosshair
    ImVec2 textSize = ImGui::CalcTextSize("Fix Gaze Onto Crosshair");
    float spacing = 20;
    ImVec2 textPos(
        screenCenter.x - textSize.x * 0.5f, screenCenter.y + textSize.y + spacing + crossHairSize
    ); // 20 pixels below crosshair
    ImGui::SetCursorPos(textPos);
    ImGui::Text("Fix Gaze Onto Crosshair");
}
