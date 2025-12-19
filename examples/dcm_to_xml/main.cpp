/**
 * @file main.cpp
 * @brief DICOM to XML Converter - DICOM Native XML PS3.19
 *
 * A command-line utility for converting DICOM files to XML format
 * following the DICOM Native XML representation standard (PS3.19).
 *
 * @see Issue #283 - dcm_to_xml / xml_to_dcm Implementation
 * @see DICOM PS3.19 - Application Hosting
 *
 * Usage:
 *   dcm_to_xml <dicom-file> [output-file] [options]
 *
 * Example:
 *   dcm_to_xml image.dcm
 *   dcm_to_xml image.dcm output.xml --pretty
 *   dcm_to_xml image.dcm --bulk-data exclude --no-pixel
 */

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Bulk data handling mode
 */
enum class bulk_data_mode {
    inline_base64,  ///< Include binary data as Base64 (InlineBinary)
    uri,            ///< Save to file and reference by BulkDataURI
    exclude         ///< Completely exclude binary data
};

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    bool pretty_print{true};
    bool compact{false};
    bulk_data_mode bulk_mode{bulk_data_mode::exclude};
    std::string bulk_data_uri_prefix{"file://"};
    std::filesystem::path bulk_data_dir;
    std::vector<std::string> filter_tags;
    bool no_pixel{false};
    bool recursive{false};
    bool include_meta{true};
    bool verbose{false};
    bool quiet{false};
    std::string charset{"UTF-8"};
};

/**
 * @brief Base64 encoding table
 */
constexpr char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

/**
 * @brief Encode binary data to Base64
 * @param data The binary data to encode
 * @return Base64 encoded string
 */
[[nodiscard]] std::string to_base64(std::span<const uint8_t> data) {
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;

        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        result += base64_chars[(triple >> 18) & 0x3F];
        result += base64_chars[(triple >> 12) & 0x3F];
        result += (i > data.size() + 1) ? '=' : base64_chars[(triple >> 6) & 0x3F];
        result += (i > data.size()) ? '=' : base64_chars[triple & 0x3F];
    }

    return result;
}

/**
 * @brief Escape string for XML output
 * @param str Input string
 * @return XML-escaped string
 */
[[nodiscard]] std::string xml_escape(const std::string& str) {
    std::ostringstream oss;
    for (unsigned char c : str) {
        switch (c) {
            case '<':
                oss << "&lt;";
                break;
            case '>':
                oss << "&gt;";
                break;
            case '&':
                oss << "&amp;";
                break;
            case '"':
                oss << "&quot;";
                break;
            case '\'':
                oss << "&apos;";
                break;
            default:
                if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                    oss << "&#" << static_cast<int>(c) << ";";
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

/**
 * @brief Format tag as 8-character hex string (GGGGEEEE)
 * @param tag The DICOM tag
 * @return Formatted tag string
 */
[[nodiscard]] std::string format_tag_key(const pacs::core::dicom_tag& tag) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0')
        << std::setw(4) << tag.group()
        << std::setw(4) << tag.element();
    return oss.str();
}

/**
 * @brief Check if VR is binary (requires BulkData handling)
 * @param vr The value representation
 * @return true if binary VR
 */
[[nodiscard]] bool is_bulk_data_vr(pacs::encoding::vr_type vr) {
    using namespace pacs::encoding;
    return vr == vr_type::OB || vr == vr_type::OD || vr == vr_type::OF ||
           vr == vr_type::OL || vr == vr_type::OV || vr == vr_type::OW ||
           vr == vr_type::UN;
}

/**
 * @brief Check if tag is pixel data
 * @param tag The DICOM tag
 * @return true if pixel data tag
 */
[[nodiscard]] bool is_pixel_data_tag(const pacs::core::dicom_tag& tag) {
    return tag.group() == 0x7FE0 && tag.element() == 0x0010;
}

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
DICOM to XML Converter (DICOM Native XML PS3.19)

Usage: )" << program_name
              << R"( <dicom-file> [output-file] [options]

Arguments:
  dicom-file        Input DICOM file or directory
  output-file       Output XML file (optional, stdout if omitted)

Options:
  -h, --help              Show this help message
  -p, --pretty            Pretty-print formatting (default)
  -c, --compact           Compact output (no formatting)
  --bulk-data <mode>      Binary data handling:
                            inline  - Include as Base64 (InlineBinary)
                            uri     - Save to file, reference by URI
                            exclude - Completely exclude (default)
  --bulk-data-uri <pfx>   BulkDataURI prefix (default: file://)
  --bulk-data-dir <dir>   Directory for bulk data files
  -t, --tag <tag>         Output specific tag only (e.g., 0010,0010)
  --no-pixel              Exclude pixel data
  --no-meta               Exclude File Meta Information
  --charset <charset>     XML encoding declaration (default: UTF-8)
  -r, --recursive         Process directories recursively
  -v, --verbose           Verbose output
  -q, --quiet             Quiet mode (errors only)

Examples:
  )" << program_name
              << R"( image.dcm
  )" << program_name
              << R"( image.dcm output.xml --pretty
  )" << program_name
              << R"( image.dcm --bulk-data inline
  )" << program_name
              << R"( image.dcm --bulk-data uri --bulk-data-dir ./bulk/
  )" << program_name
              << R"( image.dcm -t 0010,0010 -t 0010,0020
  )" << program_name
              << R"( ./dicom_folder/ --recursive --no-pixel

