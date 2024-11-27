#include <filesystem>

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h" // for string input text

#include "AppScreeningTest.h"

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

namespace TetriumApp
{

// Ishihara plates numbers -- we pick from these to generate tests
static const std::vector<int> ISHIHARA_PLATES_NUMBERS
    = {27, 35, 39, 64, 67, 68, 72, 73, 85, 87, 89, 96};

// Pick 4 random, non-repeating numbesrs from the ishihara plates
static std::array<int, 4> PickRandomFourIshiharaPlates()
{
    std::vector<int> numbers = ISHIHARA_PLATES_NUMBERS;
    std::array<int, 4> pickedPlates;
    for (int i = 0; i < 4; i++) {
        int index = rand() % numbers.size();
        pickedPlates[i] = numbers[index];
        numbers.erase(numbers.begin() + index);
    }

    return pickedPlates;
}

static std::string GetIshiharaPlateAnswerTexturePath(int plateNumber)
{
    return "../assets/textures/apps/AppScreeningTest/solutions/" + std::to_string(plateNumber)
           + ".png";
}

void TetriumApp::AppScreeningTest::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(1);
    if (ImGui::Begin("PsydoIsochromatic Test", NULL, flags)) {
        switch (_state) {
        case TestState::kSettings: // draw both settings and idle
            drawSettingsWindow(ctx);
        case TestState::kIdle:
            drawIdle(ctx);
            break;
        case TestState::kScreening:
            drawTestForSubject(_subject, ctx);
            break;
        case TestState::kScreenResult:
            drawSubjectResult(_subject, ctx);
            break;
        }
    }
    ImGui::End();
}

void TetriumApp::AppScreeningTest::drawSettingsWindow(const TetriumApp::TickContextImGui& ctx)
{
    // draw a settings pop-up window
    if (ImGui::BeginPopup("Settings", ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SliderInt("Num Attempts", &SETTINGS.NUM_ATTEMPTS, 1, 10);
        ImGui::Text("Duration of Fixation (seconds)");
        ImGui::InputFloat("##Fixation", &SETTINGS.STATE_DURATIONS_SECONDS.FIXATION);
        ImGui::Text("Duration of Identification (seconds)");
        ImGui::InputFloat("##Identification", &SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION);
        ImGui::Text("Duration of Answering (seconds)");
        ImGui::InputFloat("##Answering", &SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING);
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            _state = TestState::kIdle;
        }
        ImGui::EndPopup();
    }
}

void TetriumApp::AppScreeningTest::drawIdle(const TetriumApp::TickContextImGui& ctx)
{
    ctx.controls.musicOverride = Sound::kMusicGameMenu;
    const float buttonSpacing = 20.0f;

    // Set the button size
    ImVec2 buttonSize(200, 100);

    // Center the button on the screen
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    ImGuiTexture& tex = _textures.bairLogo;
    ImVec2 logoSize = ImVec2{(float)tex.width, (float)tex.height};

    ImVec2 elemPos((screenSize.x - logoSize.x) * 0.5f, (screenSize.y - logoSize.y) * 0.5f - 300);
    // draw the title logo
    ImGui::SetCursorPos(elemPos);
    ImGui::Image(tex.id, logoSize);

    // now for the button
    elemPos = ImVec2(
        (screenSize.x - buttonSize.x) * 0.5f,
        (screenSize.y - logoSize.y) * 0.5f - 300 + logoSize.y + 50
    );

    // Set the cursor position for the button
    ImGui::SetCursorPos(elemPos);
    ImGui::Text("Name:");
    elemPos = elemPos + ImVec2(0, 40);
    ImGui::SetCursorPos(elemPos);

    // Set the width of the input text box to be the same as the button
    ImGui::SetNextItemWidth(buttonSize.x);
    ImGui::InputText("##Input Name", &_nameInputBuffer);

    elemPos = elemPos + ImVec2(0, buttonSize.y + buttonSpacing);
    ImGui::SetCursorPos(elemPos);
    bool isNameEmpty = _nameInputBuffer.empty();
    if (isNameEmpty) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }
    if (ImGui::Button("Play", buttonSize) && !isNameEmpty) {
        newGame(ctx);
    }
    if (isNameEmpty) {
        ImGui::PopStyleColor(3);
    }

    // settings button
    elemPos = elemPos + ImVec2(0, buttonSize.y + buttonSpacing);
    ImGui::SetCursorPos(elemPos);
    if (ImGui::Button("Settings", buttonSize)) {
        ImGui::OpenPopup("Settings");
        _state = TestState::kSettings;
    }

    // exit button
    elemPos = elemPos + ImVec2(0, buttonSize.y + buttonSpacing);
    ImGui::SetCursorPos(elemPos);
    if (ImGui::Button("Exit", buttonSize)) {
        ctx.apis.PlaySound(Sound::kVineBoom);
        ctx.controls.wantExit = true;
    }

    ImGui::PopStyleVar(4);
}

