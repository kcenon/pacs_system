/**
 * @file main.cpp
 * @brief DICOM Directory (DICOMDIR) Creation and Management Utility
 * @example dcm_dir/main.cpp
 *
 * A command-line utility for creating, listing, verifying, and updating
 * DICOMDIR files as specified in DICOM PS3.3 and PS3.10.
 *
 * @see Issue #285 - dcm_dir: Implement DICOMDIR creation/management utility
 * @see DICOM PS3.3 - Basic Directory Information Object Definition
 * @see DICOM PS3.10 - Media Storage and File Format
 *
 * @code{.unparsed}
 * Usage:
 *   dcm_dir <command> [options] <arguments>
 *
 * Commands:
 *   create    Create new DICOMDIR from directory
 *   list      Display DICOMDIR contents
 *   verify    Validate DICOMDIR
 *   update    Update existing DICOMDIR (add/remove files)
 * @endcode
 */

#include "directory_index.h"
#include "kcenon/pacs/core/dicom_dictionary.h"
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"
#include "kcenon/pacs/encoding/explicit_vr_codec.h"
#include "kcenon/pacs/encoding/vr_type.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <tuple>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ============================================================================
// DICOMDIR Tags (Group 0x0004)
// ============================================================================

namespace dir_tags {
/// File-set ID
inline constexpr kcenon::pacs::core::dicom_tag file_set_id{0x0004, 0x1130};
/// File-set Descriptor File ID
inline constexpr kcenon::pacs::core::dicom_tag file_set_descriptor_file_id{0x0004, 0x1141};
/// Specific Character Set of File-set Descriptor File
inline constexpr kcenon::pacs::core::dicom_tag specific_character_set_of_file_set{0x0004, 0x1142};
/// Offset of the First Directory Record of the Root Directory Entity
inline constexpr kcenon::pacs::core::dicom_tag offset_of_first_directory_record{0x0004, 0x1200};
/// Offset of the Last Directory Record of the Root Directory Entity
inline constexpr kcenon::pacs::core::dicom_tag offset_of_last_directory_record{0x0004, 0x1202};
/// File-set Consistency Flag
inline constexpr kcenon::pacs::core::dicom_tag file_set_consistency_flag{0x0004, 0x1212};
/// Directory Record Sequence
inline constexpr kcenon::pacs::core::dicom_tag directory_record_sequence{0x0004, 0x1220};
/// Offset of the Next Directory Record
inline constexpr kcenon::pacs::core::dicom_tag offset_of_next_directory_record{0x0004, 0x1400};
/// Record In-use Flag
inline constexpr kcenon::pacs::core::dicom_tag record_in_use_flag{0x0004, 0x1410};
/// Offset of Referenced Lower-Level Directory Entity
inline constexpr kcenon::pacs::core::dicom_tag offset_of_lower_level_directory_entity{0x0004, 0x1420};
/// Directory Record Type
inline constexpr kcenon::pacs::core::dicom_tag directory_record_type{0x0004, 0x1430};
/// Private Record UID
inline constexpr kcenon::pacs::core::dicom_tag private_record_uid{0x0004, 0x1432};
/// Referenced File ID
inline constexpr kcenon::pacs::core::dicom_tag referenced_file_id{0x0004, 0x1500};
/// MRDR Directory Record Offset
inline constexpr kcenon::pacs::core::dicom_tag mrdr_directory_record_offset{0x0004, 0x1504};
/// Referenced SOP Class UID in File
inline constexpr kcenon::pacs::core::dicom_tag referenced_sop_class_uid_in_file{0x0004, 0x1510};
/// Referenced SOP Instance UID in File
inline constexpr kcenon::pacs::core::dicom_tag referenced_sop_instance_uid_in_file{0x0004, 0x1511};
/// Referenced Transfer Syntax UID in File
inline constexpr kcenon::pacs::core::dicom_tag referenced_transfer_syntax_uid_in_file{0x0004, 0x1512};
}  // namespace dir_tags

// ============================================================================
// Constants
// ============================================================================

/// Media Storage Directory Storage SOP Class UID
constexpr const char* kMediaStorageDirectorySopClassUid = "1.2.840.10008.1.3.10";

/// Implementation Class UID for DICOMDIR
constexpr const char* kImplementationClassUid = "1.2.826.0.1.3680043.8.1055.1";

/// Implementation Version Name
constexpr const char* kImplementationVersionName = "PACS_SYS_001";

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Command type
 */
enum class command_type { none, create, list, verify, update };

/**
 * @brief Directory record representing a node in DICOMDIR hierarchy
 */
struct directory_record {
    std::string type;                           // PATIENT, STUDY, SERIES, IMAGE
    std::map<std::string, std::string> attrs;   // Key attributes
    std::filesystem::path file_path;            // Referenced file path
    std::string sop_class_uid;                  // Referenced SOP Class UID
    std::string sop_instance_uid;               // Referenced SOP Instance UID
    std::string transfer_syntax_uid;            // Referenced Transfer Syntax UID
    std::vector<directory_record> children;     // Child records
};

/**
 * @brief Command line options
 */
struct options {
    command_type command{command_type::none};
    std::filesystem::path input_path;
    std::filesystem::path output_path{"DICOMDIR"};
    std::string file_set_id;
    bool recursive{true};
    bool verbose{false};
    bool tree_format{true};
    bool long_format{false};
    bool check_files{false};
    bool check_consistency{false};
    std::vector<std::filesystem::path> add_paths;
    std::vector<std::string> delete_paths;
};

