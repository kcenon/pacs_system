/**
 * @file console_utils.cpp
 * @brief Implementation of console output utilities
 */

#include "console_utils.hpp"

#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace pacs::samples {

// ============================================================================
// Static State
// ============================================================================

namespace {
bool g_color_enabled = true;
bool g_color_checked = false;
}  // namespace

// ============================================================================
// Color Support Detection
// ============================================================================

auto supports_color() noexcept -> bool {
    // Check if stdout is a terminal
    if (!isatty(fileno(stdout))) {
        return false;
    }

#ifdef _WIN32
    // Enable virtual terminal processing on Windows 10+
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) {
        // Fallback: check if TERM is set (e.g., running in mintty)
        const char* term = std::getenv("TERM");
        return term != nullptr;
    }
    return true;
#else
    // Check common environment variables
    const char* term = std::getenv("TERM");
    if (term == nullptr) {
        return false;
    }

    // Check for dumb terminal
    if (std::string_view{term} == "dumb") {
        return false;
    }

    // Check for NO_COLOR environment variable
    if (std::getenv("NO_COLOR") != nullptr) {
        return false;
    }

    return true;
#endif
}

void set_color_enabled(bool enabled) {
    g_color_enabled = enabled;
    g_color_checked = true;
}

auto is_color_enabled() noexcept -> bool {
    if (!g_color_checked) {
        g_color_enabled = supports_color();
        g_color_checked = true;
    }
    return g_color_enabled;
}

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

auto color(const char* code) -> const char* {
    return is_color_enabled() ? code : "";
}

constexpr int kDefaultWidth = 78;

}  // namespace

// ============================================================================
// Message Printing
// ============================================================================

void print_header(std::string_view title) {
    std::cout << color(colors::bold) << color(colors::cyan);
    std::cout << "\n";
    std::cout << std::string(kDefaultWidth, '=') << "\n";
    std::cout << center_text(title, kDefaultWidth) << "\n";
    std::cout << std::string(kDefaultWidth, '=') << "\n";
    std::cout << color(colors::reset) << "\n";
}

void print_section(std::string_view title) {
    std::string line = "--- " + std::string(title) + " ";
    line += std::string(kDefaultWidth - line.length(), '-');

    std::cout << "\n" << color(colors::bold) << color(colors::blue)
              << line << color(colors::reset) << "\n\n";
}

void print_success(std::string_view message) {
    std::cout << color(colors::green) << "[OK] " << color(colors::reset)
              << message << "\n";
}

void print_error(std::string_view message) {
    std::cout << color(colors::red) << "[ERROR] " << color(colors::reset)
              << message << "\n";
}

void print_warning(std::string_view message) {
    std::cout << color(colors::yellow) << "[WARN] " << color(colors::reset)
              << message << "\n";
}

void print_info(std::string_view message) {
    std::cout << color(colors::cyan) << "[INFO] " << color(colors::reset)
              << message << "\n";
}

void print_debug(std::string_view message) {
    std::cout << color(colors::dim) << "[DEBUG] " << message
              << color(colors::reset) << "\n";
}

// ============================================================================
// DICOM Data Display
// ============================================================================

void print_dataset_summary(const core::dicom_dataset& ds) {
    using namespace core::tags;

    std::vector<std::pair<std::string, std::string>> patient_rows = {
        {"Patient Name", ds.get_string(patient_name)},
        {"Patient ID", ds.get_string(patient_id)},
        {"Birth Date", ds.get_string(patient_birth_date)},
        {"Sex", ds.get_string(patient_sex)},
    };
    print_table("Patient", patient_rows);

    std::vector<std::pair<std::string, std::string>> study_rows = {
        {"Study UID", truncate(ds.get_string(study_instance_uid), 40)},
        {"Study Date", ds.get_string(study_date)},
        {"Accession #", ds.get_string(accession_number)},
        {"Description", ds.get_string(study_description)},
    };
    print_table("Study", study_rows);

    std::vector<std::pair<std::string, std::string>> series_rows = {
        {"Series UID", truncate(ds.get_string(series_instance_uid), 40)},
        {"Modality", ds.get_string(modality)},
        {"Series #", ds.get_string(series_number)},
        {"Description", ds.get_string(series_description)},
    };
    print_table("Series", series_rows);

    std::vector<std::pair<std::string, std::string>> image_rows = {
        {"SOP Instance", truncate(ds.get_string(sop_instance_uid), 40)},
        {"Rows", ds.get_string(rows)},
        {"Columns", ds.get_string(columns)},
        {"Bits Allocated", ds.get_string(bits_allocated)},
        {"Photometric", ds.get_string(photometric_interpretation)},
    };
    print_table("Image", image_rows);
}

