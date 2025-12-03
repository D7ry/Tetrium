#include "AppAutoMeasure.h"
#include "Pathing.h"
#include "imgui.h"
#include <chrono>
#include <cmath>
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

std::string getTimestampString()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    // Get milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    // Format as YYYYMMDD_HHMMSS_fff (filesystem-safe, no colons)
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

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
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1)); // White text
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

                std::string measurementDir = getTodayMeasurementsPath() + "/" + baseFileName;
                std::string timestamp = getTimestampString();
                std::stringstream fileName;
                fileName << measurementDir << "/r" << RGBO.x << "g" << RGBO.y << "b" << RGBO.z
                         << "o" << RGBO.w << "_" << timestamp << ".csv";
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

    drawLandoltCStimulus(ctx, RGBO);
    ImGui::End();
    ImGui::PopStyleColor(2); // Pop both WindowBg and Text colors
}

void AppAutoMeasure::drawColorBlock(const TetriumApp::TickContextImGui& ctx, glm::ivec4 rgbo)
{
    // Legacy function - now using drawLandoltCStimulus instead
    drawLandoltCStimulus(ctx, rgbo);
}

void AppAutoMeasure::drawLandoltCStimulus(const TetriumApp::TickContextImGui& ctx, glm::ivec4 rgbo)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 windowPos = ImGui::GetWindowPos();

    // Get available space (accounting for UI elements at top)
    ImVec2 availSize = ImGui::GetContentRegionAvail();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();

    // Center in the available space below the UI
    ImVec2 center = ImVec2(cursorPos.x + availSize.x * 0.5f, cursorPos.y + availSize.y * 0.5f);

    // Fill only the available area below the UI with black (not the entire window)
    drawList->AddRectFilled(
        cursorPos,
        ImVec2(cursorPos.x + availSize.x, cursorPos.y + availSize.y),
        IM_COL32(0, 0, 0, 255)
    );

    // Calculate stimulus size to fit in available space
    // Use a reference texture size - typical Landolt C texture is around 1024x1024
    // STIMULUS_SIZE of 0.5 means half the reference size
    const float referenceTextureSize = 1024.0f;
    float stimulusPixelSize = referenceTextureSize * STIMULUS_SIZE;

    // Ensure stimulus fits in available space
    float maxRadius = std::min(availSize.x, availSize.y); // Use 40% of available space
    float stimulusRadius = std::min(stimulusPixelSize * 0.5f, maxRadius);

    // Annulus parameters
    float outerRadius = stimulusRadius * 0.8f;     // Leave margin
    float strokeWidth = outerRadius * 1.0f;        // Ring thickness
    float innerRadius = outerRadius - strokeWidth; // Inner radius for the annulus

    // Choose color (RGBA)
    ImU32 color = ctx.colorSpace == RGB ? IM_COL32(rgbo.x, rgbo.y, rgbo.w, 255)
                                        : IM_COL32(rgbo.z, rgbo.y, rgbo.w, 255);

    // Draw a complete annulus (full ring without gap)
    // Draw outer circle filled, then subtract inner circle
    drawList->AddCircleFilled(center, outerRadius, color, 0);
    drawList->AddCircleFilled(
        center, innerRadius, IM_COL32(0, 0, 0, 255), 0
    ); // Black to create hole

    // Draw measurement boxes at gap locations and center
    drawMeasurementBoxes(center, innerRadius, outerRadius);
}

void AppAutoMeasure::drawMeasurementBoxes(ImVec2 center, float innerRadius, float outerRadius)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Box size - make it bigger for easier measurement
    float boxSize = outerRadius * 0.4f; // Box is 40% of inner radius

    // Position boxes inside the circle, at the gap locations
    // Place them at a distance between inner and outer radius, closer to inner
    float boxDistance = (outerRadius * 0.75f); // Middle of the ring

    // Box color (white outline)
    ImU32 boxColor = IM_COL32(255, 255, 255, 255);
    float boxThickness = 2.0f;

    // Draw boxes at 4 gap locations (inside the circle)
    ImVec2 positions[4] = {
        ImVec2(center.x, center.y - boxDistance), // Up
        ImVec2(center.x, center.y + boxDistance), // Down
        ImVec2(center.x - boxDistance, center.y), // Left
        ImVec2(center.x + boxDistance, center.y)  // Right
    };

    for (int i = 0; i < 4; i++) {
        ImVec2 boxMin = ImVec2(positions[i].x - boxSize * 0.5f, positions[i].y - boxSize * 0.5f);
        ImVec2 boxMax = ImVec2(positions[i].x + boxSize * 0.5f, positions[i].y + boxSize * 0.5f);
        drawList->AddRect(boxMin, boxMax, boxColor, 0.0f, 0, boxThickness);
    }

    // Draw box at center
    ImVec2 centerBoxMin = ImVec2(center.x - boxSize * 0.5f, center.y - boxSize * 0.5f);
    ImVec2 centerBoxMax = ImVec2(center.x + boxSize * 0.5f, center.y + boxSize * 0.5f);
    drawList->AddRect(centerBoxMin, centerBoxMax, boxColor, 0.0f, 0, boxThickness);
}
} // namespace TetriumApp
