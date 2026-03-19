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
 * @file time.hpp
 * @brief Compatibility header for cross-platform time functions
 *
 * This header provides a unified interface for time functions that differ
 * between POSIX (Unix/Linux/macOS) and Windows.
 *
 * Key differences:
 * - POSIX uses localtime_r(time_t*, tm*) - thread-safe, pointer first
 * - Windows uses localtime_s(tm*, time_t*) - thread-safe, tm pointer first
 *
 * Usage:
 *   #include <pacs/compat/time.hpp>
 *   std::tm tm{};
 *   kcenon::pacs::compat::localtime_safe(&time_val, &tm);
 * @author kcenon
 * @since 1.0.0
 */

#pragma once

#include <ctime>

namespace kcenon::pacs::compat {

/**
 * @brief Cross-platform thread-safe local time conversion
 *
 * Converts a time_t value to local time in a thread-safe manner.
 * Uses localtime_r on POSIX systems and localtime_s on Windows.
 *
 * @param time Pointer to the time_t value to convert
 * @param result Pointer to the tm structure to store the result
 * @return Pointer to the tm structure (result) on success, nullptr on failure
 */
inline std::tm* localtime_safe(const std::time_t* time, std::tm* result) {
#if defined(_WIN32) || defined(_WIN64)
    // Windows: localtime_s returns errno_t (0 on success)
    // Parameter order: (tm*, time_t*)
    return localtime_s(result, time) == 0 ? result : nullptr;
#else
    // POSIX: localtime_r returns tm* (result on success, nullptr on failure)
    // Parameter order: (time_t*, tm*)
    return localtime_r(time, result);
#endif
}

/**
 * @brief Cross-platform thread-safe UTC time conversion
 *
 * Converts a time_t value to UTC time in a thread-safe manner.
 * Uses gmtime_r on POSIX systems and gmtime_s on Windows.
 *
 * @param time Pointer to the time_t value to convert
 * @param result Pointer to the tm structure to store the result
 * @return Pointer to the tm structure (result) on success, nullptr on failure
 */
inline std::tm* gmtime_safe(const std::time_t* time, std::tm* result) {
#if defined(_WIN32) || defined(_WIN64)
    // Windows: gmtime_s returns errno_t (0 on success)
    return gmtime_s(result, time) == 0 ? result : nullptr;
#else
    // POSIX: gmtime_r returns tm*
    return gmtime_r(time, result);
#endif
}

}  // namespace kcenon::pacs::compat
