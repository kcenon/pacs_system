/**
 * @file main.cpp
 * @brief DICOM Modify - Tag Modification Utility
 *
 * A command-line utility for modifying DICOM tag values, similar to dcmtk's
 * dcmodify. Supports tag insertion, modification, deletion, and UID
 * regeneration.
 *
 * @see Issue #281 - dcm_modify: Implement DICOM tag modification utility
 * @see DICOM PS3.10 - Media Storage and File Format
 * @see DICOM PS3.15 - Security and System Management Profiles
 *
 * Usage:
 *   dcm_modify [options] <dicom-file>
 *
 * Examples:
 *   dcm_modify -i "(0010,0010)=Anonymous" patient.dcm
 *   dcm_modify --insert PatientName=Anonymous -o modified.dcm patient.dcm
 *   dcm_modify --script modify.txt *.dcm
 */

#include "anonymizer.hpp"

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

/**
 * @brief Modification operation type
 */
enum class operation_type {
    insert,      // Add or modify tag (tag doesn't need to exist)
    modify,      // Modify tag (tag must exist)
    erase,       // Delete single tag
    erase_all    // Delete all matching tags including in sequences
};

/**
 * @brief Tag modification operation
 */
struct modification {
    operation_type op;
    pacs::core::dicom_tag tag;
    std::string value;  // For insert/modify operations
    std::string keyword;  // Original keyword or tag string for error messages
};

/**
 * @brief Command line options
 */
struct options {
    std::vector<std::filesystem::path> input_paths;
    std::filesystem::path output_path;
    std::vector<modification> modifications;
    std::filesystem::path script_file;
    bool erase_private{false};
    bool gen_study_uid{false};
    bool gen_series_uid{false};
    bool gen_instance_uid{false};
    bool create_backup{true};
    bool in_place{false};
    bool recursive{false};
    bool verbose{false};
    bool dry_run{false};
};

/**
 * @brief UID generator for consistent UID generation
 */
class uid_generator {
public:
    std::string generate() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now.time_since_epoch())
                             .count();
        return std::string(uid_root_) + "." + std::to_string(timestamp) + "." +
               std::to_string(++counter);
    }

private:
    static constexpr const char* uid_root_ = "1.2.826.0.1.3680043.8.1055.2";
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << "\nDICOM Modify - Tag Modification Utility\n\n";
    std::cout << "Usage: " << program_name << " [options] <dicom-file>...\n\n";
    std::cout << "Arguments:\n";
    std::cout << "  dicom-file          One or more DICOM files to modify\n\n";
    std::cout << "Tag Modification Options:\n";
    std::cout << "  -i, --insert <tag=value>    Add or modify tag (creates if not exists)\n";
    std::cout << "                              Example: -i \"(0010,0010)=Anonymous\"\n";
    std::cout << "                              Example: -i PatientName=Anonymous\n";
    std::cout << "  -m, --modify <tag=value>    Modify existing tag (error if not exists)\n";
    std::cout << "                              Example: -m \"(0010,0020)=NEW_ID\"\n";
    std::cout << "  -e, --erase <tag>           Delete tag\n";
    std::cout << "                              Example: -e \"(0010,1000)\"\n";
    std::cout << "                              Example: -e OtherPatientIDs\n";
    std::cout << "  -ea, --erase-all <tag>      Delete all matching tags (including in sequences)\n";
    std::cout << "  -ep, --erase-private        Delete all private tags\n\n";
    std::cout << "UID Generation Options:\n";
    std::cout << "  -gst, --gen-stud-uid        Generate new StudyInstanceUID\n";
    std::cout << "  -gse, --gen-ser-uid         Generate new SeriesInstanceUID\n";
    std::cout << "  -gin, --gen-inst-uid        Generate new SOPInstanceUID\n\n";
    std::cout << "Output Options:\n";
    std::cout << "  -o, --output <path>         Output file or directory\n";
    std::cout << "  -nb, --no-backup            Do not create backup file (.bak)\n\n";
    std::cout << "Script Option:\n";
    std::cout << "  --script <file>             Read modification commands from script file\n\n";
    std::cout << "Processing Options:\n";
    std::cout << "  -r, --recursive             Process directories recursively\n";
    std::cout << "  --dry-run                   Show what would be done without modifying\n";
    std::cout << "  -v, --verbose               Show detailed output\n";
    std::cout << "  -h, --help                  Show this help message\n\n";
    std::cout << "Tag Format:\n";
    std::cout << "  Tags can be specified in two formats:\n";
    std::cout << "  - Numeric: (GGGG,EEEE) e.g., (0010,0010)\n";
    std::cout << "  - Keyword: e.g., PatientName, PatientID\n\n";
    std::cout << "Script File Format:\n";
    std::cout << "  Lines starting with # are comments\n";
    std::cout << "  i (0010,0010)=Anonymous     Insert/modify tag\n";
    std::cout << "  m (0008,0050)=ACC001        Modify existing tag\n";
    std::cout << "  e (0010,1000)               Erase tag\n";
    std::cout << "  ea (0010,1001)              Erase all matching tags\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -i \"(0010,0010)=Anonymous\" patient.dcm\n";
    std::cout << "  " << program_name << " -m PatientName=\"Hong^Gildong\" -o modified.dcm patient.dcm\n";
    std::cout << "  " << program_name << " -gst -gse -gin -o anonymized.dcm patient.dcm\n";
    std::cout << "  " << program_name << " --script modify.txt *.dcm\n";
    std::cout << "  " << program_name << " -i PatientID=NEW_ID patient.dcm  (in-place with backup)\n";
    std::cout << "  " << program_name << " -i PatientID=NEW_ID -nb patient.dcm  (no backup)\n\n";
    std::cout << "Exit Codes:\n";
    std::cout << "  0  Success\n";
    std::cout << "  1  Invalid arguments\n";
    std::cout << "  2  File/processing error\n";
}