Output Format (DICOM Native XML PS3.19):
  <?xml version="1.0" encoding="UTF-8"?>
  <NativeDicomModel>
    <DicomAttribute tag="00100010" vr="PN" keyword="PatientName">
      <PersonName>
        <Alphabetic>
          <FamilyName>DOE</FamilyName>
          <GivenName>JOHN</GivenName>
        </Alphabetic>
      </PersonName>
    </DicomAttribute>
  </NativeDicomModel>

Exit Codes:
  0  Success
  1  Invalid arguments
  2  File error or invalid DICOM
)";
}

/**
 * @brief Parse tag string like "0010,0010" or "(0010,0010)"
 * @param tag_str The tag string
 * @return Parsed tag or nullopt if invalid
 */
[[nodiscard]] std::optional<pacs::core::dicom_tag> parse_tag_string(
    const std::string& tag_str) {
    std::string s = tag_str;
    // Remove parentheses if present
    if (!s.empty() && s.front() == '(') s.erase(0, 1);
    if (!s.empty() && s.back() == ')') s.pop_back();
    // Remove comma
    s.erase(std::remove(s.begin(), s.end(), ','), s.end());

    if (s.length() != 8) {
        return std::nullopt;
    }

    try {
        uint16_t group = static_cast<uint16_t>(std::stoul(s.substr(0, 4), nullptr, 16));
        uint16_t elem = static_cast<uint16_t>(std::stoul(s.substr(4, 4), nullptr, 16));
        return pacs::core::dicom_tag{group, elem};
    } catch (...) {
        return std::nullopt;
    }
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
        } else if (arg == "--pretty" || arg == "-p") {
            opts.pretty_print = true;
            opts.compact = false;
        } else if (arg == "--compact" || arg == "-c") {
            opts.compact = true;
            opts.pretty_print = false;
        } else if (arg == "--bulk-data" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "inline") {
                opts.bulk_mode = bulk_data_mode::inline_base64;
            } else if (mode == "uri") {
                opts.bulk_mode = bulk_data_mode::uri;
            } else if (mode == "exclude") {
                opts.bulk_mode = bulk_data_mode::exclude;
            } else {
                std::cerr << "Error: Unknown bulk-data mode '" << mode
                          << "'. Use: inline, uri, exclude\n";
                return false;
            }
        } else if (arg == "--bulk-data-uri" && i + 1 < argc) {
            opts.bulk_data_uri_prefix = argv[++i];
        } else if (arg == "--bulk-data-dir" && i + 1 < argc) {
            opts.bulk_data_dir = argv[++i];
        } else if ((arg == "--tag" || arg == "-t") && i + 1 < argc) {
            opts.filter_tags.push_back(argv[++i]);
        } else if (arg == "--no-pixel") {
            opts.no_pixel = true;
        } else if (arg == "--no-meta") {
            opts.include_meta = false;
        } else if (arg == "--charset" && i + 1 < argc) {
            opts.charset = argv[++i];
        } else if (arg == "--recursive" || arg == "-r") {
            opts.recursive = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else if (opts.input_path.empty()) {
            opts.input_path = arg;
        } else if (opts.output_path.empty()) {
            opts.output_path = arg;
        } else {
            std::cerr << "Error: Too many arguments\n";
            return false;
        }
    }

    if (opts.input_path.empty()) {
        std::cerr << "Error: No input file specified\n";
        return false;
    }

    if (opts.quiet) {
        opts.verbose = false;
    }

    return true;
}

/**
 * @brief Check if tag should be included in output
 * @param tag The DICOM tag
 * @param opts Command line options
 * @return true if tag should be included
 */
