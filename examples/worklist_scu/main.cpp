/**
 * @file main.cpp
 * @brief Worklist SCU - Modality Worklist Query Client
 *
 * A command-line utility for querying scheduled procedures from a
 * Modality Worklist SCP. Supports filtering by modality, date, station,
 * and multiple output formats (table, JSON, CSV, XML).
 *
 * @see Issue #104 - Worklist SCU Sample
 * @see Issue #292 - Enhanced Worklist SCU Implementation
 * @see DICOM PS3.4 Section K - Basic Worklist Management Service Class
 * @see DICOM PS3.7 Section 9.1.2 - C-FIND Service
 *
 * Usage:
 *   worklist_scu [options] <host> <port>
 *
 * Example:
 *   worklist_scu --modality CT --date 20241215 localhost 11112
 */

#include "worklist_query_builder.hpp"
#include "worklist_result_formatter.hpp"

#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/worklist_scp.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

/// Default calling AE title
constexpr const char* default_calling_ae = "WORKLIST_SCU";

/// Default timeout (30 seconds)
constexpr auto default_timeout = std::chrono::milliseconds{30000};

/**
 * @brief Query key for custom DICOM tag queries
 */
struct query_key {
    uint16_t group;
    uint16_t element;
    std::string value;
};

/**
 * @brief Command-line options structure
 */
struct options {
    // Connection
    std::string host;
    uint16_t port{0};
    std::string called_ae{"ANY-SCP"};
    std::string calling_ae{default_calling_ae};
    std::chrono::milliseconds timeout{default_timeout};

    // Scheduled Procedure Step criteria
    std::string modality;
    std::string scheduled_date;
    std::string scheduled_time;
    std::string station_ae;
    std::string station_name;
    std::string physician;

    // Patient criteria
    std::string patient_name;
    std::string patient_id;

    // Study/Request criteria
    std::string accession_number;

    // Custom query keys (-k option)
    std::vector<query_key> custom_keys;

    // Output options
    worklist_scu::output_format format{worklist_scu::output_format::table};
    std::string output_file;
    bool verbose{false};
    bool debug{false};
    size_t max_results{0};  // 0 = unlimited
};

/**
 * @brief Get today's date in DICOM format (YYYYMMDD)
 */
std::string get_today_date() {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y%m%d");
    return oss.str();
}

/**
 * @brief Parse query key from string (format: "GGGG,EEEE=value")
 */
std::optional<query_key> parse_query_key(const std::string& key_str) {
    // Find '=' separator
    auto eq_pos = key_str.find('=');
    if (eq_pos == std::string::npos) {
        return std::nullopt;
    }

    std::string tag_part = key_str.substr(0, eq_pos);
    std::string value_part = key_str.substr(eq_pos + 1);

    // Find ',' separator in tag
    auto comma_pos = tag_part.find(',');
    if (comma_pos == std::string::npos) {
        return std::nullopt;
    }

    try {
        uint16_t group = static_cast<uint16_t>(
            std::stoul(tag_part.substr(0, comma_pos), nullptr, 16));
        uint16_t element = static_cast<uint16_t>(
            std::stoul(tag_part.substr(comma_pos + 1), nullptr, 16));
        return query_key{group, element, value_part};
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Worklist SCU - Modality Worklist Query Client

Usage: )" << program_name << R"( [options] <peer> <port>

Arguments:
  peer        Remote host address (IP or hostname)
  port        Remote port number (typically 104 or 11112)

Network Options:
  -aet, --aetitle <ae>     Calling AE Title (default: WLSCU)
  -aec, --call <ae>        Called AE Title (default: ANY-SCP)
  -to, --timeout <sec>     Connection timeout in seconds (default: 30)

Query Keys:
  -k, --key <tag=value>    Query key (e.g., "0008,0060=CT")
                           Can be specified multiple times

Common Filters (convenience options):
  --modality <mod>         Filter by modality (CT, MR, US, XR, NM, etc.)
  --date <date>            Scheduled date (YYYYMMDD or range YYYYMMDD-YYYYMMDD)
                           Use "today" for current date
  --time <time>            Scheduled time (HHMMSS or range)
  --station <name>         Scheduled Station AE Title
  --station-name <name>    Scheduled Station Name
  --physician <name>       Scheduled Performing Physician Name
  --patient-name <name>    Patient name (wildcards: * ?)
  --patient-id <id>        Patient ID
  --accession <num>        Accession number

Output Options:
  -o, --output <format>    Output format: text, json, csv, xml (default: text)
  --output-file <file>     Write results to file
  --max-results <n>        Maximum results to display (default: unlimited)

General Options:
  -v, --verbose            Verbose output mode
  -d, --debug              Debug output mode
  -h, --help               Show this help message

Examples:
  )" << program_name << R"( 192.168.1.100 11112 --modality CT
  )" << program_name << R"( 192.168.1.100 11112 --modality MR --date today
  )" << program_name << R"( -aec RIS_SCP --date 20241215 --station CT_SCANNER_01 192.168.1.100 11112
  )" << program_name << R"( -k "0008,0060=CT" -k "0040,0002=20241215" 192.168.1.100 11112
  )" << program_name << R"( --modality CT -o json --output-file worklist.json 192.168.1.100 11112

Exit Codes:
  0  Success - Query completed with results
  1  Success - Query completed with no results
  2  Error - Invalid arguments or connection failure
)";
}