/**
 * @brief Parse a tag string in format (GGGG,EEEE) or GGGG,EEEE
 * @param tag_str The tag string to parse
 * @return Parsed dicom_tag or nullopt if invalid
 */
std::optional<pacs::core::dicom_tag> parse_tag_string(
    const std::string& tag_str) {
    std::string s = tag_str;

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
    size_t comma_pos = s.find(',');
    if (comma_pos != std::string::npos) {
        try {
            uint16_t group =
                static_cast<uint16_t>(std::stoul(s.substr(0, comma_pos), nullptr, 16));
            uint16_t element =
                static_cast<uint16_t>(std::stoul(s.substr(comma_pos + 1), nullptr, 16));
            return pacs::core::dicom_tag{group, element};
        } catch (...) {
            return std::nullopt;
        }
    }

    // Parse GGGGEEEE format (8 hex chars)
    if (s.length() == 8) {
        try {
            uint16_t group =
                static_cast<uint16_t>(std::stoul(s.substr(0, 4), nullptr, 16));
            uint16_t element =
                static_cast<uint16_t>(std::stoul(s.substr(4, 4), nullptr, 16));
            return pacs::core::dicom_tag{group, element};
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

/**
 * @brief Look up tag by keyword or parse tag string
 * @param str Tag keyword or numeric string
 * @return The tag if found/parsed, nullopt otherwise
 */
std::optional<pacs::core::dicom_tag> resolve_tag(const std::string& str) {
    // First, try as numeric tag format
    if (str.find('(') != std::string::npos || str.find(',') != std::string::npos ||
        (str.length() == 8 && std::all_of(str.begin(), str.end(), ::isxdigit))) {
        return parse_tag_string(str);
    }

    // Try as keyword
    auto& dict = pacs::core::dicom_dictionary::instance();
    auto info = dict.find_by_keyword(str);
    if (info) {
        return info->tag;
    }

    return std::nullopt;
}

/**
 * @brief Parse a modification string (tag=value or just tag for erase)
 * @param str Input string
 * @param op Operation type
 * @param mod Output modification
 * @return true if parsing succeeded
 */
bool parse_modification_string(const std::string& str, operation_type op,
                                modification& mod) {
    mod.op = op;

    if (op == operation_type::erase || op == operation_type::erase_all) {
        // Just tag, no value
        auto tag_opt = resolve_tag(str);
        if (!tag_opt) {
            return false;
        }
        mod.tag = *tag_opt;
        mod.keyword = str;
        return true;
    }

    // For insert/modify: tag=value
    auto eq_pos = str.find('=');
    if (eq_pos == std::string::npos || eq_pos == 0) {
        return false;
    }

    std::string tag_str = str.substr(0, eq_pos);
    std::string value = str.substr(eq_pos + 1);

    // Remove surrounding quotes from value
    if (value.size() >= 2) {
        if ((value.front() == '"' && value.back() == '"') ||
            (value.front() == '\'' && value.back() == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }

    auto tag_opt = resolve_tag(tag_str);
    if (!tag_opt) {
        return false;
    }

    mod.tag = *tag_opt;
    mod.value = value;
    mod.keyword = tag_str;
    return true;
}

/**
 * @brief Parse script file and add modifications
 * @param script_path Path to script file
 * @param modifications Output vector of modifications
 * @return true if parsing succeeded
 */
bool parse_script_file(const std::filesystem::path& script_path,
                       std::vector<modification>& modifications) {
    std::ifstream file(script_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open script file: " << script_path.string()
                  << "\n";
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        ++line_num;

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) {
            continue;  // Empty line
        }
        line = line.substr(start);

        // Skip comments
        if (line[0] == '#') {
            continue;
        }

        // Remove trailing comment
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
            // Trim trailing whitespace
            size_t end = line.find_last_not_of(" \t");
            if (end != std::string::npos) {
                line = line.substr(0, end + 1);
            }
        }

        if (line.empty()) {
            continue;
        }

        // Parse command
        operation_type op;
        std::string arg;

        if (line.length() >= 2 && line[0] == 'i' && line[1] == ' ') {
            op = operation_type::insert;
            arg = line.substr(2);
        } else if (line.length() >= 2 && line[0] == 'm' && line[1] == ' ') {
            op = operation_type::modify;
            arg = line.substr(2);
        } else if (line.length() >= 2 && line[0] == 'e' && line[1] == ' ') {
            op = operation_type::erase;
            arg = line.substr(2);
        } else if (line.length() >= 3 && line.substr(0, 2) == "ea" &&
                   line[2] == ' ') {
            op = operation_type::erase_all;
            arg = line.substr(3);
        } else {
            std::cerr << "Warning: Invalid command in script file at line "
                      << line_num << ": " << line << "\n";
            continue;
        }

        // Trim argument
        start = arg.find_first_not_of(" \t");
        if (start != std::string::npos) {
            arg = arg.substr(start);
        }

        modification mod;
        if (!parse_modification_string(arg, op, mod)) {
            std::cerr << "Warning: Invalid modification in script file at line "
                      << line_num << ": " << arg << "\n";
            continue;
        }

        modifications.push_back(std::move(mod));
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
        } else if ((arg == "-i" || arg == "--insert") && i + 1 < argc) {
            modification mod;
            if (!parse_modification_string(argv[++i], operation_type::insert,
                                            mod)) {
                std::cerr << "Error: Invalid --insert format. Use tag=value "
                             "(e.g., \"(0010,0010)=Anonymous\")\n";
                return false;
            }
            opts.modifications.push_back(std::move(mod));
        } else if ((arg == "-m" || arg == "--modify") && i + 1 < argc) {
            modification mod;
            if (!parse_modification_string(argv[++i], operation_type::modify,
                                            mod)) {
                std::cerr << "Error: Invalid --modify format. Use tag=value "
                             "(e.g., \"(0010,0020)=NEW_ID\")\n";
                return false;
            }
            opts.modifications.push_back(std::move(mod));
        } else if ((arg == "-e" || arg == "--erase") && i + 1 < argc) {
            modification mod;
            if (!parse_modification_string(argv[++i], operation_type::erase,
                                            mod)) {
                std::cerr << "Error: Invalid --erase format. Use tag "
                             "(e.g., \"(0010,1000)\")\n";
                return false;
            }
            opts.modifications.push_back(std::move(mod));
        } else if ((arg == "-ea" || arg == "--erase-all") && i + 1 < argc) {
            modification mod;
            if (!parse_modification_string(argv[++i], operation_type::erase_all,
                                            mod)) {
                std::cerr << "Error: Invalid --erase-all format. Use tag "
                             "(e.g., \"(0010,1001)\")\n";
                return false;
            }
            opts.modifications.push_back(std::move(mod));
        } else if (arg == "-ep" || arg == "--erase-private") {
            opts.erase_private = true;
        } else if (arg == "-gst" || arg == "--gen-stud-uid") {
            opts.gen_study_uid = true;
        } else if (arg == "-gse" || arg == "--gen-ser-uid") {
            opts.gen_series_uid = true;
        } else if (arg == "-gin" || arg == "--gen-inst-uid") {
            opts.gen_instance_uid = true;
        } else if (arg == "-nb" || arg == "--no-backup") {
            opts.create_backup = false;
        } else if (arg == "--script" && i + 1 < argc) {
            opts.script_file = argv[++i];
        } else if (arg == "-r" || arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "--dry-run") {
            opts.dry_run = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else {
            opts.input_paths.emplace_back(arg);
        }
    }

    // Parse script file if provided
    if (!opts.script_file.empty()) {
        if (!parse_script_file(opts.script_file, opts.modifications)) {
            return false;
        }
    }

    // Validation
    if (opts.input_paths.empty()) {
        std::cerr << "Error: No input files specified\n";
        return false;
    }

    // Must have at least one modification operation
    if (opts.modifications.empty() && !opts.erase_private &&
        !opts.gen_study_uid && !opts.gen_series_uid && !opts.gen_instance_uid) {
        std::cerr << "Error: No modification operation specified\n";
        return false;
    }

    // Determine if in-place mode
    if (opts.output_path.empty()) {
        opts.in_place = true;
    }

    return true;
}

/**
 * @brief Remove all private tags from dataset (recursively)
 * @param dataset The dataset to modify
 */
void remove_private_tags_recursive(pacs::core::dicom_dataset& dataset) {
    std::vector<pacs::core::dicom_tag> private_tags;

    for (const auto& [tag, element] : dataset) {
        if (tag.is_private()) {
            private_tags.push_back(tag);
        }
    }

    for (const auto& tag : private_tags) {
        dataset.remove(tag);
    }

    // Process sequences
    for (auto& [tag, element] : dataset) {
        if (element.is_sequence()) {
            for (auto& item : element.sequence_items()) {
                remove_private_tags_recursive(item);
            }
        }
    }
}

/**
 * @brief Remove tag from dataset and all sequences recursively
 * @param dataset The dataset to modify
 * @param tag Tag to remove
 * @return Number of tags removed
 */
size_t remove_tag_recursive(pacs::core::dicom_dataset& dataset,
                            const pacs::core::dicom_tag& tag) {
    size_t count = 0;

    if (dataset.contains(tag)) {
        dataset.remove(tag);
        ++count;
    }

    // Process sequences
    for (auto& [seq_tag, element] : dataset) {
        if (element.is_sequence()) {
            for (auto& item : element.sequence_items()) {
                count += remove_tag_recursive(item, tag);
            }
        }
    }

    return count;
}

/**
 * @brief Apply modifications to a dataset
 * @param dataset The dataset to modify
 * @param opts Command line options
 * @param uid_gen UID generator for UID regeneration
 * @return true if all modifications succeeded
 */
bool apply_modifications(pacs::core::dicom_dataset& dataset, const options& opts,
                         uid_generator& uid_gen) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    auto& dict = dicom_dictionary::instance();

    // Apply tag modifications
    for (const auto& mod : opts.modifications) {
        switch (mod.op) {
            case operation_type::insert: {
                auto info = dict.find(mod.tag);
                vr_type vr = info ? static_cast<vr_type>(info->vr) : vr_type::LO;

                if (opts.verbose) {
                    std::cout << "  Insert " << mod.tag.to_string() << " ("
                              << mod.keyword << ") = \"" << mod.value << "\"\n";
                }

                dataset.set_string(mod.tag, vr, mod.value);
                break;
            }

            case operation_type::modify: {
                if (!dataset.contains(mod.tag)) {
                    std::cerr << "  Error: Tag " << mod.tag.to_string() << " ("
                              << mod.keyword << ") does not exist (use -i to insert)\n";
                    return false;
                }

                auto info = dict.find(mod.tag);
                vr_type vr = info ? static_cast<vr_type>(info->vr) : vr_type::LO;

                if (opts.verbose) {
                    std::cout << "  Modify " << mod.tag.to_string() << " ("
                              << mod.keyword << ") = \"" << mod.value << "\"\n";
                }

                dataset.set_string(mod.tag, vr, mod.value);
                break;
            }

            case operation_type::erase: {
                if (opts.verbose) {
                    std::cout << "  Erase " << mod.tag.to_string() << " ("
                              << mod.keyword << ")\n";
                }

                dataset.remove(mod.tag);
                break;
            }

            case operation_type::erase_all: {
                size_t count = remove_tag_recursive(dataset, mod.tag);
                if (opts.verbose) {
                    std::cout << "  Erase all " << mod.tag.to_string() << " ("
                              << mod.keyword << ") - removed " << count
                              << " instance(s)\n";
                }
                break;
            }
        }
    }

    // Erase private tags
    if (opts.erase_private) {
        if (opts.verbose) {
            std::cout << "  Erasing all private tags...\n";
        }
        remove_private_tags_recursive(dataset);
    }

    // Generate new UIDs
    if (opts.gen_study_uid) {
        std::string new_uid = uid_gen.generate();
        if (opts.verbose) {
            std::cout << "  Generate new StudyInstanceUID: " << new_uid << "\n";
        }
        dataset.set_string(tags::study_instance_uid, vr_type::UI, new_uid);
    }

    if (opts.gen_series_uid) {
        std::string new_uid = uid_gen.generate();
        if (opts.verbose) {
            std::cout << "  Generate new SeriesInstanceUID: " << new_uid << "\n";
        }
        dataset.set_string(tags::series_instance_uid, vr_type::UI, new_uid);
    }

    if (opts.gen_instance_uid) {
        std::string new_uid = uid_gen.generate();
        if (opts.verbose) {
            std::cout << "  Generate new SOPInstanceUID: " << new_uid << "\n";
        }
        dataset.set_string(tags::sop_instance_uid, vr_type::UI, new_uid);
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
 * @brief Create backup file
 * @param file_path Original file path
 * @return true if backup was created successfully
 */
bool create_backup(const std::filesystem::path& file_path) {
    auto backup_path = file_path;
    backup_path += ".bak";

    std::error_code ec;
    std::filesystem::copy_file(file_path, backup_path,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        std::cerr << "Warning: Failed to create backup file: " << backup_path.string()
                  << " (" << ec.message() << ")\n";
        return false;
    }

    return true;
}

/**
 * @brief Process a single DICOM file
 * @param input_path Input file path
 * @param output_path Output file path
 * @param opts Command line options
 * @param uid_gen UID generator
 * @return true on success
 */
bool process_file(const std::filesystem::path& input_path,
                  const std::filesystem::path& output_path, const options& opts,
                  uid_generator& uid_gen) {
    using namespace pacs::core;

    if (opts.verbose) {
        std::cout << "Processing: " << input_path.string() << "\n";
    }

    // Dry run mode
    if (opts.dry_run) {
        std::cout << "Would modify: " << input_path.string() << "\n";
        for (const auto& mod : opts.modifications) {
            switch (mod.op) {
                case operation_type::insert:
                    std::cout << "  Insert " << mod.tag.to_string() << " = \""
                              << mod.value << "\"\n";
                    break;
                case operation_type::modify:
                    std::cout << "  Modify " << mod.tag.to_string() << " = \""
                              << mod.value << "\"\n";
                    break;
                case operation_type::erase:
                    std::cout << "  Erase " << mod.tag.to_string() << "\n";
                    break;
                case operation_type::erase_all:
                    std::cout << "  Erase all " << mod.tag.to_string() << "\n";
                    break;
            }
        }
        if (opts.erase_private) {
            std::cout << "  Erase all private tags\n";
        }
        if (opts.gen_study_uid) {
            std::cout << "  Generate new StudyInstanceUID\n";
        }
        if (opts.gen_series_uid) {
            std::cout << "  Generate new SeriesInstanceUID\n";
        }
        if (opts.gen_instance_uid) {
            std::cout << "  Generate new SOPInstanceUID\n";
        }
        std::cout << "  Output: " << output_path.string() << "\n";
        return true;
    }

    // Create backup for in-place modification
    if (opts.in_place && opts.create_backup) {
        if (!create_backup(input_path)) {
            // Continue anyway, just warn
        }
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

    // Apply modifications
    if (!apply_modifications(dataset, opts, uid_gen)) {
        return false;
    }

    // Create output file with same transfer syntax
    auto output_file = dicom_file::create(std::move(dataset), file.transfer_syntax());

    // Ensure output directory exists
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
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
    }

    return true;
}

/**
 * @brief Process input paths (files or directories)
 * @param opts Command line options
 * @param stats Processing statistics
 */
void process_inputs(const options& opts, process_stats& stats) {
    uid_generator uid_gen;

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
                if (opts.in_place) {
                    output_path = file_path;
                } else {
                    auto relative =
                        std::filesystem::relative(file_path, input_path);
                    output_path = opts.output_path / relative;
                }

                if (process_file(file_path, output_path, opts, uid_gen)) {
                    ++stats.successful;
                } else {
                    ++stats.failed;
                }
            };

            if (opts.recursive) {
                for (const auto& entry :
                     std::filesystem::recursive_directory_iterator(input_path)) {
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
            if (opts.in_place) {
                output_path = input_path;
            } else {
                output_path = opts.output_path;
            }

            if (process_file(input_path, output_path, opts, uid_gen)) {
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
    if (stats.total_files > 1) {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "         Processing Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Total files:   " << stats.total_files << "\n";
        std::cout << "  Successful:    " << stats.successful << "\n";
        std::cout << "  Failed:        " << stats.failed << "\n";
        std::cout << "========================================\n";
    }
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

    process_stats stats;
    process_inputs(opts, stats);

    print_summary(stats);

    if (stats.failed > 0) {
        return 2;
    }

    if (stats.total_files == 1 && stats.successful == 1) {
        std::cout << "Successfully modified file.\n";
    }

    return 0;
}
