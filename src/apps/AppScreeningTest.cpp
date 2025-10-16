#include <filesystem>

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h" // for string input text

#include "AppScreeningTest.h"

#include <Pathing.h>
#include <string>

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
static const std::vector<int> ISHIHARA_PLATES_NUMBERS = [] {
    std::vector<int> v;
    for (int i = 10; i <= 99; ++i)
        v.push_back(i);
    return v;
}();

// Pick 4 random, non-repeating numbers from the ishihara plates
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
    return TETRIUM_COLOR_PATH + "TetriumColor/Assets/HiddenImages/" + std::to_string(plateNumber)
           + ".png";
}

void TetriumApp::AppScreeningTest::TickImGui(const TetriumApp::TickContextImGui& ctx)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    ImGui::SetNextWindowBgAlpha(0);
    if (ImGui::Begin("PsuedoIsochromatic Test", NULL, flags)) {
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
        ImGui::Text("Duration of Blank Period (seconds)");
        ImGui::InputFloat("##Blank", &SETTINGS.STATE_DURATIONS_SECONDS.BLANK);
        ImGui::Text("Duration of Fixation (seconds)");
        ImGui::InputFloat("##Fixation", &SETTINGS.STATE_DURATIONS_SECONDS.FIXATION);
        ImGui::Text("Duration of Identification (seconds)");
        ImGui::InputFloat("##Identification", &SETTINGS.STATE_DURATIONS_SECONDS.IDENTIFICATION);
        ImGui::Text("Duration of Answering (seconds)");
        ImGui::InputFloat("##Answering", &SETTINGS.STATE_DURATIONS_SECONDS.ANSWERING);

        // Lum noise slider
        ImGui::SliderFloat("Lum Noise", &SETTINGS.LUM_NOISE, 0.0f, 1.0f);

        // S-cone noise slider
        ImGui::SliderFloat("S-Cone Noise", &SETTINGS.S_CONE_NOISE, 0.0f, 1.0f);

        // Stimulus size slider
        ImGui::SliderFloat("Stimulus Size", &SETTINGS.STIMULUS_SIZE, 0.0f, 1.0f);

        // Music setting dropdown
        ImGui::Text("Music Setting");
        const char* musicOptions[] = {"ALL", "CORRECT_WRONG", "OFF"};
        int currentMusicSetting = static_cast<int>(SETTINGS.MUSIC_SETTING);
        if (ImGui::Combo("##Music", &currentMusicSetting, musicOptions, 3)) {
            SETTINGS.MUSIC_SETTING = static_cast<MusicSetting>(currentMusicSetting);
        }
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            _state = TestState::kIdle;
        }
        ImGui::EndPopup();
    }
}

