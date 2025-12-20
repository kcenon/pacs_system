/**
 * @file main.cpp
 * @brief Image to DICOM - Image Conversion Utility
 *
 * A command-line utility for converting regular image files (JPEG, PNG)
 * to DICOM format using the Secondary Capture SOP Class.
 *
 * @see Issue #386 - img_to_dcm: Image to DICOM converter
 * @see DICOM PS3.3 Section A.8 - Secondary Capture Image IOD
 *
 * Usage:
 *   img_to_dcm <input> <output> [options]
 *
 * Example:
 *   img_to_dcm photo.jpg output.dcm
 *   img_to_dcm photo.jpg output.dcm --patient-name "DOE^JOHN"
 *   img_to_dcm ./images/ ./dicom/ --recursive
 */

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_element.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef PACS_JPEG_FOUND
#include <jpeglib.h>
#endif

namespace {

/// Secondary Capture Image Storage SOP Class UID
constexpr std::string_view kSecondaryCaptureUID = "1.2.840.10008.5.1.4.1.1.7";

/// Secondary Capture Image Storage SOP Class UID for 8-bit color
constexpr std::string_view kSecondaryCaptureColorUID = "1.2.840.10008.5.1.4.1.1.7.4";

/**
 * @brief Generate a DICOM UID
 * @return New unique UID string
 */
std::string generate_uid() {
    static uint64_t counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
    return std::string("1.2.826.0.1.3680043.8.1055.4.") + std::to_string(timestamp) +
           "." + std::to_string(++counter);
}

/**
 * @brief Image data container
 */
struct image_data {
    std::vector<uint8_t> pixels;
    uint16_t width{0};
    uint16_t height{0};
    uint16_t bits_allocated{8};
    uint16_t bits_stored{8};
    uint16_t samples_per_pixel{1};
    std::string photometric_interpretation{"MONOCHROME2"};
    bool is_valid{false};
};

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::string patient_name{"ANONYMOUS"};
    std::string patient_id;
    std::string study_description{"Imported Image"};
    std::string series_description{"Secondary Capture"};
    std::string modality{"OT"};  // Other
    bool recursive{false};
    bool overwrite{false};
    bool verbose{false};
    bool quiet{false};
    std::string transfer_syntax;
};

/**
 * @brief Conversion statistics
 */
struct conversion_stats {
    size_t total_files{0};
    size_t success_count{0};
    size_t skip_count{0};
    size_t error_count{0};
    std::chrono::milliseconds total_time{0};
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Image to DICOM - Image Conversion Utility

Usage: )" << program_name
              << R"( <input> <output> [options]

Arguments:
  input               Input image file (JPEG) or directory
  output              Output DICOM file or directory

Patient/Study Options:
  --patient-name <name>       Patient name (default: ANONYMOUS)
  --patient-id <id>           Patient ID (auto-generated if not specified)
  --study-description <desc>  Study description (default: Imported Image)
  --series-description <desc> Series description (default: Secondary Capture)
  --modality <mod>            Modality (default: OT)

Processing Options:
  -r, --recursive         Process directory recursively
  --overwrite             Overwrite existing output files
  -v, --verbose           Verbose output
  -q, --quiet             Minimal output (errors only)

Transfer Syntax Options:
  --explicit              Explicit VR Little Endian (default)
  --implicit              Implicit VR Little Endian

Information:
  -h, --help              Show this help message

Supported Input Formats:
  - JPEG (.jpg, .jpeg) - Requires libjpeg-turbo

Examples:
  )" << program_name
              << R"( photo.jpg output.dcm
  )" << program_name
              << R"( photo.jpg output.dcm --patient-name "DOE^JOHN" --patient-id "12345"
  )" << program_name
              << R"( ./images/ ./dicom/ --recursive

