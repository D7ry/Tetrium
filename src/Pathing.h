#pragma once
#if defined(WIN32)
    const std::string ASSETS_PATH = "../../assets/";
#else
    const std::string ASSETS_PATH = "../assets/";
#endif // WIN32