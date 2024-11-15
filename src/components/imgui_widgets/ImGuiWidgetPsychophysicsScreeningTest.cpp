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
    if (ImGui::Button("Start", buttonSize)) {
        // Button click handling code goes here
        // For example:
        // startGame();
    }
}

void ImGuiWidgetPhychophysicsScreeningTest::drawTestForSubject(
    Tetrium* engine,
    ColorSpace colorSpace,
    SubjectContext& subject
)
{
}