Exit Codes:
  0  Success - All files converted successfully
  1  Error - Invalid arguments
  2  Error - Conversion failed for one or more files
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
    using namespace pacs::encoding;

    if (argc < 2) {
        return false;
    }

    // Default to Explicit VR Little Endian
    opts.transfer_syntax = std::string(transfer_syntax::explicit_vr_little_endian.uid());

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        } else if (arg == "--patient-name" && i + 1 < argc) {
            opts.patient_name = argv[++i];
        } else if (arg == "--patient-id" && i + 1 < argc) {
            opts.patient_id = argv[++i];
        } else if (arg == "--study-description" && i + 1 < argc) {
            opts.study_description = argv[++i];
        } else if (arg == "--series-description" && i + 1 < argc) {
            opts.series_description = argv[++i];
        } else if (arg == "--modality" && i + 1 < argc) {
            opts.modality = argv[++i];
        } else if (arg == "--explicit") {
            opts.transfer_syntax = std::string(transfer_syntax::explicit_vr_little_endian.uid());
        } else if (arg == "--implicit") {
            opts.transfer_syntax = std::string(transfer_syntax::implicit_vr_little_endian.uid());
        } else if (arg == "-r" || arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "--overwrite") {
            opts.overwrite = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
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
        std::cerr << "Error: No input path specified\n";
        return false;
    }

    if (opts.output_path.empty()) {
        std::cerr << "Error: No output path specified\n";
        return false;
    }

    // Quiet mode overrides verbose
    if (opts.quiet) {
        opts.verbose = false;
    }

    return true;
}

/**
 * @brief Generate a random patient ID
 * @return 8-character alphanumeric ID
 */
std::string generate_patient_id() {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);

    std::string id;
    id.reserve(8);
    for (int i = 0; i < 8; ++i) {
        id += charset[dist(gen)];
    }
    return id;
}

/**
 * @brief Get current date in DICOM format (YYYYMMDD)
 * @return Formatted date string
 */
std::string get_current_date() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y%m%d");
    return oss.str();
}

/**
 * @brief Get current time in DICOM format (HHMMSS.ffffff)
 * @return Formatted time string
 */
std::string get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
                  now.time_since_epoch()) %
              1000000;
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%H%M%S") << "." << std::setfill('0')
        << std::setw(6) << ms.count();
    return oss.str();
}

#ifdef PACS_JPEG_FOUND
/**
 * @brief Read JPEG file and extract pixel data
 * @param file_path Path to JPEG file
 * @return Image data or invalid result
 */
image_data read_jpeg(const std::filesystem::path& file_path) {
    image_data result;

    FILE* file = fopen(file_path.string().c_str(), "rb");
    if (file == nullptr) {
        std::cerr << "Error: Cannot open file: " << file_path << "\n";
        return result;
    }

    struct jpeg_decompress_struct cinfo {};
    struct jpeg_error_mgr jerr {};

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);

    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        std::cerr << "Error: Invalid JPEG header: " << file_path << "\n";
        jpeg_destroy_decompress(&cinfo);
        fclose(file);
        return result;
    }

    // Force RGB output for color images
    if (cinfo.num_components == 3) {
        cinfo.out_color_space = JCS_RGB;
    }

    jpeg_start_decompress(&cinfo);

    result.width = static_cast<uint16_t>(cinfo.output_width);
    result.height = static_cast<uint16_t>(cinfo.output_height);
    result.samples_per_pixel = static_cast<uint16_t>(cinfo.output_components);
    result.bits_allocated = 8;
    result.bits_stored = 8;

    if (result.samples_per_pixel == 1) {
        result.photometric_interpretation = "MONOCHROME2";
    } else if (result.samples_per_pixel == 3) {
        result.photometric_interpretation = "RGB";
    }

    size_t row_stride = static_cast<size_t>(cinfo.output_width) * cinfo.output_components;
    result.pixels.resize(row_stride * cinfo.output_height);

    std::vector<JSAMPROW> row_pointers(cinfo.output_height);
    for (JDIMENSION row = 0; row < cinfo.output_height; ++row) {
        row_pointers[row] = result.pixels.data() + row * row_stride;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row_pointers[cinfo.output_scanline],
                            cinfo.output_height - cinfo.output_scanline);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(file);

    result.is_valid = true;
    return result;
}
#else
/**
 * @brief Stub for JPEG reading when libjpeg is not available
 */
