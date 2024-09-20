#include "Logging.h"
#include "spdlog/spdlog.h"

void Logging::Init() { 
    spdlog::set_pattern("\033[1;37m[%H:%M:%S] [%^%l%$] [%s:%#] (%!) %v\033[0m"); 
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#endif
};