void AppScreeningTest::drawIshihara(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    ImGuiTexture tex
        = subject.prompt.currentIshiharaPlateTexture[ctx.colorSpace]; // RGB is the default

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 textureFullscreenSize = calculateFitSize(tex.width, tex.height, availSize);

    // center the texture onto the screen
    ImVec2 centerPos = ImVec2(availSize.x * 0.5f, availSize.y * 0.5f);
    ImGui::SetCursorPos(centerPos - textureFullscreenSize * 0.5f);

    ImGui::Image(tex.id, textureFullscreenSize);
}

void AppScreeningTest::drawTestForSubject(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    ctx.controls.musicOverride = Sound::kMusicGamePlay;
    // handle state transition
    subject.currStateRemainderTime -= ImGui::GetIO().DeltaTime;
    if (subject.currStateRemainderTime <= 0) {
        transitionSubjectState(subject, ctx);
    }
    ASSERT(subject.currStateRemainderTime > 0);

    switch (subject.state) {
    case SubjectState::kFixation:
        drawFixGazePage();
        break;
    case SubjectState::kIdentification:
        drawIshihara(subject, ctx);
        break;
    case SubjectState::kAnswer:
        drawAnswerPrompts(subject, ctx);
        break;
    }
}

void AppScreeningTest::drawSubjectResult(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // Calculate the size of the box
    ImVec2 boxSize(1200, 900); // Width and height of the box
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 boxPos = ImVec2((windowSize.x - boxSize.x) * 0.5f, (windowSize.y - boxSize.y) * 0.5f);

    // Set the cursor position to the top-left corner of the box
    ImGui::SetCursorPos(boxPos);

    // Draw the box
    ImGui::BeginChild(
        "CenteredBox", boxSize, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
    );

    // Center the text within the box
    ImVec2 textPos = ImVec2(
        (boxSize.x - ImGui::CalcTextSize("Subject: ").x) * 0.5f,
        (boxSize.y - ImGui::CalcTextSize("Subject: ").y) * 0.2f
    );
    ImGui::SetCursorPos(textPos);
    ImGui::Text("Subject: %s", subject.name.c_str());

    textPos.y += ImGui::GetTextLineHeightWithSpacing();
    ImGui::SetCursorPos(textPos);
    ImGui::Text("Number of successful attempts: %d", subject.numSuccessAttempts);

    textPos.y += ImGui::GetTextLineHeightWithSpacing();
    ImGui::SetCursorPos(textPos);
    ImGui::Text("Number of attempts: %d", SETTINGS.NUM_ATTEMPTS);

    textPos.y += ImGui::GetTextLineHeightWithSpacing();
    ImGui::SetCursorPos(textPos);
    ImGui::Text(
        "Success rate: %.2f%%", (float)subject.numSuccessAttempts / SETTINGS.NUM_ATTEMPTS * 100.0f
    );

    // button to exit result
    textPos.y += ImGui::GetTextLineHeightWithSpacing();
    ImVec2 buttonSize(100, 50);
    ImVec2 buttonPos = ImVec2((boxSize.x - buttonSize.x) * 0.5f, textPos.y + 20);
    ImGui::SetCursorPos(buttonPos);
    if (ImGui::Button("Okay", buttonSize)) {
        ctx.apis.PlaySound(Sound::kVineBoom);
        _state = TestState::kIdle;
    }

    ImGui::EndChild();
}

