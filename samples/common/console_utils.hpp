/**
 * @file console_utils.hpp
 * @brief Console output utilities for developer samples
 *
 * This file provides utilities for formatted console output with
 * ANSI color support and pretty-printing of DICOM data.
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace pacs::samples {

// ============================================================================
// ANSI Color Codes
// ============================================================================

/**
 * @brief ANSI escape codes for terminal colors
 *
 * These codes work on most modern terminals including:
 * - Linux terminals (GNOME Terminal, Konsole, etc.)
 * - macOS Terminal and iTerm2
 * - Windows Terminal and PowerShell (Windows 10+)
 *
 * For older Windows cmd.exe, colors may not display correctly.
 */
namespace colors {

/// Reset all formatting
constexpr const char* reset = "\033[0m";

/// Bold text
constexpr const char* bold = "\033[1m";

/// Dim/faint text
constexpr const char* dim = "\033[2m";

/// Standard colors
constexpr const char* black = "\033[30m";
constexpr const char* red = "\033[31m";
constexpr const char* green = "\033[32m";
constexpr const char* yellow = "\033[33m";
constexpr const char* blue = "\033[34m";
constexpr const char* magenta = "\033[35m";
constexpr const char* cyan = "\033[36m";
constexpr const char* white = "\033[37m";

/// Bright/bold colors
constexpr const char* bright_red = "\033[91m";
constexpr const char* bright_green = "\033[92m";
constexpr const char* bright_yellow = "\033[93m";
constexpr const char* bright_blue = "\033[94m";
constexpr const char* bright_magenta = "\033[95m";
constexpr const char* bright_cyan = "\033[96m";
constexpr const char* bright_white = "\033[97m";

/// Background colors
constexpr const char* bg_red = "\033[41m";
constexpr const char* bg_green = "\033[42m";
constexpr const char* bg_yellow = "\033[43m";
constexpr const char* bg_blue = "\033[44m";

}  // namespace colors

// ============================================================================
// Console Utilities
// ============================================================================

/**
 * @brief Check if the terminal supports ANSI colors
 * @return true if ANSI colors are supported
 *
 * Checks environment variables and terminal type to determine support.
 */
[[nodiscard]] auto supports_color() noexcept -> bool;

/**
 * @brief Enable or disable color output globally
 * @param enabled Whether to enable colors
 *
 * When disabled, all color functions will output plain text.
 */
void set_color_enabled(bool enabled);

/**
 * @brief Check if colors are currently enabled
 * @return true if colors are enabled
 */
[[nodiscard]] auto is_color_enabled() noexcept -> bool;

// ============================================================================
// Message Printing
// ============================================================================

/**
 * @brief Print a header/title with decorative border
 * @param title The title text
 *
 * Example output:
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║                        PACS System Sample Application                     ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 */
void print_header(std::string_view title);

/**
 * @brief Print a section header
 * @param title The section title
 *
 * Example output:
 * ─── Configuration ───────────────────────────────────────────────────────────
 */
void print_section(std::string_view title);

/**
 * @brief Print a success message
 * @param message The message text
 *
 * Prints with green color and checkmark symbol.
 */
void print_success(std::string_view message);

/**
 * @brief Print an error message
 * @param message The message text
 *
 * Prints with red color and X symbol.
 */
void print_error(std::string_view message);

/**
 * @brief Print a warning message
 * @param message The message text
 *
 * Prints with yellow color and warning symbol.
 */
void print_warning(std::string_view message);

/**
 * @brief Print an informational message
 * @param message The message text
 *
 * Prints with cyan color and info symbol.
 */
void print_info(std::string_view message);

/**
 * @brief Print a debug message
 * @param message The message text
 *
 * Prints with dim/gray color.
 */
void print_debug(std::string_view message);

// ============================================================================
// DICOM Data Display
// ============================================================================

/**
 * @brief Print a summary of a DICOM dataset
 * @param ds The dataset to summarize
 *
 * Displays key patient, study, series, and image information
 * in a formatted table.
 */
void print_dataset_summary(const core::dicom_dataset& ds);

/**
 * @brief Print all elements in a DICOM dataset
 * @param ds The dataset to display
 * @param max_value_length Maximum length for value display (truncated if longer)
 *
 * Displays all elements with tag, VR, and value.
 */
void print_dataset_elements(const core::dicom_dataset& ds,
                            size_t max_value_length = 60);

// ============================================================================
// Box Drawing
// ============================================================================

/**
 * @brief Print text in a decorative box
 * @param lines Vector of text lines to display
 *
 * Example output:
 * ┌─────────────────────────────────┐
 * │ Line 1                          │
 * │ Line 2                          │
 * └─────────────────────────────────┘
 */
void print_box(const std::vector<std::string>& lines);

/**
 * @brief Print a key-value table
 * @param title Table title
 * @param rows Vector of {key, value} pairs
 *
 * Example output:
 * ┌─ Patient Information ───────────┐
 * │ Name:     DOE^JOHN              │
 * │ ID:       PAT001                │
 * │ Sex:      M                     │
 * └─────────────────────────────────┘
 */
void print_table(std::string_view title,
                 const std::vector<std::pair<std::string, std::string>>& rows);

// ============================================================================
// Progress Display
// ============================================================================

/**
 * @brief Print a progress bar
 * @param current Current value
 * @param total Total value
 * @param width Width of the progress bar in characters
 * @param label Optional label to display
 *
 * Example output:
 * Processing: [████████████░░░░░░░░] 60% (6/10)
 */
void print_progress(size_t current, size_t total, int width = 40,
                    std::string_view label = "Progress");

/**
 * @brief Clear the current line (for updating progress)
 */
void clear_line();

// ============================================================================
// Formatting Utilities
// ============================================================================

/**
 * @brief Format a byte size for human-readable display
 * @param bytes Number of bytes
 * @return Formatted string (e.g., "1.5 MB", "256 KB")
 */
[[nodiscard]] auto format_bytes(uint64_t bytes) -> std::string;

/**
 * @brief Format a duration in milliseconds
 * @param milliseconds Duration in milliseconds
 * @return Formatted string (e.g., "1.5s", "250ms")
 */
[[nodiscard]] auto format_duration(uint64_t milliseconds) -> std::string;

/**
 * @brief Center a string within a given width
 * @param text The text to center
 * @param width The total width
 * @return Centered string with padding
 */
[[nodiscard]] auto center_text(std::string_view text, size_t width) -> std::string;

/**
 * @brief Truncate a string to a maximum length
 * @param text The text to truncate
 * @param max_length Maximum length
 * @param suffix Suffix to add when truncated (default: "...")
 * @return Truncated string
 */
[[nodiscard]] auto truncate(std::string_view text, size_t max_length,
                            std::string_view suffix = "...") -> std::string;

}  // namespace pacs::samples
