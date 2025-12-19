/**
 * @file main.cpp
 * @brief DICOM Conversion - Transfer Syntax Conversion Utility
 *
 * A command-line utility for converting DICOM files between different
 * Transfer Syntaxes, including compressed and uncompressed formats.
 *
 * @see Issue #280 - dcm_conv: Transfer Syntax conversion utility
 * @see DICOM PS3.5 Section 10 - Transfer Syntax
 *
 * Usage:
 *   dcm_conv <input> <output> [options]
 *
 * Example:
 *   dcm_conv image.dcm converted.dcm --explicit
 *   dcm_conv image.dcm compressed.dcm --jpeg-baseline -q 90
 *   dcm_conv ./input_dir/ ./output_dir/ --recursive --implicit
 */

#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/compression/codec_factory.hpp"
#include "pacs/encoding/compression/compression_codec.hpp"
#include "pacs/encoding/transfer_syntax.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::string target_transfer_syntax;
    int quality{90};
    bool recursive{false};
    bool verify{false};
    bool overwrite{false};
    bool verbose{false};
    bool quiet{false};
    bool show_progress{true};
    bool list_syntaxes{false};
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
DICOM Conversion - Transfer Syntax Conversion Utility

Usage: )" << program_name
              << R"( <input> <output> [options]

Arguments:
  input               Input DICOM file or directory
  output              Output DICOM file or directory

Transfer Syntax Options:
  --implicit          Implicit VR Little Endian (1.2.840.10008.1.2)
  --explicit          Explicit VR Little Endian (1.2.840.10008.1.2.1) [default]
  --explicit-be       Explicit VR Big Endian (1.2.840.10008.1.2.2) [retired]
  --jpeg-baseline     JPEG Baseline (Process 1) - lossy
  --jpeg-lossless     JPEG Lossless, Non-Hierarchical
  --jpeg2000          JPEG 2000 Image Compression (Lossless Only)
  --jpeg2000-lossy    JPEG 2000 Image Compression
  --rle               RLE Lossless
  -t, --transfer-syntax <uid>  Specify Transfer Syntax by UID

Compression Options:
  -q, --quality <1-100>   JPEG quality (default: 90, higher = better quality)

Processing Options:
  -r, --recursive         Process directory recursively
  --overwrite             Overwrite existing output files
  --verify                Verify conversion result
  -v, --verbose           Verbose output
  -q, --quiet             Minimal output (errors only)
  --no-progress           Disable progress display

Information:
  --list-syntaxes         List all supported Transfer Syntaxes
  -h, --help              Show this help message

Examples:
  )" << program_name
              << R"( image.dcm converted.dcm --explicit
  )" << program_name
              << R"( image.dcm compressed.dcm --jpeg-baseline -q 85
  )" << program_name
              << R"( ./input_dir/ ./output_dir/ --recursive --implicit
  )" << program_name
              << R"( image.dcm output.dcm -t 1.2.840.10008.1.2.4.50

Exit Codes:
  0  Success - All files converted successfully
  1  Error - Invalid arguments or help requested
  2  Error - Conversion failed for one or more files
)";
}

/**
 * @brief List all supported transfer syntaxes
 */
