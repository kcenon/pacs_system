/**
 * @file main.cpp
 * @brief DICOM Dump - File Inspection Utility
 *
 * A command-line utility for inspecting DICOM file contents, displaying
 * tag information, transfer syntax, and metadata.
 *
 * @see Issue #107 - DICOM Dump Sample
 * @see DICOM PS3.10 - Media Storage and File Format
 *
 * Usage:
 *   dcm_dump <path> [options]
 *
 * Example:
 *   dcm_dump image.dcm
 *   dcm_dump image.dcm --tags PatientName,PatientID,StudyDate
 *   dcm_dump image.dcm --pixel-info
 *   dcm_dump image.dcm --format json
 *   dcm_dump ./dicom_folder/ --recursive --summary
 */

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Output format options
 */
enum class output_format { human_readable, json };

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path path;
    std::vector<std::string> filter_tags;  // Keywords to filter
    bool pixel_info{false};
    output_format format{output_format::human_readable};
    bool recursive{false};
    bool summary{false};
    bool show_meta{true};
    bool verbose{false};
};

/**
 * @brief Summary statistics for directory scanning
 */
struct scan_summary {
    size_t total_files{0};
    size_t valid_files{0};
    size_t invalid_files{0};
    std::map<std::string, size_t> modalities;
    std::map<std::string, size_t> sop_classes;
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
DICOM Dump - File Inspection Utility

Usage: )" << program_name
              << R"( <path> [options]

Arguments:
  path              DICOM file or directory to inspect

Options:
  --tags <list>     Show only specific tags (comma-separated keywords)
                    Example: --tags PatientName,PatientID,StudyDate
  --pixel-info      Show pixel data information
  --format <fmt>    Output format: human (default), json
  --recursive, -r   Recursively scan directories
  --summary         Show summary only (for directories)
  --no-meta         Don't show File Meta Information
  --verbose, -v     Show additional details
  --help, -h        Show this help message

Examples:
  )" << program_name
              << R"( image.dcm
  )" << program_name
              << R"( image.dcm --tags PatientName,PatientID,StudyDate
  )" << program_name
              << R"( image.dcm --pixel-info
  )" << program_name
              << R"( image.dcm --format json
  )" << program_name
              << R"( ./dicom_folder/ --recursive --summary

Output Format:
  Human-readable output shows tags in the format:
    (GGGG,EEEE) VR Keyword                      [value]

  JSON output provides structured data for programmatic use.

Exit Codes:
  0  Success - File(s) parsed successfully
  1  Error - Invalid arguments
  2  Error - File not found or invalid DICOM file
)";
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param opts Output: parsed options
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 2) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        } else if (arg == "--tags" && i + 1 < argc) {
            // Parse comma-separated tag keywords
            std::string tags_str = argv[++i];
            std::stringstream ss(tags_str);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                // Trim whitespace
                tag.erase(0, tag.find_first_not_of(" \t"));
                tag.erase(tag.find_last_not_of(" \t") + 1);
                if (!tag.empty()) {
                    opts.filter_tags.push_back(tag);
                }
            }
        } else if (arg == "--pixel-info") {
            opts.pixel_info = true;
        } else if (arg == "--format" && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "json") {
                opts.format = output_format::json;
            } else if (fmt == "human") {
                opts.format = output_format::human_readable;
            } else {
                std::cerr << "Error: Unknown format '" << fmt << "'\n";
                return false;
            }
        } else if (arg == "--recursive" || arg == "-r") {
            opts.recursive = true;
        } else if (arg == "--summary") {
            opts.summary = true;
        } else if (arg == "--no-meta") {
            opts.show_meta = false;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else if (opts.path.empty()) {
            opts.path = arg;
        } else {
            std::cerr << "Error: Multiple paths specified\n";
            return false;
        }
    }

    if (opts.path.empty()) {
        std::cerr << "Error: No path specified\n";
        return false;
    }

    return true;
}

/**
 * @brief Escape string for JSON output
 * @param str Input string
 * @return JSON-escaped string
 */