/**
 * @brief Parse command line arguments
 *
 * New CLI format: worklist_scu [options] <peer> <port>
 * Options can appear anywhere before the positional arguments.
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 3) {
        return false;
    }

    // Collect positional arguments (host and port at the end)
    std::vector<std::string> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Help option
        if (arg == "--help" || arg == "-h") {
            return false;
        }
        // Verbose/Debug options
        if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
            continue;
        }
        if (arg == "--debug" || arg == "-d") {
            opts.debug = true;
            opts.verbose = true;
            continue;
        }

        // Network options
        if ((arg == "-aet" || arg == "--aetitle") && i + 1 < argc) {
            opts.calling_ae = argv[++i];
            if (opts.calling_ae.length() > 16) {
                std::cerr << "Error: Calling AE title exceeds 16 characters\n";
                return false;
            }
            continue;
        }
        if ((arg == "-aec" || arg == "--call") && i + 1 < argc) {
            opts.called_ae = argv[++i];
            if (opts.called_ae.length() > 16) {
                std::cerr << "Error: Called AE title exceeds 16 characters\n";
                return false;
            }
            continue;
        }
        if ((arg == "-to" || arg == "--timeout") && i + 1 < argc) {
            try {
                int timeout_sec = std::stoi(argv[++i]);
                if (timeout_sec < 1) {
                    std::cerr << "Error: Timeout must be positive\n";
                    return false;
                }
                opts.timeout = std::chrono::milliseconds{timeout_sec * 1000};
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid timeout value\n";
                return false;
            }
            continue;
        }

        // Query key option
        if ((arg == "-k" || arg == "--key") && i + 1 < argc) {
            auto key = parse_query_key(argv[++i]);
            if (!key) {
                std::cerr << "Error: Invalid query key format. Use 'GGGG,EEEE=value'\n";
                return false;
            }
            opts.custom_keys.push_back(*key);
            continue;
        }

        // Common filter options
        if (arg == "--modality" && i + 1 < argc) {
            opts.modality = argv[++i];
            continue;
        }
        if (arg == "--date" && i + 1 < argc) {
            std::string date_arg = argv[++i];
            if (date_arg == "today") {
                opts.scheduled_date = get_today_date();
            } else {
                opts.scheduled_date = date_arg;
            }
            continue;
        }
        if (arg == "--time" && i + 1 < argc) {
            opts.scheduled_time = argv[++i];
            continue;
        }
        if (arg == "--station" && i + 1 < argc) {
            opts.station_ae = argv[++i];
            continue;
        }
        if (arg == "--station-name" && i + 1 < argc) {
            opts.station_name = argv[++i];
            continue;
        }
        if (arg == "--physician" && i + 1 < argc) {
            opts.physician = argv[++i];
            continue;
        }
        if (arg == "--patient-name" && i + 1 < argc) {
            opts.patient_name = argv[++i];
            continue;
        }
        if (arg == "--patient-id" && i + 1 < argc) {
            opts.patient_id = argv[++i];
            continue;
        }
        if (arg == "--accession" && i + 1 < argc) {
            opts.accession_number = argv[++i];
            continue;
        }

        // Output options
        if ((arg == "-o" || arg == "--output" || arg == "--format") && i + 1 < argc) {
            opts.format = worklist_scu::parse_output_format(argv[++i]);
            continue;
        }
        if (arg == "--output-file" && i + 1 < argc) {
            opts.output_file = argv[++i];
            continue;
        }
        if (arg == "--max-results" && i + 1 < argc) {
            try {
                opts.max_results = static_cast<size_t>(std::stoul(argv[++i]));
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid max-results value\n";
                return false;
            }
            continue;
        }

        // Legacy option for backward compatibility
        if (arg == "--calling-ae" && i + 1 < argc) {
            opts.calling_ae = argv[++i];
            if (opts.calling_ae.length() > 16) {
                std::cerr << "Error: Calling AE title exceeds 16 characters\n";
                return false;
            }
            continue;
        }

        // Check if it looks like an unknown option
        if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }

        // Positional argument
        positional_args.push_back(arg);
    }

    // Need exactly 2 positional arguments: host and port
    if (positional_args.size() < 2) {
        std::cerr << "Error: Missing required arguments <peer> <port>\n";
        return false;
    }
    if (positional_args.size() > 2) {
        std::cerr << "Error: Too many positional arguments\n";
        return false;
    }

    opts.host = positional_args[0];

    // Parse port
    try {
        int port_int = std::stoi(positional_args[1]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        opts.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << positional_args[1] << "'\n";
        return false;
    }

    return true;
}

/**
 * @brief Perform MWL C-FIND query
 */
