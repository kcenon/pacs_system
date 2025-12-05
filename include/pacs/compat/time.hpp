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
 *   pacs::compat::localtime_safe(&time_val, &tm);
 */

#pragma once

#include <ctime>

namespace pacs::compat {

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

}  // namespace pacs::compat