std::string json_escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\b':
                oss << "\\b";
                break;
            case '\f':
                oss << "\\f";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                        << static_cast<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

/**
 * @brief Format binary data as hex string
 * @param data Raw data span
 * @param max_bytes Maximum bytes to show
 * @return Formatted hex string
 */
std::string format_hex(std::span<const uint8_t> data, size_t max_bytes = 32) {
    std::ostringstream oss;
    size_t count = std::min(data.size(), max_bytes);

    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            oss << "\\";
        }
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(data[i]);
    }

    if (data.size() > max_bytes) {
        oss << "...";
    }

    return oss.str();
}

/**
 * @brief Format element value for display
 * @param element The DICOM element
 * @param max_length Maximum string length
 * @return Formatted value string
 */
std::string format_value(const pacs::core::dicom_element& element,
                         size_t max_length = 64) {
    using namespace pacs::encoding;

    auto vr = element.vr();

    // Handle empty values
    if (element.is_empty()) {
        return "(empty)";
    }

    // Handle sequences
    if (element.is_sequence()) {
        const auto& items = element.sequence_items();
        return "SQ (" + std::to_string(items.size()) + " items)";
    }

    // Handle binary VRs
    if (is_binary_vr(vr)) {
        auto data = element.raw_data();
        return "[" + format_hex(data) + "] (" + std::to_string(data.size()) +
               " bytes)";
    }

    // Handle string VRs
    if (is_string_vr(vr)) {
        std::string value = element.as_string();
        if (value.length() > max_length) {
            value = value.substr(0, max_length - 3) + "...";
        }
        return value;
    }

    // Handle numeric VRs
    if (is_numeric_vr(vr)) {
        try {
            switch (vr) {
                case vr_type::US:
                    return std::to_string(element.as_numeric<uint16_t>());
                case vr_type::SS:
                    return std::to_string(element.as_numeric<int16_t>());
                case vr_type::UL:
                    return std::to_string(element.as_numeric<uint32_t>());
                case vr_type::SL:
                    return std::to_string(element.as_numeric<int32_t>());
                case vr_type::FL:
                    return std::to_string(element.as_numeric<float>());
                case vr_type::FD:
                    return std::to_string(element.as_numeric<double>());
                case vr_type::UV:
                    return std::to_string(element.as_numeric<uint64_t>());
                case vr_type::SV:
                    return std::to_string(element.as_numeric<int64_t>());
                default:
                    break;
            }
        } catch (const pacs::core::value_conversion_error&) {
            // Fall through to raw data display
        }
    }

    // Fallback: show raw bytes
    auto data = element.raw_data();
    return "[" + format_hex(data) + "]";
}

/**
 * @brief Check if a tag should be displayed based on filter
 * @param tag The DICOM tag
 * @param filter_tags List of keywords to include
 * @param dict Reference to the dictionary
 * @return true if tag should be displayed
 */
bool should_display_tag(const pacs::core::dicom_tag& tag,
                        const std::vector<std::string>& filter_tags,
                        const pacs::core::dicom_dictionary& dict) {
    if (filter_tags.empty()) {
        return true;
    }

    auto info = dict.find(tag);
    if (!info) {
        return false;
    }

    for (const auto& keyword : filter_tags) {
        if (info->keyword == keyword) {
            return true;
        }
    }

    return false;
}

/**
 * @brief Print dataset in human-readable format
 * @param dataset The dataset to print
 * @param opts Command line options
 * @param indent Indentation level for nested sequences
 */
