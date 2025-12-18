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
enum class output_format { human_readable, json, xml };

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path path;
    std::vector<std::string> filter_tags;  // Keywords to filter
    std::string search_keyword;            // Search by tag name
    bool pixel_info{false};
    output_format format{output_format::human_readable};
    bool recursive{false};
    bool summary{false};
    bool show_meta{true};
    bool verbose{false};
    bool quiet{false};
    int max_depth{-1};      // -1 means unlimited
    bool no_pixel{false};   // Exclude pixel data
    bool show_private{false};
    std::string charset{"UTF-8"};
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
  -h, --help              Show this help message
  -v, --verbose           Verbose output mode
  -q, --quiet             Minimal output mode (errors only)
  -f, --format <format>   Output format: text (default), json, xml
  -t, --tag <tag>         Output specific tag only (e.g., 0010,0010)
  --tags <list>           Show only specific tags (comma-separated keywords)
                          Example: --tags PatientName,PatientID,StudyDate
  -s, --search <keyword>  Search by tag name (case-insensitive)
  -d, --depth <n>         Limit sequence output depth (default: unlimited)
  --pixel-info            Show pixel data information
  --no-pixel              Exclude pixel data from output
  --show-private          Show private tags (hidden by default)
  --charset <charset>     Specify character set (default: UTF-8)
  --recursive, -r         Recursively scan directories
  --summary               Show summary only (for directories)
  --no-meta               Don't show File Meta Information

Examples:
  )" << program_name
              << R"( image.dcm
  )" << program_name
              << R"( image.dcm --tags PatientName,PatientID,StudyDate
  )" << program_name
              << R"( image.dcm -t 0010,0010
  )" << program_name
              << R"( image.dcm --search Patient
  )" << program_name
              << R"( image.dcm --pixel-info
  )" << program_name
              << R"( image.dcm --format json
  )" << program_name
              << R"( image.dcm --format xml
  )" << program_name
              << R"( image.dcm -d 2             # Limit sequence depth to 2
  )" << program_name
              << R"( ./dicom_folder/ --recursive --summary

Output Format:
  Human-readable (text) output shows tags in the format:
    (GGGG,EEEE) VR Keyword                      [value]

  JSON output provides DICOM PS3.18-compatible structured data.
  XML output provides DICOM Native XML format (PS3.19).

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
/**
 * @brief Parse a tag string like "0010,0010" or "(0010,0010)"
 * @param tag_str The tag string
 * @return Parsed tag or empty string if invalid
 */
