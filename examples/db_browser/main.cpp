/**
 * @file main.cpp
 * @brief Database Browser - PACS Index Viewer
 *
 * A command-line utility for inspecting PACS index database, viewing
 * indexed patients, studies, series, instances, and performing maintenance.
 *
 * @see Issue #109 - Database Browser Sample
 *
 * Usage:
 *   db_browser <database> <command> [options]
 *
 * Example:
 *   db_browser pacs.db patients
 *   db_browser pacs.db studies --patient-id "12345"
 *   db_browser pacs.db stats
 *   db_browser pacs.db vacuum
 */

#include "pacs/storage/index_database.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace pacs::storage;

namespace {

/**
 * @brief Command types supported by the browser
 */
enum class command_type {
    patients,
    studies,
    series,
    instances,
    stats,
    vacuum,
    verify,
    help
};

/**
 * @brief Command line options
 */
struct options {
    std::string db_path;
    command_type command{command_type::help};

    // Filter options
    std::string patient_id;
    std::string patient_name;
    std::string study_uid;
    std::string series_uid;
    std::string modality;
    std::string date_from;
    std::string date_to;

    // Pagination
    size_t limit{50};
    size_t offset{0};

    // Output options
    bool verbose{false};
};

/**
 * @brief Print program usage
 * @param program_name Name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Database Browser - PACS Index Viewer

Usage: )" << program_name
              << R"( <database> <command> [options]

Commands:
  patients       List all patients
  studies        List studies (optionally filtered by patient)
  series         List series (optionally filtered by study)
  instances      List instances (optionally filtered by series)
  stats          Show database statistics
  vacuum         Reclaim unused space in the database
  verify         Verify file existence for all instances

Filter Options:
  --patient-id <id>       Filter by patient ID
  --patient-name <name>   Filter by patient name (supports * wildcard)
  --study-uid <uid>       Filter by Study Instance UID
  --series-uid <uid>      Filter by Series Instance UID
  --modality <mod>        Filter by modality (e.g., CT, MR, XR)
  --from <YYYYMMDD>       Filter by date range start
  --to <YYYYMMDD>         Filter by date range end

Pagination Options:
  --limit <n>             Maximum results to show (default: 50)
  --offset <n>            Skip first n results (default: 0)

General Options:
  --verbose, -v           Show additional details
  --help, -h              Show this help message

Examples:
  )" << program_name
              << R"( pacs.db patients
  )" << program_name
              << R"( pacs.db studies --patient-id "12345"
  )" << program_name
              << R"( pacs.db studies --from 20240101 --to 20241231
  )" << program_name
              << R"( pacs.db series --study-uid "1.2.3.4.5"
  )" << program_name
              << R"( pacs.db instances --series-uid "1.2.3.4.5.6"
  )" << program_name
              << R"( pacs.db stats
  )" << program_name
              << R"( pacs.db vacuum
  )" << program_name
              << R"( pacs.db verify

Exit Codes:
  0  Success
  1  Invalid arguments or command
  2  Database error
)";
}

/**
 * @brief Parse command string to enum
 * @param cmd Command string
 * @return Corresponding command_type
 */
command_type parse_command(const std::string& cmd) {
    if (cmd == "patients") {
        return command_type::patients;
    }
    if (cmd == "studies") {
        return command_type::studies;
    }
    if (cmd == "series") {
        return command_type::series;
    }
    if (cmd == "instances") {
        return command_type::instances;
    }
    if (cmd == "stats") {
        return command_type::stats;
    }
    if (cmd == "vacuum") {
        return command_type::vacuum;
    }
    if (cmd == "verify") {
        return command_type::verify;
    }
    return command_type::help;
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param opts Output options structure
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 3) {
        return false;
    }

    opts.db_path = argv[1];
    opts.command = parse_command(argv[2]);

