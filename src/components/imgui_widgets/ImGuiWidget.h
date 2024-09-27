#pragma once
#include "imgui.h"
#include "structs/ColorSpace.h"
#include <map>

class Tetrium;

// An ImGui widget that draws information related to
// the main engine.
class ImGuiWidget
{
  public:
    ImGuiWidget() {};
    virtual void Draw(const Tetrium* engine, ColorSpace colorSpace) = 0;
};

class ImGuiWidgetMut
{
  public:
    ImGuiWidgetMut() {};
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) = 0;
};

class ImGuiWidgetDeviceInfo : public ImGuiWidget
{
  public:
    virtual void Draw(const Tetrium* engine, ColorSpace colorSpace) override;
};

class ImGuiWidgetPerfPlot : public ImGuiWidget
{

  public:
    virtual void Draw(const Tetrium* engine, ColorSpace colorSpace) override;

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
    virtual void Draw(const Tetrium* engine, ColorSpace colorSpace) override;
};


// clear values
class ImGuiWidgetGraphicsPipeline : public ImGuiWidgetMut
{
  public:
    virtual void Draw(Tetrium* engine, ColorSpace colorSpace) override;
};
