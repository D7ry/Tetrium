#include "AppAutoMeasure.h"
#include "Pathing.h"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <thread>
#include <tuple>

#include "app_components/PR650.h"

namespace
{

static struct
{
    std::string rgboValuesString;
    std::string measuringString;
} measureContext;

struct SpectrumSnapshot
{
    std::vector<double> wavelength;
    std::vector<double> power;
    double luminance = 0.0;
};

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

SpectrumSnapshot snapshotSpectrum(const PR650::SpectrumMeasure& measurement)
{
    SpectrumSnapshot snapshot;
    snapshot.wavelength = measurement.wavelength;
    snapshot.power = measurement.power;
    snapshot.luminance = measurement.luminance;
    return snapshot;
}

SpectrumSnapshot meanSpectrum(const std::vector<SpectrumSnapshot>& measurements)
{
    SpectrumSnapshot aggregate;
    if (measurements.empty()) {
        return aggregate;
    }

    const size_t n = measurements.front().power.size();
    aggregate.wavelength = measurements.front().wavelength;
    aggregate.power.resize(n, 0.0);

    for (const auto& measurement : measurements) {
        for (size_t i = 0; i < n; ++i) {
            aggregate.power[i] += measurement.power[i];
        }
        aggregate.luminance += measurement.luminance;
    }

    const double count = static_cast<double>(measurements.size());
    for (size_t i = 0; i < n; ++i) {
        aggregate.power[i] /= count;
    }
    aggregate.luminance /= count;
    return aggregate;
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
        // Draw stimulus first (behind the menu) using window draw list
        // ImGui renders draw list commands before widgets, so this will be behind
        if (!measurementData.currentPrimaries.empty() || validationState.displayValidationColor) {
            glm::ivec4 displayRGBO = glm::ivec4(0, 0, 0, 0);
            if (!measurementData.currentPrimaries.empty()) {
                glm::ivec4 RGBO
                    = measurementData.currentPrimaries[measurementData.currPrimaryIndex];
                displayRGBO
                    = validationState.displayValidationColor ? validationState.currentRGBO : RGBO;
            } else if (validationState.displayValidationColor) {
                displayRGBO = validationState.currentRGBO;
            }

            // Use window draw list - commands here render before widgets
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 windowSize = ImGui::GetWindowSize();
            ImVec2 windowPos = ImGui::GetWindowPos();

            // Estimate menu height (will be ~300-400px depending on content)
            float estimatedMenuHeight = 350.0f;
            ImVec2 center = ImVec2(
                windowPos.x + windowSize.x * 0.5f,
                windowPos.y + (windowSize.y + estimatedMenuHeight) * 0.5f
            );

            // Fill background area below menu with black
            drawList->AddRectFilled(
                ImVec2(windowPos.x, windowPos.y + estimatedMenuHeight),
                ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
                IM_COL32(0, 0, 0, 255)
            );

            // Calculate stimulus size
            float baseRadius = std::min(windowSize.x, windowSize.y - estimatedMenuHeight) * 0.4f;
            float stimulusRadius = baseRadius * stimulusSize;

            // Annulus parameters
            float outerRadius = stimulusRadius * 0.8f;
            float strokeWidth = outerRadius * 1.0f;
            float innerRadius = outerRadius - strokeWidth;

            // Choose color
            ImU32 color = ctx.colorSpace == RGB
                              ? IM_COL32(displayRGBO.x, displayRGBO.y, displayRGBO.w, 255)
                              : IM_COL32(displayRGBO.z, displayRGBO.y, displayRGBO.w, 255);

            // Draw stimulus circle
            drawList->AddCircleFilled(center, outerRadius, color, 0);
            drawList->AddCircleFilled(center, innerRadius, IM_COL32(0, 0, 0, 255), 0);
        }

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

        ImGui::Separator();
        ImGui::Checkbox(
            "Debug: skip PR650 measurements", &validationState.debugSkipPR650Measurements
        );
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("When enabled, Daily Validate will:");
            ImGui::Text("- Skip all PR650 measurements");
            ImGui::Text("- Assume primaries and validation measurements already exist");
            ImGui::Text("- Still run the Python conversion and validation scripts");
            ImGui::EndTooltip();
        }