std::string parse_tag_string(const std::string& tag_str) {
    std::string s = tag_str;
    // Remove parentheses if present
    if (!s.empty() && s.front() == '(') s.erase(0, 1);
    if (!s.empty() && s.back() == ')') s.pop_back();
    // Validate format: should be GGGG,EEEE or GGGGEEEE
    if (s.length() == 9 && s[4] == ',') {
        return s;  // Already in GGGG,EEEE format
    } else if (s.length() == 8) {
        return s.substr(0, 4) + "," + s.substr(4, 4);
    }
    return "";
}

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
        } else if ((arg == "--tag" || arg == "-t") && i + 1 < argc) {
            // Parse single tag in format (GGGG,EEEE) or GGGG,EEEE
            std::string tag_str = parse_tag_string(argv[++i]);
            if (tag_str.empty()) {
                std::cerr << "Error: Invalid tag format. Use GGGG,EEEE (e.g., 0010,0010)\n";
                return false;
            }
            opts.filter_tags.push_back(tag_str);
        } else if ((arg == "--search" || arg == "-s") && i + 1 < argc) {
            opts.search_keyword = argv[++i];
        } else if ((arg == "--depth" || arg == "-d") && i + 1 < argc) {
            try {
                opts.max_depth = std::stoi(argv[++i]);
                if (opts.max_depth < 0) {
                    std::cerr << "Error: Depth must be non-negative\n";
                    return false;
                }
            } catch (...) {
                std::cerr << "Error: Invalid depth value\n";
                return false;
            }
        } else if (arg == "--pixel-info") {
            opts.pixel_info = true;
        } else if (arg == "--no-pixel") {
            opts.no_pixel = true;
        } else if (arg == "--show-private") {
            opts.show_private = true;
        } else if (arg == "--charset" && i + 1 < argc) {
            opts.charset = argv[++i];
        } else if ((arg == "--format" || arg == "-f") && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "json") {
                opts.format = output_format::json;
            } else if (fmt == "human" || fmt == "text") {
                opts.format = output_format::human_readable;
            } else if (fmt == "xml") {
                opts.format = output_format::xml;
            } else {
                std::cerr << "Error: Unknown format '" << fmt << "'. Use: text, json, xml\n";
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
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
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

    // Quiet mode overrides verbose
    if (opts.quiet) {
        opts.verbose = false;
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
        auto result = element.as_string();
        if (result.is_ok()) {
            std::string value = result.value();
            if (value.length() > max_length) {
                value = value.substr(0, max_length - 3) + "...";
            }
            return value;
        }
        // Fall through to raw data display on error
    }

    // Handle numeric VRs
    if (is_numeric_vr(vr)) {
        switch (vr) {
            case vr_type::US:
                if (auto val = element.as_numeric<uint16_t>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            case vr_type::SS:
                if (auto val = element.as_numeric<int16_t>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            case vr_type::UL:
                if (auto val = element.as_numeric<uint32_t>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            case vr_type::SL:
                if (auto val = element.as_numeric<int32_t>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            case vr_type::FL:
                if (auto val = element.as_numeric<float>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            case vr_type::FD:
                if (auto val = element.as_numeric<double>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            case vr_type::UV:
                if (auto val = element.as_numeric<uint64_t>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            case vr_type::SV:
                if (auto val = element.as_numeric<int64_t>(); val.is_ok())
                    return std::to_string(val.value());
                break;
            default:
                break;
        }
        // Fall through to raw data display on error
    }

    // Fallback: show raw bytes
    auto data = element.raw_data();
    return "[" + format_hex(data) + "]";
}

/**
 * @brief Case-insensitive string contains check
 * @param haystack String to search in
 * @param needle String to search for
 * @return true if needle is found (case-insensitive)
 */
bool contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    if (haystack.empty()) return false;

    std::string h = haystack;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

/**
 * @brief Check if a tag is a private tag (odd group number)
 * @param tag The DICOM tag
 * @return true if tag is private
 */
bool is_private_tag(const pacs::core::dicom_tag& tag) {
    return (tag.group() % 2) != 0;
}

/**
 * @brief Check if a tag is pixel data
 * @param tag The DICOM tag
 * @return true if tag is pixel data
 */
bool is_pixel_data_tag(const pacs::core::dicom_tag& tag) {
    return tag.group() == 0x7FE0 && tag.element() == 0x0010;
}

/**
 * @brief Check if a tag should be displayed based on filter
 * @param tag The DICOM tag
 * @param opts Command line options
 * @param dict Reference to the dictionary
 * @return true if tag should be displayed
 */
bool should_display_tag(const pacs::core::dicom_tag& tag,
                        const options& opts,
                        const pacs::core::dicom_dictionary& dict) {
    // Handle pixel data exclusion
    if (opts.no_pixel && is_pixel_data_tag(tag)) {
        return false;
    }

    // Handle private tags
    if (is_private_tag(tag) && !opts.show_private) {
        return false;
    }

    auto info = dict.find(tag);
    std::string keyword = info ? std::string(info->keyword) : "";

    // Handle search keyword
    if (!opts.search_keyword.empty()) {
        if (!contains_ci(keyword, opts.search_keyword) &&
            !contains_ci(tag.to_string(), opts.search_keyword)) {
            return false;
        }
    }

    // Handle tag filter list
    if (!opts.filter_tags.empty()) {
        for (const auto& filter : opts.filter_tags) {
            // Check if filter is a tag string (contains comma or is 8 hex chars)
            if (filter.find(',') != std::string::npos || filter.length() == 8) {
                // Compare as tag string
                std::string tag_str = tag.to_string();
                // Remove parentheses for comparison
                std::string tag_cmp = tag_str.substr(1, tag_str.length() - 2);
                if (tag_cmp == filter) {
                    return true;
                }
            } else {
                // Compare as keyword
                if (keyword == filter) {
                    return true;
                }
            }
        }
        return false;
    }

    return true;
}

/**
 * @brief Print dataset in human-readable format
 * @param dataset The dataset to print
 * @param opts Command line options
 * @param current_depth Current depth level (for depth limiting)
 * @param indent Indentation level for nested sequences
 */
void print_dataset_human(const pacs::core::dicom_dataset& dataset,
                         const options& opts, int current_depth = 0, int indent = 0) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();
    std::string indent_str(indent * 2, ' ');

    for (const auto& [tag, element] : dataset) {
        // Check filter
        if (!should_display_tag(tag, opts, dict)) {
            continue;
        }

        // Get tag info from dictionary
        auto info = dict.find(tag);
        std::string keyword =
            info ? std::string(info->keyword) : "UnknownTag";

        // Mark private tags
        if (is_private_tag(tag)) {
            keyword = "Private: " + keyword;
        }

        // Format: (GGGG,EEEE) VR Keyword                      [value]
        std::cout << indent_str << tag.to_string() << " "
                  << to_string(element.vr()) << " " << std::left
                  << std::setw(36 - indent * 2) << keyword;

        // Handle sequences specially
        if (element.is_sequence()) {
            const auto& items = element.sequence_items();
            std::cout << "(" << items.size() << " items)\n";

            // Check depth limit
            if (opts.max_depth >= 0 && current_depth >= opts.max_depth) {
                std::cout << indent_str << "  ... (depth limit reached)\n";
                continue;
            }

            int item_num = 0;
            for (const auto& item : items) {
                std::cout << indent_str << "  > Item #" << item_num++ << "\n";
                print_dataset_human(item, opts, current_depth + 1, indent + 2);
            }
        } else {
            std::cout << "[" << format_value(element) << "]\n";
        }
    }
}

/**
 * @brief Print dataset as JSON (DICOM PS3.18 compatible)
 * @param dataset The dataset to print
 * @param opts Command line options
 * @param current_depth Current depth level
 * @param indent Current indentation
 */
void print_dataset_json(const pacs::core::dicom_dataset& dataset,
                        const options& opts, int current_depth = 0, int indent = 2) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();
    std::string indent_str(indent, ' ');

    std::cout << "{\n";

    bool first = true;

    for (const auto& [tag, element] : dataset) {
        // Check filter
        if (!should_display_tag(tag, opts, dict)) {
            continue;
        }

        if (!first) {
            std::cout << ",\n";
        }
        first = false;

        auto info = dict.find(tag);
        std::string keyword =
            info ? std::string(info->keyword) : tag.to_string();

        // Use DICOM PS3.18 JSON format: tag as key in GGGGEEEE format
        std::string tag_key = tag.to_string();
        tag_key.erase(std::remove(tag_key.begin(), tag_key.end(), '('), tag_key.end());
        tag_key.erase(std::remove(tag_key.begin(), tag_key.end(), ')'), tag_key.end());
        tag_key.erase(std::remove(tag_key.begin(), tag_key.end(), ','), tag_key.end());

        std::cout << indent_str << "  \"" << tag_key << "\": {\n";
        std::cout << indent_str << "    \"vr\": \"" << to_string(element.vr()) << "\"";

        if (element.is_sequence()) {
            std::cout << ",\n" << indent_str << "    \"Value\": [\n";

            // Check depth limit
            if (opts.max_depth >= 0 && current_depth >= opts.max_depth) {
                std::cout << indent_str << "      { \"_note\": \"depth limit reached\" }\n";
            } else {
                const auto& items = element.sequence_items();
                for (size_t i = 0; i < items.size(); ++i) {
                    std::cout << indent_str << "      ";
                    print_dataset_json(items[i], opts, current_depth + 1, indent + 6);
                    if (i < items.size() - 1) {
                        std::cout << ",";
                    }
                    std::cout << "\n";
                }
            }
            std::cout << indent_str << "    ]\n";
        } else {
            std::string value = json_escape(format_value(element, 256));
            std::cout << ",\n" << indent_str << "    \"Value\": [\"" << value << "\"]\n";
        }

        std::cout << indent_str << "  }";
    }

    std::cout << "\n" << indent_str << "}";
}

/**
 * @brief Escape string for XML output
 * @param str Input string
 * @return XML-escaped string
 */
std::string xml_escape(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '<': oss << "&lt;"; break;
            case '>': oss << "&gt;"; break;
            case '&': oss << "&amp;"; break;
            case '"': oss << "&quot;"; break;
            case '\'': oss << "&apos;"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                    oss << "&#" << static_cast<int>(static_cast<unsigned char>(c)) << ";";
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

/**
 * @brief Print dataset as DICOM Native XML (PS3.19)
 * @param dataset The dataset to print
 * @param opts Command line options
 * @param current_depth Current depth level
 * @param indent Current indentation
 */
void print_dataset_xml(const pacs::core::dicom_dataset& dataset,
                       const options& opts, int current_depth = 0, int indent = 2) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();
    std::string indent_str(indent, ' ');

    for (const auto& [tag, element] : dataset) {
        // Check filter
        if (!should_display_tag(tag, opts, dict)) {
            continue;
        }

        auto info = dict.find(tag);
        std::string keyword = info ? std::string(info->keyword) : "UnknownTag";
        std::string vr_str{to_string(element.vr())};

        // Format tag as GGGGEEEE
        std::string tag_str = tag.to_string();
        tag_str.erase(std::remove(tag_str.begin(), tag_str.end(), '('), tag_str.end());
        tag_str.erase(std::remove(tag_str.begin(), tag_str.end(), ')'), tag_str.end());
        tag_str.erase(std::remove(tag_str.begin(), tag_str.end(), ','), tag_str.end());

        std::cout << indent_str << "<DicomAttribute tag=\"" << tag_str
                  << "\" vr=\"" << vr_str
                  << "\" keyword=\"" << xml_escape(keyword) << "\"";

        if (element.is_sequence()) {
            std::cout << ">\n";

            // Check depth limit
            if (opts.max_depth >= 0 && current_depth >= opts.max_depth) {
                std::cout << indent_str << "  <!-- depth limit reached -->\n";
            } else {
                const auto& items = element.sequence_items();
                int item_num = 1;
                for (const auto& item : items) {
                    std::cout << indent_str << "  <Item number=\"" << item_num++ << "\">\n";
                    print_dataset_xml(item, opts, current_depth + 1, indent + 4);
                    std::cout << indent_str << "  </Item>\n";
                }
            }
            std::cout << indent_str << "</DicomAttribute>\n";
        } else if (element.is_empty()) {
            std::cout << "/>\n";
        } else {
            std::cout << ">\n";

            // Handle PersonName VR specially
            if (element.vr() == vr_type::PN) {
                auto result = element.as_string();
                if (result.is_ok()) {
                    std::cout << indent_str << "  <PersonName>\n";
                    std::cout << indent_str << "    <Alphabetic>\n";
                    std::cout << indent_str << "      <FamilyName>" << xml_escape(result.value()) << "</FamilyName>\n";
                    std::cout << indent_str << "    </Alphabetic>\n";
                    std::cout << indent_str << "  </PersonName>\n";
                }
            } else {
                std::string value = format_value(element, 1024);
                std::cout << indent_str << "  <Value number=\"1\">" << xml_escape(value) << "</Value>\n";
            }

            std::cout << indent_str << "</DicomAttribute>\n";
        }
    }
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
    if (result.is_err()) {
        std::cerr << "Error: Failed to open '" << file_path.string()
                  << "': " << result.error().message << "\n";
        return 2;
    }

    // Quiet mode: only show errors
    if (opts.quiet) {
        return 0;
    }

    auto& file = result.value();

    if (opts.format == output_format::json) {
        // JSON output (DICOM PS3.18 compatible)
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
    } else if (opts.format == output_format::xml) {
        // XML output (DICOM Native XML PS3.19)
        std::cout << "<?xml version=\"1.0\" encoding=\"" << opts.charset << "\"?>\n";
        std::cout << "<NativeDicomModel>\n";
        std::cout << "  <!-- File: " << xml_escape(file_path.string()) << " -->\n";
        std::cout << "  <!-- Transfer Syntax: " << file.transfer_syntax().name() << " -->\n";
        std::cout << "  <!-- SOP Class: " << file.sop_class_uid() << " -->\n";
        std::cout << "  <!-- SOP Instance: " << file.sop_instance_uid() << " -->\n";

        if (opts.show_meta) {
            std::cout << "  <!-- File Meta Information -->\n";
            print_dataset_xml(file.meta_information(), opts);
        }

        std::cout << "  <!-- Dataset -->\n";
        print_dataset_xml(file.dataset(), opts);
        std::cout << "</NativeDicomModel>\n";
    } else {
        // Human-readable (text) output
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
        if (result.is_err()) {
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
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        // Show banner for help
        std::cout << R"(
  ____   ____ __  __   ____  _   _ __  __ ____
 |  _ \ / ___|  \/  | |  _ \| | | |  \/  |  _ \
 | | | | |   | |\/| | | | | | | | | |\/| | |_) |
 | |_| | |___| |  | | | |_| | |_| | |  | |  __/
 |____/ \____|_|  |_| |____/ \___/|_|  |_|_|

        DICOM File Inspection Utility
)" << "\n";
        print_usage(argv[0]);
        return 1;
    }

    // Check if path exists
    if (!std::filesystem::exists(opts.path)) {
        std::cerr << "Error: Path does not exist: " << opts.path.string()
                  << "\n";
        return 2;
    }

    // Show banner only in non-quiet text/human mode
    if (!opts.quiet && opts.format == output_format::human_readable) {
        std::cout << R"(
  ____   ____ __  __   ____  _   _ __  __ ____
 |  _ \ / ___|  \/  | |  _ \| | | |  \/  |  _ \
 | | | | |   | |\/| | | | | | | | | |\/| | |_) |
 | |_| | |___| |  | | | |_| | |_| | |  | |  __/
 |____/ \____|_|  |_| |____/ \___/|_|  |_|_|

        DICOM File Inspection Utility
)" << "\n";
    }

    // Handle directory vs file
    if (std::filesystem::is_directory(opts.path)) {
        if (opts.summary) {
            if (!opts.quiet) {
                std::cout << "Scanning directory: " << opts.path.string() << "\n";
                if (opts.recursive) {
                    std::cout << "Mode: Recursive\n";
                }
                std::cout << "\n";
            }

            scan_summary summary;
            scan_directory(opts.path, opts, summary);
            if (!opts.quiet) {
                print_summary(summary, opts);
            }
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