image_data read_jpeg(const std::filesystem::path& file_path) {
    image_data result;
    std::cerr << "Error: JPEG support not available. Install libjpeg-turbo.\n";
    std::cerr << "  File: " << file_path << "\n";
    return result;
}
#endif

/**
 * @brief Read image file based on extension
 * @param file_path Path to image file
 * @return Image data or invalid result
 */
image_data read_image(const std::filesystem::path& file_path) {
    auto ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".jpg" || ext == ".jpeg") {
        return read_jpeg(file_path);
    }

    std::cerr << "Error: Unsupported image format: " << ext << "\n";
    std::cerr << "  Supported formats: JPEG (.jpg, .jpeg)\n";
    return {};
}

/**
 * @brief Convert image to DICOM dataset
 * @param img Image data
 * @param opts Conversion options
 * @return DICOM dataset
 */
pacs::core::dicom_dataset create_dicom_dataset(const image_data& img,
                                                const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    dicom_dataset dataset;

    // Generate UIDs
    std::string study_uid = generate_uid();
    std::string series_uid = generate_uid();
    std::string sop_instance_uid = generate_uid();

    // Determine SOP Class based on image type
    std::string sop_class_uid = (img.samples_per_pixel == 1)
                                    ? std::string(kSecondaryCaptureUID)
                                    : std::string(kSecondaryCaptureColorUID);

    // Patient Module (M)
    dataset.set_string(tags::patient_name, vr_type::PN, opts.patient_name);
    dataset.set_string(tags::patient_id, vr_type::LO,
                       opts.patient_id.empty() ? generate_patient_id() : opts.patient_id);
    dataset.set_string(dicom_tag{0x0010, 0x0030}, vr_type::DA, "");  // PatientBirthDate
    dataset.set_string(dicom_tag{0x0010, 0x0040}, vr_type::CS, "");  // PatientSex

    // General Study Module (M)
    dataset.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
    dataset.set_string(tags::study_date, vr_type::DA, get_current_date());
    dataset.set_string(tags::study_time, vr_type::TM, get_current_time());
    dataset.set_string(dicom_tag{0x0008, 0x0050}, vr_type::SH, "");  // AccessionNumber
    dataset.set_string(dicom_tag{0x0008, 0x0090}, vr_type::PN, "");  // ReferringPhysicianName
    dataset.set_string(dicom_tag{0x0020, 0x0010}, vr_type::SH, "1"); // StudyID
    dataset.set_string(tags::study_description, vr_type::LO, opts.study_description);

    // General Series Module (M)
    dataset.set_string(tags::series_instance_uid, vr_type::UI, series_uid);
    dataset.set_string(tags::modality, vr_type::CS, opts.modality);
    dataset.set_string(dicom_tag{0x0020, 0x0011}, vr_type::IS, "1"); // SeriesNumber
    dataset.set_string(tags::series_description, vr_type::LO, opts.series_description);

    // SC Equipment Module (M)
    dataset.set_string(dicom_tag{0x0008, 0x0064}, vr_type::CS, "DV"); // ConversionType

    // General Image Module (M)
    dataset.set_string(dicom_tag{0x0020, 0x0013}, vr_type::IS, "1"); // InstanceNumber
    dataset.set_string(dicom_tag{0x0020, 0x0020}, vr_type::CS, "");  // PatientOrientation

    // Image Pixel Module (M)
    dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, img.samples_per_pixel);
    dataset.set_string(tags::photometric_interpretation, vr_type::CS, img.photometric_interpretation);
    dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, img.height);
    dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, img.width);
    dataset.set_numeric<uint16_t>(dicom_tag{0x0028, 0x0100}, vr_type::US, img.bits_allocated);
    dataset.set_numeric<uint16_t>(dicom_tag{0x0028, 0x0101}, vr_type::US, img.bits_stored);
    dataset.set_numeric<uint16_t>(dicom_tag{0x0028, 0x0102}, vr_type::US,
                                   static_cast<uint16_t>(img.bits_stored - 1));
    dataset.set_numeric<uint16_t>(dicom_tag{0x0028, 0x0103}, vr_type::US, 0); // Unsigned

    // Planar Configuration (only for color images)
    if (img.samples_per_pixel > 1) {
        dataset.set_numeric<uint16_t>(dicom_tag{0x0028, 0x0006}, vr_type::US, 0); // Interleaved
    }

    // SOP Common Module (M)
    dataset.set_string(tags::sop_class_uid, vr_type::UI, sop_class_uid);
    dataset.set_string(tags::sop_instance_uid, vr_type::UI, sop_instance_uid);

    // Set pixel data
    dataset.insert(dicom_element(tags::pixel_data, vr_type::OW, img.pixels));

    return dataset;
}

