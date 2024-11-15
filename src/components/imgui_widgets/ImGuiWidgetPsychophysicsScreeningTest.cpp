#include "ImGuiWidgetPsychophysicsScreeningTest.h"
#include "Tetrium.h"

namespace
{
ImVec2 calculateFitSize(float width, float height, const ImVec2& availableSize)
{
    float aspectRatio = (float)width / (float)height;

    float scaleWidth = availableSize.x / (float)width;
    float scaleHeight = availableSize.y / (float)height;

    float scale = std::min(scaleWidth, scaleHeight);

    ImVec2 fitSize;
    fitSize.x = (float)width * scale;
    fitSize.y = (float)height * scale;

    return fitSize;
}
} // namespace

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
    // stall and generate ishihara textures
    NEEDS_IMPLEMENTATION(); // need to implement ishi
    _subject = SubjectContext{
        .currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION,
        .state = SubjectState::kFixation,
        .currentAttempt = 0,
        .numSuccessAttempts = 0
    };
    _state = TestState::kScreening;
}

void ImGuiWidgetPhychophysicsScreeningTest::drawTestForSubject(
    Tetrium* engine,
    ColorSpace colorSpace,
    SubjectContext& subject
)
{
    // handle state transition
    subject.currStateRemainderTime -= ImGui::GetIO().DeltaTime;
    if (subject.currStateRemainderTime <= 0) {
        switch (subject.state) {
        case SubjectState::kFixation:
            subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION;
            subject.state = SubjectState::kIdentification;
            break;
        case SubjectState::kIdentification:
            subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING;
            // TODO: finish answer presentation logic
            NEEDS_IMPLEMENTATION()
            // randomly choose one circle to place the actual answer

            // populate the rest of the circles with wrong answers


            
            subject.state = SubjectState::kAnswer;
            break;
        case SubjectState::kAnswer:
            subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
            // TODO: handle case where subject hasn't selected anything
            NEEDS_IMPLEMENTATION();

            subject.state = SubjectState::kFixation;
            break;
        }
    }

    switch (subject.state) {
    case SubjectState::kFixation:
        drawFixGazePage();
        break;
    case SubjectState::kIdentification:
        drawIshihara(engine, subject, colorSpace);
        break;
    case SubjectState::kAnswer:
        drawAnswerPrompts(engine, subject, colorSpace);
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

// draws an ishihara plate
void ImGuiWidgetPhychophysicsScreeningTest::drawIshihara(
    Tetrium* engine,
    SubjectContext& subject,
    ColorSpace cs
)
{
    ImGuiTexture tex
        = engine->getOrLoadImGuiTexture(engine->_imguiCtx, subject.currentIshiharaTexturePath[cs]);

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 textureFullscreenSize = calculateFitSize(tex.width, tex.height, availSize);

    ImGui::Image(tex.id, textureFullscreenSize);
}

void ImGuiWidgetPhychophysicsScreeningTest::drawAnswerPrompts(
    Tetrium* engine,
    SubjectContext& subject,
    ColorSpace cs
)
{
}
