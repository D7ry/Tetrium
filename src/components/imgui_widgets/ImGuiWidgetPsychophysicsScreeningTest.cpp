#include <filesystem>

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

    _subject = SubjectContext{
        .name = "Ren Der Ng",
        .currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION,
        .state = SubjectState::kFixation,
        .currentAttempt = 0,
        .numSuccessAttempts = 0,
        .currentIshiharaTexturePath // FIXME: don't use hard-coded ishihara
        = {"../assets/textures/tetra_images/neitz_common_genes_RGB.png",
           "../assets/textures/tetra_images/neitz_common_genes_OCV.png"},
        .currentAnswerTexturePath // FIXME: don't use hard-coded answer texture
        = {"../assets/textures/tetra_images/neitz_common_genes_RGB.png",
           "../assets/textures/tetra_images/neitz_common_genes_RGB.png",
           "../assets/textures/tetra_images/neitz_common_genes_RGB.png",
           "../assets/textures/tetra_images/neitz_common_genes_RGB.png"},
        .correctAnswerTextureIndex = 2 // FIXME: don't use hard-coded answer index

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
        transitionSubjectState(subject);
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
        screenCenter.x - textSize.x * 0.5f,
        screenCenter.y + textSize.y + spacing + crossHairSize
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

    // center the texture onto the screen
    ImVec2 centerPos = ImVec2(availSize.x * 0.5f, availSize.y * 0.5f);
    ImGui::SetCursorPos(centerPos - textureFullscreenSize * 0.5f);

    ImGui::Image(tex.id, textureFullscreenSize);
}

void ImGuiWidgetPhychophysicsScreeningTest::drawAnswerPrompts(
    Tetrium* engine,
    SubjectContext& subject,
    ColorSpace cs
)
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 screenSize = io.DisplaySize;
    ImVec2 centerPos = ImVec2(screenSize.x * 0.5f, screenSize.y * 0.5f);

    // Adjust these values to fine-tune the layout
    float buttonSize = 200.0f; // Reduced size for better spacing
    float horizontalSpacing = 250.0f;
    float verticalSpacing = 200.0f;

    // Calculate positions for the four buttons in AXBY layout
    ImVec2 topPos = ImVec2(centerPos.x, centerPos.y - verticalSpacing);           // Y
    ImVec2 leftPos = ImVec2(centerPos.x - horizontalSpacing, centerPos.y);        // X
    ImVec2 rightPos = ImVec2(centerPos.x + horizontalSpacing, centerPos.y);       // B
    ImVec2 bottomPos = ImVec2(centerPos.x, centerPos.y + verticalSpacing);        // A

    ImVec2 positions[4] = {bottomPos, leftPos, rightPos, topPos}; // A, X, B, Y order
    const char* buttonLabels[4] = {"A", "X", "B", "Y"};

    // Draw the four buttons
    for (int i = 0; i < 4; i++) {
        ImGuiTexture tex = engine->getOrLoadImGuiTexture(engine->_imguiCtx, subject.currentAnswerTexturePath[i]);

        ImGui::SetCursorPos(ImVec2(positions[i].x - buttonSize/2, positions[i].y - buttonSize/2));
        if (ImGui::ImageButton(buttonLabels[i], (void*)(intptr_t)tex.id, ImVec2(buttonSize, buttonSize))) {
            DEBUG("{} button clicked!", buttonLabels[i]);
            engine->_soundManager.PlaySound(SoundManager::Sound::kVineBoom);
            // Handle button click
        }

        // Add button label
        ImVec2 textPos = ImVec2(positions[i].x - 10, positions[i].y + buttonSize/2 + 5);
        ImGui::SetCursorPos(textPos);
        ImGui::Text("%s", buttonLabels[i]);
    }
}

std::pair<std::string, std::string> ImGuiWidgetPhychophysicsScreeningTest::
    generateIshiharaTestTextures(SubjectContext& subject)
{
    std::string& subjectName = subject.name;
    std::string subjectTestDataFolder = "ishihara_plates/" + subjectName;

    // create subject folder if not already existing
    if (!std::filesystem::exists(subjectTestDataFolder)) {
        DEBUG("creating ishihara plate directory in {}", subjectTestDataFolder);
        std::filesystem::create_directory(subjectTestDataFolder);
    }

    std::string textureFilePrefix = "attempt_" + std::to_string(subject.currentAttempt);

    std::string textureFileRGBName = textureFilePrefix + "_RGB.png";
    std::string textureFileOCVName = textureFilePrefix + "_OCV.png";

    // call into python file to generate
    NEEDS_IMPLEMENTATION();
}

void ImGuiWidgetPhychophysicsScreeningTest::transitionSubjectState(SubjectContext& subject)
{
    switch (subject.state) {
    case SubjectState::kFixation:
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION;
        subject.state = SubjectState::kIdentification;
        break;
    case SubjectState::kIdentification:
        subject.currentSelectedAnswer = -1;
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING;
        subject.state = SubjectState::kAnswer;
        break;
    case SubjectState::kAnswer:
        if (subject.currentSelectedAnswer == subject.correctAnswerTextureIndex) {
            subject.numSuccessAttempts += 1;
        }
        // end subject
        if (subject.currentAttempt == SETTINGS.NUM_ATTEMPTS - 1) {
            endGame(subject);
        }
        subject.currentAttempt += 1; // to next attempt
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = SubjectState::kFixation;
        break;
    }
}

void ImGuiWidgetPhychophysicsScreeningTest::endGame(SubjectContext& subject)
{
    DEBUG("ending game for subject {}", subject.name);
    _state = TestState::kIdle;
    // TODO: data colletion logic + clean up texture resources?
}