/**
 * @brief Statistics for create/verify operations
 */
struct statistics {
    size_t total_files{0};
    size_t valid_files{0};
    size_t invalid_files{0};
    size_t patients{0};
    size_t studies{0};
    size_t series{0};
    size_t images{0};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name) {
    std::cout << "\nDICOM Directory (DICOMDIR) Utility\n\n";
    std::cout << "Usage: " << program_name << " <command> [options] <arguments>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  create    Create new DICOMDIR from directory\n";
    std::cout << "  list      Display DICOMDIR contents\n";
    std::cout << "  verify    Validate DICOMDIR\n";
    std::cout << "  update    Update existing DICOMDIR\n\n";

    std::cout << "Create Command:\n";
    std::cout << "  " << program_name << " create [options] <source_directory>\n";
    std::cout << "  Options:\n";
    std::cout << "    -o, --output <file>     Output file (default: DICOMDIR)\n";
    std::cout << "    --file-set-id <id>      File-set ID\n";
    std::cout << "    -r, --recursive         Recursively scan directory (default)\n";
    std::cout << "    --no-recursive          Do not scan subdirectories\n";
    std::cout << "    -v, --verbose           Verbose output\n\n";

    std::cout << "List Command:\n";
    std::cout << "  " << program_name << " list [options] <DICOMDIR>\n";
    std::cout << "  Options:\n";
    std::cout << "    -l, --long              Detailed output\n";
    std::cout << "    --tree                  Tree format output (default)\n";
    std::cout << "    --flat                  Flat list output\n\n";

    std::cout << "Verify Command:\n";
    std::cout << "  " << program_name << " verify [options] <DICOMDIR>\n";
    std::cout << "  Options:\n";
    std::cout << "    --check-files           Verify all referenced files exist\n";
    std::cout << "    --check-consistency     Check DICOMDIR consistency\n\n";

    std::cout << "Update Command:\n";
    std::cout << "  " << program_name << " update [options] <DICOMDIR>\n";
    std::cout << "  Options:\n";
    std::cout << "    -a, --add <file/dir>    Add file or directory\n";
    std::cout << "    -d, --delete <path>     Delete entry by Referenced File ID\n\n";

    std::cout << "General Options:\n";
    std::cout << "  -h, --help                Show this help message\n\n";

    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " create -o DICOMDIR ./patient_data/\n";
    std::cout << "  " << program_name << " list --tree DICOMDIR\n";
    std::cout << "  " << program_name << " verify --check-files DICOMDIR\n";
    std::cout << "  " << program_name << " update -a ./new_study/ DICOMDIR\n\n";

    std::cout << "Exit Codes:\n";
    std::cout << "  0  Success\n";
    std::cout << "  1  Invalid arguments\n";
    std::cout << "  2  Processing error\n";
}

/**
 * @brief Parse command line arguments
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 2) {
        return false;
    }

    // Parse command
    std::string cmd = argv[1];
    if (cmd == "create") {
        opts.command = command_type::create;
    } else if (cmd == "list") {
        opts.command = command_type::list;
    } else if (cmd == "verify") {
        opts.command = command_type::verify;
    } else if (cmd == "update") {
        opts.command = command_type::update;
    } else if (cmd == "-h" || cmd == "--help") {
        return false;
    } else {
        std::cerr << "Error: Unknown command '" << cmd << "'\n";
        return false;
    }

    // Parse options
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            return false;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            opts.output_path = argv[++i];
        } else if (arg == "--file-set-id" && i + 1 < argc) {
            opts.file_set_id = argv[++i];
        } else if (arg == "-r" || arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "--no-recursive") {
            opts.recursive = false;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-l" || arg == "--long") {
            opts.long_format = true;
        } else if (arg == "--tree") {
            opts.tree_format = true;
        } else if (arg == "--flat") {
            opts.tree_format = false;
        } else if (arg == "--check-files") {
            opts.check_files = true;
        } else if (arg == "--check-consistency") {
            opts.check_consistency = true;
        } else if ((arg == "-a" || arg == "--add") && i + 1 < argc) {
            opts.add_paths.emplace_back(argv[++i]);
        } else if ((arg == "-d" || arg == "--delete") && i + 1 < argc) {
            opts.delete_paths.emplace_back(argv[++i]);
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else {
            if (opts.input_path.empty()) {
                opts.input_path = arg;
            } else {
                std::cerr << "Error: Multiple input paths specified\n";
                return false;
            }
        }
    }

    // Validate
    if (opts.input_path.empty()) {
        std::cerr << "Error: No input path specified\n";
        return false;
    }

    return true;
}

/**
 * @brief Generate UID for DICOMDIR
 */
std::string generate_uid() {
    static uint64_t counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
    return std::string("1.2.826.0.1.3680043.8.1055.3.") + std::to_string(timestamp) +
           "." + std::to_string(++counter);
}

/**
 * @brief Convert path to Referenced File ID components, preserving the actual filename
 */
std::string path_to_file_id(const std::filesystem::path& path,
                            const std::filesystem::path& base) {
    auto relative = std::filesystem::relative(path, base);
    std::string result;
    for (const auto& part : relative) {
        if (!result.empty()) {
            result += "\\";
        }
        std::string name = part.string();
        result += name;
    }
    return result;
}

// ============================================================================
// DICOM File Processing
// ============================================================================

