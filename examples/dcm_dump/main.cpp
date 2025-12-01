/**
 * @file main.cpp
 * @brief DICOM Dump - File Inspection Utility
 *
 * A command-line utility for inspecting DICOM file contents, displaying
 * metadata, tags, transfer syntax, and encoding information.
 *
 * @see Issue #107 - DICOM Dump Sample
 * @see DICOM PS3.10 - Media Storage and File Format
 *
 * Usage:
 *   dcm_dump <file_or_directory> [options]
 *
 * Options:
 *   --tags <list>     Show only specific tags (comma-separated)
 *   --pixel-info      Show pixel data information
 *   --format <fmt>    Output format: text (default), json
 *   --recursive       Process directories recursively
 *   --summary         Show summary only (for directories)
 *
 * Examples:
 *   dcm_dump image.dcm
 *   dcm_dump image.dcm --tags PatientName,PatientID,StudyDate
 *   dcm_dump image.dcm --format json
 *   dcm_dump ./dicom_folder/ --recursive --summary
 */

#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// Output format enumeration
enum class output_format {
    text,
    json
};

/// Configuration for dump operation
struct dump_config {
    std::vector<std::string> tag_filters;  ///< Tags to show (empty = all)
    bool show_pixel_info{false};           ///< Show pixel data details
    output_format format{output_format::text}; ///< Output format
    bool recursive{false};                 ///< Process directories recursively
    bool summary_only{false};              ///< Show summary only
};

/// Statistics for directory processing
struct dump_stats {
    size_t files_processed{0};
    size_t files_failed{0};
    size_t total_elements{0};
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
DICOM Dump - File Inspection Utility

Usage: )" << program_name << R"( <file_or_directory> [options]

Arguments:
  file_or_directory   DICOM file or directory to inspect

Options:
  --tags <list>       Show only specific tags (comma-separated keywords)
                      Example: --tags PatientName,PatientID,StudyDate
  --pixel-info        Show detailed pixel data information
  --format <fmt>      Output format: text (default), json
  --recursive         Process directories recursively
  --summary           Show summary only (for directories)
  --help, -h          Show this help message

Examples:
  )" << program_name << R"( image.dcm
  )" << program_name << R"( image.dcm --tags PatientName,PatientID,StudyDate
  )" << program_name << R"( image.dcm --pixel-info
  )" << program_name << R"( image.dcm --format json
  )" << program_name << R"( ./dicom_folder/ --recursive --summary

Output Format:
  (GGGG,EEEE) VR TagName                    [Value]

  GGGG,EEEE = Tag group and element in hex
  VR        = Value Representation (2 chars)
  TagName   = DICOM keyword (up to 32 chars)
  Value     = Element value (truncated if long)

Exit Codes:
  0  Success
  1  Error (file not found, invalid DICOM, etc.)
)";
}

/**
 * @brief Split a string by delimiter
 * @param str Input string
 * @param delimiter Delimiter character
 * @return Vector of substrings
 */
auto split_string(const std::string& str, char delimiter) -> std::vector<std::string> {
    std::vector<std::string> result;
    std::istringstream stream(str);
    std::string token;

    while (std::getline(stream, token, delimiter)) {
        // Trim whitespace
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) {
            result.push_back(token);
        }
    }
    return result;
}

/**
 * @brief Convert bytes to human-readable size string
 * @param bytes Number of bytes
 * @return Formatted string (e.g., "1.5 MB")
 */
auto format_size(size_t bytes) -> std::string {
    constexpr size_t KB = 1024;
    constexpr size_t MB = KB * 1024;
    constexpr size_t GB = MB * 1024;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (bytes >= GB) {
        oss << static_cast<double>(bytes) / GB << " GB";
    } else if (bytes >= MB) {
        oss << static_cast<double>(bytes) / MB << " MB";
    } else if (bytes >= KB) {
        oss << static_cast<double>(bytes) / KB << " KB";
    } else {
        oss << bytes << " bytes";
    }
    return oss.str();
}

/**
 * @brief Escape special characters for JSON string
 * @param str Input string
 * @return Escaped string
 */
auto escape_json(const std::string& str) -> std::string {
    std::string result;
    result.reserve(str.size() + 10);

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control character - escape as \uXXXX
                    std::ostringstream oss;
                    oss << "\\u" << std::hex << std::setfill('0')
                        << std::setw(4) << static_cast<int>(c);
                    result += oss.str();
                } else {
                    result += c;
                }
        }
    }
    return result;
}

/**
 * @brief Format binary data as hex string
 * @param data Binary data span
 * @param max_bytes Maximum bytes to show
 * @return Hex string representation
 */