int perform_query(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;
    using namespace pacs::services;

    if (opts.verbose) {
        std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
        std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
        std::cout << "  Called AE:   " << opts.called_ae << "\n";
        std::cout << "  Query Type:  Modality Worklist\n";
        if (!opts.modality.empty()) {
            std::cout << "  Modality:    " << opts.modality << "\n";
        }
        if (!opts.scheduled_date.empty()) {
            std::cout << "  Sched Date:  " << opts.scheduled_date << "\n";
        }
        if (!opts.station_ae.empty()) {
            std::cout << "  Station AE:  " << opts.station_ae << "\n";
        }
        std::cout << "\n";
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "WORKLIST_SCU_001";

    // Propose Modality Worklist SOP Class
    config.proposed_contexts.push_back({
        1,  // Context ID
        std::string(worklist_find_sop_class_uid),
        {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        }
    });

    // Establish association
    auto start_time = std::chrono::steady_clock::now();
    auto connect_result = association::connect(opts.host, opts.port, config, opts.timeout);

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
    if (!assoc.has_accepted_context(worklist_find_sop_class_uid)) {
        std::cerr << "Error: Modality Worklist SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    auto context_id_opt = assoc.accepted_context_id(worklist_find_sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t context_id = *context_id_opt;

    // Build query dataset
    auto query_ds = worklist_scu::worklist_query_builder()
        .patient_name(opts.patient_name)
        .patient_id(opts.patient_id)
        .modality(opts.modality)
        .scheduled_date(opts.scheduled_date)
        .scheduled_time(opts.scheduled_time)
        .scheduled_station_ae(opts.station_ae)
        .scheduled_physician(opts.physician)
        .accession_number(opts.accession_number)
        .build();

    // Apply custom query keys (-k option)
    for (const auto& key : opts.custom_keys) {
        pacs::core::dicom_tag tag{key.group, key.element};
        query_ds.set_string(tag, pacs::encoding::vr_type::LO, key.value);
        if (opts.debug) {
            std::cout << "  Custom key: (" << std::hex << std::setw(4)
                      << std::setfill('0') << key.group << ","
                      << std::setw(4) << key.element << std::dec
                      << ") = \"" << key.value << "\"\n";
        }
    }

    // Create C-FIND request
    auto find_rq = make_c_find_rq(1, worklist_find_sop_class_uid);
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
        auto recv_result = assoc.receive_dimse(opts.timeout);
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
                    results.push_back(find_rsp.dataset());
                }
            }

            if (opts.verbose && pending_count % 10 == 0) {
                std::cout << "\rReceived " << pending_count << " items..." << std::flush;
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

    auto release_result = assoc.release(opts.timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Format results
    worklist_scu::worklist_result_formatter formatter(opts.format);
    std::string formatted_output = formatter.format(results);

    // Output to file or stdout
    if (!opts.output_file.empty()) {
        std::ofstream outfile(opts.output_file);
        if (!outfile) {
            std::cerr << "Error: Cannot open output file '" << opts.output_file << "'\n";
            return 2;
        }
        outfile << formatted_output;
        outfile.close();
        if (opts.verbose) {
            std::cout << "Results written to: " << opts.output_file << "\n";
        }
    } else {
        std::cout << formatted_output;
    }

    // Print summary for table format
    if (opts.format == worklist_scu::output_format::table && opts.verbose) {
        std::cout << "\n========================================\n";
        std::cout << "              Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Total items:      " << results.size();
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
    // Only show banner for table/text format (to not corrupt json/csv/xml output)
    bool show_banner = true;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--format" || arg == "--output" || arg == "-o") && i + 1 < argc) {
            std::string fmt = argv[i + 1];
            if (fmt == "json" || fmt == "csv" || fmt == "xml") {
                show_banner = false;
            }
            break;
        }
    }

    if (show_banner) {
        std::cout << R"(
 __        __         _    _ _     _     ____   ____ _   _
 \ \      / /__  _ __| | _| (_)___| |_  / ___| / ___| | | |
  \ \ /\ / / _ \| '__| |/ / | / __| __| \___ \| |   | | | |
   \ V  V / (_) | |  |   <| | \__ \ |_   ___) | |___| |_| |
    \_/\_/ \___/|_|  |_|\_\_|_|___/\__| |____/ \____|\___/

          Modality Worklist Query Client
)" << "\n";
    }

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 2;
    }

    return perform_query(opts);
}
