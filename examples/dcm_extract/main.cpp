/**
 * @file main.cpp
 * @brief DICOM Extract - Pixel Data Extraction Utility
 *
 * A command-line utility for extracting pixel data from DICOM files
 * to standard image formats (RAW, JPEG, PNG) or raw binary data.
 *
 * @see Issue #387 - dcm_extract: DICOM pixel data extractor
 * @see DICOM PS3.5 Section 8 - Pixel Data Encoding
 *
 * Usage:
 *   dcm_extract <input> [output] [options]
 *
 * Example:
 *   dcm_extract image.dcm                     # Extract to raw binary
 *   dcm_extract image.dcm output.jpg --jpeg   # Extract to JPEG
 *   dcm_extract image.dcm --info              # Show pixel data info only
 *   dcm_extract ./dicom/ ./images/ --recursive --jpeg
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
#include <sstream>
#include <string>
#include <vector>

#ifdef PACS_JPEG_FOUND
#include <jpeglib.h>
#endif

#ifdef PACS_PNG_FOUND
#include <png.h>
#endif

namespace {

/**
 * @brief Output format options
 */
enum class output_format {
    raw,   // Raw pixel data
    jpeg,  // JPEG image
    png,   // PNG image
    ppm    // PPM/PGM (portable pixmap/graymap)
};

/**
 * @brief Pixel data information
 */
struct pixel_info {
    uint16_t rows{0};
    uint16_t columns{0};
    uint16_t bits_allocated{0};
    uint16_t bits_stored{0};
    uint16_t high_bit{0};
    uint16_t samples_per_pixel{1};
    uint16_t pixel_representation{0};
    uint16_t planar_configuration{0};
    uint32_t number_of_frames{1};
    std::string photometric_interpretation;
    size_t pixel_data_size{0};
    bool has_pixel_data{false};
};

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    output_format format{output_format::raw};
    int jpeg_quality{90};
    uint32_t frame_number{0};  // 0 = all frames
    bool extract_all_frames{true};
    bool info_only{false};
    bool recursive{false};
    bool overwrite{false};
    bool verbose{false};
    bool quiet{false};
    bool apply_window{false};
    double window_center{0.0};
    double window_width{0.0};
};

/**
 * @brief Extraction statistics
 */
struct extraction_stats {
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
DICOM Extract - Pixel Data Extraction Utility

Usage: )" << program_name
              << R"( <input> [output] [options]

Arguments:
  input               Input DICOM file or directory
  output              Output file or directory (optional for --info)

Output Format Options:
  --raw               Raw pixel data (default)
  --jpeg              JPEG image (requires libjpeg)
  --png               PNG image (requires libpng)
  --ppm               PPM/PGM portable image format

JPEG Options:
  -q, --quality <1-100>   JPEG quality (default: 90)

Frame Selection:
  --frame <n>         Extract specific frame (0-indexed, default: all)
  --all-frames        Extract all frames (default)

Windowing Options (for display):
  --window <c> <w>    Apply window center/width transformation

Processing Options:
  -r, --recursive     Process directory recursively
  --overwrite         Overwrite existing output files
  -v, --verbose       Verbose output
  --quiet             Minimal output (errors only)

Information:
  --info              Show pixel data information only (no extraction)
  -h, --help          Show this help message

Supported Transfer Syntaxes:
  - Uncompressed: Implicit VR, Explicit VR (LE/BE)
  - Compressed: Requires codec support (JPEG, JPEG2000, RLE)

Examples:
  )" << program_name
              << R"( image.dcm                       # Show pixel info
  )" << program_name
              << R"( image.dcm output.raw --raw      # Extract raw pixels
  )" << program_name
              << R"( image.dcm output.jpg --jpeg     # Extract as JPEG
  )" << program_name
              << R"( image.dcm output.png --png      # Extract as PNG
  )" << program_name
              << R"( image.dcm output.ppm --ppm      # Extract as PPM
  )" << program_name
              << R"( image.dcm output.jpg --jpeg --frame 0
  )" << program_name
              << R"( ./dicom/ ./images/ --recursive --jpeg