void AppScreeningTest::drawAnswerPrompts(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
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
    ImVec2 topPos = ImVec2(centerPos.x, centerPos.y - verticalSpacing);     // Y
    ImVec2 leftPos = ImVec2(centerPos.x - horizontalSpacing, centerPos.y);  // X
    ImVec2 rightPos = ImVec2(centerPos.x + horizontalSpacing, centerPos.y); // B
    ImVec2 bottomPos = ImVec2(centerPos.x, centerPos.y + verticalSpacing);  // A

    ImVec2 positions[4] = {bottomPos, leftPos, rightPos, topPos}; // A, X, B, Y order
    const char* buttonLabels[4] = {"A", "X", "B", "Y"};

    // Draw the four buttons
    for (int i = 0; i < 4; i++) {
        ImGuiTexture tex = subject.prompt.currentAnswerTexture[i];

        ImGui::SetCursorPos(ImVec2(positions[i].x - buttonSize / 2, positions[i].y - buttonSize / 2)
        );
        if (ImGui::ImageButton(
                buttonLabels[i], (void*)(intptr_t)tex.id, ImVec2(buttonSize, buttonSize)
            )) {
            DEBUG("{} button clicked!", buttonLabels[i]);
            subject.prompt.currentSelectedAnswer = i;
            if (subject.prompt.currentSelectedAnswer == subject.prompt.correctAnswerTextureIndex) {
                DEBUG("Correct answer!");
                // NOTE: incrementin numSuccessAttempts is done in transitionSubjectState
                ctx.apis.PlaySound(Sound::kCorrectAnswer);
            } else {
                DEBUG("Wrong answer!");
                ctx.apis.PlaySound(Sound::kWrongAnswer);
            }
            transitionSubjectState(subject, ctx);
        }

        // Add button label
        ImVec2 textPos = ImVec2(positions[i].x - 10, positions[i].y + buttonSize / 2 + 5);
        ImGui::SetCursorPos(textPos);
        ImGui::Text("%s", buttonLabels[i]);
    }

    // draw progress bar showing time left
    float totalTime = SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING;
    float progress = subject.currStateRemainderTime / totalTime;
    ImVec2 progressBarSize = ImVec2(800, 40); // Width and height of the progress bar
    ImVec2 progressBarPos = ImVec2(centerPos.x - progressBarSize.x * 0.5f, topPos.y - 300);
    ImGui::SetCursorPos(progressBarPos);
    ImGui::ProgressBar(progress, progressBarSize);
}

void AppScreeningTest::transitionSubjectState(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    switch (subject.state) {
    case SubjectState::kFixation:
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION;
        subject.state = SubjectState::kIdentification;
        break;
    case SubjectState::kIdentification:
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING;
        subject.state = SubjectState::kAnswer;
        break;
    case SubjectState::kAnswer:
        if (subject.prompt.currentSelectedAnswer == subject.prompt.correctAnswerTextureIndex) {
            subject.numSuccessAttempts += 1;
        }
        // end subject
        if (subject.currentAttempt == SETTINGS.NUM_ATTEMPTS - 1) {
            endGame(subject);
        }
        subject.currentAttempt += 1; // to next attempt
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = SubjectState::kFixation;
        populatePromptContext(subject, ctx);
        break;
    }
}

void AppScreeningTest::newGame(const TetriumApp::TickContextImGui& ctx)
{
    if (_plateGenerator) {
        delete _plateGenerator;
    }
    _plateGenerator = new TetriumColor::PseudoIsochromaticPlateGenerator(
        {"../extern/TetriumColor/TetriumColor/Assets/ColorSpaceTransforms/Neitz_530_559-RGBO"},
        {"../extern/TetriumColor/TetriumColor/Assets/PreGeneratedMetamers/"
         "Neitz_530_559-RGBO.pkl"},
        8
    );
    _subject = SubjectContext{
        .name = _nameInputBuffer,
        .currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION,
        .state = SubjectState::kFixation,
        .currentAttempt = 0,
        .numSuccessAttempts = 0,
    };
    populatePromptContext(_subject, ctx);
    _state = TestState::kScreening;
}