/**
 * @brief Convert a single image file to DICOM
 * @param input_path Input image path
 * @param output_path Output DICOM path
 * @param opts Conversion options
 * @return true on success
 */
bool convert_file(const std::filesystem::path& input_path,
                  const std::filesystem::path& output_path,
                  const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    // Check if output exists and overwrite is disabled
    if (std::filesystem::exists(output_path) && !opts.overwrite) {
        if (opts.verbose) {
            std::cout << "  Skipped (exists): " << output_path.filename().string() << "\n";
        }
        return true;
    }

    // Read input image
    auto img = read_image(input_path);
    if (!img.is_valid) {
        return false;
    }

    if (opts.verbose) {
        std::cout << "  Converting: " << input_path.filename().string() << "\n";
        std::cout << "    Size: " << img.width << " x " << img.height << "\n";
        std::cout << "    Components: " << img.samples_per_pixel << "\n";
        std::cout << "    Photometric: " << img.photometric_interpretation << "\n";
    }

    // Create DICOM dataset
    auto dataset = create_dicom_dataset(img, opts);

    // Create DICOM file with specified transfer syntax
    auto ts = transfer_syntax(opts.transfer_syntax);
    auto dicom_file = dicom_file::create(std::move(dataset), ts);

    // Ensure output directory exists
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    // Save output file
    auto save_result = dicom_file.save(output_path);
    if (save_result.is_err()) {
        std::cerr << "Error: Failed to save '" << output_path.string()
                  << "': " << save_result.error().message << "\n";
        return false;
    }

    if (opts.verbose) {
        std::cout << "    Output: " << output_path.string() << "\n";
    }

    return true;
}

/**
 * @brief Check if file is a supported image format
 * @param file_path Path to check
 * @return true if supported
 */
bool is_supported_image(const std::filesystem::path& file_path) {
    auto ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".jpg" || ext == ".jpeg";
}

/**
 * @brief Process a directory of image files
 * @param input_dir Input directory
 * @param output_dir Output directory
 * @param opts Conversion options
 * @param stats Output statistics
 */