Exit Codes:
  0  Success - All files extracted successfully
  1  Error - Invalid arguments
  2  Error - Extraction failed for one or more files
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
        } else if (arg == "--info") {
            opts.info_only = true;
        } else if (arg == "--raw") {
            opts.format = output_format::raw;
        } else if (arg == "--jpeg") {
            opts.format = output_format::jpeg;
        } else if (arg == "--png") {
            opts.format = output_format::png;
        } else if (arg == "--ppm") {
            opts.format = output_format::ppm;
        } else if ((arg == "-q" || arg == "--quality") && i + 1 < argc) {
            try {
                opts.jpeg_quality = std::stoi(argv[++i]);
                if (opts.jpeg_quality < 1 || opts.jpeg_quality > 100) {
                    std::cerr << "Error: Quality must be between 1 and 100\n";
                    return false;
                }
            } catch (...) {
                std::cerr << "Error: Invalid quality value\n";
                return false;
            }
        } else if (arg == "--frame" && i + 1 < argc) {
            try {
                opts.frame_number = static_cast<uint32_t>(std::stoul(argv[++i]));
                opts.extract_all_frames = false;
            } catch (...) {
                std::cerr << "Error: Invalid frame number\n";
                return false;
            }
        } else if (arg == "--all-frames") {
            opts.extract_all_frames = true;
        } else if (arg == "--window" && i + 2 < argc) {
            try {
                opts.window_center = std::stod(argv[++i]);
                opts.window_width = std::stod(argv[++i]);
                opts.apply_window = true;
            } catch (...) {
                std::cerr << "Error: Invalid window values\n";
                return false;
            }
        } else if (arg == "-r" || arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "--overwrite") {
            opts.overwrite = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--quiet") {
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

    if (opts.output_path.empty() && !opts.info_only) {
        // Default: show info only
        opts.info_only = true;
    }

    // Quiet mode overrides verbose
    if (opts.quiet) {
        opts.verbose = false;
    }

    return true;
}

/**
 * @brief Get pixel data information from DICOM dataset
 * @param dataset The DICOM dataset
 * @return Pixel information structure
 */
pixel_info get_pixel_info(const pacs::core::dicom_dataset& dataset) {
    using namespace pacs::core;

    pixel_info info;

    // Image dimensions
    if (auto val = dataset.get_numeric<uint16_t>(tags::rows); val) {
        info.rows = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(tags::columns); val) {
        info.columns = *val;
    }

    // Bit depth
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0100}); val) {
        info.bits_allocated = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0101}); val) {
        info.bits_stored = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0102}); val) {
        info.high_bit = *val;
    }

    // Samples and representation
    if (auto val = dataset.get_numeric<uint16_t>(tags::samples_per_pixel); val) {
        info.samples_per_pixel = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0103}); val) {
        info.pixel_representation = *val;
    }
    if (auto val = dataset.get_numeric<uint16_t>(dicom_tag{0x0028, 0x0006}); val) {
        info.planar_configuration = *val;
    }

    // Number of frames
    auto frames_str = dataset.get_string(dicom_tag{0x0028, 0x0008});
    if (!frames_str.empty()) {
        try {
            info.number_of_frames = static_cast<uint32_t>(std::stoul(frames_str));
        } catch (...) {
            info.number_of_frames = 1;
        }
    }

    // Photometric interpretation
    info.photometric_interpretation =
        dataset.get_string(tags::photometric_interpretation);

    // Check for pixel data
    auto* pixel_data = dataset.get(tags::pixel_data);
    if (pixel_data != nullptr) {
        info.has_pixel_data = true;
        info.pixel_data_size = pixel_data->length();
    }

    return info;
}

/**
 * @brief Print pixel data information
 * @param info The pixel information
 * @param file_path File path for context
 */
void print_pixel_info(const pixel_info& info,
                      const std::filesystem::path& file_path) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "        Pixel Data Information\n";
    std::cout << "========================================\n";
    std::cout << "  File:              " << file_path.filename().string() << "\n";
    std::cout << "  Dimensions:        " << info.columns << " x " << info.rows << "\n";
    std::cout << "  Bits Allocated:    " << info.bits_allocated << "\n";
    std::cout << "  Bits Stored:       " << info.bits_stored << "\n";
    std::cout << "  High Bit:          " << info.high_bit << "\n";
    std::cout << "  Samples/Pixel:     " << info.samples_per_pixel << "\n";
    std::cout << "  Pixel Rep:         "
              << (info.pixel_representation == 0 ? "Unsigned" : "Signed") << "\n";
    std::cout << "  Photometric:       " << info.photometric_interpretation << "\n";
    std::cout << "  Number of Frames:  " << info.number_of_frames << "\n";
    std::cout << "  Has Pixel Data:    " << (info.has_pixel_data ? "Yes" : "No") << "\n";
    if (info.has_pixel_data) {
        std::cout << "  Pixel Data Size:   " << info.pixel_data_size << " bytes\n";

        // Calculate expected size
        size_t expected = static_cast<size_t>(info.columns) * info.rows *
                          info.samples_per_pixel * ((info.bits_allocated + 7) / 8) *
                          info.number_of_frames;
        std::cout << "  Expected Size:     " << expected << " bytes\n";

        if (info.pixel_data_size != expected) {
            std::cout << "  Note: Size mismatch - data may be compressed\n";
        }
    }
    std::cout << "========================================\n";
}

