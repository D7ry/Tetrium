#pragma once

#include <glm/glm.hpp>
#include <string>
#include <utility>

namespace TetriumApp
{

/**
 * @brief Helper class for computing display calibration matrices
 *
 * This class provides utilities for computing transformation matrices from RYGB
 * (tetrachromatic maxbasis) to RGB and OCV display spaces based on display primaries.
 */
class DisplayCalibration
{
  public:
    /**
     * @brief Compute transformation matrices from RYGB to RGB and OCV
     *
     * @param primariesPath Path to display primaries CSV file
     * @return Pair of (RYGB->RGB matrix, RYGB->OCV matrix)
     *
     * This function calls Python TetriumColor to compute the transformation matrices
     * based on the display primaries. The matrices are 4x4 to match std140 layout
     * requirements, with the 4th row unused.
     */
    static std::pair<glm::mat4x4, glm::mat4x4> ComputeTransformMatrices(
        const std::string& primariesPath
    );

    /**
     * @brief Compute RYGB to RGB transformation matrix
     *
     * @param primariesPath Path to display primaries CSV file
     * @return 4x4 transformation matrix from RYGB to RGB
     */
    static glm::mat4x4 ComputeRYGBToRGBMatrix(const std::string& primariesPath);

    /**
     * @brief Compute RYGB to OCV transformation matrix
     *
     * @param primariesPath Path to display primaries CSV file
     * @return 4x4 transformation matrix from RYGB to OCV
     */
    static glm::mat4x4 ComputeRYGBToOCVMatrix(const std::string& primariesPath);

    /**
     * @brief Get or compute transformation matrices with caching
     *
     * @param primariesPath Path to display primaries CSV file
     * @return Pair of (RYGB->RGB matrix, RYGB->OCV matrix)
     *
     * This function checks if cached matrices exist for today's date. If yes,
     * loads from cache. If not, computes via Python and saves to cache.
     */
    static std::pair<glm::mat4x4, glm::mat4x4> GetOrComputeMatrices(
        const std::string& primariesPath
    );

    /**
     * @brief Get today's date in YYYY-MM-DD format
     *
     * @return Today's date string
     */
    static std::string GetTodayDate();

    /**
     * @brief Get path to today's primaries CSV file
     *
     * @return Path to primaries CSV (e.g., "./data/primaries/YYYY-MM-DD.csv")
     */
    static std::string GetTodayPrimariesPath();

  private:
    /**
     * @brief Helper to call Python and get transformation matrix
     *
     * @param primariesPath Path to display primaries CSV file
     * @param outputSpace "RGB" or "OCV"
     * @return 4x4 transformation matrix
     */
    static glm::mat4x4 GetMatrixFromPython(
        const std::string& primariesPath,
        const std::string& outputSpace
    );

    /**
     * @brief Get path to matrix cache file for today
     *
     * @return Path to cache file (e.g., "./data/calibration/matrix_cache_YYYY-MM-DD.bin")
     */
    static std::string GetCachePath();

    /**
     * @brief Load matrices from cache file
     *
     * @param cachePath Path to cache file
     * @param primariesPath Expected primaries path (for validation)
     * @param rgbMatrix Output RGB matrix
     * @param ocvMatrix Output OCV matrix
     * @return true if loaded successfully, false otherwise
     */
    static bool LoadCachedMatrices(
        const std::string& cachePath,
        const std::string& primariesPath,
        glm::mat4x4& rgbMatrix,
        glm::mat4x4& ocvMatrix
    );

    /**
     * @brief Save matrices to cache file
     *
     * @param cachePath Path to cache file
     * @param primariesPath Primaries path to store
     * @param rgbMatrix RGB matrix to save
     * @param ocvMatrix OCV matrix to save
     */
    static void SaveMatricesToCache(
        const std::string& cachePath,
        const std::string& primariesPath,
        const glm::mat4x4& rgbMatrix,
        const glm::mat4x4& ocvMatrix
    );
};

} // namespace TetriumApp