/**
 * @brief Hierarchical structure for building DICOMDIR
 */
struct patient_info {
    std::string patient_id;
    std::string patient_name;
    std::map<std::string, struct study_info> studies;
};

struct study_info {
    std::string study_instance_uid;
    std::string study_date;
    std::string study_id;
    std::string study_time;
    std::string study_description;
    std::string accession_number;
    std::map<std::string, struct series_info> series;
};

struct series_info {
    std::string series_instance_uid;
    std::string modality;
    std::string series_number;
    std::string series_description;
    std::vector<struct instance_info> instances;
};

struct instance_info {
    std::string sop_instance_uid;
    std::string sop_class_uid;
    std::string transfer_syntax_uid;
    std::string instance_number;
    std::filesystem::path file_path;
};

/**
 * @brief Scan directory and build hierarchical structure
 */
bool scan_directory(const std::filesystem::path& dir_path,
                    const std::filesystem::path& base_path,
                    std::map<std::string, patient_info>& patients,
                    const options& opts, statistics& stats) {
    using namespace kcenon::pacs::core;

    auto process_file = [&](const std::filesystem::path& file_path) {
        ++stats.total_files;

        // Check extension
        auto ext = file_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (!ext.empty() && ext != ".dcm" && ext != ".dicom") {
            return;
        }

        // Open DICOM file
        auto result = dicom_file::open(file_path);
        if (result.is_err()) {
            ++stats.invalid_files;
            if (opts.verbose) {
                std::cerr << "  Skip: " << file_path.filename().string()
                          << " (" << result.error().message << ")\n";
            }
            return;
        }

        auto& file = result.value();
        if (file.meta_information().get_string(tags::media_storage_sop_class_uid) ==
            kMediaStorageDirectorySopClassUid) return;
        ++stats.valid_files;
        auto& ds = file.dataset();

        // Extract patient info
        std::string patient_id = ds.get_string(tags::patient_id);
        if (patient_id.empty()) {
            patient_id = "UNKNOWN";
        }
        std::string patient_name = ds.get_string(tags::patient_name);

        // Extract study info
        std::string study_uid = ds.get_string(tags::study_instance_uid);
        if (study_uid.empty()) {
            study_uid = generate_uid();
        }

        // Extract series info
        std::string series_uid = ds.get_string(tags::series_instance_uid);
        if (series_uid.empty()) {
            series_uid = generate_uid();
        }

        // Build hierarchy
        auto& patient = patients[patient_id];
        patient.patient_id = patient_id;
        if (patient.patient_name.empty()) {
            patient.patient_name = patient_name;
        }

        auto& study = patient.studies[study_uid];
        study.study_instance_uid = study_uid;
        if (study.study_date.empty()) {
            study.study_date = ds.get_string(tags::study_date);
            study.study_id = ds.get_string(tags::study_id);
            study.study_time = ds.get_string(tags::study_time);
            study.study_description = ds.get_string(tags::study_description);
            study.accession_number = ds.get_string(tags::accession_number);
        }

        auto& series = study.series[series_uid];
        series.series_instance_uid = series_uid;
        if (series.modality.empty()) {
            series.modality = ds.get_string(tags::modality);
            series.series_number = ds.get_string(tags::series_number);
            series.series_description = ds.get_string(tags::series_description);
        }

        // Add instance
        instance_info instance;
        instance.sop_instance_uid = file.sop_instance_uid();
        instance.sop_class_uid = file.sop_class_uid();
        instance.transfer_syntax_uid = file.transfer_syntax().uid();
        instance.instance_number = ds.get_string(tags::instance_number);
        instance.file_path = file_path;
        series.instances.push_back(std::move(instance));

        if (opts.verbose) {
            std::cout << "  Add: " << file_path.filename().string() << "\n";
        }
    };

    // Iterate directory
    try {
        std::vector<std::filesystem::path> paths;
        if (opts.recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path))
                if (entry.is_regular_file()) paths.push_back(entry.path());
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(dir_path))
                if (entry.is_regular_file()) paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end());
        for (const auto& path : paths) process_file(path);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return false;
    }

    // Count statistics
    stats.patients = patients.size();
    for (const auto& [pid, patient] : patients) {
        stats.studies += patient.studies.size();
        for (const auto& [suid, study] : patient.studies) {
            stats.series += study.series.size();
            for (const auto& [seuid, series] : study.series) {
                stats.images += series.instances.size();
            }
        }
    }

    return true;
}

// ============================================================================
// DICOMDIR Creation
// ============================================================================

/**
 * @brief Create DICOMDIR dataset from hierarchical structure
 */