/**
 * @brief Apply window level transformation
 * @param pixel Pixel value
 * @param center Window center
 * @param width Window width
 * @return Transformed value (0-255)
 */
uint8_t apply_window_level(int pixel, double center, double width) {
    double lower = center - width / 2.0;
    double upper = center + width / 2.0;

    if (pixel <= lower) return 0;
    if (pixel >= upper) return 255;

    return static_cast<uint8_t>(
        ((pixel - lower) / width) * 255.0);
}

#ifdef PACS_JPEG_FOUND
/**
 * @brief Write pixel data as JPEG
 * @param output_path Output file path
 * @param pixels Pixel data
 * @param width Image width
 * @param height Image height
 * @param components Number of color components
 * @param quality JPEG quality (1-100)
 * @return true on success
 */
bool write_jpeg(const std::filesystem::path& output_path,
                const std::vector<uint8_t>& pixels,
                int width, int height, int components, int quality) {
    FILE* file = fopen(output_path.string().c_str(), "wb");
    if (file == nullptr) {
        std::cerr << "Error: Cannot create file: " << output_path << "\n";
        return false;
    }

    struct jpeg_compress_struct cinfo {};
    struct jpeg_error_mgr jerr {};

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, file);

    cinfo.image_width = static_cast<JDIMENSION>(width);
    cinfo.image_height = static_cast<JDIMENSION>(height);
    cinfo.input_components = components;
    cinfo.in_color_space = (components == 1) ? JCS_GRAYSCALE : JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    JSAMPROW row_pointer[1];
    int row_stride = width * components;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] =
            const_cast<JSAMPROW>(&pixels[cinfo.next_scanline * static_cast<size_t>(row_stride)]);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(file);

    return true;
}
#endif

#ifdef PACS_PNG_FOUND
/**
 * @brief Write pixel data as PNG
 * @param output_path Output file path
 * @param pixels Pixel data
 * @param width Image width
 * @param height Image height
 * @param components Number of color components (1=grayscale, 3=RGB)
 * @return true on success
 */
bool write_png(const std::filesystem::path& output_path,
               const std::vector<uint8_t>& pixels,
               int width, int height, int components) {
    FILE* file = fopen(output_path.string().c_str(), "wb");
    if (file == nullptr) {
        std::cerr << "Error: Cannot create file: " << output_path << "\n";
        return false;
    }

    // Create PNG write struct
    png_structp png_ptr = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png_ptr == nullptr) {
        std::cerr << "Error: Failed to create PNG write struct\n";
        fclose(file);
        return false;
    }

    // Create PNG info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        std::cerr << "Error: Failed to create PNG info struct\n";
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(file);
        return false;
    }

    // Set up error handling (required for libpng)
    if (setjmp(png_jmpbuf(png_ptr))) {
        std::cerr << "Error: PNG write error\n";
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(file);
        return false;
    }

    // Initialize I/O
    png_init_io(png_ptr, file);

    // Set compression level (6 = default, good balance of speed/size)
    png_set_compression_level(png_ptr, 6);

    // Set image attributes
    int color_type = (components == 1) ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGB;
    png_set_IHDR(png_ptr, info_ptr,
                 static_cast<png_uint_32>(width),
                 static_cast<png_uint_32>(height),
                 8,  // Bit depth
                 color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    // Write header
    png_write_info(png_ptr, info_ptr);

    // Write image data row by row
    int row_stride = width * components;
    for (int y = 0; y < height; ++y) {
        png_bytep row = const_cast<png_bytep>(&pixels[static_cast<size_t>(y) * row_stride]);
        png_write_row(png_ptr, row);
    }

    // Finish writing
    png_write_end(png_ptr, nullptr);

    // Cleanup
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(file);

    return true;
}
#endif

/**
 * @brief Write pixel data as PPM/PGM
 * @param output_path Output file path
 * @param pixels Pixel data
 * @param width Image width
 * @param height Image height
 * @param components Number of color components
 * @return true on success
 */
bool write_ppm(const std::filesystem::path& output_path,
               const std::vector<uint8_t>& pixels,
               int width, int height, int components) {
    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot create file: " << output_path << "\n";
        return false;
    }

    // Write header
    if (components == 1) {
        file << "P5\n";  // PGM binary
    } else {
        file << "P6\n";  // PPM binary
    }
    file << width << " " << height << "\n";
    file << "255\n";

    // Write pixel data
    file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));

    return file.good();
}