        // Daily Validate section - separate from regular measurements
        if (IPR650->isConnected() || validationState.debugSkipPR650Measurements) {
            ImGui::Separator();

            // Config mode selection
            ImGui::Text("Validation Config:");
            ImGui::SameLine();
            if (pr650States.validating) {
                ImGui::BeginDisabled();
            }
            const char* configLabels[] = {
                "Center only", "5x5 cubemap", "Midpoint", "Midpoint contrast"
            };
            int configModeInt = static_cast<int>(validationState.configMode);
            ImGui::SetNextItemWidth(160);
            if (ImGui::Combo("##ValidationConfig", &configModeInt, configLabels, IM_ARRAYSIZE(configLabels))) {
                validationState.configMode = static_cast<ValidationConfigMode>(configModeInt);
            }
            if (pr650States.validating) {
                ImGui::EndDisabled();
            }

            ImGui::Text("Primary Samples:");
            ImGui::SameLine();
            if (pr650States.validating) {
                ImGui::BeginDisabled();
            }
            ImGui::SetNextItemWidth(100);
            if (ImGui::InputInt("##PrimarySamples", &validationState.primaryRepeatSamples)) {
                validationState.primaryRepeatSamples
                    = std::clamp(validationState.primaryRepeatSamples, 1, 20);
            }
            if (validationState.primaryRepeatSamples < 1 || validationState.primaryRepeatSamples > 20) {
                validationState.primaryRepeatSamples
                    = std::clamp(validationState.primaryRepeatSamples, 1, 20);
            }
            if (pr650States.validating) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Number of PR650 repeats per RGBO primary.");
                ImGui::Text("The app saves each repeat and a mean aggregate.");
                ImGui::EndTooltip();
            }

            ImGui::Text("Daily Display Validation:");
            ImGui::SameLine();

            // Disable validation during regular measurements
            if (pr650States.measuring) {
                ImGui::BeginDisabled();
            }

