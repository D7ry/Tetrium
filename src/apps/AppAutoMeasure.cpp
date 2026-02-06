#include "AppAutoMeasure.h"
#include "Pathing.h"
#include "imgui.h"
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

        // Daily Validate section - separate from regular measurements
        if (IPR650->isConnected()) {
            ImGui::Separator();
            ImGui::Text("Daily Display Validation:");
            ImGui::SameLine();

            // Disable validation during regular measurements
            if (pr650States.measuring) {
                ImGui::BeginDisabled();
            }

            if (!pr650States.validating) {
                if (ImGui::Button("Daily Validate")) {
                    std::thread([this]() { runDailyValidation(); }).detach();
                    pr650States.validating = true;
                    pr650States.validationComplete = false;
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
                    ImGui::EndTooltip();
                }
            } else if (pr650States.validationComplete) {
                if (pr650States.measuring) {
                    ImGui::EndDisabled();
                }
                ImGui::Text("Validation Complete!");
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

    // Get the Y position after the menu (where we'll draw the stimulus)
    float menuEndY = ImGui::GetCursorScreenPos().y;
    float menuHeight = menuEndY - ImGui::GetWindowPos().y;

    // Use validation RGBO if validation is active, otherwise use measurement list RGBO
    glm::ivec4 displayRGBO
        = validationState.displayValidationColor ? validationState.currentRGBO : RGBO;

    drawLandoltCStimulus(ctx, displayRGBO, menuHeight);
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

    // Calculate stimulus size to fit in available space below menu
    // Use a reference texture size - typical Landolt C texture is around 1024x1024
    // STIMULUS_SIZE of 0.5 means half the reference size
    const float referenceTextureSize = 1024.0f;
    float stimulusPixelSize = referenceTextureSize * STIMULUS_SIZE;

    // Ensure stimulus fits in available space below menu
    float maxRadius = std::min(windowSize.x, windowSize.y); // Use 40% of available space
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

void AppAutoMeasure::runDailyValidation()
{
    std::string date = getCurrentDate();

    validationState.statusMessage = "Starting daily validation...";
    INFO("Starting daily validation for date: {}", date);

    // Step 1: Measure display primaries
    validationState.stepNumber = 1;
    validationState.currentStep = "Measuring Display Primaries";
    std::string primariesDir = TETRIUM_COLOR_PATH + "measurements/" + date + "/primaries/";
    if (!measureDisplayPrimaries(primariesDir)) {
        validationState.statusMessage = "ERROR: Failed to measure display primaries";
        ERROR("Failed to measure display primaries");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Step 2: Convert RYGB to RGBO
    validationState.stepNumber = 2;
    validationState.currentStep = "Converting RYGB to RGBO";
    if (!convertRYGBToRGBO(date)) {
        validationState.statusMessage = "ERROR: Failed to convert RYGB to RGBO";
        ERROR("Failed to convert RYGB to RGBO");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Step 3: Measure validation targets
    validationState.stepNumber = 3;
    validationState.currentStep = "Measuring Validation Targets";
    if (!measureValidationTargets(date)) {
        validationState.statusMessage = "ERROR: Failed to measure validation targets";
        ERROR("Failed to measure validation targets");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    // Step 4: Run validation
    validationState.stepNumber = 4;
    validationState.currentStep = "Running Validation";
    if (!runValidation(date)) {
        validationState.statusMessage = "ERROR: Validation failed";
        ERROR("Validation failed");
        pr650States.validating = false;
        validationState.displayValidationColor = false;
        return;
    }

    validationState.statusMessage = "Validation complete! Check measurements/" + date + "/";
    INFO("Daily validation complete for {}", date);
    pr650States.validationComplete = true;

    // Clear validation display state
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
        measureAndSaveSpectrum(primary, primariesDir);
    }

    INFO("Display primaries measured successfully");
    return true;
}

bool AppAutoMeasure::convertRYGBToRGBO(const std::string& date)
{
    INFO("Converting RYGB to RGBO for date: {}", date);

    // Build Python command
    std::string cmd = "conda run -n tetrium python " + TETRIUM_COLOR_PATH
                      + "scripts/validation/convert_rygb_to_rgbo.py " + "--metamers "
                      + TETRIUM_COLOR_PATH + "config/display_validation_metamers.json "
                      + "--primaries " + TETRIUM_COLOR_PATH + "measurements/" + date
                      + "/primaries/ " + "--output " + TETRIUM_COLOR_PATH + "measurements/" + date
                      + "/display_targets.csv";

    INFO("Running command: {}", cmd);
    int result = system(cmd.c_str());

    if (result != 0) {
        ERROR("RYGB to RGBO conversion failed with code: {}", result);
        return false;
    }

    INFO("RYGB to RGBO conversion complete");
    return true;
}

bool AppAutoMeasure::measureValidationTargets(const std::string& date)
{
    INFO("Measuring validation targets for date: {}", date);

    std::string targetsFile = TETRIUM_COLOR_PATH + "measurements/" + date + "/display_targets.csv";
    std::string measurementsDir
        = TETRIUM_COLOR_PATH + "measurements/" + date + "/validation_measurements/";

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

bool AppAutoMeasure::runValidation(const std::string& date)
{
    INFO("Running validation for date: {}", date);

    // Build Python command
    std::string cmd = "conda run -n tetrium python " + TETRIUM_COLOR_PATH
                      + "scripts/validation/validate_display_measurements.py " + "--metamers "
                      + TETRIUM_COLOR_PATH + "config/display_validation_metamers.json "
                      + "--primaries " + TETRIUM_COLOR_PATH + "measurements/" + date
                      + "/primaries/ " + "--measurements " + TETRIUM_COLOR_PATH + "measurements/"
                      + date + "/validation_measurements/ " + "--output " + TETRIUM_COLOR_PATH
                      + "measurements/" + date + "/validation_report.json " + "--plots "
                      + TETRIUM_COLOR_PATH + "measurements/" + date + "/validation_plots/";

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
    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> fields;

        // Parse CSV line
        while (std::getline(iss, token, ',')) {
            fields.push_back(token);
        }

        // Fields are: observer_index,genotype,q_cone_index,pair_index,metamer_index,R,G,B,O,...
        if (fields.size() >= 9) {
            int r = std::stoi(fields[5]);
            int g = std::stoi(fields[6]);
            int b = std::stoi(fields[7]);
            int o = std::stoi(fields[8]);
            uniqueRGBO.insert({r, g, b, o});
        }
    }

    file.close();

    // Convert set to vector
    std::vector<glm::ivec4> result;
    for (const auto& [r, g, b, o] : uniqueRGBO) {
        result.emplace_back(r, g, b, o);
    }

    INFO("Parsed {} unique RGBO targets", result.size());
    return result;
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

    // Determine filename format based on whether this is a primary or validation measurement
    std::string filename;
    bool isPrimary = (rgbo.x == 255 && rgbo.y == 0 && rgbo.z == 0 && rgbo.w == 0)
                     || (rgbo.x == 0 && rgbo.y == 255 && rgbo.z == 0 && rgbo.w == 0)
                     || (rgbo.x == 0 && rgbo.y == 0 && rgbo.z == 255 && rgbo.w == 0)
                     || (rgbo.x == 0 && rgbo.y == 0 && rgbo.z == 0 && rgbo.w == 255);

    if (isPrimary) {
        // Use underscore format for primaries: <R>_<G>_<B>_<O>.csv
        filename = outputDir + "/" + std::to_string(rgbo.x) + "_" + std::to_string(rgbo.y) + "_"
                   + std::to_string(rgbo.z) + "_" + std::to_string(rgbo.w) + ".csv";
    } else {
        // Use lowercase format for validation measurements: r<R>g<G>b<B>o<O>.csv
        filename = outputDir + "/r" + std::to_string(rgbo.x) + "g" + std::to_string(rgbo.y) + "b"
                   + std::to_string(rgbo.z) + "o" + std::to_string(rgbo.w) + ".csv";
    }

    // Format spectrum data (two columns: wavelength, power - no header)
    std::ostringstream resultStr;
    constexpr auto max_precision{std::numeric_limits<double>::digits10 + 1};
    resultStr << std::scientific << std::setprecision(max_precision);

    for (size_t i = 0; i < result.power.size(); i++) {
        double wavelength = result.wavelength[i];
        double power = result.power[i];
        resultStr << wavelength << ',' << power << '\n';
    }

    // Write to file
    std::ofstream file(filename);
    if (!file.is_open()) {
        ERROR("Failed to open file for writing: {}", filename);
        return;
    }

    file << resultStr.str();
    file.close();

    INFO("Saved spectrum to: {}", filename);

    // Reset measurement state
    IPR650->MeasureResult.ready = false;
}

} // namespace TetriumApp