[[nodiscard]] bool should_include_tag(const pacs::core::dicom_tag& tag,
                                       const options& opts) {
    // Exclude pixel data if requested
    if (opts.no_pixel && is_pixel_data_tag(tag)) {
        return false;
    }

    // If filter tags specified, only include those
    if (!opts.filter_tags.empty()) {
        for (const auto& filter : opts.filter_tags) {
            auto parsed = parse_tag_string(filter);
            if (parsed && *parsed == tag) {
                return true;
            }
        }
        return false;
    }

    return true;
}

// Forward declaration
void write_dataset_xml(std::ostream& out,
                       const pacs::core::dicom_dataset& dataset,
                       const options& opts,
                       const std::filesystem::path& base_path,
                       int indent_level,
                       size_t& bulk_data_counter);

/**
 * @brief Parse PersonName components from a string
 * @param pn_string The PersonName string
 * @return Tuple of (family, given, middle, prefix, suffix)
 */
[[nodiscard]] std::tuple<std::string, std::string, std::string, std::string, std::string>
parse_person_name_components(const std::string& pn_string) {
    std::vector<std::string> components;
    std::stringstream ss(pn_string);
    std::string component;
    while (std::getline(ss, component, '^')) {
        components.push_back(component);
    }

    std::string family = components.size() > 0 ? components[0] : "";
    std::string given = components.size() > 1 ? components[1] : "";
    std::string middle = components.size() > 2 ? components[2] : "";
    std::string prefix = components.size() > 3 ? components[3] : "";
    std::string suffix = components.size() > 4 ? components[4] : "";

    return {family, given, middle, prefix, suffix};
}

/**
 * @brief Write element value as XML
 * @param out Output stream
 * @param element The DICOM element
 * @param opts Command line options
 * @param base_path Base path for bulk data files
 * @param indent_level Current indentation level
 * @param bulk_data_counter Counter for bulk data files
 */
