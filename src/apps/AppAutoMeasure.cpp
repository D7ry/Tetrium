#include "AppAutoMeasure.h"
#include "Pathing.h"
#include "imgui.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

#include "app_components/PR650.h"

namespace
{

static struct
{
    // jessica, shove in the stuff here, in {r, y, g, b} format
    const std::vector<glm::ivec4> PRIMARIES{
        // PRIMARIES
        // {{255, 0, 0, 0}, {0, 255, 0, 0}, {0, 0, 255, 0}, {0, 0, 0, 255}}
        // TEST VALUES
        {{11, 77, 70, 255},  {255, 121, 66, 43}, {0, 101, 71, 239},  {255, 146, 66, 18},
         {0, 119, 75, 211},  {243, 162, 71, 0},  {0, 65, 97, 246},   {255, 110, 92, 25},
         {0, 93, 98, 220},   {253, 138, 94, 0},  {0, 116, 98, 192},  {221, 155, 94, 0},
         {0, 57, 123, 222},  {255, 102, 118, 1}, {0, 82, 126, 196},  {226, 122, 122, 0},
         {0, 108, 124, 170}, {196, 143, 121, 0}, {0, 76, 68, 249},   {255, 117, 64, 44},
         {0, 98, 71, 220},   {255, 138, 67, 16}, {0, 114, 74, 196},  {245, 152, 70, 0},
         {0, 66, 94, 231},   {255, 106, 90, 27}, {0, 92, 95, 206},   {255, 132, 91, 2},
         {0, 112, 95, 180},  {225, 148, 91, 0},  {0, 58, 118, 209},  {255, 98, 114, 4},
         {0, 82, 121, 184},  {230, 118, 118, 0}, {0, 105, 120, 160}, {199, 137, 117, 0},
         {22, 79, 72, 255},  {255, 126, 67, 44}, {0, 103, 72, 251},  {255, 154, 67, 20},
         {0, 124, 76, 220},  {244, 173, 71, 0},  {1, 64, 100, 255},  {255, 116, 95, 25},
         {0, 94, 101, 229},  {253, 145, 96, 0},  {0, 121, 101, 199}, {221, 165, 97, 0},
         {0, 56, 127, 231},  {255, 108, 121, 0}, {0, 83, 130, 204},  {225, 129, 125, 0},
         {0, 110, 128, 178}, {196, 150, 124, 0}, {51, 75, 76, 255},  {255, 145, 69, 28},
         {28, 102, 76, 255}, {255, 179, 68, 2},  {0, 125, 78, 255},  {229, 203, 71, 0},
         {31, 58, 105, 255}, {255, 134, 98, 6},  {5, 85, 107, 255},  {234, 164, 100, 0},
         {0, 119, 107, 229}, {206, 189, 100, 0}, {6, 43, 134, 255},  {235, 121, 127, 0},
         {0, 74, 137, 233},  {209, 145, 130, 0}, {0, 108, 135, 205}, {184, 170, 129, 0}}
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

void writeToFile(const std::string file_path, const std::string text_to_write)
{
    std::thread([file_path, text_to_write]() {
        try {
            // Create directory if it doesn't exist
            std::filesystem::path path(file_path);
            std::filesystem::create_directories(path.parent_path());

            std::ofstream file;
            file.open(file_path, std::ios::out | std::ios::trunc); // Overwrite existing file
            if (!file.is_open()) {
                PANIC("failed to open file: {}", file_path)
            }
            file << text_to_write;
            file.close();
            INFO("written to {}", file_path);
        } catch (const std::exception& e) {
            PANIC("error writing file: {}", e.what());
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
                // write results to file in date-based directory
                std::string primariesDir = getTodayPrimariesPath();
                std::stringstream fileName;
                fileName << primariesDir << "/r" << RGBO.x << "g" << RGBO.y << "b" << RGBO.z << "o"
                         << RGBO.w << ".csv";
                writeToFile(fileName.str(), resultStr.str());
                INFO("Saved measurement to: {}", fileName.str());

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
