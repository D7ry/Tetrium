#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "components/Logging.h"
#include "constants.h"

/* cpp stl */
#include <cstdint>

#include <stdio.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <memory>
#include <optional>
/* glm */
#include "glm/glm.hpp"
#include "glm/ext/matrix_transform.hpp" // glm::rotate(), glm::translate()


/* ImGui */
// enable freetype rendering
#define IMGUI_USE_WCHAR32
#define IMGUI_ENABLE_FREETYPE
// enable imvec math operations
#define IMGUI_DEFINE_MATH_OPERATORS
