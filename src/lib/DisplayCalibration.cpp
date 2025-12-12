#include "DisplayCalibration.h"
#include "components/Logging.h"
#include "structs/ColorSpace.h"

#include <Python.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>

namespace TetriumApp
{

// Cache file format constants
static const char CACHE_MAGIC[4] = {'R', 'Y', 'G', 'B'};
static const uint32_t CACHE_VERSION = 1;

std::string DisplayCalibration::GetTodayDate()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&time);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d");
    return oss.str();
}

std::string DisplayCalibration::GetTodayPrimariesPath()
{
    std::string date = GetTodayDate();
    return "./data/primaries/" + date + ".csv";
}

std::string DisplayCalibration::GetCachePath()
{
    std::string date = GetTodayDate();
    return "./data/calibration/matrix_cache_" + date + ".bin";
}

bool DisplayCalibration::LoadCachedMatrices(
    const std::string& cachePath,
    const std::string& primariesPath,
    glm::mat4x4& rgbMatrix,
    glm::mat4x4& ocvMatrix)
{
    std::ifstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        DEBUG("Matrix cache file not found: {}", cachePath);
        return false;
    }

    // Read and validate magic bytes
    char magic[4];
    file.read(magic, 4);
    if (std::memcmp(magic, CACHE_MAGIC, 4) != 0) {
        ERROR("Invalid cache file magic bytes: {}", cachePath);
        return false;
    }

    // Read and validate version
    uint32_t version;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != CACHE_VERSION) {
        ERROR("Cache file version mismatch: {} (expected {})", version, CACHE_VERSION);
        return false;
    }

    // Read date string
    uint32_t dateLength;
    file.read(reinterpret_cast<char*>(&dateLength), sizeof(dateLength));
    std::string cachedDate(dateLength, '\0');
    file.read(&cachedDate[0], dateLength);

    // Validate date matches today
    std::string today = GetTodayDate();
    if (cachedDate != today) {
        DEBUG("Cache date mismatch: {} (expected {})", cachedDate, today);
        return false;
    }

    // Read RGB matrix
    file.read(reinterpret_cast<char*>(&rgbMatrix), sizeof(glm::mat4x4));

    // Read OCV matrix
    file.read(reinterpret_cast<char*>(&ocvMatrix), sizeof(glm::mat4x4));

    // Read and validate primaries path
    uint32_t pathLength;
    file.read(reinterpret_cast<char*>(&pathLength), sizeof(pathLength));
    std::string cachedPath(pathLength, '\0');
    file.read(&cachedPath[0], pathLength);

    if (cachedPath != primariesPath) {
        WARN("Cache primaries path mismatch: {} (expected {})", cachedPath, primariesPath);
        WARN("Primaries CSV may have been updated - will recompute matrices");
        return false;
    }

    file.close();
    INFO("Loaded transformation matrices from cache: {}", cachePath);
    return true;
}

void DisplayCalibration::SaveMatricesToCache(
    const std::string& cachePath,
    const std::string& primariesPath,
    const glm::mat4x4& rgbMatrix,
    const glm::mat4x4& ocvMatrix)
{
    // Create directory if it doesn't exist
    std::filesystem::path cacheDir = std::filesystem::path(cachePath).parent_path();
    if (!std::filesystem::exists(cacheDir)) {
        std::filesystem::create_directories(cacheDir);
    }

    std::ofstream file(cachePath, std::ios::binary);
    if (!file.is_open()) {
        ERROR("Failed to create cache file: {}", cachePath);
        return;
    }

    // Write magic bytes
    file.write(CACHE_MAGIC, 4);

    // Write version
    file.write(reinterpret_cast<const char*>(&CACHE_VERSION), sizeof(CACHE_VERSION));

    // Write date string
    std::string date = GetTodayDate();
    uint32_t dateLength = static_cast<uint32_t>(date.length());
    file.write(reinterpret_cast<const char*>(&dateLength), sizeof(dateLength));
    file.write(date.c_str(), dateLength);

    // Write RGB matrix
    file.write(reinterpret_cast<const char*>(&rgbMatrix), sizeof(glm::mat4x4));

    // Write OCV matrix
    file.write(reinterpret_cast<const char*>(&ocvMatrix), sizeof(glm::mat4x4));

    // Write primaries path
    uint32_t pathLength = static_cast<uint32_t>(primariesPath.length());
    file.write(reinterpret_cast<const char*>(&pathLength), sizeof(pathLength));
    file.write(primariesPath.c_str(), pathLength);

    file.close();
    INFO("Saved transformation matrices to cache: {}", cachePath);
}

