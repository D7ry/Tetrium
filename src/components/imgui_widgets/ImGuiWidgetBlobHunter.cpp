#include "ImGuiWidgetBlobHunter.h"

namespace
{
float getTimeSecondsNow()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

} // namespace

void ImGuiWidgetBlobHunter::Draw(Tetrium* engine, ColorSpace colorSpace)
{

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    auto flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                 | ImGuiWindowFlags_NoResize;
    if (ImGui::Begin("Blob Hunter 🐍", NULL, flags)) {
        if (_isInGame) {
            gameplayTick(engine, colorSpace, _currentSession);
        }
    }
    ImGui::End();
}

void ImGuiWidgetBlobHunter::gameplayTick(
    Tetrium* engine,
    ColorSpace colorSpace,
    GameSessionContext& session
)
{

    ImGuiIO& io = ImGui::GetIO();
    GameAttemptContext& attempt = session.currentAttempt;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float deltaTime = io.DeltaTime;

    // perform logic update
    if (colorSpace == RGB) {
        // first attempt
        if (!session.firstAtttemptInitialized) {
            session.currentAttempt = newGameAttempt();
            session.firstAtttemptInitialized = true;
        }
        if (session.results.size() > SETTINGS.NUM_ATTEMPTS_PER_SESSION) {
            DEBUG("ending game for user {}", session.userName);
            _isInGame = false;
            return;
        }

        bool outOfTime = attempt.timeLeftSeconds <= 0;
        if (outOfTime) {
            // end current attempt, start new attempt
            DEBUG("start new attempt!");
            session.results.push_back(GameAttemptResult{.t = attempt.lastTimeCursorOnBlob});
            attempt = newGameAttempt();
            ImVec2 screenCenter = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            attempt.blobPos = screenCenter; // re-center the blob
        }
    }

    // perform blob pos test
    ImVec2 mousePos = io.MousePos;
    ImVec2 vec2distToBlob = attempt.blobPos - mousePos;
    float distToBlob = glm::distance(
        glm::vec2(mousePos.x, mousePos.y), glm::vec2(attempt.blobPos.x, attempt.blobPos.y)
    );

    bool hit = distToBlob < SETTINGS.BLOB_SIZE_RADIUS;
    if (colorSpace == RGB) {
        if (hit) { // in blob
            attempt.lastTimeCursorOnBlob = getTimeSecondsNow();
            attempt.timeLeftSeconds = SETTINGS.ERROR_MARGIN_SECONDS; // reset error margin
        } else { // out of blob
            attempt.timeLeftSeconds -= deltaTime;
        }
        // nudge blob
        updateBlobPosition(attempt, deltaTime);
    }
    // draw blob
    ImU32 colorHit = IM_COL32_WHITE;
    ImU32 colorMiss = IM_COL32_BLACK;

    ImU32 blobColor = hit ? colorHit : colorMiss;

    dl->AddCircleFilled(attempt.blobPos, SETTINGS.BLOB_SIZE_RADIUS, blobColor);
}

ImGuiWidgetBlobHunter::GameAttemptContext ImGuiWidgetBlobHunter::newGameAttempt()
{
    auto io = ImGui::GetIO();
    glm::vec2 screenCenter(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    GameAttemptContext attempt{
        .lastTimeCursorOnBlob = getTimeSecondsNow(),
        .timeLeftSeconds = SETTINGS.ERROR_MARGIN_SECONDS,
        .blobPos = ImVec2{screenCenter.x, screenCenter.y},
        .pathProgress = 0.0f
    };
    initializeBlobPath(attempt, io.DisplaySize);
    return attempt;
}

void ImGuiWidgetBlobHunter::initializeBlobPath(
    GameAttemptContext& attempt,
    const ImVec2& screenSize
)
{
    attempt.blobPath.controlPoints.clear();
    int numPoints = 100; // Increase the number of points for smoother curves
    float centerX = screenSize.x * 0.5f;
    float centerY = screenSize.y * 0.5f;
    float maxRadius = std::min(screenSize.x, screenSize.y) * 0.4f;

    for (int i = 0; i < numPoints; ++i) {
        float t = static_cast<float>(i) / numPoints;
        
        // Spiral motion
        float spiralRadius = maxRadius * (1.0f - 0.5f * t);
        float angle = t * 4.0f * glm::pi<float>();
        float spiralX = std::cos(angle) * spiralRadius;
        float spiralY = std::sin(angle) * spiralRadius;
        
        // Sinusoidal waves
        float waveX = std::sin(t * 10.0f * glm::pi<float>()) * (maxRadius * 0.2f);
        float waveY = std::cos(t * 8.0f * glm::pi<float>()) * (maxRadius * 0.2f);
        
        // Random jitter
        float jitterX = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * (maxRadius * 0.1f);
        float jitterY = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * (maxRadius * 0.1f);
        
        // Combine all motions
        float x = centerX + spiralX + waveX + jitterX;
        float y = centerY + spiralY + waveY + jitterY;
        
        // Ensure the point is within the screen bounds
        x = glm::clamp(x, 0.0f, screenSize.x);
        y = glm::clamp(y, 0.0f, screenSize.y);
        
        attempt.blobPath.controlPoints.emplace_back(x, y);
    }
    
    // Add a few more random points to increase unpredictability
    for (int i = 0; i < 100; ++i) {
        float x = static_cast<float>(rand()) / RAND_MAX * screenSize.x;
        float y = static_cast<float>(rand()) / RAND_MAX * screenSize.y;
        attempt.blobPath.controlPoints.emplace_back(x, y);
    }
    
    attempt.pathProgress = 0.0f;
}

void ImGuiWidgetBlobHunter::updateBlobPosition(GameAttemptContext& attempt, float deltaTime)
{
    float speed = 10.f; // Adjust this value to change the blob's speed
    attempt.pathProgress += speed * deltaTime;
    if (attempt.pathProgress >= attempt.blobPath.controlPoints.size() - 1) {
        attempt.pathProgress -= attempt.blobPath.controlPoints.size() - 1;
    }
    auto pos = attempt.blobPath.interpolate(attempt.pathProgress);
    attempt.blobPos = ImVec2(pos.x, pos.y);
}
