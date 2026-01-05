/**
 * @file main.cpp
 * @brief Query SCU - DICOM C-FIND Client
 *
 * A command-line utility for searching DICOM studies on a remote SCP.
 * Supports all query levels (PATIENT, STUDY, SERIES, IMAGE) and
 * multiple output formats (table, JSON, CSV).
 *
 * @see Issue #102 - Query SCU Sample
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1.2 - C-FIND Service
 *
 * Usage:
 *   query_scu <host> <port> <called_ae> [options]
 *
 * Example:
 *   query_scu localhost 11112 PACS_SCP --level STUDY --patient-name "DOE^*"
 */

#include "query_builder.hpp"
#include "result_formatter.hpp"

#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/query_scp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// Default calling AE title
constexpr const char* default_calling_ae = "QUERY_SCU";

/// Default timeout (30 seconds)
constexpr auto default_timeout = std::chrono::milliseconds{30000};

/**
 * @brief Command-line options structure
 */
struct options {
    // Connection
    std::string host;
    uint16_t port{0};
    std::string called_ae;
    std::string calling_ae{default_calling_ae};

    // Query parameters
    pacs::services::query_level level{pacs::services::query_level::study};
    std::string query_model{"study"};  // "patient" or "study" root

    // Search criteria
    std::string patient_name;
    std::string patient_id;
    std::string patient_birth_date;
    std::string patient_sex;
    std::string study_date;
    std::string study_time;
    std::string accession_number;
    std::string study_uid;
    std::string study_id;
    std::string study_description;
    std::string modality;
    std::string series_uid;
    std::string sop_instance_uid;

    // Output options
    query_scu::output_format format{query_scu::output_format::table};
    bool verbose{false};
    size_t max_results{0};  // 0 = unlimited
};

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Query SCU - DICOM C-FIND Client

Usage: )" << program_name << R"( <host> <port> <called_ae> [options]

Arguments:
  host        Remote host address (IP or hostname)
  port        Remote port number (typically 104 or 11112)
  called_ae   Called AE Title (remote SCP's AE title)

Query Options:
  --level <level>       Query level: PATIENT, STUDY, SERIES, IMAGE (default: STUDY)
  --model <model>       Query model: patient, study (default: study)

Search Criteria:
  --patient-name <name>   Patient name (wildcards: * ?)
  --patient-id <id>       Patient ID
  --patient-birth-date <date>  Patient birth date (YYYYMMDD)
  --patient-sex <sex>     Patient sex (M, F, O)
  --study-date <date>     Study date (YYYYMMDD or range YYYYMMDD-YYYYMMDD)
  --study-time <time>     Study time (HHMMSS or range)
  --accession-number <num>  Accession number
  --study-uid <uid>       Study Instance UID
  --study-id <id>         Study ID
  --study-description <desc>  Study description
  --modality <mod>        Modality (CT, MR, US, XR, etc.)
  --series-uid <uid>      Series Instance UID
  --sop-instance-uid <uid>  SOP Instance UID

Output Options:
  --format <fmt>        Output format: table, json, csv (default: table)
  --max-results <n>     Maximum results to display (default: unlimited)
  --calling-ae <ae>     Calling AE Title (default: QUERY_SCU)
  --verbose, -v         Show detailed progress
  --help, -h            Show this help message

Examples:
  )" << program_name << R"( localhost 11112 PACS_SCP --level PATIENT --patient-name "Smith*"
  )" << program_name << R"( localhost 11112 PACS_SCP --level STUDY --patient-id "12345" --study-date "20240101-20241231"
  )" << program_name << R"( localhost 11112 PACS_SCP --level SERIES --study-uid "1.2.3.4.5" --format json
  )" << program_name << R"( localhost 11112 PACS_SCP --modality CT --format csv > results.csv

Exit Codes:
  0  Success - Query completed
  1  Error - Query failed or no results
  2  Error - Invalid arguments or connection failure
)";
}

/**
 * @brief Parse query level from string
 */
std::optional<pacs::services::query_level> parse_level(std::string_view level_str) {
    if (level_str == "PATIENT" || level_str == "patient") {
        return pacs::services::query_level::patient;
    }
    if (level_str == "STUDY" || level_str == "study") {
        return pacs::services::query_level::study;
    }
    if (level_str == "SERIES" || level_str == "series") {
        return pacs::services::query_level::series;
    }
    if (level_str == "IMAGE" || level_str == "image" ||
        level_str == "INSTANCE" || level_str == "instance") {
        return pacs::services::query_level::image;
    }
    return std::nullopt;
}