std::pair<glm::mat4x4, glm::mat4x4> DisplayCalibration::GetOrComputeMatrices(
    const std::string& primariesPath)
{
    std::string cachePath = GetCachePath();
    glm::mat4x4 rgbMatrix, ocvMatrix;

    // Try to load from cache
    if (LoadCachedMatrices(cachePath, primariesPath, rgbMatrix, ocvMatrix)) {
        return {rgbMatrix, ocvMatrix};
    }

    // Cache miss - compute matrices via Python
    INFO("Computing transformation matrices from primaries: {}", primariesPath);
    auto [computedRGB, computedOCV] = ComputeTransformMatrices(primariesPath);

    // Save to cache for next time
    SaveMatricesToCache(cachePath, primariesPath, computedRGB, computedOCV);

    return {computedRGB, computedOCV};
}

std::pair<glm::mat4x4, glm::mat4x4> DisplayCalibration::ComputeTransformMatrices(
    const std::string& primariesPath)
{
    glm::mat4x4 rgbMatrix = ComputeRYGBToRGBMatrix(primariesPath);
    glm::mat4x4 ocvMatrix = ComputeRYGBToOCVMatrix(primariesPath);
    return {rgbMatrix, ocvMatrix};
}

glm::mat4x4 DisplayCalibration::ComputeRYGBToRGBMatrix(const std::string& primariesPath)
{
    return GetMatrixFromPython(primariesPath, "RGB");
}

glm::mat4x4 DisplayCalibration::ComputeRYGBToOCVMatrix(const std::string& primariesPath)
{
    return GetMatrixFromPython(primariesPath, "OCV");
}

