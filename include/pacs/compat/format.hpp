/**
 * @file format.hpp
 * @brief Compatibility header for std::format vs fmt::format
 *
 * This header provides a unified interface for string formatting that works
 * across different compilers and C++ standard library implementations.
 *
 * - GCC 13+, Clang 14+, MSVC 19.29+: Uses std::format
 * - Older compilers: Uses fmt::format as fallback
 *
 * Usage:
 *   #include <pacs/compat/format.hpp>
 *   auto s = pacs::compat::format("Hello, {}!", name);
 *
 * @see logger_system/cmake/LoggerCompatibility.cmake for detection patterns
 */

#pragma once

// Detect std::format availability
// GCC 13+ has full std::format support
// Clang 14+ has full std::format support
// MSVC 19.29+ (VS 2019 16.10+) has full std::format support
#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
    #define PACS_HAS_STD_FORMAT 1
#elif defined(_MSC_VER) && _MSC_VER >= 1929
    #define PACS_HAS_STD_FORMAT 1
#elif defined(__GNUC__) && __GNUC__ >= 13
    #define PACS_HAS_STD_FORMAT 1
#elif defined(__clang__) && __clang_major__ >= 14 && !defined(__apple_build_version__)
    #define PACS_HAS_STD_FORMAT 1
#else
    #define PACS_HAS_STD_FORMAT 0
#endif

#if PACS_HAS_STD_FORMAT
    #include <format>
    namespace pacs::compat {
        using std::format;
        template <typename... Args>
        using format_string = std::format_string<Args...>;
    }
#else
    // Use fmt library as fallback
    #include <fmt/format.h>
    namespace pacs::compat {
        using fmt::format;
        template <typename... Args>
        using format_string = fmt::format_string<Args...>;
    }
#endif

// Convenience macro for simple string concatenation (no formatting needed)
// Use when you just need to build error messages without {} placeholders
#define PACS_CONCAT(...) (std::string() + __VA_ARGS__)
