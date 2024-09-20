#pragma once
#ifndef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#endif
#include "spdlog/spdlog.h"
#include "vulkan/vk_enum_string_helper.h"

namespace Logging
{
void Init();
}

#define INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define FATAL(...)                                                                                 \
    SPDLOG_CRITICAL(__VA_ARGS__);                                                                  \
    throw std::runtime_error("Fatal error: " + fmt::format(__VA_ARGS__))
#define NEEDS_IMPLEMENTATION() FATAL("This method needs to be implemented")
#define PANIC(...)                                                                                 \
    SPDLOG_CRITICAL(__VA_ARGS__);                                                                  \
    SPDLOG_CRITICAL("Engine Panic; Exiting");                                                      \
    exit(1);
#ifndef NDEBUG
#define ASSERT(...)                                                                                \
    if (!(__VA_ARGS__)) {                                                                          \
        PANIC("Assertion failed: {}", #__VA_ARGS__);                                               \
    }
#else
#define ASSERT(...)
#endif // NDEBUG

#define VK_CHECK_RESULT(expr)                                                                      \
    {                                                                                              \
        auto res = expr;                                                                           \
        if (res != VK_SUCCESS) {                                                                   \
            PANIC("{} returns none VK_SUCCESS result: {}", #expr, string_VkResult(res));           \
        }                                                                                          \
    }

#define INIT_LOGS() Logging::Init()