void print_dataset_human(const pacs::core::dicom_dataset& dataset,
                         const options& opts, int indent = 0) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();
    std::string indent_str(indent * 2, ' ');

    for (const auto& [tag, element] : dataset) {
        // Check filter
        if (!should_display_tag(tag, opts.filter_tags, dict)) {
            continue;
        }

        // Get tag info from dictionary
        auto info = dict.find(tag);
        std::string keyword =
            info ? std::string(info->keyword) : "UnknownTag";

        // Format: (GGGG,EEEE) VR Keyword                      [value]
        std::cout << indent_str << tag.to_string() << " "
                  << to_string(element.vr()) << " " << std::left
                  << std::setw(36 - indent * 2) << keyword;

        // Handle sequences specially
        if (element.is_sequence()) {
            std::cout << "\n";
            const auto& items = element.sequence_items();
            int item_num = 0;
            for (const auto& item : items) {
                std::cout << indent_str << "  > Item #" << item_num++ << "\n";
                print_dataset_human(item, opts, indent + 2);
            }
        } else {
            std::cout << "[" << format_value(element) << "]\n";
        }
    }
}

/**
 * @brief Print dataset as JSON
 * @param dataset The dataset to print
 * @param opts Command line options
 * @param indent Current indentation
 */
void print_dataset_json(const pacs::core::dicom_dataset& dataset,
                        const options& opts, int indent = 2) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();
    std::string indent_str(indent, ' ');

    std::cout << "{\n";

    size_t count = 0;
    size_t total = dataset.size();

    for (const auto& [tag, element] : dataset) {
        ++count;

        // Check filter
        if (!should_display_tag(tag, opts.filter_tags, dict)) {
            continue;
        }

        auto info = dict.find(tag);
        std::string keyword =
            info ? std::string(info->keyword) : tag.to_string();

        std::cout << indent_str << "  \"" << keyword << "\": {\n";
        std::cout << indent_str << "    \"tag\": \"" << tag.to_string()
                  << "\",\n";
        std::cout << indent_str << "    \"vr\": \"" << to_string(element.vr())
                  << "\",\n";
        std::cout << indent_str
                  << "    \"length\": " << element.length() << ",\n";

        if (element.is_sequence()) {
            std::cout << indent_str << "    \"value\": [\n";
            const auto& items = element.sequence_items();
            for (size_t i = 0; i < items.size(); ++i) {
                std::cout << indent_str << "      ";
                print_dataset_json(items[i], opts, indent + 6);
                if (i < items.size() - 1) {
                    std::cout << ",";
                }
                std::cout << "\n";
            }
            std::cout << indent_str << "    ]\n";
        } else {
            std::string value = json_escape(format_value(element, 256));
            std::cout << indent_str << "    \"value\": \"" << value << "\"\n";
        }

        std::cout << indent_str << "  }";
        if (count < total) {
            std::cout << ",";
        }
        std::cout << "\n";
    }

    std::cout << indent_str << "}";
}

/**
 * @brief Print pixel data information
 * @param dataset The dataset containing pixel data
 */
void print_pixel_info(const pacs::core::dicom_dataset& dataset) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    std::cout << "\n# Pixel Data Information\n";
    std::cout << "----------------------------------------\n";

    // Rows and Columns
    auto rows = dataset.get_numeric<uint16_t>(tags::rows);
    auto cols = dataset.get_numeric<uint16_t>(tags::columns);
    if (rows && cols) {
        std::cout << "  Dimensions:        " << *cols << " x " << *rows << "\n";
    }

    // Bits Allocated / Stored / High Bit
    auto bits_allocated =
        dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0100});
    auto bits_stored = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0101});
    auto high_bit = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0102});
    if (bits_allocated) {
        std::cout << "  Bits Allocated:    " << *bits_allocated << "\n";
    }
    if (bits_stored) {
        std::cout << "  Bits Stored:       " << *bits_stored << "\n";
    }
    if (high_bit) {
        std::cout << "  High Bit:          " << *high_bit << "\n";
    }

    // Pixel Representation
    auto pixel_rep = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0103});
    if (pixel_rep) {
        std::cout << "  Pixel Rep:         "
                  << (*pixel_rep == 0 ? "Unsigned" : "Signed") << "\n";
    }

    // Samples Per Pixel
    auto samples = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0002});
    if (samples) {
        std::cout << "  Samples/Pixel:     " << *samples << "\n";
    }

    // Photometric Interpretation
    auto photometric = dataset.get_string(dicom_tag{0x0028, 0x0004});
    if (!photometric.empty()) {
        std::cout << "  Photometric:       " << photometric << "\n";
    }

    // Number of Frames
    auto frames = dataset.get_string(dicom_tag{0x0028, 0x0008});
    if (!frames.empty()) {
        std::cout << "  Number of Frames:  " << frames << "\n";
    }

    // Pixel Data element info
    auto* pixel_data = dataset.get(dicom_tag{0x7FE0, 0x0010});
    if (pixel_data != nullptr) {
        std::cout << "  Pixel Data:        " << pixel_data->length()
                  << " bytes\n";
        std::cout << "  Pixel Data VR:     " << to_string(pixel_data->vr())
                  << "\n";
    } else {
        std::cout << "  Pixel Data:        (not present)\n";
    }

    std::cout << "----------------------------------------\n";
}