kcenon::pacs::Result<kcenon::pacs::core::dicom_file> create_dicomdir(
    const std::map<std::string, patient_info>& patients,
    const std::filesystem::path& base_path,
    const options& opts, const std::string& file_set_uid = {}) {
    using namespace kcenon::pacs::core;
    using namespace kcenon::pacs::encoding;

    dicom_dataset ds;

    // Set basic DICOMDIR attributes
    ds.set_string(dir_tags::file_set_id, vr_type::CS,
                  opts.file_set_id.empty() && file_set_uid.empty() ? "PACS_SYSTEM" : opts.file_set_id);
    ds.set_numeric<uint16_t>(dir_tags::file_set_consistency_flag, vr_type::US, 0);

    ds.set_numeric<uint32_t>(dir_tags::offset_of_first_directory_record, vr_type::UL, 0);
    ds.set_numeric<uint32_t>(dir_tags::offset_of_last_directory_record, vr_type::UL, 0);
    // Fixed-size placeholders keep both serialization passes identical in size.
    std::vector<dicom_dataset> records;
    constexpr size_t none = std::numeric_limits<size_t>::max();
    std::vector<size_t> next, lower, roots;
    auto append = [&](dicom_dataset record, std::vector<size_t>& siblings) {
        const auto i = records.size();
        record.set_numeric<uint32_t>(dir_tags::offset_of_next_directory_record, vr_type::UL, 0);
        record.set_numeric<uint32_t>(dir_tags::offset_of_lower_level_directory_entity, vr_type::UL, 0);
        records.push_back(std::move(record));
        next.push_back(none);
        lower.push_back(none);
        if (!siblings.empty()) next[siblings.back()] = i;
        siblings.push_back(i);
        return i;
    };

    for (const auto& [pid, patient] : patients) {
        // Create PATIENT record
        dicom_dataset patient_rec;
        patient_rec.set_string(dir_tags::directory_record_type, vr_type::CS, "PATIENT");
        patient_rec.set_numeric<uint16_t>(dir_tags::record_in_use_flag, vr_type::US, 0xFFFF);
        patient_rec.set_string(tags::patient_id, vr_type::LO, patient.patient_id);
        patient_rec.set_string(tags::patient_name, vr_type::PN, patient.patient_name);

        const auto patient_index = append(std::move(patient_rec), roots);
        std::vector<size_t> studies;
        for (const auto& [suid, study] : patient.studies) {
            // Create STUDY record
            dicom_dataset study_rec;
            study_rec.set_string(dir_tags::directory_record_type, vr_type::CS, "STUDY");
            study_rec.set_numeric<uint16_t>(dir_tags::record_in_use_flag, vr_type::US, 0xFFFF);
            study_rec.set_string(tags::study_instance_uid, vr_type::UI, study.study_instance_uid);
            study_rec.set_string(tags::study_date, vr_type::DA, study.study_date);
            study_rec.set_string(tags::study_time, vr_type::TM, study.study_time);
            study_rec.set_string(tags::study_description, vr_type::LO, study.study_description);
            study_rec.set_string(tags::accession_number, vr_type::SH, study.accession_number);
            study_rec.set_string(tags::study_id, vr_type::SH, study.study_id);

            const auto study_index = append(std::move(study_rec), studies);
            std::vector<size_t> series_indices;
            for (const auto& [seuid, series] : study.series) {
                // Create SERIES record
                dicom_dataset series_rec;
                series_rec.set_string(dir_tags::directory_record_type, vr_type::CS, "SERIES");
                series_rec.set_numeric<uint16_t>(dir_tags::record_in_use_flag, vr_type::US, 0xFFFF);
                series_rec.set_string(tags::series_instance_uid, vr_type::UI, series.series_instance_uid);
                series_rec.set_string(tags::modality, vr_type::CS, series.modality);
                series_rec.set_string(tags::series_number, vr_type::IS, series.series_number);

                const auto series_index = append(std::move(series_rec), series_indices);
                std::vector<size_t> images;
                auto instances = series.instances;
                std::sort(instances.begin(), instances.end(), [](const auto& a, const auto& b) {
                    return std::tie(a.sop_instance_uid, a.file_path) < std::tie(b.sop_instance_uid, b.file_path);
                });
                for (const auto& instance : instances) {
                    // Create IMAGE record
                    dicom_dataset image_rec;
                    image_rec.set_string(dir_tags::directory_record_type, vr_type::CS, "IMAGE");
                    image_rec.set_numeric<uint16_t>(dir_tags::record_in_use_flag, vr_type::US, 0xFFFF);

                    // Referenced File ID
                    std::string file_id = path_to_file_id(instance.file_path, base_path);
                    image_rec.set_string(dir_tags::referenced_file_id, vr_type::CS, file_id);

                    // Referenced SOP info
                    image_rec.set_string(dir_tags::referenced_sop_class_uid_in_file,
                                        vr_type::UI, instance.sop_class_uid);
                    image_rec.set_string(dir_tags::referenced_sop_instance_uid_in_file,
                                        vr_type::UI, instance.sop_instance_uid);
                    image_rec.set_string(dir_tags::referenced_transfer_syntax_uid_in_file,
                                        vr_type::UI, instance.transfer_syntax_uid);

                    // Instance Number
                    image_rec.set_string(tags::instance_number, vr_type::IS, instance.instance_number);

                    append(std::move(image_rec), images);
                }
                if (!images.empty()) lower[series_index] = images.front();
            }
            if (!series_indices.empty()) lower[study_index] = series_indices.front();
        }
        if (!studies.empty()) lower[patient_index] = studies.front();
    }

    // Create sequence element and set items
    dicom_element seq_elem(dir_tags::directory_record_sequence, vr_type::SQ);
    auto& items = seq_elem.sequence_items();
    items = std::move(records);
    ds.insert(std::move(seq_elem));

    auto file = dicom_file::create(std::move(ds), transfer_syntax::explicit_vr_little_endian);
    // Basic Directory IOD carries these identifiers only in File Meta Information.
    auto& meta = file.meta_information();
    meta.set_string(tags::media_storage_sop_class_uid, vr_type::UI, kMediaStorageDirectorySopClassUid);
    meta.set_string(tags::media_storage_sop_instance_uid, vr_type::UI,
        file_set_uid.empty() ? generate_uid() : file_set_uid);
    meta.set_numeric<uint32_t>(tags::file_meta_information_group_length, vr_type::UL,
        static_cast<uint32_t>(explicit_vr_codec::encode(meta).size()));
    auto first_pass = file.to_bytes();
    auto offsets = pacs::tools::directory::item_offsets(first_pass);
    if (offsets.is_err()) return offsets.error();
    if (offsets.value().size() != next.size()) {
        return kcenon::pacs::pacs_error<dicom_file>(
            kcenon::pacs::error_codes::encode_error, "DICOMDIR item count changed during serialization");
    }
    auto& output = file.dataset();
    if (!roots.empty()) {
        output.set_numeric<uint32_t>(dir_tags::offset_of_first_directory_record, vr_type::UL, offsets.value()[roots.front()]);
        output.set_numeric<uint32_t>(dir_tags::offset_of_last_directory_record, vr_type::UL, offsets.value()[roots.back()]);
    }
    auto& encoded_records = output.get(dir_tags::directory_record_sequence)->sequence_items();
    for (size_t i = 0; i < encoded_records.size(); ++i) {
        encoded_records[i].set_numeric<uint32_t>(dir_tags::offset_of_next_directory_record,
            vr_type::UL, next[i] == none ? 0 : offsets.value()[next[i]]);
        encoded_records[i].set_numeric<uint32_t>(dir_tags::offset_of_lower_level_directory_entity,
            vr_type::UL, lower[i] == none ? 0 : offsets.value()[lower[i]]);
    }
    auto second_pass = file.to_bytes();
    auto checked = pacs::tools::directory::inspect(second_pass, file);
    if (checked.is_err()) return checked.error();
    if (first_pass.size() != second_pass.size() || offsets.value() != checked.value().offsets) {
        return kcenon::pacs::pacs_error<dicom_file>(
            kcenon::pacs::error_codes::encode_error, "DICOMDIR offset layout changed during serialization");
    }
    return file;
}