auto format_hex(std::span<const uint8_t> data, size_t max_bytes = 16) -> std::string {
    std::ostringstream oss;
    size_t count = std::min(data.size(), max_bytes);

    for (size_t i = 0; i < count; ++i) {
        if (i > 0) oss << "\\";
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(data[i]);
    }

    if (data.size() > max_bytes) {
        oss << "... (" << data.size() << " bytes total)";
    }

    return oss.str();
}

/**
 * @brief Format element value for display
 * @param element The DICOM element
 * @param max_length Maximum string length to display
 * @return Formatted value string
 */
auto format_value(const pacs::core::dicom_element& element,
                  size_t max_length = 64) -> std::string {
    using namespace pacs::encoding;

    auto vr = element.vr();

    // Handle empty values
    if (element.is_empty()) {
        return "(no value)";
    }

    // Handle sequences
    if (element.is_sequence()) {
        auto& items = element.sequence_items();
        return "Sequence with " + std::to_string(items.size()) + " item(s)";
    }

    // Handle binary data (pixel data, etc.)
    if (is_binary_vr(vr)) {
        return format_hex(element.raw_data());
    }

    // Handle string and numeric values
    try {
        std::string value = element.as_string();

        // Truncate long values
        if (value.length() > max_length) {
            value = value.substr(0, max_length - 3) + "...";
        }

        // Replace control characters with escaped versions
        for (char& c : value) {
            if (c == '\0') c = ' ';
            else if (static_cast<unsigned char>(c) < 0x20 && c != '\n' && c != '\r') {
                c = '.';
            }
        }

        return value;
    } catch (const std::exception&) {
        return format_hex(element.raw_data());
    }
}

/**
 * @brief Check if an element should be shown based on tag filters
 * @param element The element to check
 * @param filters List of keyword filters (empty = show all)
 * @return true if element should be shown
 */
auto should_show_element(const pacs::core::dicom_element& element,
                         const std::vector<std::string>& filters) -> bool {
    if (filters.empty()) {
        return true;
    }

    auto& dict = pacs::core::dicom_dictionary::instance();
    auto info = dict.find(element.tag());

    if (!info) {
        return false;  // Unknown tags not shown when filtering
    }

    for (const auto& filter : filters) {
        if (info->keyword == filter) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Dump a single element in text format
 * @param element The element to dump
 * @param indent Indentation level for sequences
 */
void dump_element_text(const pacs::core::dicom_element& element, int indent = 0) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();
    auto info = dict.find(element.tag());

    // Format: (GGGG,EEEE) VR Keyword [Value]
    std::string indent_str(indent * 2, ' ');

    std::cout << indent_str
              << element.tag().to_string() << " "
              << to_string(element.vr()) << " ";

    // Keyword (padded to 32 chars)
    std::string keyword = info ? std::string(info->keyword) : "Unknown";
    std::cout << std::left << std::setw(32) << keyword;

    // Value
    std::cout << "[" << format_value(element) << "]";

    // Show length for binary data
    if (is_binary_vr(element.vr())) {
        std::cout << " (" << format_size(element.length()) << ")";
    }

    std::cout << "\n";

    // Recursively dump sequence items
    if (element.is_sequence()) {
        int item_num = 0;
        for (const auto& item : element.sequence_items()) {
            std::cout << indent_str << "  > Item #" << (item_num++) << "\n";
            for (const auto& [tag, seq_elem] : item) {
                dump_element_text(seq_elem, indent + 2);
            }
        }
    }
}

/**
 * @brief Dump a single element in JSON format
 * @param element The element to dump
 * @param is_last Whether this is the last element (for comma handling)
 * @param indent Indentation level
 */
void dump_element_json(const pacs::core::dicom_element& element,
                       bool is_last, int indent = 2) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();
    auto info = dict.find(element.tag());

    std::string indent_str(indent, ' ');
    std::string keyword = info ? std::string(info->keyword) :
                                 element.tag().to_string();

    std::cout << indent_str << "\"" << keyword << "\": {\n";
    std::cout << indent_str << "  \"tag\": \"" << element.tag().to_string() << "\",\n";
    std::cout << indent_str << "  \"vr\": \"" << to_string(element.vr()) << "\",\n";
    std::cout << indent_str << "  \"length\": " << element.length() << ",\n";

    // Value
    if (element.is_sequence()) {
        std::cout << indent_str << "  \"value\": [\n";
        const auto& items = element.sequence_items();
        for (size_t i = 0; i < items.size(); ++i) {
            std::cout << indent_str << "    {\n";
            size_t elem_count = 0;
            for (const auto& [tag, seq_elem] : items[i]) {
                dump_element_json(seq_elem, elem_count == items[i].size() - 1, indent + 6);
                ++elem_count;
            }
            std::cout << indent_str << "    }" << (i < items.size() - 1 ? "," : "") << "\n";
        }
        std::cout << indent_str << "  ]\n";
    } else if (is_binary_vr(element.vr())) {
        std::cout << indent_str << "  \"value\": \"(binary data)\"\n";
    } else {
        std::string value = escape_json(format_value(element, 256));
        std::cout << indent_str << "  \"value\": \"" << value << "\"\n";
    }

    std::cout << indent_str << "}" << (is_last ? "" : ",") << "\n";
}