/**
 * @brief Dump a single DICOM file
 * @param file_path Path to the DICOM file
 * @param opts Command line options
 * @return 0 on success, non-zero on error
 */
int dump_file(const std::filesystem::path& file_path, const options& opts) {
    using namespace pacs::core;

    auto result = dicom_file::open(file_path);
    if (!result.has_value()) {
        std::cerr << "Error: Failed to open '" << file_path.string()
                  << "': " << to_string(result.error()) << "\n";
        return 2;
    }

    auto& file = result.value();

    if (opts.format == output_format::json) {
        // JSON output
        std::cout << "{\n";
        std::cout << "  \"file\": \"" << json_escape(file_path.string())
                  << "\",\n";
        std::cout << "  \"transferSyntax\": \""
                  << file.transfer_syntax().name() << "\",\n";
        std::cout << "  \"sopClassUID\": \"" << file.sop_class_uid() << "\",\n";
        std::cout << "  \"sopInstanceUID\": \"" << file.sop_instance_uid()
                  << "\",\n";

        if (opts.show_meta) {
            std::cout << "  \"metaInformation\": ";
            print_dataset_json(file.meta_information(), opts);
            std::cout << ",\n";
        }

        std::cout << "  \"dataset\": ";
        print_dataset_json(file.dataset(), opts);
        std::cout << "\n}\n";
    } else {
        // Human-readable output
        std::cout << "# File: " << file_path.string() << "\n";
        std::cout << "# Transfer Syntax: " << file.transfer_syntax().name()
                  << " (" << file.transfer_syntax().uid() << ")\n";
        std::cout << "# SOP Class: " << file.sop_class_uid() << "\n";
        std::cout << "# SOP Instance: " << file.sop_instance_uid() << "\n";
        std::cout << "\n";

        if (opts.show_meta) {
            std::cout << "# File Meta Information\n";
            print_dataset_human(file.meta_information(), opts);
            std::cout << "\n";
        }

        std::cout << "# Dataset\n";
        print_dataset_human(file.dataset(), opts);

        if (opts.pixel_info) {
            print_pixel_info(file.dataset());
        }
    }

    return 0;
}

/**
 * @brief Scan directory and collect statistics
 * @param dir_path Directory to scan
 * @param opts Command line options
 * @param summary Output statistics
 */
void scan_directory(const std::filesystem::path& dir_path, const options& opts,
                    scan_summary& summary) {
    using namespace pacs::core;

    auto process_file = [&](const std::filesystem::path& file_path) {
        ++summary.total_files;

        auto result = dicom_file::open(file_path);
        if (!result.has_value()) {
            ++summary.invalid_files;
            if (opts.verbose) {
                std::cerr << "  Invalid: " << file_path.filename().string()
                          << "\n";
            }
            return;
        }

        ++summary.valid_files;
        auto& file = result.value();

        // Collect modality
        auto modality = file.dataset().get_string(dicom_tag{0x0008, 0x0060});
        if (!modality.empty()) {
            summary.modalities[modality]++;
        }

        // Collect SOP Class
        auto sop_class = file.sop_class_uid();
        if (!sop_class.empty()) {
            summary.sop_classes[sop_class]++;
        }

        if (opts.verbose) {
            std::cout << "  OK: " << file_path.filename().string();
            if (!modality.empty()) {
                std::cout << " [" << modality << "]";
            }
            std::cout << "\n";
        }
    };

    if (opts.recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".dcm" || ext == ".dicom" || ext.empty()) {
                    process_file(entry.path());
                }
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".dcm" || ext == ".dicom" || ext.empty()) {
                    process_file(entry.path());
                }
            }
        }
    }
}