/**
 * @brief Execute create command
 */
int execute_create(const options& opts) {
    using namespace kcenon::pacs::core;
    using namespace kcenon::pacs::encoding;

    std::cout << "Creating DICOMDIR from: " << opts.input_path.string() << "\n";

    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: Source directory does not exist\n";
        return 2;
    }

    if (!std::filesystem::is_directory(opts.input_path)) {
        std::cerr << "Error: Source path is not a directory\n";
        return 2;
    }

    // Scan directory
    std::map<std::string, patient_info> patients;
    statistics stats;

    std::cout << "Scanning directory...\n";
    if (!scan_directory(opts.input_path, opts.input_path, patients, opts, stats)) {
        return 2;
    }

    if (stats.valid_files == 0 && stats.invalid_files != 0) {
        std::cerr << "Error: No valid DICOM files found; input contained unreadable files\n";
        return 2;
    }

    std::filesystem::path output_path = opts.output_path;
    if (output_path.is_relative()) output_path = opts.input_path / output_path;
    std::cout << "Building DICOMDIR structure...\n";
    auto created = create_dicomdir(patients, output_path.parent_path(), opts);
    if (created.is_err()) {
        std::cerr << "Error: " << created.error().message << "\n";
        return 2;
    }
    auto& file = created.value();

    // Save
    std::cout << "Saving to: " << output_path.string() << "\n";
    auto result = file.save(output_path);
    if (result.is_err()) {
        std::cerr << "Error: Failed to save DICOMDIR: " << result.error().message << "\n";
        return 2;
    }

    // Print summary
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "         DICOMDIR Created\n";
    std::cout << "========================================\n";
    std::cout << "  Total files scanned:  " << stats.total_files << "\n";
    std::cout << "  Valid DICOM files:    " << stats.valid_files << "\n";
    std::cout << "  Invalid/Skipped:      " << stats.invalid_files << "\n";
    std::cout << "  --------------------------------\n";
    std::cout << "  Patients:             " << stats.patients << "\n";
    std::cout << "  Studies:              " << stats.studies << "\n";
    std::cout << "  Series:               " << stats.series << "\n";
    std::cout << "  Images:               " << stats.images << "\n";
    std::cout << "========================================\n";

    return 0;
}

// ============================================================================
// DICOMDIR Listing
// ============================================================================

/**
 * @brief Parse DICOMDIR and build record tree
 */
