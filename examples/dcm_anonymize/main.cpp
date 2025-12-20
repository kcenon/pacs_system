/**
 * @file main.cpp
 * @brief DICOM Anonymize - De-identification Utility
 *
 * A command-line utility for DICOM de-identification/anonymization compliant
 * with DICOM PS3.15 (Security Profiles). Supports multiple anonymization
 * profiles including HIPAA Safe Harbor and GDPR compliance.
 *
 * @see Issue #284 - dcm_anonymize: Implement DICOM anonymization utility
 * @see DICOM PS3.15 Annex E - Attribute Confidentiality Profiles
 *
 * Usage:
 *   dcm_anonymize [options] <input> [output]
 *
 * Examples:
 *   dcm_anonymize patient.dcm anonymous.dcm
 *   dcm_anonymize --profile hipaa_safe_harbor patient.dcm anonymized.dcm
 *   dcm_anonymize --recursive -o anonymized/ ./originals/
 */

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/security/anonymization_profile.hpp"
#include "pacs/security/anonymizer.hpp"
#include "pacs/security/uid_mapping.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

/**
 * @brief Command line options
 */
struct options {
    std::vector<std::filesystem::path> input_paths;
    std::filesystem::path output_path;
    pacs::security::anonymization_profile profile{
        pacs::security::anonymization_profile::basic};
    std::optional<std::string> new_patient_id;
    std::optional<std::string> new_patient_name;
    std::vector<pacs::core::dicom_tag> keep_tags;
    std::map<pacs::core::dicom_tag, std::string> replace_tags;
    std::filesystem::path mapping_file;
    bool retain_uid{false};
    bool recursive{false};
    bool verify{false};
    bool verbose{false};
    bool dry_run{false};
    bool create_backup{true};
    bool detailed_report{false};
    std::optional<int> date_offset_days;
};

/**
 * @brief Processing statistics
 */
struct process_stats {
    std::size_t total_files{0};
    std::size_t successful{0};
    std::size_t failed{0};
    std::size_t tags_removed{0};
    std::size_t tags_replaced{0};
    std::size_t tags_kept{0};
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << "\nDICOM Anonymize - De-identification Utility\n\n";
    std::cout << "Usage: " << program_name << " [options] <input> [output]\n\n";
    std::cout << "Arguments:\n";
    std::cout
        << "  input               Input DICOM file or directory to anonymize\n";
    std::cout
        << "  output              Output file or directory (optional for "
           "single file)\n\n";

    std::cout << "Profile Options:\n";
    std::cout << "  -p, --profile <name>  Anonymization profile (default: "
                 "basic)\n";
    std::cout << "                        Available profiles:\n";
    std::cout
        << "                          basic                - Remove direct "
           "identifiers\n";
    std::cout
        << "                          clean_pixel          - Remove burned-in "
           "annotations\n";
    std::cout
        << "                          clean_descriptions   - Clean free-text "
           "fields\n";
    std::cout
        << "                          retain_longitudinal  - Preserve temporal "
           "relationships\n";
    std::cout << "                          retain_patient_characteristics - "
                 "Keep demographics\n";
    std::cout
        << "                          hipaa_safe_harbor    - HIPAA 18-identifier "
           "removal\n";
    std::cout
        << "                          gdpr_compliant       - GDPR pseudonymization\n\n";

    std::cout << "Tag Customization Options:\n";
    std::cout
        << "  -k, --keep <tag>        Keep specific tag unchanged\n";
    std::cout << "                          Example: -k \"(0010,0040)\" or -k "
                 "PatientSex\n";
    std::cout << "  -r, --replace <tag=val> Replace tag with specific value\n";
    std::cout << "                          Example: -r \"PatientName=Anonymous\"\n";
    std::cout << "      --patient-id <id>   Set new PatientID\n";
    std::cout << "      --patient-name <n>  Set new PatientName\n";
    std::cout << "      --retain-uid        Retain original UIDs\n";
    std::cout << "      --date-offset <days> Shift dates by specified days\n\n";