    if (opts.command == command_type::help) {
        // Check if it's actually --help flag
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h") {
            return false;
        }
        std::cerr << "Error: Unknown command '" << argv[2] << "'\n";
        return false;
    }

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--patient-id" && i + 1 < argc) {
            opts.patient_id = argv[++i];
        } else if (arg == "--patient-name" && i + 1 < argc) {
            opts.patient_name = argv[++i];
        } else if (arg == "--study-uid" && i + 1 < argc) {
            opts.study_uid = argv[++i];
        } else if (arg == "--series-uid" && i + 1 < argc) {
            opts.series_uid = argv[++i];
        } else if (arg == "--modality" && i + 1 < argc) {
            opts.modality = argv[++i];
        } else if (arg == "--from" && i + 1 < argc) {
            opts.date_from = argv[++i];
        } else if (arg == "--to" && i + 1 < argc) {
            opts.date_to = argv[++i];
        } else if (arg == "--limit" && i + 1 < argc) {
            opts.limit = std::stoull(argv[++i]);
        } else if (arg == "--offset" && i + 1 < argc) {
            opts.offset = std::stoull(argv[++i]);
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

/**
 * @brief Format date string for display (YYYYMMDD -> YYYY-MM-DD)
 * @param date Date string in YYYYMMDD format
 * @return Formatted date string
 */
std::string format_date(const std::string& date) {
    if (date.length() == 8) {
        return date.substr(0, 4) + "-" + date.substr(4, 2) + "-" +
               date.substr(6, 2);
    }
    return date.empty() ? "-" : date;
}

/**
 * @brief Format file size for display
 * @param bytes Size in bytes
 * @return Human-readable size string
 */
std::string format_size(int64_t bytes) {
    constexpr int64_t KB = 1024;
    constexpr int64_t MB = KB * 1024;
    constexpr int64_t GB = MB * 1024;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);

    if (bytes >= GB) {
        oss << static_cast<double>(bytes) / GB << " GB";
    } else if (bytes >= MB) {
        oss << static_cast<double>(bytes) / MB << " MB";
    } else if (bytes >= KB) {
        oss << static_cast<double>(bytes) / KB << " KB";
    } else {
        oss << bytes << " B";
    }

    return oss.str();
}

/**
 * @brief Truncate string to fit column width
 * @param str Input string
 * @param max_len Maximum length
 * @return Truncated string
 */
std::string truncate(const std::string& str, size_t max_len) {
    if (str.length() <= max_len) {
        return str;
    }
    if (max_len <= 3) {
        return str.substr(0, max_len);
    }
    return str.substr(0, max_len - 3) + "...";
}

/**
 * @brief Print horizontal separator line
 * @param widths Column widths
 */
void print_separator(const std::vector<size_t>& widths) {
    for (size_t i = 0; i < widths.size(); ++i) {
        if (i > 0) {
            std::cout << "+";
        }
        std::cout << std::string(widths[i] + 2, '-');
    }
    std::cout << "\n";
}

/**
 * @brief Print table row
 * @param values Column values
 * @param widths Column widths
 */
void print_row(const std::vector<std::string>& values,
               const std::vector<size_t>& widths) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            std::cout << "|";
        }
        std::cout << " " << std::left << std::setw(static_cast<int>(widths[i]))
                  << truncate(values[i], widths[i]) << " ";
    }
    std::cout << "\n";
}

/**
 * @brief List patients in the database
 * @param db Database handle
 * @param opts Command options
 * @return Exit code
 */
int list_patients(index_database& db, const options& opts) {
    patient_query query;
    if (!opts.patient_id.empty()) {
        query.patient_id = opts.patient_id;
    }
    if (!opts.patient_name.empty()) {
        query.patient_name = opts.patient_name;
    }
    query.limit = opts.limit;
    query.offset = opts.offset;

    auto patients = db.search_patients(query);
    auto total = db.patient_count();

    std::cout << "\n=== Patients (" << patients.size();
    if (opts.limit > 0 && patients.size() == opts.limit) {
        std::cout << " of " << total;
    }
    std::cout << " total) ===\n\n";

    if (patients.empty()) {
        std::cout << "No patients found.\n";
        return 0;
    }

    // Table layout
    std::vector<std::string> headers = {"ID", "Name", "Birth Date", "Sex",
                                        "Studies"};
    std::vector<size_t> widths = {12, 24, 12, 4, 8};

    print_row(headers, widths);
    print_separator(widths);

    for (const auto& patient : patients) {
        auto study_count = db.study_count(patient.patient_id);
        std::vector<std::string> row = {patient.patient_id, patient.patient_name,
                                        format_date(patient.birth_date),
                                        patient.sex.empty() ? "-" : patient.sex,
                                        std::to_string(study_count)};
        print_row(row, widths);
    }

    if (opts.verbose && !patients.empty()) {
        std::cout << "\nShowing " << patients.size() << " of " << total
                  << " patients";
        if (opts.offset > 0) {
            std::cout << " (offset: " << opts.offset << ")";
        }
        std::cout << "\n";
    }

    return 0;
}

/**
 * @brief List studies in the database
 * @param db Database handle
 * @param opts Command options
 * @return Exit code
 */