void process_directory(const std::filesystem::path& input_dir,
                       const std::filesystem::path& output_dir,
                       const options& opts,
                       conversion_stats& stats) {
    auto process_file = [&](const std::filesystem::path& file_path) {
        if (!is_supported_image(file_path)) {
            return;
        }

        ++stats.total_files;

        // Calculate output path with .dcm extension
        auto relative_path = std::filesystem::relative(file_path, input_dir);
        auto output_path = output_dir / relative_path;
        output_path.replace_extension(".dcm");

        auto start = std::chrono::steady_clock::now();

        if (convert_file(file_path, output_path, opts)) {
            ++stats.success_count;
        } else {
            ++stats.error_count;
        }

        auto end = std::chrono::steady_clock::now();
        stats.total_time +=
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (!opts.quiet) {
            std::cout << "\rProcessed: " << stats.total_files
                      << " (Success: " << stats.success_count
                      << ", Errors: " << stats.error_count << ")" << std::flush;
        }
    };

    if (opts.recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(input_dir)) {
            if (entry.is_regular_file()) {
                process_file(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(input_dir)) {
            if (entry.is_regular_file()) {
                process_file(entry.path());
            }
        }
    }

    if (!opts.quiet) {
        std::cout << "\n";
    }
}

/**
 * @brief Print conversion summary
 * @param stats The conversion statistics
 */
void print_summary(const conversion_stats& stats) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "        Conversion Summary\n";
    std::cout << "========================================\n";
    std::cout << "  Total files:   " << stats.total_files << "\n";
    std::cout << "  Successful:    " << stats.success_count << "\n";
    std::cout << "  Skipped:       " << stats.skip_count << "\n";
    std::cout << "  Errors:        " << stats.error_count << "\n";
    std::cout << "  Total time:    " << stats.total_time.count() << " ms\n";
    if (stats.total_files > 0) {
        auto avg_time =
            stats.total_time.count() / static_cast<double>(stats.total_files);
        std::cout << "  Avg per file:  " << std::fixed << std::setprecision(1)
                  << avg_time << " ms\n";
    }
    std::cout << "========================================\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        std::cout << R"(
  ___ __  __  ____   ____    ____   ____ __  __
 |_ _|  \/  |/ ___| |___ \  |  _ \ / ___|  \/  |
  | || |\/| | |  _    __) | | | | | |   | |\/| |
  | || |  | | |_| |  / __/  | |_| | |___| |  | |
 |___|_|  |_|\____| |_____| |____/ \____|_|  |_|

      Image to DICOM Conversion Utility
)" << "\n";
        print_usage(argv[0]);
        return 1;
    }

    // Check input exists
    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: Input path does not exist: " << opts.input_path.string()
                  << "\n";
        return 2;
    }

    // Show banner
    if (!opts.quiet) {
        std::cout << R"(
  ___ __  __  ____   ____    ____   ____ __  __
 |_ _|  \/  |/ ___| |___ \  |  _ \ / ___|  \/  |
  | || |\/| | |  _    __) | | | | | |   | |\/| |
  | || |  | | |_| |  / __/  | |_| | |___| |  | |
 |___|_|  |_|\____| |_____| |____/ \____|_|  |_|

      Image to DICOM Conversion Utility
)" << "\n";
    }

    conversion_stats stats;
    auto start_time = std::chrono::steady_clock::now();

    if (std::filesystem::is_directory(opts.input_path)) {
        // Process directory
        if (!std::filesystem::exists(opts.output_path)) {
            std::filesystem::create_directories(opts.output_path);
        }

        if (!opts.quiet) {
            std::cout << "Processing directory: " << opts.input_path.string() << "\n";
            if (opts.recursive) {
                std::cout << "Mode: Recursive\n\n";
            }
        }

        process_directory(opts.input_path, opts.output_path, opts, stats);
    } else {
        // Process single file
        ++stats.total_files;

        if (convert_file(opts.input_path, opts.output_path, opts)) {
            ++stats.success_count;
            if (!opts.quiet) {
                std::cout << "Conversion completed successfully.\n";
                std::cout << "  Output: " << opts.output_path.string() << "\n";
            }
        } else {
            ++stats.error_count;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    stats.total_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Print summary for directory processing
    if (std::filesystem::is_directory(opts.input_path) && !opts.quiet) {
        print_summary(stats);
    }

    return stats.error_count > 0 ? 2 : 0;
}