    std::cout << "Mapping Options:\n";
    std::cout << "  -m, --mapping-file <f>  UID mapping file (JSON format)\n";
    std::cout << "                          Used for consistent anonymization "
                 "across files\n\n";

    std::cout << "Output Options:\n";
    std::cout << "  -o, --output-dir <dir>  Output directory for batch "
                 "processing\n";
    std::cout << "      --no-backup         Do not create backup file\n\n";

    std::cout << "Processing Options:\n";
    std::cout << "  --recursive             Process directories recursively\n";
    std::cout << "  --verify                Verify anonymization result\n";
    std::cout << "  --dry-run               Show what would be done without "
                 "modifying\n";
    std::cout << "  --detailed              Show detailed anonymization "
                 "report\n";
    std::cout << "  -v, --verbose           Verbose output\n";
    std::cout << "  -h, --help              Show this help message\n\n";

    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " patient.dcm anonymous.dcm\n";
    std::cout << "  " << program_name
              << " --profile hipaa_safe_harbor patient.dcm output.dcm\n";
    std::cout << "  " << program_name << " --patient-id \"STUDY001_001\" -m "
                 "mapping.json patient.dcm\n";
    std::cout << "  " << program_name
              << " --recursive -o anonymized/ ./originals/\n";
    std::cout << "  " << program_name
              << " -k PatientSex -r \"InstitutionName=Research\" patient.dcm\n\n";

    std::cout << "Anonymization Profiles (DICOM PS3.15 Annex E):\n";
    std::cout
        << "  basic                    - Removes patient name, ID, birth date, "
           "etc.\n";
    std::cout
        << "  clean_pixel              - Extends basic with pixel data cleaning\n";
    std::cout
        << "  clean_descriptions       - Extends basic with description field cleaning\n";
    std::cout
        << "  retain_longitudinal      - Date shifting for temporal studies\n";
    std::cout
        << "  retain_patient_characteristics - Keeps sex, age, size, weight\n";
    std::cout
        << "  hipaa_safe_harbor        - Full HIPAA Safe Harbor compliance\n";
    std::cout
        << "  gdpr_compliant           - GDPR pseudonymization requirements\n\n";

    std::cout << "Exit Codes:\n";
    std::cout << "  0  Success\n";
    std::cout << "  1  Invalid arguments\n";
    std::cout << "  2  File/processing error\n";
}

/**
 * @brief Parse a tag string in format (GGGG,EEEE) or keyword
 * @param tag_str The tag string to parse
 * @return Parsed dicom_tag or nullopt if invalid
 */