int list_studies(index_database& db, const options& opts) {
    study_query query;
    if (!opts.patient_id.empty()) {
        query.patient_id = opts.patient_id;
    }
    if (!opts.patient_name.empty()) {
        query.patient_name = opts.patient_name;
    }
    if (!opts.study_uid.empty()) {
        query.study_uid = opts.study_uid;
    }
    if (!opts.modality.empty()) {
        query.modality = opts.modality;
    }
    if (!opts.date_from.empty()) {
        query.study_date_from = opts.date_from;
    }
    if (!opts.date_to.empty()) {
        query.study_date_to = opts.date_to;
    }
    query.limit = opts.limit;
    query.offset = opts.offset;

    auto studies = db.search_studies(query);
    auto total = db.study_count();

    std::cout << "\n=== Studies (" << studies.size();
    if (opts.limit > 0 && studies.size() == opts.limit) {
        std::cout << " of " << total;
    }
    std::cout << " total) ===\n\n";

    if (studies.empty()) {
        std::cout << "No studies found.\n";
        return 0;
    }

    // Table layout
    std::vector<std::string> headers = {"Study UID", "Date", "Description",
                                        "Modalities", "Series"};
    std::vector<size_t> widths = {28, 12, 24, 12, 7};

    print_row(headers, widths);
    print_separator(widths);

    for (const auto& study : studies) {
        std::vector<std::string> row = {
            study.study_uid, format_date(study.study_date),
            study.study_description.empty() ? "-" : study.study_description,
            study.modalities_in_study.empty() ? "-" : study.modalities_in_study,
            std::to_string(study.num_series)};
        print_row(row, widths);
    }

    if (opts.verbose) {
        std::cout << "\nShowing " << studies.size() << " of " << total
                  << " studies\n";
    }

    return 0;
}

/**
 * @brief List series in the database
 * @param db Database handle
 * @param opts Command options
 * @return Exit code
 */
int list_series(index_database& db, const options& opts) {
    series_query query;
    if (!opts.study_uid.empty()) {
        query.study_uid = opts.study_uid;
    }
    if (!opts.series_uid.empty()) {
        query.series_uid = opts.series_uid;
    }
    if (!opts.modality.empty()) {
        query.modality = opts.modality;
    }
    query.limit = opts.limit;
    query.offset = opts.offset;

    auto series_list = db.search_series(query);
    auto total = db.series_count();

    std::cout << "\n=== Series (" << series_list.size();
    if (opts.limit > 0 && series_list.size() == opts.limit) {
        std::cout << " of " << total;
    }
    std::cout << " total) ===\n\n";

    if (series_list.empty()) {
        std::cout << "No series found.\n";
        return 0;
    }

    // Table layout
    std::vector<std::string> headers = {"Series UID", "Modality", "Number",
                                        "Description", "Instances"};
    std::vector<size_t> widths = {28, 10, 7, 24, 10};

    print_row(headers, widths);
    print_separator(widths);

    for (const auto& s : series_list) {
        std::string series_num =
            s.series_number.has_value() ? std::to_string(*s.series_number) : "-";
        std::vector<std::string> row = {
            s.series_uid, s.modality.empty() ? "-" : s.modality, series_num,
            s.series_description.empty() ? "-" : s.series_description,
            std::to_string(s.num_instances)};
        print_row(row, widths);
    }

    if (opts.verbose) {
        std::cout << "\nShowing " << series_list.size() << " of " << total
                  << " series\n";
    }

    return 0;
}

/**
 * @brief List instances in the database
 * @param db Database handle
 * @param opts Command options
 * @return Exit code
 */
int list_instances(index_database& db, const options& opts) {
    instance_query query;
    if (!opts.series_uid.empty()) {
        query.series_uid = opts.series_uid;
    }
    query.limit = opts.limit;
    query.offset = opts.offset;

    auto instances = db.search_instances(query);
    auto total = db.instance_count();

    std::cout << "\n=== Instances (" << instances.size();
    if (opts.limit > 0 && instances.size() == opts.limit) {
        std::cout << " of " << total;
    }
    std::cout << " total) ===\n\n";

    if (instances.empty()) {
        std::cout << "No instances found.\n";
        return 0;
    }

    // Table layout
    std::vector<std::string> headers = {"SOP Instance UID", "Number", "Size",
                                        "File Path"};
    std::vector<size_t> widths = {32, 7, 10, 40};

    print_row(headers, widths);
    print_separator(widths);

    for (const auto& inst : instances) {
        std::string inst_num =
            inst.instance_number.has_value()
                ? std::to_string(*inst.instance_number)
                : "-";
        std::vector<std::string> row = {inst.sop_uid, inst_num,
                                        format_size(inst.file_size),
                                        inst.file_path};
        print_row(row, widths);
    }

    if (opts.verbose) {
        std::cout << "\nShowing " << instances.size() << " of " << total
                  << " instances\n";
    }

    return 0;
}

/**
 * @brief Show database statistics
 * @param db Database handle
 * @param opts Command options (unused, reserved for future use)
 * @return Exit code
 */