bool parse_dicomdir(const std::filesystem::path& dicomdir_path,
                    std::vector<directory_record>& root_records,
                    statistics& stats) {
    using namespace kcenon::pacs::core;

    std::ifstream input(dicomdir_path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    if (!input || size < 0 || static_cast<uint64_t>(size) > std::numeric_limits<uint32_t>::max()) {
        std::cerr << "Error: Cannot read DICOMDIR within the 32-bit offset range\n";
        return false;
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        std::cerr << "Error: Failed to read DICOMDIR\n";
        return false;
    }
    auto offsets = pacs::tools::directory::item_offsets(bytes);
    if (offsets.is_err()) {
        std::cerr << "Error: " << offsets.error().message << "\n";
        return false;
    }
    auto result = dicom_file::from_bytes(bytes);
    if (result.is_err()) {
        std::cerr << "Error: Failed to open DICOMDIR: " << result.error().message << "\n";
        return false;
    }
    auto checked = pacs::tools::directory::inspect(bytes, result.value());
    if (checked.is_err()) {
        std::cerr << "Error: " << checked.error().message << "\n";
        return false;
    }
    const auto& items = result.value().dataset().get(dir_tags::directory_record_sequence)->sequence_items();
    std::function<directory_record(size_t)> build;
    build = [&](size_t index) {
        const auto& item = items[index];
        directory_record rec;
        rec.type = item.get_string(dir_tags::directory_record_type);

        // Extract type-specific attributes
        if (rec.type == "PATIENT") {
            rec.attrs["PatientID"] = item.get_string(tags::patient_id);
            rec.attrs["PatientName"] = item.get_string(tags::patient_name);
            ++stats.patients;
        } else if (rec.type == "STUDY") {
            rec.attrs["StudyInstanceUID"] = item.get_string(tags::study_instance_uid);
            rec.attrs["StudyDate"] = item.get_string(tags::study_date);
            rec.attrs["StudyTime"] = item.get_string(tags::study_time);
            rec.attrs["StudyID"] = item.get_string(tags::study_id);
            rec.attrs["StudyDescription"] = item.get_string(tags::study_description);
            rec.attrs["AccessionNumber"] = item.get_string(tags::accession_number);
            ++stats.studies;
        } else if (rec.type == "SERIES") {
            rec.attrs["SeriesInstanceUID"] = item.get_string(tags::series_instance_uid);
            rec.attrs["Modality"] = item.get_string(tags::modality);
            rec.attrs["SeriesNumber"] = item.get_string(tags::series_number);
            rec.attrs["SeriesDescription"] = item.get_string(tags::series_description);
            ++stats.series;
        } else if (rec.type == "IMAGE") {
            rec.attrs["InstanceNumber"] = item.get_string(tags::instance_number);
            rec.file_path = item.get_string(dir_tags::referenced_file_id);
            rec.sop_class_uid = item.get_string(dir_tags::referenced_sop_class_uid_in_file);
            rec.sop_instance_uid = item.get_string(dir_tags::referenced_sop_instance_uid_in_file);
            rec.transfer_syntax_uid = item.get_string(dir_tags::referenced_transfer_syntax_uid_in_file);
            ++stats.images;
        }

        for (auto child : checked.value().children[index]) rec.children.push_back(build(child));
        return rec;
    };
    for (auto root : checked.value().roots) root_records.push_back(build(root));

    return true;
}

/**
 * @brief Print record tree recursively
 */
void print_record_tree(const directory_record& rec, int depth, const options& opts) {
    std::string indent(depth * 2, ' ');
    std::string prefix = depth == 0 ? "" : "├── ";

    if (rec.type == "PATIENT") {
        std::cout << indent << prefix << "[PATIENT] "
                  << rec.attrs.at("PatientName") << " (" << rec.attrs.at("PatientID") << ")\n";
    } else if (rec.type == "STUDY") {
        std::cout << indent << prefix << "[STUDY] "
                  << rec.attrs.at("StudyDate") << " "
                  << rec.attrs.at("StudyDescription") << "\n";
        if (opts.long_format) {
            std::cout << indent << "    UID: " << rec.attrs.at("StudyInstanceUID") << "\n";
            std::cout << indent << "    Accession: " << rec.attrs.at("AccessionNumber") << "\n";
        }
    } else if (rec.type == "SERIES") {
        std::cout << indent << prefix << "[SERIES] "
                  << rec.attrs.at("Modality") << " #" << rec.attrs.at("SeriesNumber") << "\n";
        if (opts.long_format) {
            std::cout << indent << "    UID: " << rec.attrs.at("SeriesInstanceUID") << "\n";
        }
    } else if (rec.type == "IMAGE") {
        std::cout << indent << prefix << "[IMAGE] #" << rec.attrs.at("InstanceNumber");
        if (!rec.file_path.empty()) {
            std::cout << " -> " << rec.file_path.string();
        }
        std::cout << "\n";
        if (opts.long_format) {
            std::cout << indent << "    SOP: " << rec.sop_class_uid << "\n";
        }
    } else {
        std::cout << indent << prefix << "[" << rec.type << "]\n";
    }

    // Print children
    for (const auto& child : rec.children) {
        print_record_tree(child, depth + 1, opts);
    }
}

/**
 * @brief Execute list command
 */
int execute_list(const options& opts) {
    std::cout << "DICOMDIR: " << opts.input_path.string() << "\n\n";

    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: DICOMDIR file does not exist\n";
        return 2;
    }

    std::vector<directory_record> root_records;
    statistics stats;

    if (!parse_dicomdir(opts.input_path, root_records, stats)) {
        return 2;
    }

    // Print tree
    if (opts.tree_format) {
        for (const auto& rec : root_records) {
            print_record_tree(rec, 0, opts);
        }
    } else {
        // Flat format
        std::function<void(const directory_record&)> print_flat;
        print_flat = [&](const directory_record& rec) {
            if (rec.type == "IMAGE" && !rec.file_path.empty()) {
                std::cout << rec.file_path.string() << "\n";
            }
            for (const auto& child : rec.children) {
                print_flat(child);
            }
        };
        for (const auto& rec : root_records) {
            print_flat(rec);
        }
    }

    // Print summary
    std::cout << "\n";
    std::cout << "----------------------------------------\n";
    std::cout << "  Patients: " << stats.patients << "\n";
    std::cout << "  Studies:  " << stats.studies << "\n";
    std::cout << "  Series:   " << stats.series << "\n";
    std::cout << "  Images:   " << stats.images << "\n";
    std::cout << "----------------------------------------\n";

    return 0;
}