void write_element_value_xml(std::ostream& out,
                              const pacs::core::dicom_element& element,
                              const pacs::core::dicom_tag& tag,
                              const options& opts,
                              const std::filesystem::path& base_path,
                              int indent_level,
                              size_t& bulk_data_counter) {
    using namespace pacs::encoding;
    using namespace pacs::core;

    const std::string indent = opts.compact ? "" : std::string(indent_level * 2, ' ');
    const std::string newline = opts.compact ? "" : "\n";

    auto vr = element.vr();
    auto vr_str = to_string(vr);

    // Get keyword from dictionary
    auto& dict = dicom_dictionary::instance();
    auto info = dict.find(tag);
    std::string keyword = info ? std::string(info->keyword) : "";

    // Build opening tag
    std::ostringstream attr_oss;
    attr_oss << indent << "<DicomAttribute tag=\"" << format_tag_key(tag)
             << "\" vr=\"" << vr_str << "\"";
    if (!keyword.empty()) {
        attr_oss << " keyword=\"" << xml_escape(keyword) << "\"";
    }

    // Handle sequences
    if (element.is_sequence()) {
        out << attr_oss.str() << ">" << newline;

        const auto& items = element.sequence_items();
        int item_num = 1;
        for (const auto& item : items) {
            out << indent << "  <Item number=\"" << item_num++ << "\">" << newline;
            write_dataset_xml(out, item, opts, base_path, indent_level + 2, bulk_data_counter);
            out << indent << "  </Item>" << newline;
        }

        out << indent << "</DicomAttribute>" << newline;
        return;
    }

    // Handle empty elements
    if (element.is_empty()) {
        out << attr_oss.str() << "/>" << newline;
        return;
    }

    // Handle bulk data (binary VRs)
    if (is_bulk_data_vr(vr)) {
        switch (opts.bulk_mode) {
            case bulk_data_mode::inline_base64: {
                auto data = element.raw_data();
                out << attr_oss.str() << ">" << newline;
                out << indent << "  <InlineBinary>" << to_base64(data) << "</InlineBinary>" << newline;
                out << indent << "</DicomAttribute>" << newline;
                break;
            }
            case bulk_data_mode::uri: {
                std::string filename = "bulk_" + std::to_string(bulk_data_counter++) + ".raw";
                std::filesystem::path bulk_path = opts.bulk_data_dir.empty()
                    ? base_path / filename
                    : opts.bulk_data_dir / filename;

                // Write bulk data to file
                std::ofstream bulk_file(bulk_path, std::ios::binary);
                if (bulk_file) {
                    auto data = element.raw_data();
                    bulk_file.write(reinterpret_cast<const char*>(data.data()),
                                    static_cast<std::streamsize>(data.size()));
                }

                std::string uri = opts.bulk_data_uri_prefix + bulk_path.string();
                out << attr_oss.str() << ">" << newline;
                out << indent << "  <BulkData uri=\"" << xml_escape(uri) << "\"/>" << newline;
                out << indent << "</DicomAttribute>" << newline;
                break;
            }
            case bulk_data_mode::exclude:
            default:
                out << attr_oss.str() << "/>" << newline;
                break;
        }
        return;
    }

    // Handle PersonName (PN) VR specially per PS3.19
    if (vr == vr_type::PN) {
        out << attr_oss.str() << ">" << newline;

        auto result = element.as_string();
        if (result.is_ok()) {
            std::string value = result.value();
            // Split by backslash for VM > 1
            std::vector<std::string> names;
            std::stringstream ss(value);
            std::string name;
            while (std::getline(ss, name, '\\')) {
                names.push_back(name);
            }

            int pn_number = 1;
            for (const auto& pn : names) {
                // Split by '=' for different name representations
                std::vector<std::string> representations;
                std::stringstream rep_ss(pn);
                std::string rep;
                while (std::getline(rep_ss, rep, '=')) {
                    representations.push_back(rep);
                }

                out << indent << "  <PersonName number=\"" << pn_number++ << "\">" << newline;

                // Alphabetic representation
                if (!representations.empty() && !representations[0].empty()) {
                    auto [family, given, middle, prefix, suffix] =
                        parse_person_name_components(representations[0]);
                    out << indent << "    <Alphabetic>" << newline;
                    if (!family.empty())
                        out << indent << "      <FamilyName>" << xml_escape(family) << "</FamilyName>" << newline;
                    if (!given.empty())
                        out << indent << "      <GivenName>" << xml_escape(given) << "</GivenName>" << newline;
                    if (!middle.empty())
                        out << indent << "      <MiddleName>" << xml_escape(middle) << "</MiddleName>" << newline;
                    if (!prefix.empty())
                        out << indent << "      <NamePrefix>" << xml_escape(prefix) << "</NamePrefix>" << newline;
                    if (!suffix.empty())
                        out << indent << "      <NameSuffix>" << xml_escape(suffix) << "</NameSuffix>" << newline;
                    out << indent << "    </Alphabetic>" << newline;
                }

                // Ideographic representation
                if (representations.size() > 1 && !representations[1].empty()) {
                    auto [family, given, middle, prefix, suffix] =
                        parse_person_name_components(representations[1]);
                    out << indent << "    <Ideographic>" << newline;
                    if (!family.empty())
                        out << indent << "      <FamilyName>" << xml_escape(family) << "</FamilyName>" << newline;
                    if (!given.empty())
                        out << indent << "      <GivenName>" << xml_escape(given) << "</GivenName>" << newline;
                    if (!middle.empty())
                        out << indent << "      <MiddleName>" << xml_escape(middle) << "</MiddleName>" << newline;
                    if (!prefix.empty())
                        out << indent << "      <NamePrefix>" << xml_escape(prefix) << "</NamePrefix>" << newline;
                    if (!suffix.empty())
                        out << indent << "      <NameSuffix>" << xml_escape(suffix) << "</NameSuffix>" << newline;
                    out << indent << "    </Ideographic>" << newline;
                }

                // Phonetic representation
                if (representations.size() > 2 && !representations[2].empty()) {
                    auto [family, given, middle, prefix, suffix] =
                        parse_person_name_components(representations[2]);
                    out << indent << "    <Phonetic>" << newline;
                    if (!family.empty())
                        out << indent << "      <FamilyName>" << xml_escape(family) << "</FamilyName>" << newline;
                    if (!given.empty())
                        out << indent << "      <GivenName>" << xml_escape(given) << "</GivenName>" << newline;
                    if (!middle.empty())
                        out << indent << "      <MiddleName>" << xml_escape(middle) << "</MiddleName>" << newline;
                    if (!prefix.empty())
                        out << indent << "      <NamePrefix>" << xml_escape(prefix) << "</NamePrefix>" << newline;
                    if (!suffix.empty())
                        out << indent << "      <NameSuffix>" << xml_escape(suffix) << "</NameSuffix>" << newline;
                    out << indent << "    </Phonetic>" << newline;
                }

                out << indent << "  </PersonName>" << newline;
            }
        }

        out << indent << "</DicomAttribute>" << newline;
        return;
    }

    // Handle string VRs
    if (is_string_vr(vr)) {
        out << attr_oss.str() << ">" << newline;

        auto result = element.as_string();
        if (result.is_ok()) {
            std::string value = result.value();
            // Split by backslash for VM > 1
            std::vector<std::string> values;
            std::stringstream ss(value);
            std::string v;
            while (std::getline(ss, v, '\\')) {
                values.push_back(v);
            }

            int val_number = 1;
            for (const auto& val : values) {
                out << indent << "  <Value number=\"" << val_number++ << "\">"
                    << xml_escape(val) << "</Value>" << newline;
            }
        }

        out << indent << "</DicomAttribute>" << newline;
        return;
    }

    // Handle numeric VRs
    if (is_numeric_vr(vr)) {
        out << attr_oss.str() << ">" << newline;

        auto write_numeric_values = [&]<typename T>() {
            auto result = element.as_numeric_list<T>();
            if (result.is_ok()) {
                const auto& values = result.value();
                int val_number = 1;
                for (const auto& val : values) {
                    out << indent << "  <Value number=\"" << val_number++ << "\">";
                    if constexpr (std::is_floating_point_v<T>) {
                        out << std::setprecision(17) << val;
                    } else {
                        out << static_cast<int64_t>(val);
                    }
                    out << "</Value>" << newline;
                }
            }
        };

        switch (vr) {
            case vr_type::US:
                write_numeric_values.template operator()<uint16_t>();
                break;
            case vr_type::SS:
                write_numeric_values.template operator()<int16_t>();
                break;
            case vr_type::UL:
                write_numeric_values.template operator()<uint32_t>();
                break;
            case vr_type::SL:
                write_numeric_values.template operator()<int32_t>();
                break;
            case vr_type::FL:
                write_numeric_values.template operator()<float>();
                break;
            case vr_type::FD:
                write_numeric_values.template operator()<double>();
                break;
            case vr_type::UV:
                write_numeric_values.template operator()<uint64_t>();
                break;
            case vr_type::SV:
                write_numeric_values.template operator()<int64_t>();
                break;
            default:
                // Fallback: try as string
                if (auto result = element.as_string(); result.is_ok()) {
                    out << indent << "  <Value number=\"1\">" << xml_escape(result.value())
                        << "</Value>" << newline;
                }
                break;
        }

        out << indent << "</DicomAttribute>" << newline;
        return;
    }

    // Handle AT (Attribute Tag) VR
    if (vr == vr_type::AT) {
        out << attr_oss.str() << ">" << newline;

        auto data = element.raw_data();
        int val_number = 1;
        for (size_t i = 0; i + 4 <= data.size(); i += 4) {
            uint16_t group = static_cast<uint16_t>(data[i] | (data[i + 1] << 8));
            uint16_t elem = static_cast<uint16_t>(data[i + 2] | (data[i + 3] << 8));
            out << indent << "  <Value number=\"" << val_number++ << "\">";
            out << std::hex << std::uppercase << std::setfill('0')
                << std::setw(4) << group << std::setw(4) << elem;
            out << "</Value>" << newline << std::dec;
        }

        out << indent << "</DicomAttribute>" << newline;
        return;
    }

    // Fallback: treat as string
    out << attr_oss.str() << ">" << newline;
    if (auto result = element.as_string(); result.is_ok()) {
        out << indent << "  <Value number=\"1\">" << xml_escape(result.value())
            << "</Value>" << newline;
    }
    out << indent << "</DicomAttribute>" << newline;
}

