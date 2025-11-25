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

    // Scan for available measurement files
    scanMeasurementFiles();
};

void AppAutoMeasure::Cleanup(TetriumApp::CleanupContext& ctx) { delete IPR650; };

bool AppAutoMeasure::loadPrimariesFromFile(const std::string& filename)
{
    std::string fullPath = ASSETS_PATH + "apps/AppAutoMeasure/" + filename;
    std::ifstream file(fullPath);

    if (!file.is_open()) {
        ERROR("Failed to open measurement file: {}", fullPath);
        return false;
    }

    measurementData.currentPrimaries.clear();
    std::string line;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);
        int r, g, b, o;

        if (iss >> r >> g >> b >> o) {
            // Validate range
            if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255 && o >= 0
                && o <= 255) {
                measurementData.currentPrimaries.emplace_back(r, g, b, o);
            } else {
                WARN("Invalid RGBO values in line: {}", line);
            }
        } else {
            WARN("Failed to parse line: {}", line);
        }
    }

    file.close();

    if (measurementData.currentPrimaries.empty()) {
        ERROR("No valid primaries loaded from file: {}", filename);
        return false;
    }

    INFO("Loaded {} primaries from {}", measurementData.currentPrimaries.size(), filename);
    measurementData.selectedFile = filename;
    measurementData.currPrimaryIndex = 0;
    return true;
}

void AppAutoMeasure::scanMeasurementFiles()
{
    measurementData.availableFiles.clear();
    std::string measurementDir = ASSETS_PATH + "apps/AppAutoMeasure/";

    try {
        for (const auto& entry : std::filesystem::directory_iterator(measurementDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                measurementData.availableFiles.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        WARN("Failed to scan measurement directory: {}", e.what());
    }

    // Load the first available file by default
    if (!measurementData.availableFiles.empty()) {
        measurementData.selectedFileIndex = 0;
        loadPrimariesFromFile(measurementData.availableFiles[0]);
    }
}

void AppAutoMeasure::TickImGui(const TetriumApp::TickContextImGui& ctx)
{

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
    ImGuiWindowFlags flags = 0;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize;

    if (ImGui::Begin("measure", NULL, flags)) {
        // Measurement file selection dropdown
        ImGui::Text("Measurement File:");
        ImGui::SameLine();

        if (!measurementData.availableFiles.empty()) {
            const char* currentFile = measurementData.selectedFile.c_str();
            if (ImGui::BeginCombo("##MeasurementFile", currentFile)) {
                for (int i = 0; i < measurementData.availableFiles.size(); i++) {
                    bool isSelected = (measurementData.selectedFileIndex == i);
                    if (ImGui::Selectable(measurementData.availableFiles[i].c_str(), isSelected)) {
                        measurementData.selectedFileIndex = i;
                        loadPrimariesFromFile(measurementData.availableFiles[i]);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("Refresh Files")) {
                scanMeasurementFiles();
            }
        } else {
            ImGui::Text("No .txt files found in assets/apps/AppAutoMeasure/");
            if (ImGui::Button("Refresh Files")) {
                scanMeasurementFiles();
            }
        }

        ImGui::Separator();

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

    if (measurementData.currentPrimaries.empty()) {
        ImGui::Text("No measurement data loaded. Please select a valid .txt file.");
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }

    glm::ivec4 RGBO = measurementData.currentPrimaries[measurementData.currPrimaryIndex];
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
                = "Measuring " + std::to_string(measurementData.currPrimaryIndex + 1) + "/"
                  + std::to_string(measurementData.currentPrimaries.size());
            ImGui::Button(measureContext.measuringString.c_str());
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
                // write results to file in measurement-specific directory
                std::string baseFileName = measurementData.selectedFile;
                // Remove .txt extension
                if (baseFileName.size() > 4
                    && baseFileName.substr(baseFileName.size() - 4) == ".txt") {
                    baseFileName = baseFileName.substr(0, baseFileName.size() - 4);
                }

                std::string measurementDir = getTodayPrimariesPath() + "/" + baseFileName;
                std::stringstream fileName;
                fileName << measurementDir << "/r" << RGBO.x << "g" << RGBO.y << "b" << RGBO.z
                         << "o" << RGBO.w << ".csv";
                writeToFile(fileName.str(), resultStr.str());
                INFO("Saved measurement to: {}", fileName.str());

                // measure next primary
                measurementData.currPrimaryIndex++;
                // done measuring
                if (measurementData.currPrimaryIndex == measurementData.currentPrimaries.size()) {
                    pr650States.measuring = false;
                    measurementData.currPrimaryIndex = 0;
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
    ImGui::Text("%s", measureContext.rgboValuesString.c_str());

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