std::optional<pacs::core::dicom_tag> resolve_tag(const std::string& tag_str) {
    std::string s = tag_str;

    // Check if it's a numeric tag format
    if (s.find('(') != std::string::npos || s.find(',') != std::string::npos) {
        // Remove parentheses if present
        if (!s.empty() && s.front() == '(') {
            s.erase(0, 1);
        }
        if (!s.empty() && s.back() == ')') {
            s.pop_back();
        }

        // Remove spaces
        s.erase(std::remove(s.begin(), s.end(), ' '), s.end());

        // Parse GGGG,EEEE format
        std::size_t comma_pos = s.find(',');
        if (comma_pos != std::string::npos) {
            try {
                auto group = static_cast<std::uint16_t>(
                    std::stoul(s.substr(0, comma_pos), nullptr, 16));
                auto element = static_cast<std::uint16_t>(
                    std::stoul(s.substr(comma_pos + 1), nullptr, 16));
                return pacs::core::dicom_tag{group, element};
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    // Try as keyword
    auto& dict = pacs::core::dicom_dictionary::instance();
    auto info = dict.find_by_keyword(tag_str);
    if (info) {
        return info->tag;
    }

    return std::nullopt;
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
        } else if ((arg == "-p" || arg == "--profile") && i + 1 < argc) {
            std::string profile_name = argv[++i];
            auto profile =
                pacs::security::profile_from_string(profile_name);
            if (!profile) {
                std::cerr << "Error: Unknown profile '" << profile_name
                          << "'\n";
                std::cerr << "Available profiles: basic, clean_pixel, "
                             "clean_descriptions,\n";
                std::cerr << "  retain_longitudinal, "
                             "retain_patient_characteristics,\n";
                std::cerr << "  hipaa_safe_harbor, gdpr_compliant\n";
                return false;
            }
            opts.profile = *profile;
        } else if ((arg == "-k" || arg == "--keep") && i + 1 < argc) {
            auto tag = resolve_tag(argv[++i]);
            if (!tag) {
                std::cerr << "Error: Invalid tag format for --keep\n";
                return false;
            }
            opts.keep_tags.push_back(*tag);
        } else if ((arg == "-r" || arg == "--replace") && i + 1 < argc) {
            std::string replace_arg = argv[++i];
            auto eq_pos = replace_arg.find('=');
            if (eq_pos == std::string::npos) {
                std::cerr << "Error: --replace requires tag=value format\n";
                return false;
            }
            std::string tag_str = replace_arg.substr(0, eq_pos);
            std::string value = replace_arg.substr(eq_pos + 1);
            auto tag = resolve_tag(tag_str);
            if (!tag) {
                std::cerr << "Error: Invalid tag format for --replace\n";
                return false;
            }
            opts.replace_tags[*tag] = value;
        } else if (arg == "--patient-id" && i + 1 < argc) {
            opts.new_patient_id = argv[++i];
        } else if (arg == "--patient-name" && i + 1 < argc) {
            opts.new_patient_name = argv[++i];
        } else if (arg == "--retain-uid") {
            opts.retain_uid = true;
        } else if (arg == "--date-offset" && i + 1 < argc) {
            try {
                opts.date_offset_days = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << "Error: Invalid date offset value\n";
                return false;
            }
        } else if ((arg == "-m" || arg == "--mapping-file") && i + 1 < argc) {
            opts.mapping_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output-dir") && i + 1 < argc) {
            opts.output_path = argv[++i];
        } else if (arg == "--no-backup") {
            opts.create_backup = false;
        } else if (arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "--verify") {
            opts.verify = true;
        } else if (arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "--detailed") {
            opts.detailed_report = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else {
            opts.input_paths.emplace_back(arg);
        }
    }

    // Validation
    if (opts.input_paths.empty()) {
        std::cerr << "Error: No input files specified\n";
        return false;
    }

    // If two positional args and first is file, second is output
    if (opts.input_paths.size() == 2 &&
        std::filesystem::is_regular_file(opts.input_paths[0])) {
        opts.output_path = opts.input_paths[1];
        opts.input_paths.resize(1);
    }

    return true;
}

/**
 * @brief Load UID mapping from file
 * @param path Path to mapping file
 * @param mapping Output mapping object
 * @return true on success
 */
bool load_mapping(const std::filesystem::path& path,
                  pacs::security::uid_mapping& mapping) {
    if (!std::filesystem::exists(path)) {
        // File doesn't exist yet - will be created after anonymization
        return true;
    }

    std::ifstream file(path);
    if (!file) {
        std::cerr << "Error: Cannot open mapping file: " << path.string()
                  << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto result = mapping.from_json(buffer.str());
    if (result.is_err()) {
        std::cerr << "Error: Invalid mapping file format\n";
        return false;
    }

    return true;
}

/**
 * @brief Save UID mapping to file
 * @param path Path to mapping file
 * @param mapping Mapping object to save
 * @return true on success
 */
bool save_mapping(const std::filesystem::path& path,
                  const pacs::security::uid_mapping& mapping) {
    std::ofstream file(path);
    if (!file) {
        std::cerr << "Error: Cannot write mapping file: " << path.string()
                  << "\n";
        return false;
    }

    file << mapping.to_json();
    return true;
}

/**
 * @brief Create backup file
 * @param file_path Original file path
 * @return true if backup was created successfully
 */
bool create_backup(const std::filesystem::path& file_path) {
    auto backup_path = file_path;
    backup_path += ".bak";

    std::error_code ec;
    std::filesystem::copy_file(
        file_path, backup_path,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Warning: Failed to create backup: " << backup_path
                  << "\n";
        return false;
    }

    return true;
}

/**
 * @brief Verify that anonymization was successful
 * @param dataset The anonymized dataset
 * @param profile The anonymization profile used
 * @return Vector of warnings (empty if verification passed)
 */
std::vector<std::string> verify_anonymization(
    const pacs::core::dicom_dataset& dataset,
    pacs::security::anonymization_profile profile) {
    std::vector<std::string> warnings;

    using namespace pacs::core;

    // Check critical identifiers based on profile
    std::vector<std::pair<dicom_tag, std::string>> checks = {
        {tags::patient_name, "PatientName"},
        {tags::patient_id, "PatientID"},
        {tags::patient_birth_date, "PatientBirthDate"},
        {dicom_tag{0x0010, 0x0050}, "PatientInsurancePlanCode"},
        {dicom_tag{0x0010, 0x1000}, "OtherPatientIDs"},
        {dicom_tag{0x0008, 0x0080}, "InstitutionName"},
        {dicom_tag{0x0008, 0x0081}, "InstitutionAddress"},
    };

    for (const auto& [tag, name] : checks) {
        auto value = dataset.get_string(tag);
        if (!value.empty() && value != "Anonymous" && value != "ANONYMOUS" &&
            value.find("ANON") == std::string::npos) {
            warnings.push_back("Tag " + name + " may contain identifying "
                               "information: " + value);
        }
    }

    return warnings;
}

/**
 * @brief Process a single DICOM file
 * @param input_path Input file path
 * @param output_path Output file path
 * @param opts Command line options
 * @param mapping UID mapping for consistent anonymization
 * @param stats Processing statistics
 * @return true on success
 */
bool process_file(const std::filesystem::path& input_path,
                  const std::filesystem::path& output_path,
                  const options& opts,
                  pacs::security::uid_mapping& mapping,
                  process_stats& stats) {
    using namespace pacs::core;
    using namespace pacs::security;

    if (opts.verbose) {
        std::cout << "Processing: " << input_path.string() << "\n";
    }

    // Dry run mode
    if (opts.dry_run) {
        std::cout << "Would anonymize: " << input_path.string() << "\n";
        std::cout << "  Profile: " << to_string(opts.profile) << "\n";
        std::cout << "  Output: " << output_path.string() << "\n";
        if (!opts.keep_tags.empty()) {
            std::cout << "  Keep tags: " << opts.keep_tags.size() << "\n";
        }
        if (!opts.replace_tags.empty()) {
            std::cout << "  Replace tags: " << opts.replace_tags.size() << "\n";
        }
        return true;
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

    // Create anonymizer with profile
    anonymizer anon(opts.profile);

    // Configure date offset if specified
    if (opts.date_offset_days) {
        anon.set_date_offset(std::chrono::days{*opts.date_offset_days});
    }

    // Add keep tags (action: keep)
    for (const auto& tag : opts.keep_tags) {
        anon.add_tag_action(tag, tag_action_config::make_keep());
        if (opts.verbose) {
            std::cout << "  Keeping tag: " << tag.to_string() << "\n";
        }
    }

    // Add replace tags
    for (const auto& [tag, value] : opts.replace_tags) {
        anon.add_tag_action(tag, tag_action_config::make_replace(value));
        if (opts.verbose) {
            std::cout << "  Replacing tag: " << tag.to_string()
                      << " = " << value << "\n";
        }
    }

    // Handle patient ID/name replacements
    if (opts.new_patient_id) {
        anon.add_tag_action(
            tags::patient_id,
            tag_action_config::make_replace(*opts.new_patient_id));
    }
    if (opts.new_patient_name) {
        anon.add_tag_action(
            tags::patient_name,
            tag_action_config::make_replace(*opts.new_patient_name));
    }

    // Handle UID retention
    if (opts.retain_uid) {
        anon.add_tag_action(tags::study_instance_uid,
                            tag_action_config::make_keep());
        anon.add_tag_action(tags::series_instance_uid,
                            tag_action_config::make_keep());
        anon.add_tag_action(tags::sop_instance_uid,
                            tag_action_config::make_keep());
    }

    // Perform anonymization
    auto anon_result = (!opts.mapping_file.empty() || !mapping.empty())
        ? anon.anonymize_with_mapping(dataset, mapping)
        : anon.anonymize(dataset);

    if (anon_result.is_err()) {
        std::cerr << "Error: Anonymization failed for '"
                  << input_path.string()
                  << "': " << anon_result.error().message << "\n";
        return false;
    }

    auto report = anon_result.value();
    stats.tags_removed += report.tags_removed;
    stats.tags_replaced += report.tags_replaced;
    stats.tags_kept += report.tags_kept;

    // Show summary if detailed report requested
    if (opts.detailed_report) {
        std::cout << "  Processed: " << report.total_tags_processed << " tags\n";
        std::cout << "    Removed: " << report.tags_removed << "\n";
        std::cout << "    Replaced: " << report.tags_replaced << "\n";
        std::cout << "    Emptied: " << report.tags_emptied << "\n";
        std::cout << "    UIDs replaced: " << report.uids_replaced << "\n";
        std::cout << "    Dates shifted: " << report.dates_shifted << "\n";
        std::cout << "    Kept: " << report.tags_kept << "\n";
    }

    // Verify if requested
    if (opts.verify) {
        auto warnings = verify_anonymization(dataset, opts.profile);
        if (!warnings.empty()) {
            std::cout << "  Verification warnings:\n";
            for (const auto& warning : warnings) {
                std::cout << "    - " << warning << "\n";
            }
        } else if (opts.verbose) {
            std::cout << "  Verification: PASSED\n";
        }
    }

    // Create output file with same transfer syntax
    auto output_file =
        dicom_file::create(std::move(dataset), file.transfer_syntax());

    // Ensure output directory exists
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    // Create backup if in-place and backup is enabled
    if (input_path == output_path && opts.create_backup) {
        create_backup(input_path);
    }

    // Save
    auto save_result = output_file.save(output_path);
    if (save_result.is_err()) {
        std::cerr << "Error: Failed to save '" << output_path.string()
                  << "': " << save_result.error().message << "\n";
        return false;
    }

    if (opts.verbose) {
        std::cout << "  Saved: " << output_path.string() << "\n";
        std::cout << "  Tags removed: " << report.tags_removed
                  << ", replaced: " << report.tags_replaced
                  << ", kept: " << report.tags_kept << "\n";
    }

    return true;
}

/**
 * @brief Process input paths (files or directories)
 * @param opts Command line options
 * @param stats Processing statistics
 * @param mapping UID mapping for consistent anonymization
 */
void process_inputs(const options& opts, process_stats& stats,
                    pacs::security::uid_mapping& mapping) {
    for (const auto& input_path : opts.input_paths) {
        if (!std::filesystem::exists(input_path)) {
            std::cerr << "Error: Path does not exist: " << input_path.string()
                      << "\n";
            ++stats.failed;
            continue;
        }

        if (std::filesystem::is_directory(input_path)) {
            // Directory mode
            auto process_entry = [&](const std::filesystem::path& file_path) {
                auto ext = file_path.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".dcm" && ext != ".dicom" && !ext.empty()) {
                    return;  // Skip non-DICOM files
                }

                ++stats.total_files;

                std::filesystem::path output_path;
                if (!opts.output_path.empty()) {
                    auto relative =
                        std::filesystem::relative(file_path, input_path);
                    output_path = opts.output_path / relative;
                } else {
                    // In-place anonymization
                    output_path = file_path;
                }

                if (process_file(file_path, output_path, opts, mapping,
                                 stats)) {
                    ++stats.successful;
                } else {
                    ++stats.failed;
                }
            };

            if (opts.recursive) {
                for (const auto& entry :
                     std::filesystem::recursive_directory_iterator(
                         input_path)) {
                    if (entry.is_regular_file()) {
                        process_entry(entry.path());
                    }
                }
            } else {
                for (const auto& entry :
                     std::filesystem::directory_iterator(input_path)) {
                    if (entry.is_regular_file()) {
                        process_entry(entry.path());
                    }
                }
            }
        } else {
            // Single file mode
            ++stats.total_files;

            std::filesystem::path output_path;
            if (!opts.output_path.empty()) {
                output_path = opts.output_path;
            } else {
                // In-place anonymization
                output_path = input_path;
            }

            if (process_file(input_path, output_path, opts, mapping, stats)) {
                ++stats.successful;
            } else {
                ++stats.failed;
            }
        }
    }
}

/**
 * @brief Print processing summary
 * @param stats Processing statistics
 */
void print_summary(const process_stats& stats) {
    if (stats.total_files > 1 || stats.tags_removed > 0) {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "       Anonymization Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Total files:    " << stats.total_files << "\n";
        std::cout << "  Successful:     " << stats.successful << "\n";
        std::cout << "  Failed:         " << stats.failed << "\n";
        std::cout << "  ----------------------------------------\n";
        std::cout << "  Tags removed:   " << stats.tags_removed << "\n";
        std::cout << "  Tags replaced:  " << stats.tags_replaced << "\n";
        std::cout << "  Tags kept:      " << stats.tags_kept << "\n";
        std::cout << "========================================\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____   ____ __  __      _    _   _  ___  _   ___   ____  __ ___ ________
 |  _ \ / ___|  \/  |    / \  | \ | |/ _ \| \ | \ \ / /  \/  |_ _|__  / __|
 | | | | |   | |\/| |   / _ \ |  \| | | | |  \| |\ V /| |\/| || |  / /| _|
 | |_| | |___| |  | |  / ___ \| |\  | |_| | |\  | | | | |  | || | / /_| |__
 |____/ \____|_|  |_| /_/   \_\_| \_|\___/|_| \_| |_| |_|  |_|___/____|____|

      DICOM De-identification Utility (PS3.15 Compliant)
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 1;
    }

    // Load existing UID mapping if specified
    pacs::security::uid_mapping mapping;
    if (!opts.mapping_file.empty()) {
        if (!load_mapping(opts.mapping_file, mapping)) {
            return 2;
        }
        if (opts.verbose && !mapping.empty()) {
            std::cout << "Loaded " << mapping.size()
                      << " existing UID mappings\n";
        }
    }

    // Show profile info
    if (opts.verbose) {
        std::cout << "Anonymization profile: " << to_string(opts.profile)
                  << "\n";
    }

    // Process files
    process_stats stats;
    process_inputs(opts, stats, mapping);

    // Save UID mapping if specified
    if (!opts.mapping_file.empty() && !mapping.empty() && !opts.dry_run) {
        if (!save_mapping(opts.mapping_file, mapping)) {
            std::cerr << "Warning: Failed to save UID mapping file\n";
        } else if (opts.verbose) {
            std::cout << "Saved " << mapping.size() << " UID mappings to "
                      << opts.mapping_file.string() << "\n";
        }
    }

    // Print summary
    print_summary(stats);

    if (stats.failed > 0) {
        return 2;
    }

    if (stats.total_files == 1 && stats.successful == 1) {
        std::cout << "Successfully anonymized file.\n";
    }

    return 0;
}