glm::mat4x4 DisplayCalibration::GetMatrixFromPython(
    const std::string& primariesPath,
    const std::string& outputSpace)
{
    // 1. Import TetriumColor.Observer module
    PyObject* pObserverModule = PyImport_ImportModule("TetriumColor.Observer.Observer");
    if (!pObserverModule) {
        PyErr_Print();
        ERROR("Failed to import TetriumColor.Observer.Observer");
        return glm::mat4x4(1.0f);
    }

    // 2. Create tetrachromat observer
    PyObject* pCreateTetrachromat = PyObject_GetAttrString(pObserverModule, "create_tetrachromat");
    if (!pCreateTetrachromat) {
        PyErr_Print();
        ERROR("Failed to get create_tetrachromat function");
        Py_DECREF(pObserverModule);
        return glm::mat4x4(1.0f);
    }

    // Create cone peaks list [420, 530, 559, 610]
    PyObject* pConePeaks = PyList_New(4);
    PyList_SetItem(pConePeaks, 0, PyFloat_FromDouble(420.0));
    PyList_SetItem(pConePeaks, 1, PyFloat_FromDouble(530.0));
    PyList_SetItem(pConePeaks, 2, PyFloat_FromDouble(559.0));
    PyList_SetItem(pConePeaks, 3, PyFloat_FromDouble(610.0));

    PyObject* pObserver = PyObject_CallFunctionObjArgs(pCreateTetrachromat, pConePeaks, NULL);
    Py_DECREF(pConePeaks);
    Py_DECREF(pCreateTetrachromat);
    Py_DECREF(pObserverModule);

    if (!pObserver) {
        PyErr_Print();
        ERROR("Failed to create tetrachromat observer");
        return glm::mat4x4(1.0f);
    }

    // 3. Import TetriumColor.Measurement module and load primaries
    PyObject* pMeasurementModule = PyImport_ImportModule("TetriumColor.Measurement");
    if (!pMeasurementModule) {
        PyErr_Print();
        ERROR("Failed to import TetriumColor.Measurement");
        Py_DECREF(pObserver);
        return glm::mat4x4(1.0f);
    }

    PyObject* pLoadPrimaries = PyObject_GetAttrString(pMeasurementModule, "load_primaries_from_csv");
    if (!pLoadPrimaries) {
        PyErr_Print();
        ERROR("Failed to get load_primaries_from_csv function");
        Py_DECREF(pMeasurementModule);
        Py_DECREF(pObserver);
        return glm::mat4x4(1.0f);
    }

    PyObject* pPrimaries = PyObject_CallFunction(pLoadPrimaries, "s", primariesPath.c_str());
    Py_DECREF(pLoadPrimaries);
    Py_DECREF(pMeasurementModule);

    if (!pPrimaries) {
        PyErr_Print();
        ERROR("Failed to load primaries from {}", primariesPath);
        Py_DECREF(pObserver);
        return glm::mat4x4(1.0f);
    }

    // 4. Import TetriumColor.ColorSpace module
    PyObject* pColorSpaceModule = PyImport_ImportModule("TetriumColor.ColorSpace");
    if (!pColorSpaceModule) {
        PyErr_Print();
        ERROR("Failed to import TetriumColor.ColorSpace");
        Py_DECREF(pPrimaries);
        Py_DECREF(pObserver);
        return glm::mat4x4(1.0f);
    }

    PyObject* pColorSpaceClass = PyObject_GetAttrString(pColorSpaceModule, "ColorSpace");
    Py_DECREF(pColorSpaceModule);

    if (!pColorSpaceClass) {
        PyErr_Print();
        ERROR("Failed to get ColorSpace class");
        Py_DECREF(pPrimaries);
        Py_DECREF(pObserver);
        return glm::mat4x4(1.0f);
    }

    // 5. Create ColorSpace instance
    PyObject* pColorSpace = PyObject_CallFunctionObjArgs(pColorSpaceClass, pObserver, pPrimaries, NULL);
    Py_DECREF(pColorSpaceClass);
    Py_DECREF(pPrimaries);
    Py_DECREF(pObserver);

    if (!pColorSpace) {
        PyErr_Print();
        ERROR("Failed to create ColorSpace instance");
        return glm::mat4x4(1.0f);
    }

    // 6. Call compute_transform_matrix("RYGB", outputSpace)
    PyObject* pComputeMatrix = PyObject_GetAttrString(pColorSpace, "compute_transform_matrix");
    if (!pComputeMatrix) {
        PyErr_Print();
        ERROR("Failed to get compute_transform_matrix method");
        Py_DECREF(pColorSpace);
        return glm::mat4x4(1.0f);
    }

    PyObject* pMatrix = PyObject_CallFunction(pComputeMatrix, "ss", "RYGB", outputSpace.c_str());
    Py_DECREF(pComputeMatrix);
    Py_DECREF(pColorSpace);

    if (!pMatrix) {
        PyErr_Print();
        ERROR("Failed to compute transform matrix");
        return glm::mat4x4(1.0f);
    }

    // 7. Extract matrix data (3x4 numpy array)
    // The matrix is a numpy array, we need to extract the values
    PyObject* pTolist = PyObject_CallMethod(pMatrix, "tolist", NULL);
    Py_DECREF(pMatrix);

    if (!pTolist) {
        PyErr_Print();
        ERROR("Failed to convert matrix to list");
        return glm::mat4x4(1.0f);
    }

    // Convert to glm::mat4x4 (add 4th row of zeros for std140 layout)
    glm::mat4x4 matrix(0.0f);

    // Extract 3x4 matrix
    for (int row = 0; row < 3; ++row) {
        PyObject* pRow = PyList_GetItem(pTolist, row);
        for (int col = 0; col < 4; ++col) {
            PyObject* pValue = PyList_GetItem(pRow, col);
            double value = PyFloat_AsDouble(pValue);
            matrix[col][row] = static_cast<float>(value);
        }
    }
    Py_DECREF(pTolist);

    // Set 4th row to zeros (already done by initialization)

    // Special handling for OCV: swap R and O columns
    if (outputSpace == "OCV") {
        glm::vec4 tempCol = matrix[0];
        matrix[0] = matrix[3];
        matrix[3] = tempCol;
    }

    return matrix;
}

} // namespace TetriumApp
