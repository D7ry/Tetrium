#pragma once
#include "imgui.h"
#include "structs/ColorSpace.h"
#include <map>

class Quarkolor;

// An ImGui widget that draws information related to
// the main engine.
class ImGuiWidget
{
  public:
    ImGuiWidget() {};
    virtual void Draw(const Quarkolor* engine, ColorSpace colorSpace) = 0;
};

class ImGuiWidgetMut
{
  public:
    ImGuiWidgetMut() {};
    virtual void Draw(Quarkolor* engine, ColorSpace colorSpace) = 0;
};

class ImGuiWidgetDeviceInfo : public ImGuiWidget
{
  public:
    virtual void Draw(const Quarkolor* engine, ColorSpace colorSpace) override;
};

class ImGuiWidgetPerfPlot : public ImGuiWidget
{

  public:
    virtual void Draw(const Quarkolor* engine, ColorSpace colorSpace) override;

  private:
    struct ScrollingBuffer
    {
        int MaxSize;
        int Offset;
        ImVector<ImVec2> Data;

        // need as many frame to hold the points
        ScrollingBuffer(
            int max_size = DEFAULTS::PROFILER_PERF_PLOT_RANGE_SECONDS * DEFAULTS::MAX_FPS
        );

        void AddPoint(float x, float y);

        void Erase();

        bool Empty() { return Data.empty(); }
    };

    std::map<const char*, ScrollingBuffer> _scrollingBuffers;

    // shows the profiler plot; note on
    // lower-end systems the profiler plot itself consumes
    // CPU cycles (~2ms on a M3 mac)
    bool _wantShowPerfPlot = true;
};

// view and edit engineUBO
class ImGuiWidgetUBOViewer : public ImGuiWidget
{
  public:
    virtual void Draw(const Quarkolor* engine, ColorSpace colorSpace) override;
};

// even-odd frame
class ImGuiWidgetEvenOdd : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Quarkolor* engine, ColorSpace colorSpace) override;

  private:
    bool _drawTestWindow = false;
    bool _drawQuadColorTest = false;
    void drawCalibrationWindow(Quarkolor* engine, ColorSpace colorSpace);
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
        Quarkolor* engine,
        int start,
        int end,
        int stepSize,
        int& worstOffset,
        float progressWeight
    );
    int combinedCalibration(Quarkolor* engine);

    void startAutoCalibration(Quarkolor* engine);
    void autoCalibrationThread(Quarkolor* engine);
    int measureDroppedFrames(Quarkolor* engine, int offset, int duration);
};

// clear values
class ImGuiWidgetGraphicsPipeline : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Quarkolor* engine, ColorSpace colorSpace) override;
};
