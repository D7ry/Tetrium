#pragma once
#include "ImGuiWidget.h"
#include <glm/gtx/spline.hpp>

// hunt the blobs, get the scores
class ImGuiWidgetBlobHunter : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;

  private:
    struct CatmullRomSpline
    {
        std::vector<glm::vec2> controlPoints;
        float alpha = 0.5f; // 0.5 for centripetal, 0.0 for uniform, 1.0 for chordal

        glm::vec2 interpolate(float t) const
        {
            if (controlPoints.size() < 4)
                return glm::vec2(0.0f);

            int p0, p1, p2, p3;
            p1 = (int)t;
            p2 = (p1 + 1) % controlPoints.size();
            p3 = (p2 + 1) % controlPoints.size();
            p0 = p1 >= 1 ? p1 - 1 : controlPoints.size() - 1;

            t = t - (int)t;

            float t0 = 0.0f;
            float t1 = getT(t0, controlPoints[p0], controlPoints[p1]);
            float t2 = getT(t1, controlPoints[p1], controlPoints[p2]);
            float t3 = getT(t2, controlPoints[p2], controlPoints[p3]);

            t = glm::mix(t1, t2, t);

            return glm::catmullRom(
                controlPoints[p0],
                controlPoints[p1],
                controlPoints[p2],
                controlPoints[p3],
                (t - t1) / (t2 - t1)
            );
        }

      private:
        float getT(float t, const glm::vec2& p0, const glm::vec2& p1) const
        {
            glm::vec2 d = p1 - p0;
            float a = glm::dot(d, d);
            float b = std::pow(a, alpha * 0.5f);
            return (b + t);
        }
    };

    inline static const struct Settings
    {
        static const uint32_t NUM_ATTEMPTS_PER_SESSION = 3;
        static constexpr float ERROR_MARGIN_SECONDS = 3;
        static constexpr float BLOB_SIZE_RADIUS = 150; // radius of the blob
    } SETTINGS;

    // each attempt is a actual gameplay loop
    // where the user traces the moving blob with mouse cursor
    struct GameAttemptResult
    {
        float t; // the curoff point
    };

    struct GameAttemptContext
    {
        // if it reaches 0, gg
        // keeps decrementing by delta time if the cursor is not focused on blob
        float lastTimeCursorOnBlob = 0;
        float timeLeftSeconds = SETTINGS.ERROR_MARGIN_SECONDS;
        ImVec2 blobPos; // position of the blob
        CatmullRomSpline blobPath;
        float pathProgress;
    };

    // each session corresponds to one user and NUM_TRY's attempt
    struct GameSessionContext
    {
        std::string userName = "John Doe";
        std::vector<GameAttemptResult> results = {};
        GameAttemptContext currentAttempt;
        bool firstAtttemptInitialized = false;
    };

  private:
    bool _isInGame = true;
    GameSessionContext _currentSession;

    void gameplayTick(Tetrium* engine, ColorSpace colorSpace, GameSessionContext& session);

    static GameAttemptContext newGameAttempt();
    static void initializeBlobPath(GameAttemptContext& attempt, const ImVec2& screenSize);
    static void updateBlobPosition(GameAttemptContext& attempt, float deltaTime);
};
