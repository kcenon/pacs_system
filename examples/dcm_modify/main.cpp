/**
 * @file main.cpp
 * @brief DICOM Modify - Tag Modification Utility
 *
 * A command-line utility for modifying DICOM tag values, anonymizing
 * patient information, and converting transfer syntax.
 *
 * @see Issue #108 - DICOM Modify Sample
 * @see DICOM PS3.10 - Media Storage and File Format
 * @see DICOM PS3.15 - Security and System Management Profiles
 *
 * Usage:
 *   dcm_modify <input> [options]
 *
 * Examples:
 *   dcm_modify image.dcm --set PatientName="Anonymous" -o modified.dcm
 *   dcm_modify image.dcm --anonymize -o anonymized.dcm
 *   dcm_modify image.dcm --transfer-syntax explicit-le -o converted.dcm
 *   dcm_modify ./input/ --anonymize -o ./output/ --recursive
 */

#include "anonymizer.hpp"

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
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

/**
 * @brief Tag modification operation
 */
struct tag_modification {
    std::string keyword;  // Tag keyword (e.g., "PatientName")
    std::string value;    // New value
};

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::vector<tag_modification> modifications;
    std::vector<std::string> delete_tags;
    bool anonymize{false};
    std::string transfer_syntax;  // Target transfer syntax
    bool recursive{false};
    bool in_place{false};
    bool parallel{false};
    bool verbose{false};
    bool dry_run{false};
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
DICOM Modify - Tag Modification Utility

Usage: )" << program_name
              << R"( <input> [options]

Arguments:
  input               DICOM file or directory to modify

Options:
  -o, --output <path> Output file or directory
  --set <key>=<val>   Set tag value (can be repeated)
                      Example: --set PatientName="John Doe"
  --delete <key>      Delete tag (can be repeated)
                      Example: --delete PatientBirthDate
  --anonymize         Apply basic anonymization (DICOM PS3.15)
  --transfer-syntax   Convert to specified transfer syntax:
                      implicit-le, explicit-le, explicit-be
  --recursive, -r     Process directories recursively
  --in-place          Modify files in place (DANGEROUS!)
  --parallel          Process files in parallel (batch mode)
  --dry-run           Show what would be done without modifying
  --verbose, -v       Show detailed output
  --help, -h          Show this help message

Examples:
  # Modify single tag
  )" << program_name
              << R"( image.dcm --set PatientName="Anonymous" -o modified.dcm

  # Modify multiple tags
  )" << program_name
              << R"( image.dcm \
    --set PatientName="Anonymous" \
    --set PatientID="ANON001" \
    --delete PatientBirthDate \
    -o anonymized.dcm

  # Anonymize (preset)
  )" << program_name
              << R"( image.dcm --anonymize -o anonymized.dcm

  # Change transfer syntax
  )" << program_name
              << R"( image.dcm --transfer-syntax explicit-le -o converted.dcm

  # Batch modify directory
  )" << program_name
              << R"( ./input/ --anonymize -o ./output/ --recursive

  # In-place modification (dangerous!)
  )" << program_name
              << R"( image.dcm --set PatientID="NEW_ID" --in-place

Anonymization Tags Removed/Replaced (--anonymize):
  PatientName, PatientID, PatientBirthDate, PatientAddress,
  ReferringPhysicianName, AccessionNumber, and more.
  UIDs are regenerated to prevent correlation.

Transfer Syntax Options:
  implicit-le     Implicit VR Little Endian (1.2.840.10008.1.2)
  explicit-le     Explicit VR Little Endian (1.2.840.10008.1.2.1)
  explicit-be     Explicit VR Big Endian (1.2.840.10008.1.2.2) [Retired]

Exit Codes:
  0  Success
  1  Invalid arguments
  2  File/processing error
)";
}

/**
 * @brief Parse a tag modification string (Key=Value)
 * @param str Input string
 * @param mod Output modification
 * @return true if parsing succeeded
 */
bool parse_modification(const std::string& str, tag_modification& mod) {
    auto pos = str.find('=');
    if (pos == std::string::npos || pos == 0) {
        return false;
    }

    mod.keyword = str.substr(0, pos);
    mod.value = str.substr(pos + 1);

    // Remove surrounding quotes from value
    if (mod.value.size() >= 2) {
        if ((mod.value.front() == '"' && mod.value.back() == '"') ||
            (mod.value.front() == '\'' && mod.value.back() == '\'')) {
            mod.value = mod.value.substr(1, mod.value.size() - 2);
        }
    }

    return true;
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
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            opts.output_path = argv[++i];
        } else if (arg == "--set" && i + 1 < argc) {
            tag_modification mod;
            if (!parse_modification(argv[++i], mod)) {
                std::cerr << "Error: Invalid --set format. Use Key=Value\n";
                return false;
            }
            opts.modifications.push_back(std::move(mod));
        } else if (arg == "--delete" && i + 1 < argc) {
            opts.delete_tags.push_back(argv[++i]);
        } else if (arg == "--anonymize") {
            opts.anonymize = true;
        } else if (arg == "--transfer-syntax" && i + 1 < argc) {
            opts.transfer_syntax = argv[++i];
        } else if (arg == "--recursive" || arg == "-r") {
            opts.recursive = true;
        } else if (arg == "--in-place") {
            opts.in_place = true;
        } else if (arg == "--parallel") {
            opts.parallel = true;
        } else if (arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else if (opts.input_path.empty()) {
            opts.input_path = arg;
        } else {
            std::cerr << "Error: Multiple inputs specified\n";
            return false;
        }
    }

    // Validation
    if (opts.input_path.empty()) {
        std::cerr << "Error: No input path specified\n";
        return false;
    }

    if (!opts.in_place && opts.output_path.empty()) {
        std::cerr << "Error: No output path specified (use -o or --in-place)\n";
        return false;
    }

    if (opts.in_place && !opts.output_path.empty()) {
        std::cerr << "Error: Cannot use both --in-place and -o\n";
        return false;
    }

    // Must have at least one modification operation
    if (opts.modifications.empty() && opts.delete_tags.empty() &&
        !opts.anonymize && opts.transfer_syntax.empty()) {
        std::cerr << "Error: No modification operation specified\n";
        return false;
    }

    return true;
}

