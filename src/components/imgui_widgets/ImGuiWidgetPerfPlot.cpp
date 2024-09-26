#include "implot.h"

#include "ImGuiWidget.h"

#include "VulkanEngine.h"

ImGuiWidgetPerfPlot::ScrollingBuffer::ScrollingBuffer(int max_size)
{
    MaxSize = max_size;
    Offset = 0;
    Data.reserve(MaxSize);
}

void ImGuiWidgetPerfPlot::ScrollingBuffer::AddPoint(float x, float y)
{
    if (Data.size() < MaxSize)
        Data.push_back(ImVec2(x, y));
    else {
        Data[Offset] = ImVec2(x, y);
        Offset = (Offset + 1) % MaxSize;
    }
}

void ImGuiWidgetPerfPlot::ScrollingBuffer::Erase()
{
    if (Data.size() > 0) {
        Data.shrink(0);
        Offset = 0;
    }
}

void ImGuiWidgetPerfPlot::Draw(const VulkanEngine* engine, ColorSpace colorSpace)
{
    ImGui::Checkbox("Show Perf Plot", std::addressof(_wantShowPerfPlot));
    double deltaTimeSeconds = engine->_deltaTimer.GetDeltaTimeSeconds();
    ImGui::Text("Framerate: %f", 1 / deltaTimeSeconds);

    bool showingPlot = false;
    if (_wantShowPerfPlot) {
        ImVec2 plotSize = {ImGui::GetWindowWidth(), ImGui::GetWindowHeight() / 2};
        showingPlot = ImPlot::BeginPlot("Profiler", plotSize);
    }

    if (showingPlot) { // set up x and y boundary
        ImPlot::SetupAxis(ImAxis_X1, "Time(Sec)");
        ImPlot::SetupAxisLimits(
            ImAxis_X1,
            engine->_timeSinceStartSeconds - DEFAULTS::PROFILER_PERF_PLOT_RANGE_SECONDS,
            engine->_timeSinceStartSeconds,
            ImPlotCond_Always
        );

        ImPlot::SetupAxis(ImAxis_Y1, "Cost(Ms)");
        // adaptively set y range
        const double deltaTimeMiliseconds = deltaTimeSeconds / 1000;
        ImPlot::SetupAxisLimits(
            ImAxis_Y1, 0, deltaTimeMiliseconds < 15 ? 15 : 30, ImPlotCond_Always
        );
    }

    ASSERT(engine->_lastProfilerData != nullptr);

    // iterate over entries, update scrolling buffers and plot
    // also shows text for the raw numbers under the plot
    for (Profiler::Entry& entry : *engine->_lastProfilerData) {
        // ms time
        double ms = std::chrono::duration<double, std::chrono::milliseconds::period>(
                        entry.end - entry.begin
        )
                        .count();
        if (showingPlot) {
            auto it = _scrollingBuffers.find(entry.name);
            if (it == _scrollingBuffers.end()) {
                auto res = _scrollingBuffers.emplace(entry.name, ScrollingBuffer());
                ASSERT(res.second); // insertion success
                it = res.first;
            }
            ScrollingBuffer& buf = it->second;

            // NOTE: we only update the scrolling buffer on RGB pass,
            // and present on CMY pass; no need to write the same data twice.
            if (colorSpace == ColorSpace::RGB) {
                buf.AddPoint(engine->_timeSinceStartSeconds, ms);
            }
            if (!buf.Empty()) {
                ImPlot::PlotLine(
                    entry.name,
                    &buf.Data[0].x,
                    &buf.Data[0].y,
                    buf.Data.size(),
                    0,
                    buf.Offset,
                    2 * sizeof(float)
                );
            }
        }
        { // text section
            int indentWidth = entry.level * 10;
            if (indentWidth != 0) {
                ImGui::Indent(indentWidth);
            }
            // entry name
            ImGui::Text("%s", entry.name);
            ImGui::Text("%f MS", ms);
            if (indentWidth != 0) {
                ImGui::Unindent(indentWidth);
            }
        }
    }
    if (showingPlot) {
        ImPlot::EndPlot();
    }
};