int show_stats(index_database& db, [[maybe_unused]] const options& opts) {
    auto stats = db.get_storage_stats();

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "      Database Statistics\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    std::cout << "  Database Path:     " << db.path() << "\n";
    std::cout << "  Schema Version:    " << db.schema_version() << "\n";
    std::cout << "  Database Size:     " << format_size(stats.database_size)
              << "\n";
    std::cout << "\n";
    std::cout << "  --- Record Counts ---\n";
    std::cout << "  Patients:          " << stats.total_patients << "\n";
    std::cout << "  Studies:           " << stats.total_studies << "\n";
    std::cout << "  Series:            " << stats.total_series << "\n";
    std::cout << "  Instances:         " << stats.total_instances << "\n";
    std::cout << "\n";
    std::cout << "  --- Storage Usage ---\n";
    std::cout << "  Total File Size:   " << format_size(stats.total_file_size)
              << "\n";

    if (stats.total_instances > 0) {
        auto avg_size = stats.total_file_size / stats.total_instances;
        std::cout << "  Average File Size: " << format_size(avg_size) << "\n";
    }

    std::cout << "========================================\n";

    return 0;
}

/**
 * @brief Perform database vacuum operation
 * @param db Database handle
 * @param opts Command options (unused, reserved for future use)
 * @return Exit code
 */
int do_vacuum(index_database& db, [[maybe_unused]] const options& opts) {
    std::cout << "Performing VACUUM operation...\n";

    auto stats_before = db.get_storage_stats();
    auto result = db.vacuum();

    if (result.is_err()) {
        std::cerr << "Error: VACUUM failed: " << result.error().message
                  << "\n";
        return 2;
    }

    auto stats_after = db.get_storage_stats();

    std::cout << "VACUUM completed successfully.\n";
    std::cout << "  Before: " << format_size(stats_before.database_size) << "\n";
    std::cout << "  After:  " << format_size(stats_after.database_size) << "\n";

    auto saved = stats_before.database_size - stats_after.database_size;
    if (saved > 0) {
        std::cout << "  Saved:  " << format_size(saved) << "\n";
    }

    return 0;
}

/**
 * @brief Verify file existence for all instances
 * @param db Database handle
 * @param opts Command options
 * @return Exit code
 */
int do_verify(index_database& db, const options& opts) {
    std::cout << "Verifying file existence...\n\n";

    instance_query query;
    query.limit = 0;  // No limit for verification
    auto instances = db.search_instances(query);

    size_t total = instances.size();
    size_t existing = 0;
    size_t missing = 0;
    std::vector<std::string> missing_files;

    for (const auto& inst : instances) {
        if (fs::exists(inst.file_path)) {
            ++existing;
        } else {
            ++missing;
            if (opts.verbose || missing_files.size() < 10) {
                missing_files.push_back(inst.file_path);
            }
        }
    }

    std::cout << "========================================\n";
    std::cout << "      File Verification Results\n";
    std::cout << "========================================\n";
    std::cout << "  Total Instances:   " << total << "\n";
    std::cout << "  Files Found:       " << existing << "\n";
    std::cout << "  Files Missing:     " << missing << "\n";
    std::cout << "========================================\n";

    if (missing > 0) {
        std::cout << "\nMissing Files";
        if (!opts.verbose && missing > 10) {
            std::cout << " (showing first 10)";
        }
        std::cout << ":\n";
        for (const auto& path : missing_files) {
            std::cout << "  - " << path << "\n";
        }
        if (!opts.verbose && missing > missing_files.size()) {
            std::cout << "  ... and " << (missing - missing_files.size())
                      << " more\n";
        }
        std::cout << "\nUse --verbose to see all missing files.\n";
        return 1;
    }

    std::cout << "\nAll files verified successfully.\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____  ____    ____
 |  _ \| __ )  | __ ) _ __ _____      _____  ___ _ __
 | | | |  _ \  |  _ \| '__/ _ \ \ /\ / / __|/ _ \ '__|
 | |_| | |_) | | |_) | | | (_) \ V  V /\__ \  __/ |
 |____/|____/  |____/|_|  \___/ \_/\_/ |___/\___|_|

        PACS Index Database Browser
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 1;
    }

    // Check database file exists
    if (!fs::exists(opts.db_path)) {
        std::cerr << "Error: Database file not found: " << opts.db_path << "\n";
        return 2;
    }

    // Open database
    auto db_result = index_database::open(opts.db_path);
    if (db_result.is_err()) {
        std::cerr << "Error: Failed to open database: "
                  << db_result.error().message << "\n";
        return 2;
    }

    auto& db = *db_result.value();

    // Execute command
    switch (opts.command) {
        case command_type::patients:
            return list_patients(db, opts);
        case command_type::studies:
            return list_studies(db, opts);
        case command_type::series:
            return list_series(db, opts);
        case command_type::instances:
            return list_instances(db, opts);
        case command_type::stats:
            return show_stats(db, opts);
        case command_type::vacuum:
            return do_vacuum(db, opts);
        case command_type::verify:
            return do_verify(db, opts);
        case command_type::help:
            print_usage(argv[0]);
            return 0;
    }

    return 0;
}
