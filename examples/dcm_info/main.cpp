/**
 * @file main.cpp
 * @brief DICOM Info - File Summary Utility
 *
 * A command-line utility for displaying summary information of DICOM files,
 * providing a quick overview of patient, study, series, and image metadata.
 *
 * @see Issue #378 - dcm_info: Implement DICOM file summary utility
 * @see Parent Issue #278 - DICOM File Utilities Implementation
 * @see DICOM PS3.10 - Media Storage and File Format
 *
 * Usage:
 *   dcm_info <path> [options]
 *
 * Example:
 *   dcm_info image.dcm
 *   dcm_info image.dcm --format json
 *   dcm_info ./dicom_folder/ --recursive
 */

#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Output format options
 */
enum class output_format { text, json };

/**
 * @brief Command line options
 */
struct options {
    std::vector<std::filesystem::path> paths;
    output_format format{output_format::text};
    bool recursive{false};
    bool verbose{false};
    bool quiet{false};
    bool show_file_info{true};
};

/**
 * @brief DICOM file summary information
 */
struct dicom_summary {
    // File info
    std::string file_path;
    std::uintmax_t file_size{0};
    std::string transfer_syntax;
    std::string transfer_syntax_uid;
    std::string sop_class_uid;
    std::string sop_instance_uid;

    // Patient info
    std::string patient_name;
    std::string patient_id;
    std::string patient_birth_date;
    std::string patient_sex;

    // Study info
    std::string study_date;
    std::string study_time;
    std::string study_description;
    std::string study_instance_uid;
    std::string accession_number;

    // Series info
    std::string modality;
    std::string series_number;
    std::string series_description;
    std::string series_instance_uid;

    // Instance info
    std::string instance_number;
    std::string acquisition_date;
    std::string acquisition_time;

    // Image info
    uint16_t rows{0};
    uint16_t columns{0};
    uint16_t bits_allocated{0};
    uint16_t bits_stored{0};
    uint16_t samples_per_pixel{0};
    std::string photometric_interpretation;
    std::string number_of_frames;
    size_t pixel_data_size{0};
    bool has_pixel_data{false};
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
DICOM Info - File Summary Utility

Usage: )" << program_name
              << R"( <path> [path2 ...] [options]

Arguments:
  path              DICOM file(s) or directory to inspect

Options:
  -h, --help        Show this help message
  -v, --verbose     Verbose output (show all available fields)
  -q, --quiet       Minimal output (file path and basic info only)
  -f, --format <f>  Output format: text (default), json
  -r, --recursive   Recursively scan directories
  --no-file-info    Don't show file information (size, transfer syntax)

Examples:
  )" << program_name
              << R"( image.dcm
  )" << program_name
              << R"( image1.dcm image2.dcm image3.dcm
  )" << program_name
              << R"( image.dcm --format json
  )" << program_name
              << R"( ./dicom_folder/ --recursive
  )" << program_name
              << R"( ./dicom_folder/ -r -q

Output:
  Displays summary information organized by:
  - File: Path, size, transfer syntax
  - Patient: Name, ID, birth date, sex
  - Study: Date, description, accession number
  - Series: Modality, number, description
  - Image: Dimensions, bits, photometric interpretation

Exit Codes:
  0  Success
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
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            opts.quiet = true;
        } else if (arg == "--recursive" || arg == "-r") {
            opts.recursive = true;
        } else if (arg == "--no-file-info") {
            opts.show_file_info = false;
        } else if ((arg == "--format" || arg == "-f") && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "json") {
                opts.format = output_format::json;
            } else if (fmt == "text") {
                opts.format = output_format::text;
            } else {
                std::cerr << "Error: Unknown format '" << fmt
                          << "'. Use: text, json\n";
                return false;
            }
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else {
            opts.paths.emplace_back(arg);
        }
    }

    if (opts.paths.empty()) {
        std::cerr << "Error: No path specified\n";
        return false;
    }

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
 * @brief Format file size for human-readable output
 * @param size Size in bytes
 * @return Formatted string
 */
std::string format_file_size(std::uintmax_t size) {
    std::ostringstream oss;
    if (size >= 1024 * 1024 * 1024) {
        oss << std::fixed << std::setprecision(2)
            << static_cast<double>(size) / (1024 * 1024 * 1024) << " GB";
    } else if (size >= 1024 * 1024) {
        oss << std::fixed << std::setprecision(2)
            << static_cast<double>(size) / (1024 * 1024) << " MB";
    } else if (size >= 1024) {
        oss << std::fixed << std::setprecision(2)
            << static_cast<double>(size) / 1024 << " KB";
    } else {
        oss << size << " bytes";
    }
    return oss.str();
}