/**
 * @brief Show pixel data information
 * @param dataset The dataset containing pixel data
 */
void show_pixel_info(const pacs::core::dicom_dataset& dataset) {
    using namespace pacs::core;

    std::cout << "\n=== Pixel Data Information ===\n";

    auto get_uint = [&](dicom_tag tag) -> std::optional<uint32_t> {
        auto val = dataset.get_numeric<uint16_t>(tag);
        return val ? std::optional<uint32_t>(*val) : std::nullopt;
    };

    // Basic image dimensions
    auto rows = get_uint(tags::rows);
    auto cols = get_uint(tags::columns);
    auto bits_allocated = get_uint(tags::bits_allocated);
    auto bits_stored = get_uint(tags::bits_stored);
    auto high_bit = get_uint(tags::high_bit);
    auto samples_per_pixel = get_uint(tags::samples_per_pixel);
    auto pixel_representation = get_uint(tags::pixel_representation);
    // Number of Frames (0028,0008) - not in tag constants, use direct tag
    constexpr dicom_tag number_of_frames_tag{0x0028, 0x0008};
    auto number_of_frames = dataset.get_numeric<int32_t>(number_of_frames_tag);

    if (rows) std::cout << "  Rows:                 " << *rows << "\n";
    if (cols) std::cout << "  Columns:              " << *cols << "\n";
    if (bits_allocated) std::cout << "  Bits Allocated:       " << *bits_allocated << "\n";
    if (bits_stored) std::cout << "  Bits Stored:          " << *bits_stored << "\n";
    if (high_bit) std::cout << "  High Bit:             " << *high_bit << "\n";
    if (samples_per_pixel) std::cout << "  Samples per Pixel:    " << *samples_per_pixel << "\n";
    if (pixel_representation) {
        std::cout << "  Pixel Representation: " << *pixel_representation
                  << " (" << (*pixel_representation == 0 ? "unsigned" : "signed") << ")\n";
    }
    if (number_of_frames) std::cout << "  Number of Frames:     " << *number_of_frames << "\n";

    // Photometric interpretation
    std::string photo = dataset.get_string(tags::photometric_interpretation);
    if (!photo.empty()) {
        std::cout << "  Photometric Interp:   " << photo << "\n";
    }

    // Calculate expected pixel data size
    if (rows && cols && bits_allocated && samples_per_pixel) {
        size_t frame_size = static_cast<size_t>(*rows) * *cols *
                           (*bits_allocated / 8) * *samples_per_pixel;
        size_t frames = number_of_frames ? static_cast<size_t>(*number_of_frames) : 1;
        size_t total_size = frame_size * frames;
        std::cout << "  Expected Pixel Size:  " << format_size(total_size) << "\n";
    }

    // Check if pixel data exists
    if (auto* pixel_elem = dataset.get(tags::pixel_data)) {
        std::cout << "  Actual Pixel Size:    " << format_size(pixel_elem->length()) << "\n";
    } else {
        std::cout << "  Pixel Data:           Not present\n";
    }
}

/**
 * @brief Dump a DICOM file
 * @param path Path to the file
 * @param config Dump configuration
 * @param stats Statistics to update
 * @return true on success
 */