/**
 * @brief Write pixel data as raw binary
 * @param output_path Output file path
 * @param pixels Pixel data
 * @return true on success
 */
bool write_raw(const std::filesystem::path& output_path,
               const std::vector<uint8_t>& pixels) {
    std::ofstream file(output_path, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot create file: " << output_path << "\n";
        return false;
    }

    file.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));

    return file.good();
}

/**
 * @brief Convert 16-bit pixel data to 8-bit
 * @param data 16-bit pixel data
 * @param info Pixel information
 * @param opts Options (for windowing)
 * @return 8-bit pixel data
 */
std::vector<uint8_t> convert_to_8bit(const std::vector<uint8_t>& data,
                                      const pixel_info& info,
                                      const options& opts) {
    size_t num_pixels = static_cast<size_t>(info.columns) * info.rows * info.samples_per_pixel;
    std::vector<uint8_t> result(num_pixels);

    // Find min/max for auto-windowing if not specified
    double window_center = opts.window_center;
    double window_width = opts.window_width;

    if (!opts.apply_window) {
        // Auto-calculate window from data
        int min_val = std::numeric_limits<int>::max();
        int max_val = std::numeric_limits<int>::min();

        for (size_t i = 0; i < num_pixels; ++i) {
            int16_t pixel;
            if (info.pixel_representation == 0) {
                pixel = static_cast<int16_t>(
                    data[i * 2] | (data[i * 2 + 1] << 8));
            } else {
                pixel = static_cast<int16_t>(
                    data[i * 2] | (data[i * 2 + 1] << 8));
            }
            min_val = std::min(min_val, static_cast<int>(pixel));
            max_val = std::max(max_val, static_cast<int>(pixel));
        }

        window_width = static_cast<double>(max_val - min_val);
        window_center = static_cast<double>(min_val + max_val) / 2.0;

        if (window_width < 1) window_width = 1;
    }

    // Apply windowing
    for (size_t i = 0; i < num_pixels; ++i) {
        int16_t pixel = static_cast<int16_t>(
            data[i * 2] | (data[i * 2 + 1] << 8));
        result[i] = apply_window_level(pixel, window_center, window_width);
    }

    return result;
}

/**
 * @brief Extract pixel data from a single DICOM file
 * @param input_path Input DICOM path
 * @param output_path Output path
 * @param opts Extraction options
 * @return true on success
 */
bool extract_file(const std::filesystem::path& input_path,
                  const std::filesystem::path& output_path,
                  const options& opts) {
    using namespace pacs::core;

    // Open DICOM file
    auto result = dicom_file::open(input_path);
    if (result.is_err()) {
        std::cerr << "Error: Failed to open '" << input_path.string()
                  << "': " << result.error().message << "\n";
        return false;
    }

    auto& file = result.value();
    const auto& dataset = file.dataset();

    // Get pixel information
    auto info = get_pixel_info(dataset);

    // Info only mode
    if (opts.info_only) {
        print_pixel_info(info, input_path);
        return true;
    }

    // Check if pixel data exists
    if (!info.has_pixel_data) {
        std::cerr << "Error: No pixel data in file: " << input_path << "\n";
        return false;
    }

    // Check if output exists and overwrite is disabled
    if (std::filesystem::exists(output_path) && !opts.overwrite) {
        if (opts.verbose) {
            std::cout << "  Skipped (exists): " << output_path.filename().string()
                      << "\n";
        }
        return true;
    }

    // Get pixel data
    auto* pixel_element = dataset.get(tags::pixel_data);
    if (pixel_element == nullptr) {
        std::cerr << "Error: Cannot read pixel data: " << input_path << "\n";
        return false;
    }

    auto pixel_data = pixel_element->raw_data();
    std::vector<uint8_t> pixels(pixel_data.begin(), pixel_data.end());

    if (opts.verbose) {
        std::cout << "  Extracting: " << input_path.filename().string() << "\n";
        std::cout << "    Size: " << info.columns << " x " << info.rows << "\n";
        std::cout << "    Bits: " << info.bits_stored << "/" << info.bits_allocated
                  << "\n";
    }

    // Convert to 8-bit if necessary
    if (info.bits_allocated == 16) {
        pixels = convert_to_8bit(pixels, info, opts);
    }

    // Handle MONOCHROME1 inversion
    if (info.photometric_interpretation == "MONOCHROME1") {
        for (auto& p : pixels) {
            p = static_cast<uint8_t>(255 - p);
        }
    }

    // Ensure output directory exists
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    // Write output based on format
    bool success = false;

    switch (opts.format) {
        case output_format::raw:
            success = write_raw(output_path, pixels);
            break;

        case output_format::jpeg:
#ifdef PACS_JPEG_FOUND
            success = write_jpeg(output_path, pixels, info.columns, info.rows,
                                 info.samples_per_pixel, opts.jpeg_quality);
#else
            std::cerr << "Error: JPEG support not available. Install libjpeg-turbo.\n";
            success = false;
#endif
            break;

        case output_format::png:
#ifdef PACS_PNG_FOUND
            success = write_png(output_path, pixels, info.columns, info.rows,
                                info.samples_per_pixel);
#else
            std::cerr << "Error: PNG support not available. Install libpng.\n";
            success = false;
#endif
            break;

        case output_format::ppm:
            success =
                write_ppm(output_path, pixels, info.columns, info.rows, info.samples_per_pixel);
            break;
    }

    if (success && opts.verbose) {
        std::cout << "    Output: " << output_path.string() << "\n";
    }

    return success;
}