// ============================================================================
// DICOMDIR Verification
// ============================================================================

/**
 * @brief Verify referenced files exist
 */
void verify_files(const std::vector<directory_record>& records,
                  const std::filesystem::path& base_path,
                  statistics& stats) {
    std::function<void(const directory_record&)> check;
    check = [&](const directory_record& rec) {
        if (rec.type == "IMAGE" && !rec.file_path.empty()) {
            // Convert Referenced File ID to filesystem path
            std::string file_id = rec.file_path.string();
            std::replace(file_id.begin(), file_id.end(), '\\', '/');
            std::filesystem::path full_path = base_path / file_id;

            if (!std::filesystem::exists(full_path)) {
                stats.errors.push_back("Missing file: " + full_path.string());
            } else {
                ++stats.valid_files;
            }
            ++stats.total_files;
        }

        for (const auto& child : rec.children) {
            check(child);
        }
    };

    for (const auto& rec : records) {
        check(rec);
    }
}

/**
 * @brief Execute verify command
 */
int execute_verify(const options& opts) {
    std::cout << "Verifying DICOMDIR: " << opts.input_path.string() << "\n\n";

    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: DICOMDIR file does not exist\n";
        return 2;
    }

    std::vector<directory_record> root_records;
    statistics stats;

    // Parse DICOMDIR
    std::cout << "Parsing DICOMDIR...\n";
    if (!parse_dicomdir(opts.input_path, root_records, stats)) {
        return 2;
    }
    std::cout << "  Found " << stats.images << " image records\n";

    // Check files if requested
    if (opts.check_files) {
        std::cout << "\nVerifying referenced files...\n";
        std::filesystem::path base_path = opts.input_path.parent_path();
        verify_files(root_records, base_path, stats);

        std::cout << "  Files found:   " << stats.valid_files << "/" << stats.total_files << "\n";
        stats.invalid_files = stats.total_files - stats.valid_files;
    }

    // Check consistency if requested
    if (opts.check_consistency) {
        std::cout << "\nChecking consistency...\n";

        // Check for duplicate SOP Instance UIDs
        std::set<std::string> sop_uids;
        std::function<void(const directory_record&)> check_duplicates;
        check_duplicates = [&](const directory_record& rec) {
            if (rec.type == "IMAGE" && !rec.sop_instance_uid.empty()) {
                if (sop_uids.count(rec.sop_instance_uid) > 0) {
                    stats.warnings.push_back("Duplicate SOP Instance UID: " + rec.sop_instance_uid);
                } else {
                    sop_uids.insert(rec.sop_instance_uid);
                }
            }
            for (const auto& child : rec.children) {
                check_duplicates(child);
            }
        };

        for (const auto& rec : root_records) {
            check_duplicates(rec);
        }

        std::cout << "  Unique SOP Instance UIDs: " << sop_uids.size() << "\n";
    }

    // Print results
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "         Verification Results\n";
    std::cout << "========================================\n";
    std::cout << "  Patients: " << stats.patients << "\n";
    std::cout << "  Studies:  " << stats.studies << "\n";
    std::cout << "  Series:   " << stats.series << "\n";
    std::cout << "  Images:   " << stats.images << "\n";

    if (opts.check_files) {
        std::cout << "  --------------------------------\n";
        std::cout << "  Files verified: " << stats.valid_files << "/" << stats.total_files << "\n";
        if (stats.invalid_files > 0) {
            std::cout << "  Missing files:  " << stats.invalid_files << "\n";
        }
    }

    // Print errors
    if (!stats.errors.empty()) {
        std::cout << "  --------------------------------\n";
        std::cout << "  Errors: " << stats.errors.size() << "\n";
        for (const auto& err : stats.errors) {
            std::cout << "    - " << err << "\n";
        }
    }

    // Print warnings
    if (!stats.warnings.empty()) {
        std::cout << "  --------------------------------\n";
        std::cout << "  Warnings: " << stats.warnings.size() << "\n";
        for (const auto& warn : stats.warnings) {
            std::cout << "    - " << warn << "\n";
        }
    }

    std::cout << "========================================\n";

    bool success = stats.errors.empty() &&
                   (stats.invalid_files == 0 || !opts.check_files);
    std::cout << "\nResult: " << (success ? "PASSED" : "FAILED") << "\n";

    return success ? 0 : 2;
}

// ============================================================================
// DICOMDIR Update
// ============================================================================

/**
 * @brief Execute update command
 */
