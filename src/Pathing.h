#pragma once
// NOTE: following must be inlined to avoid static initialization order fiasco
#if defined(WIN32)
    inline const std::string ASSETS_PATH = "../../assets/";
#else
    inline const std::string ASSETS_PATH = "../assets/";
#endif // WIN32
