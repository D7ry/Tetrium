#include <thread>
#include <fstream>
#include "AppAutoMeasure.h"
#include "imgui.h"

#include "app_components/PR650.h"

namespace
{
std::string getCurrentTimeForFileName()
{
    // Get the current time as a time_point
    auto now = std::chrono::system_clock::now();

    // Convert to time_t to obtain the time in seconds
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    // Convert to tm struct for formatting
    std::tm tm = *std::localtime(&t);

    // Create a string stream to format the date/time
    std::stringstream ss;

    // Format as YYYY-MM-DD_HH-MM-SS
    ss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");

    return ss.str();
}

void appendToFile(const std::string file_path, const std::string text_to_append)
{
    std::thread([file_path, text_to_append]() {
        std::ofstream file;
        try {
            file.open(file_path, std::ios::out | std::ios::app);
            if (!file.is_open()) {
                PANIC("failed to open file")
            }
            file << text_to_append;
            file.close();
            INFO("written to {}", file_path);
        } catch (const std::exception& e) {
            PANIC("error appending");
        }
    }).detach(); // Detach the thread to let it run independently
}
}

namespace {
    PR650* IPR650;
}

namespace TetriumApp {

void AppAutoMeasure::Init(TetriumApp::InitContext& ctx) {
    std::string portName = "COM3";
    IPR650 = new PR650(portName);
};

void AppAutoMeasure::Cleanup(TetriumApp::CleanupContext& ctx) {
    delete IPR650;
};

void AppAutoMeasure::TickImGui(const TetriumApp::TickContextImGui& ctx) {
    static glm::ivec4 rgbo = {255, 0, 0, 255};

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGuiWindowFlags flags = 0;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize;

    if (ImGui::Begin("measure", NULL, flags)) {
        if (!IPR650->isConnected()) {
            ImGui::Text("PR650 not connected");
            ImGui::SameLine();
            if (pr650States.connecting == false) {
                if (ImGui::Button("connect to PR650")) {
                    std::thread t([] () {IPR650->Init();});
                    t.detach();
                    pr650States.connecting = true;
                }
            } else {
                ImGui::Text("Spinning thread to attempt connection...");
            }
        } else {
            ImGui::Text("PR650 connected!");
        }
    }

    if (IPR650->isConnected()) {
        if (!pr650States.measuring) {
            // if not measuring, prompt user to start measuring
            if (ImGui::Button("Measure")) {
                std::thread t([]() {
                    IPR650->StartMeasuring();
                });
                t.detach();
                pr650States.measuring = true;
            }
        } else {
            // query pr650 to see if data is ready
            if (IPR650->MeasureResult.ready) {
                pr650States.measuring = false;
                // read back pr650 states
                INFO("PR650 results:");
                auto& result = IPR650->MeasureResult;
                INFO("luminance: {}", result.luminance);

                std::ostringstream resultStr;
                resultStr << "wavelength, power, luminance\n";
                std::string file = "result";
                constexpr auto max_precision{std::numeric_limits<double>::digits10 + 1};
                resultStr << std::scientific << std::setprecision(max_precision);
                for (int i = 0; i < result.power.size(); i++) {
                    double wavelength = result.wavelength[i];
                    double power = result.power[i];
                    resultStr << wavelength << ',' << power << ',' << result.luminance << '\n';
                    INFO("{} : {} {}", i, power, wavelength);
                }
                // write results to ffile
                std::stringstream fileName;
                fileName << 
                    'r' << rgbo.x << 
                    'g' << rgbo.y << 
                    'b' << rgbo.z <<
                    'o' << rgbo.w << 
                    ".csv";
                appendToFile(fileName.str(), resultStr.str());
            }
            ImGui::Text("PR650 measuring");
        }
    }
    constexpr std::array<const char*, 4> labels = {
        "r", "g", "b", "o"
    };

    for (int i = 0; i < 4; i++) {
        const char* label = labels.at(i);
        ImGui::SliderInt(label, &rgbo[i], 0, 255);
    }
    
    drawColorBlock(ctx, rgbo);
    ImGui::End();
    ImGui::PopStyleColor();
}

void AppAutoMeasure::drawColorBlock(const TetriumApp::TickContextImGui& ctx, glm::ivec4 rgbo) {
    ImVec2 start_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    // Define the rectangle from current draw line (cursor Y) to the bottom of the window
    ImVec2 rect_min = start_pos;
    ImVec2 rect_max = ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y);

    // Choose your color (RGBA)
    ImU32 color = ctx.colorSpace == RGB ? IM_COL32(rgbo.x, rgbo.y, rgbo.w, 255) : IM_COL32(rgbo.z, rgbo.y, rgbo.w, 255);

    // Draw the filled rectangle
    ImGui::GetWindowDrawList()->AddRectFilled(rect_min, rect_max, color);
}
}