void AppScreeningTest::endGame(SubjectContext& subject)
{
    DEBUG("ending game for subject {}", subject.name);
    _state = TestState::kScreenResult;
    // TODO: data colletion logic + clean up texture resources?
}

void AppScreeningTest::drawFixGazePage()
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

// FIXME: make each ishihara plate unique -- probably using time as filename??
std::pair<std::string, std::string> AppScreeningTest::generateIshiharaTestTextures(
    SubjectContext& subject,
    int number
)
{
    std::string rgbTexturePath
        = "./temp/" + subject.name + "_" + std::to_string(number) + "_RGB.png";
    std::string ocvTexturePath
        = "./temp/" + subject.name + "_" + std::to_string(number) + "_OCV.png";

    _plateGenerator->NewPlate(rgbTexturePath, ocvTexturePath, number);
    return {rgbTexturePath, ocvTexturePath};
}

void AppScreeningTest::populatePromptContext(
    SubjectContext& subject,
    const TetriumApp::TickContextImGui& ctx
)
{
    // stall and generate ishihara textures
    std::array<int, 4> ishiharaPlateNumbers = PickRandomFourIshiharaPlates();
    int answerPlateIndex = rand() % ishiharaPlateNumbers.size();
    int answerPlateNumber = ishiharaPlateNumbers[answerPlateIndex];

    auto [rgbTexturePath, ocvTexturePath]
        = generateIshiharaTestTextures(_subject, answerPlateNumber);

    // unload previous textures
    if (_subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::RGB] != 0) {
        ctx.apis.UnloadTexture(_subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::RGB]);
    }
    if (_subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::OCV] != 0) {
        ctx.apis.UnloadTexture(_subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::OCV]);
    }

    _subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::RGB]
        = ctx.apis.LoadTexture(rgbTexturePath);
    _subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::OCV]
        = ctx.apis.LoadTexture(ocvTexturePath);

    _subject.prompt.currentIshiharaPlateTexture[ColorSpace::RGB] = ctx.apis.InitImGuiTexture(
        _subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::RGB]
    );
    _subject.prompt.currentIshiharaPlateTexture[ColorSpace::OCV] = ctx.apis.InitImGuiTexture(
        _subject.prompt.currentIshiharaPlateTextureHandle[ColorSpace::OCV]
    );

    // populate answer textures -- they're pre-generated
    for (int i = 0; i < 4; i++) {
        _subject.prompt.currentAnswerTextureHandle[i]
            = _answerPromptTextureHandles[ishiharaPlateNumbers[i]];
        _subject.prompt.currentAnswerTexture[i]
            = _answerPromptImGuiTextures[ishiharaPlateNumbers[i]];
    }
}

void AppScreeningTest::Init(TetriumApp::InitContext& ctx)
{
    for (int ishiharaPlateNumber : ISHIHARA_PLATES_NUMBERS) {
        std::string path = GetIshiharaPlateAnswerTexturePath(ishiharaPlateNumber);
        uint32_t textureHandle = ctx.api.LoadTexture(path);
        _answerPromptTextureHandles[ishiharaPlateNumber] = textureHandle;
        _answerPromptImGuiTextures[ishiharaPlateNumber] = ctx.api.InitImGuiTexture(textureHandle);
    }
    // load bair logo
    // FIXME: free the logo texture when cleaning up
    _textures.bairLogo = ctx.api.InitImGuiTexture(ctx.api.LoadTexture("../assets/textures/BAIR_logo.png"));
};

void AppScreeningTest::Cleanup(TetriumApp::CleanupContext& ctx){
    for (int ishiharaPlateNumber : ISHIHARA_PLATES_NUMBERS) {
        ctx.api.UnloadTexture(_answerPromptTextureHandles[ishiharaPlateNumber]);
    }
};
} // namespace TetriumApp
