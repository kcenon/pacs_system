// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file format.hpp
 * @brief Compatibility header for std::format vs fmt::format
 *
 * This header provides a unified interface for string formatting that works
 * across different compilers and C++ standard library implementations.
 *
 * Detection is primarily based on the __cpp_lib_format feature test macro,
 * which is the most reliable way to detect std::format availability.
 *
 * Usage:
 *   #include <pacs/compat/format.hpp>
 *   auto s = pacs::compat::format("Hello, {}!", name);
 *
 * @see logger_system/cmake/LoggerCompatibility.cmake for detection patterns
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <version>  // For feature test macros

// Detect std::format availability
//
// Detection strategy:
// 1. __cpp_lib_format feature test macro (most reliable for libstdc++ and libc++)
// 2. Apple Clang 15+ with libc++ (may not define __cpp_lib_format)
// 3. MSVC 19.29+ with C++20 mode
//
// Note: Non-Apple Clang on Linux typically uses libstdc++ which requires GCC 13+
// for std::format support, so we rely on __cpp_lib_format for that case.
#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
    #define PACS_HAS_STD_FORMAT 1
#elif defined(__APPLE__) && defined(__clang__) && __clang_major__ >= 15
    // Apple Clang 15+ with libc++ supports std::format
    // Note: __apple_build_version__ is always defined for Apple Clang
    #define PACS_HAS_STD_FORMAT 1
#elif defined(_MSC_VER) && _MSC_VER >= 1929 && defined(_HAS_CXX20) && _HAS_CXX20
    // MSVC with C++20 mode enabled
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
