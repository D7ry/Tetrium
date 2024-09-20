#define STB_IMAGE_IMPLEMENTATION
#ifndef NDEBUG // disable SIMD in debug mode
#define STBI_NO_SIMD
#endif
#include "stb_image.h"