auto dump_file(const fs::path& path, const dump_config& config,
               dump_stats& stats) -> bool {
    using namespace pacs::core;

    auto result = dicom_file::open(path);
    if (!result) {
        std::cerr << "Error: " << to_string(result.error())
                  << " - " << path.string() << "\n";
        ++stats.files_failed;
        return false;
    }

    auto& file = *result;
    ++stats.files_processed;

    if (config.summary_only) {
        // Just count elements for summary
        stats.total_elements += file.dataset().size();
        stats.total_elements += file.meta_information().size();
        return true;
    }

    // JSON format
    if (config.format == output_format::json) {
        std::cout << "{\n";
        std::cout << "  \"filename\": \"" << escape_json(path.filename().string()) << "\",\n";
        std::cout << "  \"transfer_syntax\": \"" << file.transfer_syntax().uid() << "\",\n";
        std::cout << "  \"sop_class_uid\": \"" << escape_json(file.sop_class_uid()) << "\",\n";
        std::cout << "  \"sop_instance_uid\": \"" << escape_json(file.sop_instance_uid()) << "\",\n";

        // Meta information
        std::cout << "  \"meta_information\": {\n";
        size_t meta_count = 0;
        for (const auto& [tag, element] : file.meta_information()) {
            if (should_show_element(element, config.tag_filters)) {
                dump_element_json(element, meta_count == file.meta_information().size() - 1, 4);
            }
            ++meta_count;
        }
        std::cout << "  },\n";

        // Dataset
        std::cout << "  \"dataset\": {\n";
        size_t ds_count = 0;
        for (const auto& [tag, element] : file.dataset()) {
            if (should_show_element(element, config.tag_filters)) {
                dump_element_json(element, ds_count == file.dataset().size() - 1, 4);
            }
            ++ds_count;
        }
        std::cout << "  }\n";
        std::cout << "}\n";

        return true;
    }

    // Text format
    std::cout << "\n";
    std::cout << "=== " << path.filename().string() << " ===\n";
    std::cout << "Transfer Syntax: " << file.transfer_syntax().name()
              << " (" << file.transfer_syntax().uid() << ")\n";
    std::cout << "SOP Class:       " << file.sop_class_uid() << "\n";
    std::cout << "SOP Instance:    " << file.sop_instance_uid() << "\n";

    // File Meta Information
    std::cout << "\n# File Meta Information\n";
    for (const auto& [tag, element] : file.meta_information()) {
        if (should_show_element(element, config.tag_filters)) {
            dump_element_text(element);
        }
    }

    // Dataset
    std::cout << "\n# Dataset\n";
    for (const auto& [tag, element] : file.dataset()) {
        if (should_show_element(element, config.tag_filters)) {
            dump_element_text(element);
        }
    }

    // Pixel data info
    if (config.show_pixel_info) {
        show_pixel_info(file.dataset());
    }

    return true;
}

/**
 * @brief Process a directory of DICOM files
 * @param path Directory path
 * @param config Dump configuration
 * @param stats Statistics to update
 * @return true if any files were processed successfully
 */
auto process_directory(const fs::path& path, const dump_config& config,
                       dump_stats& stats) -> bool {
    std::vector<fs::path> dicom_files;

    // Collect DICOM files
    if (config.recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                dicom_files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                dicom_files.push_back(entry.path());
            }
        }
    }

    // Sort by name
    std::sort(dicom_files.begin(), dicom_files.end());

    // Process each file
    for (const auto& file_path : dicom_files) {
        dump_file(file_path, config, stats);
    }

    // Show summary
    if (config.summary_only || dicom_files.size() > 1) {
        std::cout << "\n=== Summary ===\n";
        std::cout << "Files processed: " << stats.files_processed << "\n";
        std::cout << "Files failed:    " << stats.files_failed << "\n";
        if (config.summary_only) {
            std::cout << "Total elements:  " << stats.total_elements << "\n";
        }
    }

    return stats.files_processed > 0;
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param path Output: file or directory path
 * @param config Output: dump configuration
 * @return true if arguments are valid
 */
auto parse_arguments(int argc, char* argv[], fs::path& path,
                     dump_config& config) -> bool {
    if (argc < 2) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;  // Trigger help display
        }

        if (arg == "--tags" && i + 1 < argc) {
            config.tag_filters = split_string(argv[++i], ',');
        } else if (arg == "--pixel-info") {
            config.show_pixel_info = true;
        } else if (arg == "--format" && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "json") {
                config.format = output_format::json;
            } else if (fmt == "text") {
                config.format = output_format::text;
            } else {
                std::cerr << "Error: Unknown format '" << fmt << "'\n";
                return false;
            }
        } else if (arg == "--recursive") {
            config.recursive = true;
        } else if (arg == "--summary") {
            config.summary_only = true;
        } else if (arg[0] != '-') {
            path = arg;
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    if (path.empty()) {
        std::cerr << "Error: No file or directory specified\n";
        return false;
    }

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____   ____ __  __   ____  _   _ __  __ ____
 |  _ \ / ___|  \/  | |  _ \| | | |  \/  |  _ \
 | | | | |   | |\/| | | | | | | | | |\/| | |_) |
 | |_| | |___| |  | | | |_| | |_| | |  | |  __/
 |____/ \____|_|  |_| |____/ \___/|_|  |_|_|

          DICOM File Inspection Utility
)" << "\n";

    fs::path path;
    dump_config config;

    if (!parse_arguments(argc, argv, path, config)) {
        print_usage(argv[0]);
        return 1;
    }

    // Check if path exists
    if (!fs::exists(path)) {
        std::cout << "Error: Path does not exist: " << path.string() << "\n";
        return 1;
    }

    dump_stats stats;
    bool success = false;

    auto start_time = std::chrono::steady_clock::now();

    if (fs::is_directory(path)) {
        success = process_directory(path, config, stats);
    } else {
        success = dump_file(path, config, stats);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (stats.files_processed > 1 || config.summary_only) {
        std::cout << "Processing time: " << duration.count() << " ms\n";
    }

    return success ? 0 : 1;
}