/**
 * @brief Write dataset as XML
 * @param out Output stream
 * @param dataset The DICOM dataset
 * @param opts Command line options
 * @param base_path Base path for bulk data files
 * @param indent_level Current indentation level
 * @param bulk_data_counter Counter for bulk data files
 */
void write_dataset_xml(std::ostream& out,
                       const pacs::core::dicom_dataset& dataset,
                       const options& opts,
                       const std::filesystem::path& base_path,
                       int indent_level,
                       size_t& bulk_data_counter) {
    for (const auto& [tag, element] : dataset) {
        if (!should_include_tag(tag, opts)) {
            continue;
        }

        write_element_value_xml(out, element, tag, opts, base_path,
                                 indent_level, bulk_data_counter);
    }
}

/**
 * @brief Convert a single DICOM file to XML
 * @param input_path Path to the DICOM file
 * @param output Output stream
 * @param opts Command line options
 * @return 0 on success, non-zero on error
 */
int convert_file(const std::filesystem::path& input_path,
                 std::ostream& output,
                 const options& opts) {
    using namespace pacs::core;

    auto result = dicom_file::open(input_path);
    if (result.is_err()) {
        std::cerr << "Error: Failed to open '" << input_path.string()
                  << "': " << result.error().message << "\n";
        return 2;
    }

    auto& file = result.value();
    size_t bulk_data_counter = 0;
    std::filesystem::path base_path = input_path.parent_path();

    const std::string newline = opts.compact ? "" : "\n";

    // XML declaration
    output << "<?xml version=\"1.0\" encoding=\"" << opts.charset << "\"?>" << newline;

    // Root element
    output << "<NativeDicomModel";
    output << " xmlns=\"http://dicom.nema.org/PS3.19/models/NativeDICOM\"";
    output << ">" << newline;

    // Include File Meta Information if requested
    if (opts.include_meta && !file.meta_information().empty()) {
        write_dataset_xml(output, file.meta_information(), opts, base_path, 1, bulk_data_counter);
    }

    // Write main dataset
    write_dataset_xml(output, file.dataset(), opts, base_path, 1, bulk_data_counter);

    output << "</NativeDicomModel>" << newline;

    return 0;
}