void print_dataset_elements(const core::dicom_dataset& ds,
                            size_t max_value_length) {
    std::cout << color(colors::bold)
              << std::left << std::setw(14) << "Tag"
              << std::setw(6) << "VR"
              << "Value"
              << color(colors::reset) << "\n";
    std::cout << std::string(kDefaultWidth, '-') << "\n";

    for (const auto& [tag, element] : ds) {
        std::cout << color(colors::cyan) << tag.to_string() << color(colors::reset)
                  << "  ";

        std::cout << color(colors::yellow)
                  << std::setw(4) << encoding::to_string(element.vr())
                  << color(colors::reset) << "  ";

        auto value_result = element.as_string();
        std::string value = value_result.is_ok() ? value_result.value() : "<binary>";
        std::cout << truncate(value, max_value_length) << "\n";
    }
}

// ============================================================================
// Box Drawing
// ============================================================================

void print_box(const std::vector<std::string>& lines) {
    if (lines.empty()) {
        return;
    }

    // Find maximum line length
    size_t max_len = 0;
    for (const auto& line : lines) {
        max_len = std::max(max_len, line.length());
    }
    max_len = std::min(max_len, static_cast<size_t>(kDefaultWidth - 4));

    std::cout << color(colors::dim);
    std::cout << "+" << std::string(max_len + 2, '-') << "+\n";
    for (const auto& line : lines) {
        std::cout << "| " << std::left << std::setw(static_cast<int>(max_len))
                  << truncate(line, max_len) << " |\n";
    }
    std::cout << "+" << std::string(max_len + 2, '-') << "+\n";
    std::cout << color(colors::reset);
}

void print_table(std::string_view title,
                 const std::vector<std::pair<std::string, std::string>>& rows) {
    if (rows.empty()) {
        return;
    }

    // Find maximum key length
    size_t max_key_len = 0;
    for (const auto& [key, _] : rows) {
        max_key_len = std::max(max_key_len, key.length());
    }

    std::cout << color(colors::bold) << color(colors::magenta)
              << title << ":" << color(colors::reset) << "\n";

    for (const auto& [key, value] : rows) {
        std::cout << "  " << color(colors::dim)
                  << std::right << std::setw(static_cast<int>(max_key_len)) << key
                  << color(colors::reset) << ": "
                  << value << "\n";
    }
    std::cout << "\n";
}

// ============================================================================
// Progress Display
// ============================================================================

void print_progress(size_t current, size_t total, int width,
                    std::string_view label) {
    if (total == 0) {
        return;
    }

    double progress = static_cast<double>(current) / static_cast<double>(total);
    int filled = static_cast<int>(progress * width);
    int percent = static_cast<int>(progress * 100.0);

    std::cout << "\r" << label << ": [";
    std::cout << color(colors::green);
    for (int i = 0; i < filled; ++i) {
        std::cout << "#";
    }
    std::cout << color(colors::dim);
    for (int i = filled; i < width; ++i) {
        std::cout << "-";
    }
    std::cout << color(colors::reset);
    std::cout << "] " << std::setw(3) << percent << "% ("
              << current << "/" << total << ")" << std::flush;
}

void clear_line() {
    std::cout << "\r" << std::string(kDefaultWidth, ' ') << "\r" << std::flush;
}

// ============================================================================
// Formatting Utilities
// ============================================================================

auto format_bytes(uint64_t bytes) -> std::string {
    constexpr uint64_t KB = 1024;
    constexpr uint64_t MB = KB * 1024;
    constexpr uint64_t GB = MB * 1024;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (bytes >= GB) {
        oss << static_cast<double>(bytes) / static_cast<double>(GB) << " GB";
    } else if (bytes >= MB) {
        oss << static_cast<double>(bytes) / static_cast<double>(MB) << " MB";
    } else if (bytes >= KB) {
        oss << static_cast<double>(bytes) / static_cast<double>(KB) << " KB";
    } else {
        oss << bytes << " B";
    }

    return oss.str();
}

auto format_duration(uint64_t milliseconds) -> std::string {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (milliseconds >= 60000) {
        oss << static_cast<double>(milliseconds) / 60000.0 << " min";
    } else if (milliseconds >= 1000) {
        oss << static_cast<double>(milliseconds) / 1000.0 << " s";
    } else {
        oss << milliseconds << " ms";
    }

    return oss.str();
}

auto center_text(std::string_view text, size_t width) -> std::string {
    if (text.length() >= width) {
        return std::string{text.substr(0, width)};
    }

    size_t padding = (width - text.length()) / 2;
    return std::string(padding, ' ') + std::string{text} +
           std::string(width - padding - text.length(), ' ');
}

auto truncate(std::string_view text, size_t max_length,
              std::string_view suffix) -> std::string {
    if (text.length() <= max_length) {
        return std::string{text};
    }

    if (max_length <= suffix.length()) {
        return std::string{suffix.substr(0, max_length)};
    }

    return std::string{text.substr(0, max_length - suffix.length())} +
           std::string{suffix};
}

}  // namespace pacs::samples