void TetriumApp::AppScreeningTest::drawIdle(const TetriumApp::TickContextImGui& ctx)
{
    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
        ctx.controls.musicOverride = Sound::kMusicGameMenu;
    } else {
        ctx.controls.musicOverride = std::nullopt;
    }
    const float buttonSpacing = 20.0f;

    // Set the button size
    ImVec2 buttonSize(200, 100);

    // Center the button on the screen
    ImVec2 screenSize = ImGui::GetIO().DisplaySize;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImGuiTexture& tex = _textures.chromalabLogo;
    ImVec2 logoSize = calculateFitSize(tex.width, tex.height, availSize);
    logoSize = logoSize * 0.75f;

    ImVec2 elemPos((availSize.x - logoSize.x) * 0.5f + 25, logoSize.y / 2 - 50);
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
    ImGui::Text("Subject ID:");
    elemPos = elemPos + ImVec2(10, 50);
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
        if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
            ctx.apis.PlaySound(Sound::kVineBoom);
        }
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
    // ImVec2 textureFullscreenSize = calculateFitSize(tex.width, tex.height, availSize);
    // need to scale this such that the stimuli is 2 degrees when we look at in on windows
    ImVec2 textureFullscreenSize
        = ImVec2(tex.width * SETTINGS.STIMULUS_SIZE, tex.height * SETTINGS.STIMULUS_SIZE);

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
    if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
        ctx.controls.musicOverride = Sound::kMusicGamePlay;
    } else {
        ctx.controls.musicOverride = std::nullopt;
    }
    // handle state transition
    subject.currStateRemainderTime -= ImGui::GetIO().DeltaTime;
    if (subject.currStateRemainderTime <= 0) {
        transitionSubjectState(subject, ctx);
        // If the game just ended, we switched out of screening; stop drawing this frame
        if (_state != TestState::kScreening) {
            return;
        }
    }
    ASSERT(subject.currStateRemainderTime > 0);

    if (ImGui::IsKeyPressed(ImGuiKey_GamepadBack)) {
        ctx.apis.PlaySound(Sound::kVineBoom);
        _state = TestState::kIdle;
    }

    switch (subject.state) {
    case SubjectState::kBlank:
        // Blank state - draw nothing (entirely black)
        break;
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
    // Calculate the size and position of the box
    ImVec2 boxSize(1200, 900);
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 boxPos((windowSize.x - boxSize.x) * 0.5f, (windowSize.y - boxSize.y) * 0.5f);

    // Draw centered box
    ImGui::SetCursorPos(boxPos);
    ImGui::BeginChild(
        "CenteredBox", boxSize, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
    );

    // --- NEW CONTENT BELOW ---
    int numMisses = SETTINGS.NUM_ATTEMPTS - subject.numSuccessAttempts;
    bool perfect = numMisses < 1;

    const char* mainMsg = perfect ? "Congratulations!" : "Tough luck!";
    const char* followMsg
        = perfect ? "You're a Tetrachromat!" : "You probably won't do better next time.";

    // Vertically center text block
    float lineSpacing = ImGui::GetTextLineHeightWithSpacing();
    float yStart = (boxSize.y - (lineSpacing * 5.0f)) * 0.5f;

    // 1. "You scored x/x"
    ImVec2 textSize1 = ImGui::CalcTextSize("You scored 00/00");
    ImGui::SetCursorPos(ImVec2((boxSize.x - textSize1.x) * 0.5f, yStart));
    ImGui::Text("You scored %d/%d", subject.numSuccessAttempts, SETTINGS.NUM_ATTEMPTS);

    // 2. Large "Congratulations" or "Tough luck!"
    ImGui::SetWindowFontScale(2.0f);
    ImVec2 textSize2 = ImGui::CalcTextSize(mainMsg);
    ImGui::SetCursorPos(
        ImVec2((boxSize.x - textSize2.x * 2.0f * 0.5f) * 0.5f, yStart + lineSpacing * 2.0f)
    );
    ImGui::Text("%s", mainMsg);
    ImGui::SetWindowFontScale(1.0f);

    // 3. Normal text follow-up line
    ImVec2 textSize3 = ImGui::CalcTextSize(followMsg);
    ImGui::SetCursorPos(ImVec2((boxSize.x - textSize3.x) * 0.5f, yStart + lineSpacing * 4.0f));
    ImGui::Text("%s", followMsg);

    // 4. "Okay" button centered below text
    ImVec2 buttonSize(150, 60);
    ImVec2 buttonPos((boxSize.x - buttonSize.x) * 0.5f, yStart + lineSpacing * 6.0f);
    ImGui::SetCursorPos(buttonPos);
    if (ImGui::Button("Okay", buttonSize)) {
        if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
            ctx.apis.PlaySound(Sound::kVineBoom);
        }
        _state = TestState::kIdle;
    }

    ImGui::EndChild();
    return;
    /* // Calculate the size of the box
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
        if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL) {
            ctx.apis.PlaySound(Sound::kVineBoom);
        }
        _state = TestState::kIdle;
    }

    ImGui::EndChild(); */
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

    // Gamepad button keys corresponding to each answer button
    ImGuiKey gamepadKeys[4] = {
        ImGuiKey_GamepadFaceDown,  // A button
        ImGuiKey_GamepadFaceLeft,  // X button
        ImGuiKey_GamepadFaceRight, // B button
        ImGuiKey_GamepadFaceUp     // Y button
    };

    // Check for gamepad input first
    int pressedButton = -1;
    for (int i = 0; i < 4; i++) {
        if (ImGui::IsKeyPressed(gamepadKeys[i])) {
            pressedButton = i;
            break;
        }
    }

    // Draw the four buttons
    for (int i = 0; i < 4; i++) {
        ImGuiTexture tex = subject.prompt.currentAnswerTexture[i];

        ImGui::SetCursorPos(ImVec2(positions[i].x - buttonSize / 2, positions[i].y - buttonSize / 2)
        );

        // Set button background to black to match the overall background
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

        bool buttonClicked = ImGui::ImageButton(
            buttonLabels[i], (void*)(intptr_t)tex.id, ImVec2(buttonSize, buttonSize)
        );

        // Pop the button style colors
        ImGui::PopStyleColor(3);

        // Add button label
        ImVec2 textSize = ImGui::CalcTextSize(buttonLabels[i]);
        ImVec2 textPos
            = ImVec2(positions[i].x - textSize.x * 0.5f + 3, positions[i].y + buttonSize / 2 + 10);
        ImGui::SetCursorPos(textPos);
        ImGui::Text("%s", buttonLabels[i]);

        // Check if this button was activated (either by click or gamepad)
        if (buttonClicked || pressedButton == i) {
            printf("%s button clicked!\n", buttonLabels[i]);
            subject.prompt.currentSelectedAnswer = i;
            if (subject.prompt.currentSelectedAnswer == subject.prompt.correctAnswerTextureIndex) {
                printf("Correct answer!\n");
                // NOTE: incrementing numSuccessAttempts is done in transitionSubjectState
                if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL
                    || SETTINGS.MUSIC_SETTING == MusicSetting::CORRECT_WRONG) {
                    ctx.apis.PlaySound(Sound::kCorrectAnswer);
                }
            } else {
                printf("Wrong answer!\n");
                if (SETTINGS.MUSIC_SETTING == MusicSetting::ALL
                    || SETTINGS.MUSIC_SETTING == MusicSetting::CORRECT_WRONG) {
                    ctx.apis.PlaySound(Sound::kWrongAnswer);
                }
            }
            transitionSubjectState(subject, ctx);
            // Only process one button press per frame
            break;
        }
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
    case SubjectState::kBlank:
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.FIXATION;
        subject.state = SubjectState::kFixation;
        break;
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
        // If we've reached the last attempt, end game and stop further transitions/prompts
        if (subject.currentAttempt >= (SETTINGS.NUM_ATTEMPTS - 1)) {
            endGame(subject);
            return;
        }
        // Otherwise advance to next attempt and show blank screen
        subject.currentAttempt += 1;
        subject.currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.BLANK;
        subject.state = SubjectState::kBlank;
        populatePromptContext(subject, ctx);
        break;
    }
}