            if (!pr650States.validating) {
                if (ImGui::Button("Daily Validate")) {
                    bool debugSkip = validationState.debugSkipPR650Measurements;
                    std::thread([this, debugSkip]() { runDailyValidation(debugSkip); }).detach();
                    pr650States.validating = true;
                    pr650States.validationComplete = false;
                }
                ImGui::SameLine();
                if (!IPR650->isConnected()) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Measure Primaries Only")) {
                    std::thread([this]() { runPrimariesOnlyMeasurement(); }).detach();
                    pr650States.validating = true;
                    pr650States.validationComplete = false;
                }
                if (!IPR650->isConnected()) {
                    ImGui::EndDisabled();
                }
                if (pr650States.measuring) {
                    ImGui::EndDisabled();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Runs automated display validation:");
                    ImGui::Text("1. Measures display primaries (RGBO)");
                    ImGui::Text("2. Converts RYGB metamers to display values");
                    ImGui::Text("3. Measures validation targets");
                    ImGui::Text("4. Validates metamer accuracy");
                    ImGui::Separator();
                    ImGui::Text("Measure Primaries Only records just RGBO primaries");
                    ImGui::Text("to measurements/<date>/primaries/.");
                    ImGui::EndTooltip();
                }
            } else if (pr650States.validationComplete) {
                if (pr650States.measuring) {
                    ImGui::EndDisabled();
                }
                ImGui::Text("%s", validationState.completeLabel.c_str());
                ImGui::SameLine();
                if (ImGui::Button("OK##validation")) {
                    pr650States.validating = false;
                    pr650States.validationComplete = false;
                    validationState.displayValidationColor = false;
                }
            } else {
                if (pr650States.measuring) {
                    ImGui::EndDisabled();
                }
                std::string statusText = "Step " + std::to_string(validationState.stepNumber) + "/"
                                         + std::to_string(validationState.totalSteps) + ": "
                                         + validationState.currentStep;
                ImGui::Text("%s", statusText.c_str());
            }

            // Show validation status message if available
            if (!validationState.statusMessage.empty()) {
                ImGui::TextColored(
                    ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", validationState.statusMessage.c_str()
                );
            }

            // Show warning if measurement is blocking validation
            if (pr650States.measuring) {
                ImGui::TextColored(
                    ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Validation disabled during list measurements"
                );
            }
        }

        ImGui::Separator();
        ImGui::Text("Custom Measurement Lists:");
    }

    if (measurementData.currentPrimaries.empty()) {
        ImGui::Text("No measurement data loaded. Please select a valid .txt file.");
        ImGui::End();
        ImGui::PopStyleColor(2);
        return;
    }

    glm::ivec4 RGBO = measurementData.currentPrimaries[measurementData.currPrimaryIndex];
    if (IPR650->isConnected()) {
        // Disable measurement controls during validation
        if (pr650States.validating) {
            ImGui::BeginDisabled();
        }

        if (!pr650States.measuring) {
            // if not measuring, prompt user to start measuring
            if (ImGui::Button("Measure List")) {
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

                // Log measurement details
                for (int i = 0; i < result.power.size(); i++) {
                    INFO("{} : {} {}", i, result.power[i], result.wavelength[i]);
                }

                // write results to file in measurement-specific directory
                std::string baseFileName = measurementData.selectedFile;
                // Remove .txt extension
                if (baseFileName.size() > 4
                    && baseFileName.substr(baseFileName.size() - 4) == ".txt") {
                    baseFileName = baseFileName.substr(0, baseFileName.size() - 4);
                }

                std::string measurementDir = getTodayMeasurementsPath() + "/" + baseFileName;

                // Use the shared saveSpectrumData function
                saveSpectrumData(RGBO, measurementDir, result);

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

    // Use validation RGBO if validation is active, otherwise use measurement list RGBO
    glm::ivec4 displayRGBO
        = validationState.displayValidationColor ? validationState.currentRGBO : RGBO;

    measureContext.rgboValuesString = "RGBO: ";
    for (int i = 0; i < 4; i++) {
        measureContext.rgboValuesString += std::to_string(displayRGBO[i]);
        measureContext.rgboValuesString += ' ';
    }
    ImGui::Text("%s", measureContext.rgboValuesString.c_str());

    // Stimulus size slider (multiplies the radius)
    ImGui::SliderFloat("Stimulus Size", &stimulusSize, 0.1f, 3.0f, "%.2fx");

    // End disabled state if validation is running
    if (pr650States.validating) {
        ImGui::EndDisabled();
        ImGui::TextColored(
            ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Measurement controls disabled during validation"
        );
    }

    // for (int i = 0; i < 4; i++) {
    //     const char* label = labels.at(i);
    //     ImGui::SliderInt(label, &RGBO[i], 0, 255);
    // }

    // Menu is already drawn, stimulus was drawn first (behind menu)
    // No need to draw stimulus again here
    ImGui::End();
    ImGui::PopStyleColor(2); // Pop both WindowBg and Text colors
}

void AppAutoMeasure::drawColorBlock(const TetriumApp::TickContextImGui& ctx, glm::ivec4 rgbo)
{
    // Legacy function - now using drawLandoltCStimulus instead
    // Get menu height (0 if called from elsewhere)
    float menuHeight = ImGui::GetCursorScreenPos().y - ImGui::GetWindowPos().y;
    drawLandoltCStimulus(ctx, rgbo, menuHeight);
}

void AppAutoMeasure::drawLandoltCStimulus(
    const TetriumApp::TickContextImGui& ctx,
    glm::ivec4 rgbo,
    float menuHeight
)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 windowPos = ImGui::GetWindowPos();
    // Calculate available space below the menud    float availableHeight = windowSize.y;

    // Center horizontally, but vertically center in the space below the menu
    ImVec2 center = ImVec2(windowPos.x + windowSize.x * 0.5f, (windowSize.y - menuHeight) * 0.5f);

    // Fill only the area below the menu with black
    drawList->AddRectFilled(
        ImVec2(windowPos.x, windowPos.y + menuHeight),
        ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
        IM_COL32(0, 0, 0, 255)
    );

    // Calculate stimulus size - use available space and multiply by stimulusSize slider
    // Base radius uses 40% of available space
    float baseRadius = std::min(windowSize.x, windowSize.y - menuHeight) * 0.4f;
    // Apply stimulusSize multiplier directly to the radius
    float stimulusRadius = baseRadius * stimulusSize;

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

    // Measurement boxes removed per user request
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

void AppAutoMeasure::runDailyValidation(bool debugSkipPR650Measurements)
{
    std::string date = getCurrentDate();

    validationState.totalSteps = 4;
    validationState.completeLabel = "Validation Complete!";
    validationState.statusMessage = "Starting daily validation...";
    INFO("Starting daily validation for date: {}", date);

    // Step 1: Measure display primaries
    validationState.stepNumber = 1;
    validationState.currentStep = "Measuring Display Primaries";
    std::string primariesDir = TETRIUM_COLOR_PATH + "measurements/" + date + "/primaries/";
    if (!debugSkipPR650Measurements && !measureDisplayPrimaries(primariesDir)) {
        validationState.statusMessage = "ERROR: Failed to measure display primaries";
        ERROR("Failed to measure display primaries");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Generate timestamp for validation folder
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "validation_%Y-%m-%d_%H-%M-%S");
    std::string validationFolderName = oss.str();
    std::string validationFolder
        = TETRIUM_COLOR_PATH + "measurements/" + date + "/" + validationFolderName + "/";

    // Create validation folder
    std::filesystem::create_directories(validationFolder);
    INFO("Created validation folder: {}", validationFolder);

    // Copy primaries to validation folder
    std::string validationPrimariesDir = validationFolder + "primaries/";
    if (!copyPrimariesToValidationFolder(primariesDir, validationPrimariesDir)) {
        validationState.statusMessage = "ERROR: Failed to copy primaries to validation folder";
        ERROR("Failed to copy primaries to validation folder");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Copy config to validation folder (select based on configMode)
    std::string configFileName;
    switch (validationState.configMode) {
    case kValidationConfigCenter:
        configFileName = "display_validation_metamers_center.json";
        break;
    case kValidationConfigMidpoint:
        configFileName = "display_validation_metamers_midpoint.json";
        break;
    case kValidationConfigMidpointContrast:
        configFileName = "display_validation_metamers_midpoint_contrast.json";
        break;
    default:
        configFileName = "display_validation_metamers.json";
        break;
    }
    std::string configSourcePath = TETRIUM_COLOR_PATH + "config/" + configFileName;
    std::string validationConfigPath = validationFolder + "display_validation_metamers.json";
    if (!copyConfigToValidationFolder(configSourcePath, validationConfigPath)) {
        validationState.statusMessage = "ERROR: Failed to copy config to validation folder";
        ERROR("Failed to copy config to validation folder");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Step 2: Convert RYGB to RGBO
    validationState.stepNumber = 2;
    validationState.currentStep = "Converting RYGB to RGBO";
    if (!convertRYGBToRGBO(date, validationFolder)) {
        validationState.statusMessage = "ERROR: Failed to convert RYGB to RGBO";
        ERROR("Failed to convert RYGB to RGBO");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Step 3: Measure validation targets
    validationState.stepNumber = 3;
    validationState.currentStep = "Measuring Validation Targets";
    if (!debugSkipPR650Measurements && !measureValidationTargets(date, validationFolder)) {
        validationState.statusMessage = "ERROR: Failed to measure validation targets";
        ERROR("Failed to measure validation targets");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Step 4: Run validation
    validationState.stepNumber = 4;
    validationState.currentStep = "Running Validation";
    if (!runValidation(date, validationFolder)) {
        validationState.statusMessage = "ERROR: Validation failed";
        ERROR("Validation failed");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    validationState.statusMessage
        = "Validation complete! Check measurements/" + date + "/" + validationFolderName + "/";
    INFO("Daily validation complete for {} in folder {}", date, validationFolderName);
    pr650States.validationComplete = true;

    // Clear validation display state
    validationState.displayValidationColor = false;
}

void AppAutoMeasure::runPrimariesOnlyMeasurement()
{
    if (!IPR650->isConnected()) {
        validationState.statusMessage = "ERROR: PR650 not connected";
        ERROR("Cannot measure primaries: PR650 not connected");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    std::string date = getCurrentDate();
    std::string primariesDir = TETRIUM_COLOR_PATH + "measurements/" + date + "/primaries/";

    validationState.totalSteps = 1;
    validationState.stepNumber = 1;
    validationState.currentStep = "Measuring Display Primaries";
    validationState.completeLabel = "Primaries Measurement Complete!";
    validationState.statusMessage = "Measuring display primaries only...";
    INFO("Starting primaries-only measurement for date: {}", date);

    if (!measureDisplayPrimaries(primariesDir)) {
        validationState.statusMessage = "ERROR: Failed to measure display primaries";
        ERROR("Failed to measure display primaries");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    validationState.statusMessage = "Primaries saved to measurements/" + date + "/primaries/";
    INFO("Primaries-only measurement complete for {}", date);
    pr650States.validationComplete = true;
    validationState.displayValidationColor = false;
}

bool AppAutoMeasure::measureDisplayPrimaries(const std::string& primariesDir)
{
    INFO("Measuring display primaries to {}", primariesDir);

    // Create directory
    std::filesystem::create_directories(primariesDir);

    // Measure each primary: R, G, B, O
    std::vector<glm::ivec4> primaries = {
        glm::ivec4(255, 0, 0, 0), // R
        glm::ivec4(0, 255, 0, 0), // G
        glm::ivec4(0, 0, 255, 0), // B
        glm::ivec4(0, 0, 0, 255)  // O
    };

    for (const auto& primary : primaries) {
        measureAndSavePrimarySpectrumRobust(primary, primariesDir);
    }

    INFO("Display primaries measured successfully");
    return true;
}

bool AppAutoMeasure::copyPrimariesToValidationFolder(
    const std::string& primariesSourceDir,
    const std::string& validationPrimariesDir
)
{
    INFO("Copying primaries from {} to {}", primariesSourceDir, validationPrimariesDir);

    // Create destination directory
    std::filesystem::create_directories(validationPrimariesDir);

    // Copy all CSV files from source to destination
    try {
        if (!std::filesystem::exists(primariesSourceDir)) {
            ERROR("Source primaries directory does not exist: {}", primariesSourceDir);
            return false;
        }

        int copiedCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(primariesSourceDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".csv") {
                std::filesystem::path destPath
                    = validationPrimariesDir + entry.path().filename().string();
                std::filesystem::copy_file(
                    entry.path(), destPath, std::filesystem::copy_options::overwrite_existing
                );
                copiedCount++;
            }
        }

        INFO("Copied {} primary CSV files to validation folder", copiedCount);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        ERROR("Failed to copy primaries: {}", e.what());
        return false;
    }
}

bool AppAutoMeasure::copyConfigToValidationFolder(
    const std::string& configSourcePath,
    const std::string& validationConfigPath
)
{
    INFO("Copying config from {} to {}", configSourcePath, validationConfigPath);

    try {
        if (!std::filesystem::exists(configSourcePath)) {
            ERROR("Source config file does not exist: {}", configSourcePath);
            return false;
        }

        // Create parent directory if needed
        std::filesystem::path destPath(validationConfigPath);
        std::filesystem::create_directories(destPath.parent_path());

        std::filesystem::copy_file(
            configSourcePath,
            validationConfigPath,
            std::filesystem::copy_options::overwrite_existing
        );
        INFO("Copied config file to validation folder");
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        ERROR("Failed to copy config: {}", e.what());
        return false;
    }
}

bool AppAutoMeasure::convertRYGBToRGBO(const std::string& date, const std::string& validationFolder)
{
    INFO("Converting RYGB to RGBO for date: {} in validation folder: {}", date, validationFolder);

    // Use config and primaries from validation folder
    std::string configPath = validationFolder + "display_validation_metamers.json";
    std::string primariesPath = validationFolder + "primaries/";
    std::string outputPath = validationFolder + "display_targets.csv";

    // Build Python command
    std::string cmd = "conda run -n tetrium python " + TETRIUM_COLOR_PATH
                      + "scripts/validation/convert_bgyr_to_bgor.py " + "--metamers " + configPath
                      + " --primaries " + primariesPath + " --output " + outputPath;

    INFO("Running command: {}", cmd);
    int result = system(cmd.c_str());

    if (result != 0) {
        ERROR("RYGB to RGBO conversion failed with code: {}", result);
        return false;
    }

    INFO("RYGB to RGBO conversion complete");
    return true;
}

bool AppAutoMeasure::measureValidationTargets(
    const std::string& date,
    const std::string& validationFolder
)
{
    INFO(
        "Measuring validation targets for date: {} in validation folder: {}", date, validationFolder
    );

    std::string targetsFile = validationFolder + "display_targets.csv";
    std::string measurementsDir = validationFolder + "validation_measurements/";

    // Create measurements directory
    std::filesystem::create_directories(measurementsDir);

    // Parse RGBO targets
    auto targets = parseRGBOTargets(targetsFile);

    if (targets.empty()) {
        ERROR("No targets found in {}", targetsFile);
        return false;
    }

    INFO("Found {} unique RGBO targets to measure", targets.size());

    // Measure each target
    for (const auto& rgbo : targets) {
        measureAndSaveSpectrum(rgbo, measurementsDir);
    }

    INFO("Validation targets measured successfully");
    return true;
}

bool AppAutoMeasure::runValidation(const std::string& date, const std::string& validationFolder)
{
    INFO("Running validation for date: {} in validation folder: {}", date, validationFolder);

    // Use config and primaries from validation folder
    std::string configPath = validationFolder + "display_validation_metamers.json";
    std::string primariesPath = validationFolder + "primaries/";
    std::string plotsPath = validationFolder + "validation_plots/";
    std::string measurementsPath = validationFolder + "validation_measurements/";

    // Build Python command
    std::string cmd = "conda run -n tetrium python " + TETRIUM_COLOR_PATH
                      + "scripts/validation/validate_display_measurements.py " + "--metamers "
                      + configPath + " --primaries " + primariesPath + " --plots " + plotsPath
                      + " ";
    if (!validationState.debugSkipPR650Measurements) {
        cmd += "--measurements " + measurementsPath + " ";
    }
    INFO("Running command: {}", cmd);
    int result = system(cmd.c_str());

    if (result != 0) {
        ERROR("Validation failed with code: {}", result);
        return false;
    }

    INFO("Validation complete");
    return true;
}

std::vector<glm::ivec4> AppAutoMeasure::parseRGBOTargets(const std::string& csvPath)
{
    INFO("Parsing RGBO targets from {}", csvPath);

    std::set<std::tuple<int, int, int, int>> uniqueRGBO;
    std::ifstream file(csvPath);

    if (!file.is_open()) {
        ERROR("Failed to open targets file: {}", csvPath);
        return {};
    }

    std::string line;
    // Skip header line (R,G,B,O)
    std::getline(file, line);

    // Parse simplified CSV format: R,G,B,O
    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Simple CSV parsing: split by comma
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;

        while (std::getline(ss, field, ',')) {
            // Trim whitespace
            field.erase(0, field.find_first_not_of(" \t"));
            field.erase(field.find_last_not_of(" \t") + 1);
            fields.push_back(field);
        }

        // Expect exactly 4 fields: R, G, B, O
        if (fields.size() == 4) {
            try {
                int r = std::stoi(fields[0]); // R
                int g = std::stoi(fields[1]); // G
                int b = std::stoi(fields[2]); // B
                int o = std::stoi(fields[3]); // O

                // Validate they're in valid range
                if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255 && o >= 0
                    && o <= 255) {
                    uniqueRGBO.insert({r, g, b, o});
                } else {
                    WARN("RGBO values out of range [0-255]: R={}, G={}, B={}, O={}", r, g, b, o);
                }
            } catch (const std::exception& e) {
                WARN("Failed to parse RGBO values from line (error: {}): {}", e.what(), line);
            }
        } else {
            WARN("Expected 4 fields (R,G,B,O), got {} fields in line: {}", fields.size(), line);
        }
    }

    file.close();

    // Convert set to vector
    std::vector<glm::ivec4> result;
    result.reserve(uniqueRGBO.size());
    for (const auto& rgbo : uniqueRGBO) {
        result.push_back(
            glm::ivec4(std::get<0>(rgbo), std::get<1>(rgbo), std::get<2>(rgbo), std::get<3>(rgbo))
        );
    }

    INFO("Parsed {} unique RGBO targets", result.size());
    return result;
    return result;
}

void AppAutoMeasure::saveSpectrumData(
    glm::ivec4 rgbo,
    const std::string& outputDir,
    const PR650::SpectrumMeasure& result,
    const std::string& filenameSuffix
)
{
    // Ensure output directory exists
    try {
        std::filesystem::create_directories(outputDir);
        INFO("Output directory: {}", std::filesystem::absolute(outputDir).string());
    } catch (const std::exception& e) {
        ERROR("Failed to create directory {}: {}", outputDir, e.what());
        return;
    }

    // Generate filename with timestamp: r<R>g<G>b<B>o<O>_<timestamp>.csv
    // This format is used by both primaries and validation measurements
    std::string timestamp = getTimestampString();
    std::string filename = outputDir + "/r" + std::to_string(rgbo.x) + "g" + std::to_string(rgbo.y)
                           + "b" + std::to_string(rgbo.z) + "o" + std::to_string(rgbo.w) + "_"
                           + timestamp + filenameSuffix + ".csv";

    // Format spectrum data (three columns: wavelength, power, luminance - with header)
    std::ostringstream resultStr;
    constexpr auto max_precision{std::numeric_limits<double>::digits10 + 1};
    resultStr << std::scientific << std::setprecision(max_precision);

    // Write header
    resultStr << "wavelength, power, luminance\n";

    for (size_t i = 0; i < result.power.size(); i++) {
        double wavelength = result.wavelength[i];
        double power = result.power[i];
        resultStr << wavelength << ',' << power << ',' << result.luminance << '\n';
    }

    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
        ERROR("Failed to open file for writing: {}", filename);
        ERROR("Absolute path would be: {}", std::filesystem::absolute(filename).string());
        return;
    }

    file << resultStr.str();
    file.close();

    INFO("Saved spectrum to: {}", std::filesystem::absolute(filename).string());
}

void AppAutoMeasure::measureAndSavePrimarySpectrumRobust(glm::ivec4 rgbo, const std::string& outputDir)
{
    const int primaryRepeats = std::clamp(validationState.primaryRepeatSamples, 1, 20);
    constexpr int kInitialSettleMs = 2000;
    constexpr int kRepeatSettleMs = 250;

    INFO(
        "Measuring robust primary spectrum for RGBO: ({}, {}, {}, {}) with {} repeats",
        rgbo.x,
        rgbo.y,
        rgbo.z,
        rgbo.w,
        primaryRepeats
    );

    validationState.currentRGBO = rgbo;
    validationState.displayValidationColor = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(kInitialSettleMs));

    std::vector<SpectrumSnapshot> validMeasurements;
    validMeasurements.reserve(primaryRepeats);

    for (int repeat = 0; repeat < primaryRepeats; ++repeat) {
        if (repeat > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kRepeatSettleMs));
        }

        validationState.statusMessage = "Measuring primary repeat " + std::to_string(repeat + 1)
                                        + "/" + std::to_string(primaryRepeats);

        IPR650->StartMeasuring();
        while (!IPR650->MeasureResult.ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        const PR650::SpectrumMeasure& result = IPR650->MeasureResult;

        saveSpectrumData(rgbo, outputDir, result, "_repeat" + std::to_string(repeat + 1));
        SpectrumSnapshot resultSnapshot = snapshotSpectrum(result);
        IPR650->MeasureResult.ready = false;

        if (resultSnapshot.wavelength.empty() || resultSnapshot.power.empty()
            || resultSnapshot.wavelength.size() != resultSnapshot.power.size()) {
            WARN(
                "Skipping invalid primary repeat {} for RGBO ({}, {}, {}, {}): {} wavelengths, {} powers",
                repeat + 1,
                rgbo.x,
                rgbo.y,
                rgbo.z,
                rgbo.w,
                resultSnapshot.wavelength.size(),
                resultSnapshot.power.size()
            );
            continue;
        }

        if (!validMeasurements.empty()
            && resultSnapshot.power.size() != validMeasurements.front().power.size()) {
            WARN(
                "Skipping primary repeat {} for RGBO ({}, {}, {}, {}): length {} does not match {}",
                repeat + 1,
                rgbo.x,
                rgbo.y,
                rgbo.z,
                rgbo.w,
                resultSnapshot.power.size(),
                validMeasurements.front().power.size()
            );
            continue;
        }

        validMeasurements.push_back(resultSnapshot);
    }

    if (validMeasurements.empty()) {
        ERROR(
            "No valid primary repeats for RGBO ({}, {}, {}, {}); aggregate not saved",
            rgbo.x,
            rgbo.y,
            rgbo.z,
            rgbo.w
        );
        return;
    }

    SpectrumSnapshot aggregateSnapshot = meanSpectrum(validMeasurements);
    PR650::SpectrumMeasure aggregate;
    aggregate.wavelength = aggregateSnapshot.wavelength;
    aggregate.power = aggregateSnapshot.power;
    aggregate.luminance = aggregateSnapshot.luminance;
    aggregate.ready = true;
    saveSpectrumData(rgbo, outputDir, aggregate, "_zz_mean");
    INFO(
        "Saved mean primary aggregate for RGBO ({}, {}, {}, {}) from {} valid repeats",
        rgbo.x,
        rgbo.y,
        rgbo.z,
        rgbo.w,
        validMeasurements.size()
    );
}

void AppAutoMeasure::measureAndSaveSpectrum(glm::ivec4 rgbo, const std::string& outputDir)
{
    INFO("Measuring spectrum for RGBO: ({}, {}, {}, {})", rgbo.x, rgbo.y, rgbo.z, rgbo.w);

    // Display the color on screen
    validationState.currentRGBO = rgbo;
    validationState.displayValidationColor = true;

    // Wait for display to update and stabilize (increased from 500ms to 1000ms)
    // This ensures the ImGui rendering loop has time to display the new color
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Start measurement
    IPR650->StartMeasuring();

    // Wait for measurement to complete
    while (!IPR650->MeasureResult.ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Get measurement results
    auto& result = IPR650->MeasureResult;

    // Save the spectrum data
    saveSpectrumData(rgbo, outputDir, result);

    // Reset measurement state
    IPR650->MeasureResult.ready = false;
}

} // namespace TetriumApp