/**
 * @brief Extract summary information from a DICOM file
 * @param file_path Path to the DICOM file
 * @param summary Output: summary information
 * @return true on success
 */
bool extract_summary(const std::filesystem::path& file_path,
                     dicom_summary& summary) {
    using namespace pacs::core;

    auto result = dicom_file::open(file_path);
    if (result.is_err()) {
        return false;
    }

    auto& file = result.value();
    const auto& dataset = file.dataset();

    // File info
    summary.file_path = file_path.string();
    summary.file_size = std::filesystem::file_size(file_path);
    summary.transfer_syntax = file.transfer_syntax().name();
    summary.transfer_syntax_uid = file.transfer_syntax().uid();
    summary.sop_class_uid = file.sop_class_uid();
    summary.sop_instance_uid = file.sop_instance_uid();

    // Patient info
    summary.patient_name = dataset.get_string(tags::patient_name);
    summary.patient_id = dataset.get_string(tags::patient_id);
    summary.patient_birth_date = dataset.get_string(tags::patient_birth_date);
    summary.patient_sex = dataset.get_string(tags::patient_sex);

    // Study info
    summary.study_date = dataset.get_string(tags::study_date);
    summary.study_time = dataset.get_string(tags::study_time);
    summary.study_description = dataset.get_string(tags::study_description);
    summary.study_instance_uid = dataset.get_string(tags::study_instance_uid);
    summary.accession_number = dataset.get_string(tags::accession_number);

    // Series info
    summary.modality = dataset.get_string(tags::modality);
    summary.series_number = dataset.get_string(tags::series_number);
    summary.series_description = dataset.get_string(tags::series_description);
    summary.series_instance_uid = dataset.get_string(tags::series_instance_uid);

    // Instance info
    summary.instance_number = dataset.get_string(tags::instance_number);
    summary.acquisition_date = dataset.get_string(dicom_tag{0x0008, 0x0022});
    summary.acquisition_time = dataset.get_string(dicom_tag{0x0008, 0x0032});

    // Image info
    if (auto val = dataset.get_numeric<uint16_t>(tags::rows)) {
        summary.rows = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(tags::columns)) {
        summary.columns = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0100})) {
        summary.bits_allocated = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0101})) {
        summary.bits_stored = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0002})) {
        summary.samples_per_pixel = *val;
    }
    summary.photometric_interpretation =
        dataset.get_string(dicom_tag{0x0028, 0x0004});
    summary.number_of_frames = dataset.get_string(dicom_tag{0x0028, 0x0008});

    // Pixel data
    auto* pixel_data = dataset.get(dicom_tag{0x7FE0, 0x0010});
    if (pixel_data != nullptr) {
        summary.has_pixel_data = true;
        summary.pixel_data_size = pixel_data->length();
    }

    return true;
}

/**
 * @brief Print summary in text format
 * @param summary The summary to print
 * @param opts Command line options
 */