/**
 * @brief Look up tag by keyword
 * @param keyword Tag keyword
 * @return The tag if found
 */
std::optional<pacs::core::dicom_tag> find_tag_by_keyword(
    const std::string& keyword) {
    auto& dict = pacs::core::dicom_dictionary::instance();

    // Try direct lookup (case-sensitive)
    auto info = dict.find_by_keyword(keyword);
    if (info) {
        return info->tag;
    }

    // Dictionary doesn't support iteration, so we can't do case-insensitive
    // search. Return nullopt if exact match not found.
    return std::nullopt;
}

/**
 * @brief Get transfer syntax from string identifier
 * @param id Transfer syntax identifier
 * @return The transfer syntax if valid
 */
std::optional<pacs::encoding::transfer_syntax> get_transfer_syntax(
    const std::string& id) {
    using namespace pacs::encoding;

    if (id == "implicit-le" || id == "implicit") {
        return transfer_syntax::implicit_vr_little_endian;
    } else if (id == "explicit-le" || id == "explicit") {
        return transfer_syntax::explicit_vr_little_endian;
    } else if (id == "explicit-be") {
        return transfer_syntax::explicit_vr_big_endian;
    }

    return std::nullopt;
}

/**
 * @brief Apply tag modifications to a dataset
 * @param dataset The dataset to modify
 * @param opts Command line options
 * @param verbose Print details
 * @return true if all modifications succeeded
 */
bool apply_modifications(pacs::core::dicom_dataset& dataset,
                         const options& opts, bool verbose) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();

    // Apply tag modifications
    for (const auto& mod : opts.modifications) {
        auto tag_opt = find_tag_by_keyword(mod.keyword);
        if (!tag_opt) {
            std::cerr << "  Warning: Unknown tag keyword '" << mod.keyword
                      << "'\n";
            continue;
        }

        auto tag = *tag_opt;
        auto info = dict.find(tag);
        // Cast from uint16_t to vr_type enum
        vr_type vr = info ? static_cast<vr_type>(info->vr) : vr_type::LO;

        if (verbose) {
            std::cout << "  Set " << tag.to_string() << " " << mod.keyword
                      << " = \"" << mod.value << "\"\n";
        }

        dataset.set_string(tag, vr, mod.value);
    }

    // Delete tags
    for (const auto& keyword : opts.delete_tags) {
        auto tag_opt = find_tag_by_keyword(keyword);
        if (!tag_opt) {
            std::cerr << "  Warning: Unknown tag keyword '" << keyword << "'\n";
            continue;
        }

        if (verbose) {
            std::cout << "  Delete " << tag_opt->to_string() << " " << keyword
                      << "\n";
        }

        dataset.remove(*tag_opt);
    }

    return true;
}

/**
 * @brief Processing statistics
 */
struct process_stats {
    size_t total_files{0};
    size_t successful{0};
    size_t failed{0};
};

/**
 * @brief Process a single DICOM file
 * @param input_path Input file path
 * @param output_path Output file path
 * @param opts Command line options
 * @param anonymizer Anonymizer instance (shared for consistent UIDs)
 * @return true on success
 */