void list_supported_syntaxes() {
    using namespace pacs::encoding;

    std::cout << "\nSupported Transfer Syntaxes:\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << std::left << std::setw(40) << "Name"
              << std::setw(30) << "UID" << "\n";
    std::cout << std::string(70, '-') << "\n";

    auto syntaxes = supported_transfer_syntaxes();
    for (const auto& ts : syntaxes) {
        std::cout << std::left << std::setw(40) << ts.name()
                  << std::setw(30) << ts.uid() << "\n";
    }

    std::cout << std::string(70, '-') << "\n";
    std::cout << "Total: " << syntaxes.size() << " transfer syntaxes\n\n";

    // Also list compression codecs if available
    auto codec_uids = compression::codec_factory::supported_transfer_syntaxes();
    if (!codec_uids.empty()) {
        std::cout << "Compression Codecs Available:\n";
        std::cout << std::string(50, '-') << "\n";
        for (const auto& uid : codec_uids) {
            if (auto ts = find_transfer_syntax(uid); ts.has_value()) {
                std::cout << "  " << ts->name() << "\n";
            }
        }
        std::cout << std::string(50, '-') << "\n";
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
    using namespace pacs::encoding;

    if (argc < 2) {
        return false;
    }

    // Default to Explicit VR Little Endian
    opts.target_transfer_syntax = std::string(transfer_syntax::explicit_vr_little_endian.uid());

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        } else if (arg == "--list-syntaxes") {
            opts.list_syntaxes = true;
            return true;
        } else if (arg == "--implicit") {
            opts.target_transfer_syntax = std::string(transfer_syntax::implicit_vr_little_endian.uid());
        } else if (arg == "--explicit") {
            opts.target_transfer_syntax = std::string(transfer_syntax::explicit_vr_little_endian.uid());
        } else if (arg == "--explicit-be") {
            opts.target_transfer_syntax = std::string(transfer_syntax::explicit_vr_big_endian.uid());
        } else if (arg == "--jpeg-baseline") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg_baseline.uid());
        } else if (arg == "--jpeg-lossless") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg_lossless.uid());
        } else if (arg == "--jpeg2000") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg2000_lossless.uid());
        } else if (arg == "--jpeg2000-lossy") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg2000_lossy.uid());
        } else if (arg == "--rle") {
            opts.target_transfer_syntax = std::string(transfer_syntax::rle_lossless.uid());
        } else if ((arg == "-t" || arg == "--transfer-syntax") && i + 1 < argc) {
            opts.target_transfer_syntax = argv[++i];
        } else if ((arg == "-q" || arg == "--quality") && i + 1 < argc) {
            try {
                opts.quality = std::stoi(argv[++i]);
                if (opts.quality < 1 || opts.quality > 100) {
                    std::cerr << "Error: Quality must be between 1 and 100\n";
                    return false;
                }
            } catch (...) {
                std::cerr << "Error: Invalid quality value\n";
                return false;
            }
        } else if (arg == "-r" || arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "--overwrite") {
            opts.overwrite = true;
        } else if (arg == "--verify") {
            opts.verify = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--quiet") {
            opts.quiet = true;
            opts.show_progress = false;
        } else if (arg == "--no-progress") {
            opts.show_progress = false;
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

    // Validate transfer syntax
    auto ts = find_transfer_syntax(opts.target_transfer_syntax);
    if (!ts.has_value()) {
        std::cerr << "Error: Unknown Transfer Syntax UID '" << opts.target_transfer_syntax << "'\n";
        std::cerr << "Use --list-syntaxes to see available options\n";
        return false;
    }

    if (!opts.list_syntaxes) {
        if (opts.input_path.empty()) {
            std::cerr << "Error: No input path specified\n";
            return false;
        }

        if (opts.output_path.empty()) {
            std::cerr << "Error: No output path specified\n";
            return false;
        }
    }

    // Quiet mode overrides verbose
    if (opts.quiet) {
        opts.verbose = false;
    }

    return true;
}

