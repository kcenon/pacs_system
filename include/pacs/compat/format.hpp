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
 * @brief Compatibility header providing pacs::compat::format as an alias for std::format
 *
 * All supported compilers (GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 15+)
 * provide C++20 std::format. This header aliases it into the pacs::compat namespace
 * so existing call sites continue to work without modification.
 *
 * Usage:
 *   #include <pacs/compat/format.hpp>
 *   auto s = pacs::compat::format("Hello, {}!", name);
 *
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <format>
#include <string>

#define PACS_HAS_STD_FORMAT 1

namespace pacs::compat {
    using std::format;
    template <typename... Args>
    using format_string = std::format_string<Args...>;
}

// Convenience macro for simple string concatenation (no formatting needed)
// Use when you just need to build error messages without {} placeholders
#define PACS_CONCAT(...) (std::string() + __VA_ARGS__)