bool process_file(const std::filesystem::path& input_path,
                  const std::filesystem::path& output_path, const options& opts,
                  pacs::dcm_modify::anonymizer& anonymizer) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    if (opts.verbose) {
        std::cout << "Processing: " << input_path.string() << "\n";
    }

    // Open input file
    auto result = dicom_file::open(input_path);
    if (result.is_err()) {
        std::cerr << "Error: Failed to open '" << input_path.string()
                  << "': " << result.error().message << "\n";
        return false;
    }

    auto file = std::move(result.value());
    auto& dataset = file.dataset();

    // Dry run mode
    if (opts.dry_run) {
        std::cout << "Would modify: " << input_path.string() << "\n";
        if (opts.anonymize) {
            std::cout << "  Apply anonymization\n";
        }
        if (!opts.transfer_syntax.empty()) {
            std::cout << "  Convert transfer syntax to: " << opts.transfer_syntax
                      << "\n";
        }
        for (const auto& mod : opts.modifications) {
            std::cout << "  Set " << mod.keyword << " = \"" << mod.value
                      << "\"\n";
        }
        for (const auto& tag : opts.delete_tags) {
            std::cout << "  Delete " << tag << "\n";
        }
        std::cout << "  Output: " << output_path.string() << "\n";
        return true;
    }

    // Apply anonymization first (before other modifications)
    if (opts.anonymize) {
        if (opts.verbose) {
            std::cout << "  Applying anonymization...\n";
        }
        anonymizer.anonymize(dataset);
    }

    // Apply manual modifications
    if (!apply_modifications(dataset, opts, opts.verbose)) {
        return false;
    }

    // Handle transfer syntax conversion
    transfer_syntax target_ts = file.transfer_syntax();
    if (!opts.transfer_syntax.empty()) {
        auto ts_opt = get_transfer_syntax(opts.transfer_syntax);
        if (!ts_opt) {
            std::cerr << "Error: Unknown transfer syntax '" << opts.transfer_syntax
                      << "'\n";
            return false;
        }
        target_ts = *ts_opt;

        if (opts.verbose) {
            std::cout << "  Converting transfer syntax to: " << target_ts.name()
                      << "\n";
        }
    }

    // Create output file with target transfer syntax
    auto output_file = dicom_file::create(std::move(dataset), target_ts);

    // Ensure output directory exists
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    // Save
    auto save_result = output_file.save(output_path);
    if (!save_result.has_value()) {
        std::cerr << "Error: Failed to save '" << output_path.string()
                  << "': " << to_string(save_result.error()) << "\n";
        return false;
    }

    if (opts.verbose) {
        std::cout << "  Saved: " << output_path.string() << "\n";
    }

    return true;
}

/**
 * @brief Process a directory of DICOM files
 * @param input_dir Input directory
 * @param output_dir Output directory
 * @param opts Command line options
 * @param stats Processing statistics
 */
void process_directory(const std::filesystem::path& input_dir,
                       const std::filesystem::path& output_dir,
                       const options& opts, process_stats& stats) {
    // Shared anonymizer for consistent UID mapping across files
    pacs::dcm_modify::anonymizer anonymizer;

    auto process_entry = [&](const std::filesystem::path& input_path) {
        // Check extension
        auto ext = input_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".dcm" && ext != ".dicom" && !ext.empty()) {
            return;  // Skip non-DICOM files
        }

        ++stats.total_files;

        // Calculate output path
        auto relative = std::filesystem::relative(input_path, input_dir);
        auto output_path = output_dir / relative;

        if (process_file(input_path, output_path, opts, anonymizer)) {
            ++stats.successful;
        } else {
            ++stats.failed;
        }
    };

    if (opts.recursive) {
        for (const auto& entry :
             std::filesystem::recursive_directory_iterator(input_dir)) {
            if (entry.is_regular_file()) {
                process_entry(entry.path());
            }
        }
    } else {
        for (const auto& entry :
             std::filesystem::directory_iterator(input_dir)) {
            if (entry.is_regular_file()) {
                process_entry(entry.path());
            }
        }
    }
}

/**
 * @brief Print processing summary
 * @param stats Processing statistics
 */
void print_summary(const process_stats& stats) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "         Processing Summary\n";
    std::cout << "========================================\n";
    std::cout << "  Total files:   " << stats.total_files << "\n";
    std::cout << "  Successful:    " << stats.successful << "\n";
    std::cout << "  Failed:        " << stats.failed << "\n";
    std::cout << "========================================\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____   ____ __  __   __  __  ___  ____ ___ _______   __
 |  _ \ / ___|  \/  | |  \/  |/ _ \|  _ \_ _|  ___\ \ / /
 | | | | |   | |\/| | | |\/| | | | | | | | || |_   \ V /
 | |_| | |___| |  | | | |  | | |_| | |_| | ||  _|   | |
 |____/ \____|_|  |_| |_|  |_|\___/|____/___|_|     |_|

         DICOM Tag Modification Utility
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 1;
    }

    // Check input exists
    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: Input path does not exist: "
                  << opts.input_path.string() << "\n";
        return 2;
    }

    // Handle in-place mode
    if (opts.in_place) {
        opts.output_path = opts.input_path;
    }

    // Process
    if (std::filesystem::is_directory(opts.input_path)) {
        // Directory mode
        if (!std::filesystem::is_directory(opts.output_path) &&
            std::filesystem::exists(opts.output_path)) {
            std::cerr << "Error: Output must be a directory for batch processing\n";
            return 1;
        }

        process_stats stats;
        process_directory(opts.input_path, opts.output_path, opts, stats);
        print_summary(stats);

        return stats.failed > 0 ? 2 : 0;
    } else {
        // Single file mode
        pacs::dcm_modify::anonymizer anonymizer;

        if (process_file(opts.input_path, opts.output_path, opts, anonymizer)) {
            std::cout << "Successfully modified: " << opts.output_path.string()
                      << "\n";
            return 0;
        } else {
            return 2;
        }
    }
}