/**
 * @brief Process directory of DICOM files
 * @param dir_path Path to the directory
 * @param opts Command line options
 * @return 0 on success, non-zero on error
 */
int process_directory(const std::filesystem::path& dir_path, const options& opts) {
    int exit_code = 0;

    auto process = [&](const std::filesystem::path& file_path) {
        auto ext = file_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".dcm" && ext != ".dicom" && !ext.empty()) {
            return;
        }

        // Generate output filename
        std::filesystem::path output_path = file_path;
        output_path.replace_extension(".xml");

        if (!opts.quiet) {
            std::cout << "Converting: " << file_path.string() << " -> "
                      << output_path.string() << "\n";
        }

        std::ofstream output(output_path);
        if (!output) {
            std::cerr << "Error: Cannot create output file: " << output_path.string() << "\n";
            exit_code = 2;
            return;
        }

        if (convert_file(file_path, output, opts) != 0) {
            exit_code = 2;
        }
    };

    if (opts.recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                process(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                process(entry.path());
            }
        }
    }

    return exit_code;
}

}  // namespace

int main(int argc, char* argv[]) {
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        std::cout << R"(
  ____   ____ __  __   _____  ___   __  ____  __ _
 |  _ \ / ___|  \/  | |_   _|/ _ \  \ \/ /  \/  | |
 | | | | |   | |\/| |   | | | | | |  \  /| |\/| | |
 | |_| | |___| |  | |   | | | |_| |  /  \| |  | | |___
 |____/ \____|_|  |_|   |_|  \___/  /_/\_\_|  |_|_____|

        DICOM to XML Converter (PS3.19)
)" << "\n";
        print_usage(argv[0]);
        return 1;
    }

    // Check if input path exists
    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: Path does not exist: " << opts.input_path.string() << "\n";
        return 2;
    }

    // Create bulk data directory if needed
    if (opts.bulk_mode == bulk_data_mode::uri && !opts.bulk_data_dir.empty()) {
        std::filesystem::create_directories(opts.bulk_data_dir);
    }

    // Show banner in non-quiet mode
    if (!opts.quiet) {
        std::cout << R"(
  ____   ____ __  __   _____  ___   __  ____  __ _
 |  _ \ / ___|  \/  | |_   _|/ _ \  \ \/ /  \/  | |
 | | | | |   | |\/| |   | | | | | |  \  /| |\/| | |
 | |_| | |___| |  | |   | | | |_| |  /  \| |  | | |___
 |____/ \____|_|  |_|   |_|  \___/  /_/\_\_|  |_|_____|

        DICOM to XML Converter (PS3.19)
)" << "\n";
    }

    // Handle directory vs file
    if (std::filesystem::is_directory(opts.input_path)) {
        return process_directory(opts.input_path, opts);
    } else {
        // Single file
        if (opts.output_path.empty()) {
            // Output to stdout
            return convert_file(opts.input_path, std::cout, opts);
        } else {
            // Output to file
            std::ofstream output(opts.output_path);
            if (!output) {
                std::cerr << "Error: Cannot create output file: "
                          << opts.output_path.string() << "\n";
                return 2;
            }
            return convert_file(opts.input_path, output, opts);
        }
    }
}