/**
 * @brief Convert a single DICOM file to the target transfer syntax
 * @param input_path Input file path
 * @param output_path Output file path
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
        return true;  // Not an error, just skipped
    }

    // Open input file
    auto result = dicom_file::open(input_path);
    if (result.is_err()) {
        std::cerr << "Error: Failed to open '" << input_path.string()
                  << "': " << result.error().message << "\n";
        return false;
    }

    auto& input_file = result.value();
    auto source_ts = input_file.transfer_syntax();
    auto target_ts = transfer_syntax(opts.target_transfer_syntax);

    // Check if conversion is needed
    if (source_ts == target_ts) {
        if (opts.verbose) {
            std::cout << "  Skipped (same TS): " << input_path.filename().string() << "\n";
        }
        // Just copy the file
        std::filesystem::copy_file(input_path, output_path,
            std::filesystem::copy_options::overwrite_existing);
        return true;
    }

    // Check if target transfer syntax is supported
    if (!target_ts.is_supported()) {
        std::cerr << "Error: Target Transfer Syntax '" << target_ts.name()
                  << "' is not currently supported\n";
        return false;
    }

    if (opts.verbose) {
        std::cout << "  Converting: " << input_path.filename().string() << "\n";
        std::cout << "    From: " << source_ts.name() << "\n";
        std::cout << "    To:   " << target_ts.name() << "\n";
    }

    // Create output file with new transfer syntax
    auto output_file = dicom_file::create(
        std::move(input_file.dataset()),
        target_ts
    );

    // Ensure output directory exists
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    // Save output file
    auto save_result = output_file.save(output_path);
    if (save_result.is_err()) {
        std::cerr << "Error: Failed to save '" << output_path.string()
                  << "': " << save_result.error().message << "\n";
        return false;
    }

    // Verify if requested
    if (opts.verify) {
        auto verify_result = dicom_file::open(output_path);
        if (verify_result.is_err()) {
            std::cerr << "Error: Verification failed for '" << output_path.string()
                      << "': " << verify_result.error().message << "\n";
            return false;
        }

        auto& verified_file = verify_result.value();
        if (verified_file.transfer_syntax() != target_ts) {
            std::cerr << "Error: Verification failed - Transfer Syntax mismatch\n";
            return false;
        }

        if (opts.verbose) {
            std::cout << "    Verified: OK\n";
        }
    }

    return true;
}

/**
 * @brief Process a directory of DICOM files
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
        // Check file extension
        auto ext = file_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".dcm" && ext != ".dicom" && !ext.empty()) {
            return;  // Skip non-DICOM files
        }

        ++stats.total_files;

        // Calculate relative path and construct output path
        auto relative_path = std::filesystem::relative(file_path, input_dir);
        auto output_path = output_dir / relative_path;

        auto start = std::chrono::steady_clock::now();

        if (convert_file(file_path, output_path, opts)) {
            ++stats.success_count;
        } else {
            ++stats.error_count;
        }

        auto end = std::chrono::steady_clock::now();
        stats.total_time += std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (opts.show_progress && !opts.quiet) {
            std::cout << "\rProcessed: " << stats.total_files
                      << " (Success: " << stats.success_count
                      << ", Errors: " << stats.error_count << ")" << std::flush;
        }
    };

    if (opts.recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(input_dir)) {
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

    if (opts.show_progress && !opts.quiet) {
        std::cout << "\n";  // New line after progress
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
        auto avg_time = stats.total_time.count() / static_cast<double>(stats.total_files);
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
  ____   ____ __  __    ____ ___  _   ___     __
 |  _ \ / ___|  \/  |  / ___/ _ \| \ | \ \   / /
 | | | | |   | |\/| | | |  | | | |  \| |\ \ / /
 | |_| | |___| |  | | | |__| |_| | |\  | \ V /
 |____/ \____|_|  |_|  \____\___/|_| \_|  \_/

       DICOM Transfer Syntax Converter
)" << "\n";
        print_usage(argv[0]);
        return 1;
    }

    if (opts.list_syntaxes) {
        list_supported_syntaxes();
        return 0;
    }

    // Show banner in verbose mode
    if (!opts.quiet) {
        std::cout << R"(
  ____   ____ __  __    ____ ___  _   ___     __
 |  _ \ / ___|  \/  |  / ___/ _ \| \ | \ \   / /
 | | | | |   | |\/| | | |  | | | |  \| |\ \ / /
 | |_| | |___| |  | | | |__| |_| | |\  | \ V /
 |____/ \____|_|  |_|  \____\___/|_| \_|  \_/

       DICOM Transfer Syntax Converter
)" << "\n";
    }

    // Check input exists
    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: Input path does not exist: " << opts.input_path.string() << "\n";
        return 2;
    }

    auto target_ts = pacs::encoding::transfer_syntax(opts.target_transfer_syntax);
    if (!opts.quiet) {
        std::cout << "Target Transfer Syntax: " << target_ts.name() << "\n";
        std::cout << "                   UID: " << target_ts.uid() << "\n\n";
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
        } else {
            ++stats.error_count;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    stats.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Print summary for directory processing
    if (std::filesystem::is_directory(opts.input_path) && !opts.quiet) {
        print_summary(stats);
    } else if (!opts.quiet) {
        if (stats.success_count > 0) {
            std::cout << "Conversion completed successfully.\n";
        }
    }

    return stats.error_count > 0 ? 2 : 0;
}