/**
 * @brief Print scan summary
 * @param summary The collected statistics
 * @param opts Command line options
 */
void print_summary(const scan_summary& summary, const options& opts) {
    if (opts.format == output_format::json) {
        std::cout << "{\n";
        std::cout << "  \"totalFiles\": " << summary.total_files << ",\n";
        std::cout << "  \"validFiles\": " << summary.valid_files << ",\n";
        std::cout << "  \"invalidFiles\": " << summary.invalid_files << ",\n";

        std::cout << "  \"modalities\": {\n";
        size_t mod_count = 0;
        for (const auto& [modality, count] : summary.modalities) {
            std::cout << "    \"" << modality << "\": " << count;
            if (++mod_count < summary.modalities.size()) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "  },\n";

        std::cout << "  \"sopClasses\": {\n";
        size_t sop_count = 0;
        for (const auto& [sop_class, count] : summary.sop_classes) {
            std::cout << "    \"" << sop_class << "\": " << count;
            if (++sop_count < summary.sop_classes.size()) {
                std::cout << ",";
            }
            std::cout << "\n";
        }
        std::cout << "  }\n";
        std::cout << "}\n";
    } else {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "           Directory Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Total files:    " << summary.total_files << "\n";
        std::cout << "  Valid DICOM:    " << summary.valid_files << "\n";
        std::cout << "  Invalid/Other:  " << summary.invalid_files << "\n";
        std::cout << "\n";

        if (!summary.modalities.empty()) {
            std::cout << "  Modalities:\n";
            for (const auto& [modality, count] : summary.modalities) {
                std::cout << "    " << std::left << std::setw(10) << modality
                          << " " << count << " file(s)\n";
            }
            std::cout << "\n";
        }

        if (!summary.sop_classes.empty() && opts.verbose) {
            std::cout << "  SOP Classes:\n";
            for (const auto& [sop_class, count] : summary.sop_classes) {
                std::cout << "    " << sop_class << ": " << count
                          << " file(s)\n";
            }
        }

        std::cout << "========================================\n";
    }
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

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 1;
    }

    // Check if path exists
    if (!std::filesystem::exists(opts.path)) {
        std::cerr << "Error: Path does not exist: " << opts.path.string()
                  << "\n";
        return 2;
    }

    // Handle directory vs file
    if (std::filesystem::is_directory(opts.path)) {
        if (opts.summary) {
            std::cout << "Scanning directory: " << opts.path.string() << "\n";
            if (opts.recursive) {
                std::cout << "Mode: Recursive\n";
            }
            std::cout << "\n";

            scan_summary summary;
            scan_directory(opts.path, opts, summary);
            print_summary(summary, opts);
            return summary.invalid_files > 0 ? 1 : 0;
        } else {
            // Dump each file
            int exit_code = 0;
            auto process = [&](const std::filesystem::path& file_path) {
                auto ext = file_path.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".dcm" || ext == ".dicom" || ext.empty()) {
                    if (dump_file(file_path, opts) != 0) {
                        exit_code = 1;
                    }
                    std::cout << "\n";
                }
            };

            if (opts.recursive) {
                for (const auto& entry :
                     std::filesystem::recursive_directory_iterator(opts.path)) {
                    if (entry.is_regular_file()) {
                        process(entry.path());
                    }
                }
            } else {
                for (const auto& entry :
                     std::filesystem::directory_iterator(opts.path)) {
                    if (entry.is_regular_file()) {
                        process(entry.path());
                    }
                }
            }

            return exit_code;
        }
    } else {
        // Single file
        return dump_file(opts.path, opts);
    }
}