void print_summary_text(const dicom_summary& summary, const options& opts) {
    // Quiet mode: minimal output
    if (opts.quiet) {
        std::cout << summary.file_path;
        if (!summary.modality.empty()) {
            std::cout << " [" << summary.modality << "]";
        }
        if (summary.rows > 0 && summary.columns > 0) {
            std::cout << " " << summary.columns << "x" << summary.rows;
        }
        std::cout << "\n";
        return;
    }

    const int label_width = 24;

    std::cout << "========================================\n";

    // File Information
    if (opts.show_file_info) {
        std::cout << "File Information\n";
        std::cout << "----------------------------------------\n";
        std::cout << std::left << std::setw(label_width) << "  Path:"
                  << summary.file_path << "\n";
        std::cout << std::left << std::setw(label_width) << "  Size:"
                  << format_file_size(summary.file_size) << " ("
                  << summary.file_size << " bytes)\n";
        std::cout << std::left << std::setw(label_width) << "  Transfer Syntax:"
                  << summary.transfer_syntax << "\n";
        if (opts.verbose) {
            std::cout << std::left << std::setw(label_width) << "  TS UID:"
                      << summary.transfer_syntax_uid << "\n";
            std::cout << std::left << std::setw(label_width) << "  SOP Class:"
                      << summary.sop_class_uid << "\n";
            std::cout << std::left << std::setw(label_width) << "  SOP Instance:"
                      << summary.sop_instance_uid << "\n";
        }
        std::cout << "\n";
    }

    // Patient Information
    std::cout << "Patient Information\n";
    std::cout << "----------------------------------------\n";
    std::cout << std::left << std::setw(label_width) << "  Name:"
              << (summary.patient_name.empty() ? "(not specified)"
                                               : summary.patient_name)
              << "\n";
    std::cout << std::left << std::setw(label_width) << "  ID:"
              << (summary.patient_id.empty() ? "(not specified)"
                                             : summary.patient_id)
              << "\n";
    if (opts.verbose || !summary.patient_birth_date.empty()) {
        std::cout << std::left << std::setw(label_width) << "  Birth Date:"
                  << (summary.patient_birth_date.empty() ? "(not specified)"
                                                         : summary.patient_birth_date)
                  << "\n";
    }
    if (opts.verbose || !summary.patient_sex.empty()) {
        std::cout << std::left << std::setw(label_width) << "  Sex:"
                  << (summary.patient_sex.empty() ? "(not specified)"
                                                  : summary.patient_sex)
                  << "\n";
    }
    std::cout << "\n";

    // Study Information
    std::cout << "Study Information\n";
    std::cout << "----------------------------------------\n";
    std::cout << std::left << std::setw(label_width) << "  Date:"
              << (summary.study_date.empty() ? "(not specified)"
                                             : summary.study_date)
              << "\n";
    if (opts.verbose || !summary.study_time.empty()) {
        std::cout << std::left << std::setw(label_width) << "  Time:"
                  << (summary.study_time.empty() ? "(not specified)"
                                                 : summary.study_time)
                  << "\n";
    }
    if (opts.verbose || !summary.study_description.empty()) {
        std::cout << std::left << std::setw(label_width) << "  Description:"
                  << (summary.study_description.empty() ? "(not specified)"
                                                        : summary.study_description)
                  << "\n";
    }
    if (opts.verbose || !summary.accession_number.empty()) {
        std::cout << std::left << std::setw(label_width) << "  Accession #:"
                  << (summary.accession_number.empty() ? "(not specified)"
                                                       : summary.accession_number)
                  << "\n";
    }
    if (opts.verbose) {
        std::cout << std::left << std::setw(label_width) << "  Study UID:"
                  << summary.study_instance_uid << "\n";
    }
    std::cout << "\n";

    // Series Information
    std::cout << "Series Information\n";
    std::cout << "----------------------------------------\n";
    std::cout << std::left << std::setw(label_width) << "  Modality:"
              << (summary.modality.empty() ? "(not specified)"
                                           : summary.modality)
              << "\n";
    if (opts.verbose || !summary.series_number.empty()) {
        std::cout << std::left << std::setw(label_width) << "  Series #:"
                  << (summary.series_number.empty() ? "(not specified)"
                                                    : summary.series_number)
                  << "\n";
    }
    if (opts.verbose || !summary.series_description.empty()) {
        std::cout << std::left << std::setw(label_width) << "  Description:"
                  << (summary.series_description.empty() ? "(not specified)"
                                                         : summary.series_description)
                  << "\n";
    }
    if (opts.verbose) {
        std::cout << std::left << std::setw(label_width) << "  Series UID:"
                  << summary.series_instance_uid << "\n";
    }
    std::cout << "\n";

    // Instance Information (verbose only)
    if (opts.verbose) {
        std::cout << "Instance Information\n";
        std::cout << "----------------------------------------\n";
        std::cout << std::left << std::setw(label_width) << "  Instance #:"
                  << (summary.instance_number.empty() ? "(not specified)"
                                                      : summary.instance_number)
                  << "\n";
        if (!summary.acquisition_date.empty()) {
            std::cout << std::left << std::setw(label_width) << "  Acquisition Date:"
                      << summary.acquisition_date << "\n";
        }
        if (!summary.acquisition_time.empty()) {
            std::cout << std::left << std::setw(label_width) << "  Acquisition Time:"
                      << summary.acquisition_time << "\n";
        }
        std::cout << "\n";
    }

    // Image Information
    if (summary.rows > 0 || summary.columns > 0 || summary.has_pixel_data) {
        std::cout << "Image Information\n";
        std::cout << "----------------------------------------\n";
        if (summary.rows > 0 && summary.columns > 0) {
            std::cout << std::left << std::setw(label_width) << "  Dimensions:"
                      << summary.columns << " x " << summary.rows << " pixels\n";
        }
        if (summary.bits_allocated > 0) {
            std::cout << std::left << std::setw(label_width) << "  Bits:"
                      << summary.bits_stored << " stored / "
                      << summary.bits_allocated << " allocated\n";
        }
        if (summary.samples_per_pixel > 0) {
            std::cout << std::left << std::setw(label_width) << "  Samples/Pixel:"
                      << summary.samples_per_pixel << "\n";
        }
        if (!summary.photometric_interpretation.empty()) {
            std::cout << std::left << std::setw(label_width) << "  Photometric:"
                      << summary.photometric_interpretation << "\n";
        }
        if (!summary.number_of_frames.empty()) {
            std::cout << std::left << std::setw(label_width) << "  Frames:"
                      << summary.number_of_frames << "\n";
        }
        if (summary.has_pixel_data) {
            std::cout << std::left << std::setw(label_width) << "  Pixel Data:"
                      << format_file_size(summary.pixel_data_size) << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "========================================\n";
}

/**
 * @brief Print summary in JSON format
 * @param summary The summary to print
 * @param opts Command line options
 * @param is_last Whether this is the last item in an array
 */
void print_summary_json(const dicom_summary& summary, const options& opts,
                        bool is_last = true) {
    std::cout << "{\n";

    // File info
    if (opts.show_file_info) {
        std::cout << "  \"file\": {\n";
        std::cout << "    \"path\": \"" << json_escape(summary.file_path) << "\",\n";
        std::cout << "    \"size\": " << summary.file_size << ",\n";
        std::cout << "    \"sizeFormatted\": \"" << format_file_size(summary.file_size)
                  << "\",\n";
        std::cout << "    \"transferSyntax\": \"" << json_escape(summary.transfer_syntax)
                  << "\",\n";
        std::cout << "    \"transferSyntaxUID\": \""
                  << json_escape(summary.transfer_syntax_uid) << "\",\n";
        std::cout << "    \"sopClassUID\": \"" << json_escape(summary.sop_class_uid)
                  << "\",\n";
        std::cout << "    \"sopInstanceUID\": \"" << json_escape(summary.sop_instance_uid)
                  << "\"\n";
        std::cout << "  },\n";
    }

    // Patient info
    std::cout << "  \"patient\": {\n";
    std::cout << "    \"name\": \"" << json_escape(summary.patient_name) << "\",\n";
    std::cout << "    \"id\": \"" << json_escape(summary.patient_id) << "\",\n";
    std::cout << "    \"birthDate\": \"" << json_escape(summary.patient_birth_date)
              << "\",\n";
    std::cout << "    \"sex\": \"" << json_escape(summary.patient_sex) << "\"\n";
    std::cout << "  },\n";

    // Study info
    std::cout << "  \"study\": {\n";
    std::cout << "    \"date\": \"" << json_escape(summary.study_date) << "\",\n";
    std::cout << "    \"time\": \"" << json_escape(summary.study_time) << "\",\n";
    std::cout << "    \"description\": \"" << json_escape(summary.study_description)
              << "\",\n";
    std::cout << "    \"instanceUID\": \"" << json_escape(summary.study_instance_uid)
              << "\",\n";
    std::cout << "    \"accessionNumber\": \"" << json_escape(summary.accession_number)
              << "\"\n";
    std::cout << "  },\n";

    // Series info
    std::cout << "  \"series\": {\n";
    std::cout << "    \"modality\": \"" << json_escape(summary.modality) << "\",\n";
    std::cout << "    \"number\": \"" << json_escape(summary.series_number) << "\",\n";
    std::cout << "    \"description\": \"" << json_escape(summary.series_description)
              << "\",\n";
    std::cout << "    \"instanceUID\": \"" << json_escape(summary.series_instance_uid)
              << "\"\n";
    std::cout << "  },\n";

    // Instance info
    std::cout << "  \"instance\": {\n";
    std::cout << "    \"number\": \"" << json_escape(summary.instance_number) << "\",\n";
    std::cout << "    \"acquisitionDate\": \"" << json_escape(summary.acquisition_date)
              << "\",\n";
    std::cout << "    \"acquisitionTime\": \"" << json_escape(summary.acquisition_time)
              << "\"\n";
    std::cout << "  },\n";

    // Image info
    std::cout << "  \"image\": {\n";
    std::cout << "    \"rows\": " << summary.rows << ",\n";
    std::cout << "    \"columns\": " << summary.columns << ",\n";
    std::cout << "    \"bitsAllocated\": " << summary.bits_allocated << ",\n";
    std::cout << "    \"bitsStored\": " << summary.bits_stored << ",\n";
    std::cout << "    \"samplesPerPixel\": " << summary.samples_per_pixel << ",\n";
    std::cout << "    \"photometricInterpretation\": \""
              << json_escape(summary.photometric_interpretation) << "\",\n";
    std::cout << "    \"numberOfFrames\": \"" << json_escape(summary.number_of_frames)
              << "\",\n";
    std::cout << "    \"hasPixelData\": " << (summary.has_pixel_data ? "true" : "false")
              << ",\n";
    std::cout << "    \"pixelDataSize\": " << summary.pixel_data_size << "\n";
    std::cout << "  }\n";

    std::cout << "}" << (is_last ? "" : ",") << "\n";
}

/**
 * @brief Process a single DICOM file
 * @param file_path Path to the DICOM file
 * @param opts Command line options
 * @param is_last Whether this is the last file (for JSON formatting)
 * @return 0 on success, non-zero on error
 */
int process_file(const std::filesystem::path& file_path, const options& opts,
                 bool is_last = true) {
    dicom_summary summary;

    if (!extract_summary(file_path, summary)) {
        if (!opts.quiet) {
            std::cerr << "Error: Failed to read DICOM file: " << file_path.string()
                      << "\n";
        }
        return 2;
    }

    if (opts.format == output_format::json) {
        print_summary_json(summary, opts, is_last);
    } else {
        print_summary_text(summary, opts);
    }

    return 0;
}

/**
 * @brief Collect DICOM files from a directory
 * @param dir_path Directory path
 * @param recursive Whether to scan recursively
 * @return Vector of file paths
 */
std::vector<std::filesystem::path> collect_files(
    const std::filesystem::path& dir_path, bool recursive) {
    std::vector<std::filesystem::path> files;

    auto is_dicom_file = [](const std::filesystem::path& p) {
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".dcm" || ext == ".dicom" || ext.empty();
    };

    if (recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(dir_path)) {
            if (entry.is_regular_file() && is_dicom_file(entry.path())) {
                files.push_back(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            if (entry.is_regular_file() && is_dicom_file(entry.path())) {
                files.push_back(entry.path());
            }
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

}  // namespace

int main(int argc, char* argv[]) {
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        std::cout << R"(
  ____   ____ __  __   ___ _   _ _____ ___
 |  _ \ / ___|  \/  | |_ _| \ | |  ___/ _ \
 | | | | |   | |\/| |  | ||  \| | |_ | | | |
 | |_| | |___| |  | |  | || |\  |  _|| |_| |
 |____/ \____|_|  |_| |___|_| \_|_|   \___/

        DICOM File Summary Utility
)" << "\n";
        print_usage(argv[0]);
        return 1;
    }

    // Collect all files to process
    std::vector<std::filesystem::path> all_files;
    for (const auto& path : opts.paths) {
        if (!std::filesystem::exists(path)) {
            std::cerr << "Error: Path does not exist: " << path.string() << "\n";
            return 2;
        }

        if (std::filesystem::is_directory(path)) {
            auto dir_files = collect_files(path, opts.recursive);
            all_files.insert(all_files.end(), dir_files.begin(), dir_files.end());
        } else {
            all_files.push_back(path);
        }
    }

    if (all_files.empty()) {
        std::cerr << "Error: No DICOM files found\n";
        return 2;
    }

    // Print banner for text format (non-quiet)
    if (opts.format == output_format::text && !opts.quiet) {
        std::cout << R"(
  ____   ____ __  __   ___ _   _ _____ ___
 |  _ \ / ___|  \/  | |_ _| \ | |  ___/ _ \
 | | | | |   | |\/| |  | ||  \| | |_ | | | |
 | |_| | |___| |  | |  | || |\  |  _|| |_| |
 |____/ \____|_|  |_| |___|_| \_|_|   \___/

        DICOM File Summary Utility
)" << "\n";

        if (all_files.size() > 1) {
            std::cout << "Processing " << all_files.size() << " files...\n\n";
        }
    }

    // JSON array wrapper
    if (opts.format == output_format::json && all_files.size() > 1) {
        std::cout << "[\n";
    }

    int exit_code = 0;
    for (size_t i = 0; i < all_files.size(); ++i) {
        bool is_last = (i == all_files.size() - 1);
        if (process_file(all_files[i], opts, is_last) != 0) {
            exit_code = 2;
        }

        // Add newline between files for text format
        if (opts.format == output_format::text && !is_last && !opts.quiet) {
            std::cout << "\n";
        }
    }

    // Close JSON array
    if (opts.format == output_format::json && all_files.size() > 1) {
        std::cout << "]\n";
    }

    return exit_code;
}