/**
 * @brief Parse command line arguments
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 4) {
        return false;
    }

    opts.host = argv[1];

    // Parse port
    try {
        int port_int = std::stoi(argv[2]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        opts.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[2] << "'\n";
        return false;
    }

    opts.called_ae = argv[3];
    if (opts.called_ae.length() > 16) {
        std::cerr << "Error: Called AE title exceeds 16 characters\n";
        return false;
    }

    // Parse optional arguments
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "--help" || arg == "-h")) {
            return false;
        }
        if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--level" && i + 1 < argc) {
            auto level = parse_level(argv[++i]);
            if (!level) {
                std::cerr << "Error: Invalid query level '" << argv[i] << "'\n";
                return false;
            }
            opts.level = *level;
        } else if (arg == "--model" && i + 1 < argc) {
            opts.query_model = argv[++i];
            if (opts.query_model != "patient" && opts.query_model != "study") {
                std::cerr << "Error: Invalid query model (use 'patient' or 'study')\n";
                return false;
            }
        } else if (arg == "--patient-name" && i + 1 < argc) {
            opts.patient_name = argv[++i];
        } else if (arg == "--patient-id" && i + 1 < argc) {
            opts.patient_id = argv[++i];
        } else if (arg == "--patient-birth-date" && i + 1 < argc) {
            opts.patient_birth_date = argv[++i];
        } else if (arg == "--patient-sex" && i + 1 < argc) {
            opts.patient_sex = argv[++i];
        } else if (arg == "--study-date" && i + 1 < argc) {
            opts.study_date = argv[++i];
        } else if (arg == "--study-time" && i + 1 < argc) {
            opts.study_time = argv[++i];
        } else if (arg == "--accession-number" && i + 1 < argc) {
            opts.accession_number = argv[++i];
        } else if (arg == "--study-uid" && i + 1 < argc) {
            opts.study_uid = argv[++i];
        } else if (arg == "--study-id" && i + 1 < argc) {
            opts.study_id = argv[++i];
        } else if (arg == "--study-description" && i + 1 < argc) {
            opts.study_description = argv[++i];
        } else if (arg == "--modality" && i + 1 < argc) {
            opts.modality = argv[++i];
        } else if (arg == "--series-uid" && i + 1 < argc) {
            opts.series_uid = argv[++i];
        } else if (arg == "--sop-instance-uid" && i + 1 < argc) {
            opts.sop_instance_uid = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            opts.format = query_scu::parse_output_format(argv[++i]);
        } else if (arg == "--max-results" && i + 1 < argc) {
            try {
                opts.max_results = static_cast<size_t>(std::stoul(argv[++i]));
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid max-results value\n";
                return false;
            }
        } else if (arg == "--calling-ae" && i + 1 < argc) {
            opts.calling_ae = argv[++i];
            if (opts.calling_ae.length() > 16) {
                std::cerr << "Error: Calling AE title exceeds 16 characters\n";
                return false;
            }
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

/**
 * @brief Get SOP Class UID for the query model
 */
std::string_view get_find_sop_class_uid(const std::string& model) {
    if (model == "patient") {
        return pacs::services::patient_root_find_sop_class_uid;
    }
    return pacs::services::study_root_find_sop_class_uid;
}

/**
 * @brief Perform C-FIND query
 */