int execute_update(const options& opts) {
    std::cout << "Updating DICOMDIR: " << opts.input_path.string() << "\n\n";

    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: DICOMDIR file does not exist\n";
        return 2;
    }

    if (opts.add_paths.empty() && opts.delete_paths.empty()) {
        std::cerr << "Error: No add or delete operations specified\n";
        return 1;
    }

    // Parse existing DICOMDIR
    std::vector<directory_record> root_records;
    statistics stats;

    if (!parse_dicomdir(opts.input_path, root_records, stats)) {
        return 2;
    }

    std::filesystem::path base_path = opts.input_path.parent_path();

    // Handle add operations
    if (!opts.add_paths.empty()) {
        std::map<std::string, patient_info> patients;

        // First, rebuild patient map from existing records
        std::function<void(const directory_record&, patient_info*, study_info*, series_info*)> rebuild;
        rebuild = [&](const directory_record& rec, patient_info* patient,
                      study_info* study, series_info* series) {
            if (rec.type == "PATIENT") {
                auto& p = patients[rec.attrs.at("PatientID")];
                p.patient_id = rec.attrs.at("PatientID");
                p.patient_name = rec.attrs.at("PatientName");
                for (const auto& child : rec.children) {
                    rebuild(child, &p, nullptr, nullptr);
                }
            } else if (rec.type == "STUDY" && patient != nullptr) {
                auto& s = patient->studies[rec.attrs.at("StudyInstanceUID")];
                s.study_instance_uid = rec.attrs.at("StudyInstanceUID");
                s.study_date = rec.attrs.at("StudyDate");
                s.study_time = rec.attrs.at("StudyTime");
                s.study_id = rec.attrs.at("StudyID");
                s.study_description = rec.attrs.at("StudyDescription");
                s.accession_number = rec.attrs.at("AccessionNumber");
                for (const auto& child : rec.children) {
                    rebuild(child, patient, &s, nullptr);
                }
            } else if (rec.type == "SERIES" && study != nullptr) {
                auto& se = study->series[rec.attrs.at("SeriesInstanceUID")];
                se.series_instance_uid = rec.attrs.at("SeriesInstanceUID");
                se.modality = rec.attrs.at("Modality");
                se.series_number = rec.attrs.at("SeriesNumber");
                se.series_description = rec.attrs.at("SeriesDescription");
                for (const auto& child : rec.children) {
                    rebuild(child, patient, study, &se);
                }
            } else if (rec.type == "IMAGE" && series != nullptr) {
                instance_info inst;
                inst.sop_instance_uid = rec.sop_instance_uid;
                inst.sop_class_uid = rec.sop_class_uid;
                inst.transfer_syntax_uid = rec.transfer_syntax_uid;
                inst.instance_number = rec.attrs.at("InstanceNumber");

                // Convert file ID to path
                std::string file_id = rec.file_path.string();
                std::replace(file_id.begin(), file_id.end(), '\\', '/');
                inst.file_path = base_path / file_id;

                series->instances.push_back(std::move(inst));
            }
        };

        for (const auto& rec : root_records) {
            rebuild(rec, nullptr, nullptr, nullptr);
        }

        // Add new files
        for (const auto& add_path : opts.add_paths) {
            std::cout << "Adding: " << add_path.string() << "\n";
            options scan_opts = opts;
            scan_opts.verbose = true;

            if (std::filesystem::is_directory(add_path)) {
                scan_directory(add_path, base_path, patients, scan_opts, stats);
            } else if (std::filesystem::is_regular_file(add_path)) {
                // Handle single file - create a temporary directory iterator
                std::map<std::string, patient_info> temp_patients;
                options single_opts = scan_opts;
                single_opts.recursive = false;
                scan_directory(add_path.parent_path(), base_path, temp_patients, single_opts, stats);
                // Merge into main patients
                for (auto& [pid, patient] : temp_patients) {
                    auto& p = patients[pid];
                    if (p.patient_id.empty()) {
                        p = std::move(patient);
                    } else {
                        for (auto& [suid, study] : patient.studies) {
                            if (p.studies.count(suid) == 0) {
                                p.studies[suid] = std::move(study);
                            } else {
                                for (auto& [seuid, series] : study.series) {
                                    if (p.studies[suid].series.count(seuid) == 0) {
                                        p.studies[suid].series[seuid] = std::move(series);
                                    } else {
                                        for (auto& inst : series.instances) {
                                            p.studies[suid].series[seuid].instances.push_back(std::move(inst));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Recreate DICOMDIR
        std::cout << "\nRebuilding DICOMDIR...\n";
        auto previous = kcenon::pacs::core::dicom_file::open(opts.input_path);
        if (previous.is_err()) {
            std::cerr << "Error: " << previous.error().message << "\n";
            return 2;
        }
        auto update_opts = opts;
        if (update_opts.file_set_id.empty())
            update_opts.file_set_id = previous.value().dataset().get_string(dir_tags::file_set_id);
        auto created = create_dicomdir(patients, base_path, update_opts,
            previous.value().meta_information().get_string(
                kcenon::pacs::core::tags::media_storage_sop_instance_uid));
        if (created.is_err()) {
            std::cerr << "Error: " << created.error().message << "\n";
            return 2;
        }
        auto result = created.value().save(opts.input_path);
        if (result.is_err()) {
            std::cerr << "Error: Failed to save updated DICOMDIR: "
                      << result.error().message << "\n";
            return 2;
        }
    }

    // Handle delete operations
    if (!opts.delete_paths.empty()) {
        std::cout << "Delete operation not yet implemented\n";
        return 1;
    }

    std::cout << "\nDICOMDIR updated successfully.\n";
    return 0;
}

}  // namespace

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____   ____ __  __   ____ ___ ____
 |  _ \ / ___|  \/  | |  _ \_ _|  _ \
 | | | | |   | |\/| | | | | | || |_) |
 | |_| | |___| |  | | | |_| | ||  _ <
 |____/ \____|_|  |_| |____/___|_| \_\

    DICOMDIR Creation/Management Utility
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 1;
    }

    switch (opts.command) {
        case command_type::create:
            return execute_create(opts);
        case command_type::list:
            return execute_list(opts);
        case command_type::verify:
            return execute_verify(opts);
        case command_type::update:
            return execute_update(opts);
        default:
            print_usage(argv[0]);
            return 1;
    }
}
