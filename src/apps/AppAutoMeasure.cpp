#include "AppAutoMeasure.h"
#include "imgui.h"
#include <fstream>
#include <thread>

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

static struct
{
    // jessica, shove in the stuff here, in {r, y, g, b} format
    const std::vector<glm::ivec4> PRIMARIES{
        // PRIMARIES
        {{255, 0, 0, 0}, {0, 255, 0, 0}, {0, 0, 255, 0}, {0, 0, 0, 255}}
        // TEST VALUES
        //{{38, 78, 45, 255}, {152, 116, 42, 103}}
        // CUBEMAP
        /*  {{54, 91, 89, 255},  {255, 129, 85, 109}, {41, 109, 84, 255}, {255, 150, 80, 100},
           {18, 129, 83, 255}, {255, 174, 78, 83},  {0, 148, 86, 247},  {255, 196, 81, 63},
           {0, 164, 92, 228},  {255, 212, 86, 43},  {44, 79, 110, 255}, {255, 119, 106, 102},
           {30, 98, 108, 255}, {255, 141, 103, 92}, {3, 121, 108, 255}, {255, 169, 102, 73},
           {0, 147, 110, 233}, {255, 195, 104, 48}, {0, 164, 113, 212}, {255, 212, 108, 28},
           {25, 66, 137, 255}, {255, 109, 132, 89}, {8, 84, 139, 255},  {255, 130, 134, 76},
           {0, 112, 141, 238}, {255, 160, 136, 54}, {0, 140, 141, 214}, {255, 188, 135, 29},
           {0, 159, 139, 195}, {255, 207, 134, 10}, {0, 55, 163, 255},  {255, 104, 158, 70},
           {0, 75, 169, 239},  {255, 123, 164, 54}, {0, 102, 173, 217}, {255, 150, 167, 32},
           {0, 129, 171, 195}, {255, 177, 165, 11}, {0, 149, 165, 180}, {249, 196, 160, 0},
           {0, 54, 183, 236},  {255, 102, 177, 52}, {0, 71, 190, 220},  {255, 119, 184, 35},
           {0, 94, 193, 200},  {255, 142, 188, 16}, {0, 118, 191, 183}, {252, 165, 186, 0},
           {0, 137, 184, 170}, {235, 181, 179, 0}}*/
        /* {{0, 148, 86, 247},
          {255, 196, 81, 63},
          {255, 212, 86, 43},
          {255, 130, 134, 76},
          {0, 54, 183, 236},
          {0, 102, 173, 217},
          {255, 150, 167, 32},
          {0, 129, 171, 195},
          {0, 54, 183, 236}}*/
    };
    int currPrimaryIndex = 0;

    std::string rgboValuesString;
    std::string measuringString;
} measureContext;

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
} // namespace

namespace
{
PR650* IPR650;
}

namespace TetriumApp
{

void AppAutoMeasure::Init(TetriumApp::InitContext& ctx)
{
    std::string portName;

#if defined(_WIN32) || defined(_WIN64)
    // On Windows, serial ports are typically COM1, COM2, etc.
    portName = "COM1"; // Update this to match your PR650 port
#elif defined(__APPLE__)
    // On macOS, serial ports are typically /dev/tty.usbserial-* or /dev/tty.usbmodem*
    portName = "/dev/tty.usbserial-PR650"; // Update as appropriate
#elif defined(__linux__)
    // On Linux, serial ports are typically /dev/ttyUSB0, /dev/ttyACM0, or /dev/ttyS0
    portName = "/dev/ttyUSB0"; // Update as appropriate
#else
#error "Unknown platform! Please define the serial port for your platform."
#endif

    IPR650 = new PR650(portName);
};

void AppAutoMeasure::Cleanup(TetriumApp::CleanupContext& ctx) { delete IPR650; };

void AppAutoMeasure::TickImGui(const TetriumApp::TickContextImGui& ctx)
{

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
                    std::thread t([]() { IPR650->Init(); });
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

    glm::ivec4 RGBO = measureContext.PRIMARIES[measureContext.currPrimaryIndex];
    if (IPR650->isConnected()) {
        if (!pr650States.measuring) {
            // if not measuring, prompt user to start measuring
            if (ImGui::Button("Measure")) {
                std::thread t([]() { IPR650->StartMeasuring(); });
                t.detach();
                pr650States.measuring = true;
            }
        } else {
            measureContext.measuringString
                = "Measuring" + std::to_string(measureContext.currPrimaryIndex + 1) + '/'
                  + std::to_string(measureContext.PRIMARIES.size());
            ImGui::Button(measureContext.measuringString.data());
            // query pr650 to see if data is ready
            if (IPR650->MeasureResult.ready) {
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
                fileName << 'r' << RGBO.x << 'g' << RGBO.y << 'b' << RGBO.z << 'o' << RGBO.w
                         << ".csv";
                appendToFile(fileName.str(), resultStr.str());

                // measure next primary
                measureContext.currPrimaryIndex++;
                // done measuring
                if (measureContext.currPrimaryIndex == measureContext.PRIMARIES.size()) {
                    pr650States.measuring = false;
                    measureContext.currPrimaryIndex = 0;
                } else {
                    // update internal states to proceed to measure next
                    std::thread t([]() { IPR650->StartMeasuring(); });
                    t.detach();
                }
            }
        }
    }
    constexpr std::array<const char*, 4> labels = {"r", "g", "b", "o"};
    measureContext.rgboValuesString = "RGBO: ";
    for (int i = 0; i < 4; i++) {
        measureContext.rgboValuesString += std::to_string(RGBO[i]);
        measureContext.rgboValuesString += ' ';
    }
    ImGui::Text(measureContext.rgboValuesString.data());

    // for (int i = 0; i < 4; i++) {
    //     const char* label = labels.at(i);
    //     ImGui::SliderInt(label, &RGBO[i], 0, 255);
    // }

    drawColorBlock(ctx, RGBO);
    ImGui::End();
    ImGui::PopStyleColor();
}

void AppAutoMeasure::drawColorBlock(const TetriumApp::TickContextImGui& ctx, glm::ivec4 rgbo)
{
    ImVec2 start_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    // Define the rectangle from current draw line (cursor Y) to the bottom of the window
    ImVec2 rect_min = start_pos;
    ImVec2 rect_max = ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y);

    // Choose your color (RGBA)
    ImU32 color = ctx.colorSpace == RGB ? IM_COL32(rgbo.x, rgbo.y, rgbo.w, 255)
                                        : IM_COL32(rgbo.z, rgbo.y, rgbo.w, 255);

    // Draw the filled rectangle
    ImGui::GetWindowDrawList()->AddRectFilled(rect_min, rect_max, color);
}
} // namespace TetriumApp
