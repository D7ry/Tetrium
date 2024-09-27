#pragma once
#include "ImGuiWidget.h"

class ImGuiWidgetEvenOddCalibration : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;

    void DrawCalibrationProgress(Tetrium* engine);

  private:
    bool _drawTestWindow = false;
    bool _drawQuadColorTest = false;
    void drawCalibrationWindow(Tetrium* engine, ColorSpace colorSpace);
    void drawColorQuadTest();

    // for stress testing even odd stability
    static inline const int NUM_STRESS_THREADS = 100000;
    std::thread _stressThreads[NUM_STRESS_THREADS];
    bool _stressTesting;

    // Calibration fields
    std::atomic<bool> _calibrationInProgress{false};
    std::atomic<float> _calibrationProgress{0.0f};
    std::atomic<bool> _calibrationComplete{false};
    std::atomic<int> _optimalOffset{0};
    std::atomic<int> _highestDroppedFrames{std::numeric_limits<int>::max()};

    void recursiveDescentCalibration(
        Tetrium* engine,
        int start,
        int end,
        int stepSize,
        int& worstOffset,
        float progressWeight
    );
    int combinedCalibration(Tetrium* engine);

    void startAutoCalibration(Tetrium* engine);
    void autoCalibrationThread(Tetrium* engine);
    int measureDroppedFrames(Tetrium* engine, int offset, int duration);
};
