#pragma once
// NOTE: following must be inlined to avoid static initialization order fiasco
#if defined(WIN32)
inline const std::string ASSETS_PATH = "../../assets/";
inline const std::string TETRIUM_COLOR_PATH = "../../extern/TetriumColor/";
#else
inline const std::string ASSETS_PATH = "../assets/";
inline const std::string TETRIUM_COLOR_PATH = "../extern/TetriumColor/";
#endif // WIN32