void AppScreeningTest::newGame(const TetriumApp::TickContextImGui& ctx)
{
    // Clean up old generators
    if (_plateGenerator) {
        delete _plateGenerator;
    }
    if (_colorGenerator) {
        delete _colorGenerator;
    }

    // Create color generator with the new interface
    // Matching the Python snippet:
    // primaries = load_primaries_from_csv("./measurements/2025-05-06/primaries")
    // color_generator = GeneticCDFTestColorGenerator(
    //     sex='female', percentage_screened=0.99, cst_display_type='led',
    //     display_primaries=primaries, dimensions=[2])

    std::vector<int> dimensions = {2};
    _colorGenerator = new TetriumColor::ColorGenerator(
        "female",   // sex
        0.999f,     // percentage_screened
        547.0f,     // peak_to_test (default from Python)
        dimensions, // dimensions
        "led",      // cst_display_type
        TETRIUM_COLOR_PATH + "measurements/2025-10-12/primaries" // display_primaries_path
    );

    // Create plate generator with color generator
    _plateGenerator = new TetriumColor::PseudoIsochromaticPlateGenerator(
        *_colorGenerator,
        42 // seed
    );
    if (SETTINGS.NUM_ATTEMPTS > static_cast<int>(_colorGenerator->GetNumSamples())) {
        INFO(
            "Number of attempts is greater than the number of samples, setting to number of samples"
        );
        SETTINGS.NUM_ATTEMPTS = _colorGenerator->GetNumSamples();
    }

    _subject = SubjectContext{
        .name = _nameInputBuffer,
        .currStateRemainderTime = SETTINGS.STATE_DURATIONS_SECONDS.BLANK,
        .state = SubjectState::kBlank,
        .currentAttempt = 0,
        .numSuccessAttempts = 0,
        .pyObject
        = nullptr, // Explicitly initialize to null to prevent carrying over old Python data
    };
    populatePromptContext(_subject, ctx);
    _state = TestState::kScreening;
}

void AppScreeningTest::endGame(SubjectContext& subject)
{
    DEBUG("ending game for subject {}", subject.name);
    _state = TestState::kScreenResult;
    // TODO: data collection logic + clean up texture resources?
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

// Updated to use the new single-filename interface
std::pair<std::string, std::string> AppScreeningTest::generateIshiharaTestTextures(
    SubjectContext& subject,
    int number
)
{
    // The new interface generates both RGB and OCV versions with a single filename
    // It will create: filename_0.png through filename_5.png (6P format)
    // and filename_srgb.png
    // Ensure the temp directory exists before using it
    std::filesystem::create_directories("./temp");
    std::string baseFilename = "./temp/" + subject.name + "_" + std::to_string(number);

    // Call NewPlate with DISP_6P output space
    _plateGenerator->NewPlate(
        baseFilename,
        number,
        TetriumColor::ColorSpaceType::DISP_6P,
        SETTINGS.LUM_NOISE,
        SETTINGS.S_CONE_NOISE
    );

    // Return paths - the 6P files will be at baseFilename_0.png ... baseFilename_5.png
    // and sRGB at baseFilename_srgb.png
    // For compatibility, we'll use the sRGB version for RGB and one of the 6P channels for OCV
    std::string rgbTexturePath = baseFilename + "_RGB.png";
    std::string ocvTexturePath = baseFilename + "_OCV.png"; // Use first channel of 6P

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

    // Set the correct answer index
    _subject.prompt.correctAnswerTextureIndex = answerPlateIndex;
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
    _textures.chromalabLogo
        = ctx.api.InitImGuiTexture(ctx.api.LoadTexture(ASSETS_PATH + "textures/chromalab-logo.png")
        );
};

void AppScreeningTest::Cleanup(TetriumApp::CleanupContext& ctx)
{
    for (int ishiharaPlateNumber : ISHIHARA_PLATES_NUMBERS) {
        ctx.api.UnloadTexture(_answerPromptTextureHandles[ishiharaPlateNumber]);
    }

    // Clean up generators
    if (_plateGenerator) {
        delete _plateGenerator;
        _plateGenerator = nullptr;
    }
    if (_colorGenerator) {
        delete _colorGenerator;
        _colorGenerator = nullptr;
    }
}
} // namespace TetriumApp