int perform_query(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;
    using namespace pacs::services;

    auto sop_class_uid = get_find_sop_class_uid(opts.query_model);

    if (opts.verbose) {
        std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
        std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
        std::cout << "  Called AE:   " << opts.called_ae << "\n";
        std::cout << "  Query Model: " << opts.query_model << " root\n";
        std::cout << "  Query Level: " << to_string(opts.level) << "\n\n";
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "QUERY_SCU_001";

    // Propose Query/Retrieve SOP Class
    config.proposed_contexts.push_back({
        1,  // Context ID
        std::string(sop_class_uid),
        {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        }
    });

    // Establish association
    auto start_time = std::chrono::steady_clock::now();
    auto connect_result = association::connect(opts.host, opts.port, config, default_timeout);

    if (connect_result.is_err()) {
        std::cerr << "Failed to establish association: "
                  << connect_result.error().message << "\n";
        return 2;
    }

    auto& assoc = connect_result.value();
    auto connect_time = std::chrono::steady_clock::now();

    if (opts.verbose) {
        auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            connect_time - start_time);
        std::cout << "Association established in " << connect_duration.count() << " ms\n";
    }

    // Check if context was accepted
    if (!assoc.has_accepted_context(sop_class_uid)) {
        std::cerr << "Error: Query SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    auto context_id_opt = assoc.accepted_context_id(sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t context_id = *context_id_opt;

    // Build query dataset
    auto query_ds = query_scu::query_builder()
        .level(opts.level)
        .patient_name(opts.patient_name)
        .patient_id(opts.patient_id)
        .patient_birth_date(opts.patient_birth_date)
        .patient_sex(opts.patient_sex)
        .study_date(opts.study_date)
        .study_time(opts.study_time)
        .accession_number(opts.accession_number)
        .study_instance_uid(opts.study_uid)
        .study_id(opts.study_id)
        .study_description(opts.study_description)
        .modality(opts.modality)
        .series_instance_uid(opts.series_uid)
        .sop_instance_uid(opts.sop_instance_uid)
        .build();

    // Create C-FIND request
    auto find_rq = make_c_find_rq(1, sop_class_uid);
    find_rq.set_dataset(std::move(query_ds));

    if (opts.verbose) {
        std::cout << "Sending C-FIND request...\n";
    }

    // Send C-FIND request
    auto send_result = assoc.send_dimse(context_id, find_rq);
    if (send_result.is_err()) {
        std::cerr << "Failed to send C-FIND: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Receive responses
    std::vector<pacs::core::dicom_dataset> results;
    bool query_complete = false;
    size_t pending_count = 0;

    while (!query_complete) {
        auto recv_result = assoc.receive_dimse(default_timeout);
        if (recv_result.is_err()) {
            std::cerr << "Failed to receive C-FIND response: "
                      << recv_result.error().message << "\n";
            assoc.abort();
            return 2;
        }

        auto& [recv_context_id, find_rsp] = recv_result.value();

        if (find_rsp.command() != command_field::c_find_rsp) {
            std::cerr << "Error: Unexpected response (expected C-FIND-RSP)\n";
            assoc.abort();
            return 2;
        }

        auto status = find_rsp.status();

        // Check for pending status (more results coming)
        if (status == status_pending || status == status_pending_warning) {
            ++pending_count;

            if (find_rsp.has_dataset()) {
                // Check max results limit
                if (opts.max_results == 0 || results.size() < opts.max_results) {
                    auto dataset_result = find_rsp.dataset();
                    if (dataset_result.is_ok()) {
                        results.push_back(dataset_result.value().get());
                    }
                }
            }

            if (opts.verbose && pending_count % 10 == 0) {
                std::cout << "\rReceived " << pending_count << " results..." << std::flush;
            }
        } else if (status == status_success) {
            // Query complete
            query_complete = true;
            if (opts.verbose) {
                std::cout << "\rQuery completed successfully.\n";
            }
        } else if (status == status_cancel) {
            query_complete = true;
            std::cerr << "Query was cancelled.\n";
        } else {
            // Error status
            query_complete = true;
            std::cerr << "Query failed with status: 0x"
                      << std::hex << status << std::dec << "\n";
        }
    }

    // Release association gracefully
    if (opts.verbose) {
        std::cout << "Releasing association...\n";
    }

    auto release_result = assoc.release(default_timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Format and display results
    query_scu::result_formatter formatter(opts.format, opts.level);
    std::cout << formatter.format(results);

    // Print summary for table format
    if (opts.format == query_scu::output_format::table && opts.verbose) {
        std::cout << "\n========================================\n";
        std::cout << "              Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Query level:      " << to_string(opts.level) << "\n";
        std::cout << "  Total results:    " << results.size();
        if (opts.max_results > 0 && pending_count > opts.max_results) {
            std::cout << " (limited from " << pending_count << ")";
        }
        std::cout << "\n";
        std::cout << "  Query time:       " << total_duration.count() << " ms\n";
        std::cout << "========================================\n";
    }

    return results.empty() ? 1 : 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Only show banner for table format (to not corrupt json/csv output)
    bool show_banner = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--format" && i + 1 < argc) {
            std::string fmt = argv[i + 1];
            if (fmt == "json" || fmt == "csv") {
                show_banner = false;
            }
            break;
        }
    }

    if (show_banner) {
        std::cout << R"(
   ___  _   _ _____ ______   __  ____   ____ _   _
  / _ \| | | | ____|  _ \ \ / / / ___| / ___| | | |
 | | | | | | |  _| | |_) \ V /  \___ \| |   | | | |
 | |_| | |_| | |___|  _ < | |    ___) | |___| |_| |
  \__\_\\___/|_____|_| \_\|_|   |____/ \____|\___/

          DICOM C-FIND Client
)" << "\n";
    }

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 2;
    }

    return perform_query(opts);
}
