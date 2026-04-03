// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file format.h
 * @brief Compatibility header providing kcenon::pacs::compat::format as an alias for std::format
 *
 * All supported compilers (GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 15+)
 * provide C++20 std::format. This header aliases it into the kcenon::pacs::compat namespace
 * so existing call sites continue to work without modification.
 *
 * Usage:
 *   #include <kcenon/pacs/compat/format.h>
 *   auto s = kcenon::pacs::compat::format("Hello, {}!", name);
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <format>
#include <string>

#define PACS_HAS_STD_FORMAT 1

namespace kcenon::pacs::compat {
    using std::format;
    template <typename... Args>
    using format_string = std::format_string<Args...>;
}

// Convenience macro for simple string concatenation (no formatting needed)
// Use when you just need to build error messages without {} placeholders
#define PACS_CONCAT(...) (std::string() + __VA_ARGS__)