/**
 * @brief Get output extension for format
 * @param format Output format
 * @return File extension
 */
std::string get_output_extension(output_format format) {
    switch (format) {
        case output_format::raw:
            return ".raw";
        case output_format::jpeg:
            return ".jpg";
        case output_format::png:
            return ".png";
        case output_format::ppm:
            return ".ppm";
    }
    return ".raw";
}

/**
 * @brief Check if file is a DICOM file
 * @param file_path Path to check
 * @return true if likely DICOM
 */
bool is_dicom_file(const std::filesystem::path& file_path) {
    auto ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".dcm" || ext == ".dicom" || ext.empty();
}

/**
 * @brief Process a directory of DICOM files
 * @param input_dir Input directory
 * @param output_dir Output directory
 * @param opts Extraction options
 * @param stats Output statistics
 */
void process_directory(const std::filesystem::path& input_dir,
                       const std::filesystem::path& output_dir,
                       const options& opts,
                       extraction_stats& stats) {
    auto process_file = [&](const std::filesystem::path& file_path) {
        if (!is_dicom_file(file_path)) {
            return;
        }

        ++stats.total_files;

        // Calculate output path with appropriate extension
        auto relative_path = std::filesystem::relative(file_path, input_dir);
        auto output_path = output_dir / relative_path;
        output_path.replace_extension(get_output_extension(opts.format));

        auto start = std::chrono::steady_clock::now();

        if (extract_file(file_path, output_path, opts)) {
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
 * @brief Print extraction summary
 * @param stats The extraction statistics
 */
void print_summary(const extraction_stats& stats) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "        Extraction Summary\n";
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
  ____   ____ __  __   _______  _______ ____      _    ____ _____
 |  _ \ / ___|  \/  | | ____\ \/ /_   _|  _ \    / \  / ___|_   _|
 | | | | |   | |\/| | |  _|  \  /  | | | |_) |  / _ \| |     | |
 | |_| | |___| |  | | | |___ /  \  | | |  _ <  / ___ \ |___  | |
 |____/ \____|_|  |_| |_____/_/\_\ |_| |_| \_\/_/   \_\____| |_|

      DICOM Pixel Data Extraction Utility
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
  ____   ____ __  __   _______  _______ ____      _    ____ _____
 |  _ \ / ___|  \/  | | ____\ \/ /_   _|  _ \    / \  / ___|_   _|
 | | | | |   | |\/| | |  _|  \  /  | | | |_) |  / _ \| |     | |
 | |_| | |___| |  | | | |___ /  \  | | |  _ <  / ___ \ |___  | |
 |____/ \____|_|  |_| |_____/_/\_\ |_| |_| \_\/_/   \_\____| |_|

      DICOM Pixel Data Extraction Utility
)" << "\n";
    }

    extraction_stats stats;
    auto start_time = std::chrono::steady_clock::now();

    if (std::filesystem::is_directory(opts.input_path)) {
        // Process directory
        if (!opts.info_only) {
            if (!std::filesystem::exists(opts.output_path)) {
                std::filesystem::create_directories(opts.output_path);
            }
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

        if (extract_file(opts.input_path, opts.output_path, opts)) {
            ++stats.success_count;
            if (!opts.quiet && !opts.info_only) {
                std::cout << "Extraction completed successfully.\n";
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
    if (std::filesystem::is_directory(opts.input_path) && !opts.quiet &&
        !opts.info_only) {
        print_summary(stats);
    }

    return stats.error_count > 0 ? 2 : 0;
}
