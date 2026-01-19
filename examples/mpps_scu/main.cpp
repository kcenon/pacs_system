/**
 * @file main.cpp
 * @brief MPPS SCU - Modality Performed Procedure Step Client
 *
 * A command-line utility for reporting procedure status to an MPPS SCP (RIS).
 * Uses the mpps_scu library class for N-CREATE and N-SET operations.
 *
 * @see Issue #534 - MPPS SCU Library Implementation
 * @see DICOM PS3.4 Section F - MPPS SOP Class
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 *
 * Usage:
 *   mpps_scu <host> <port> <called_ae> <command> [options]
 *
 * Commands:
 *   create  Create new MPPS instance (IN PROGRESS)
 *   set     Update existing MPPS instance (COMPLETED/DISCONTINUED)
 *
 * Examples:
 *   mpps_scu localhost 11112 RIS_SCP create --patient-id P001 --modality CT
 *   mpps_scu localhost 11112 RIS_SCP set --mpps-uid 1.2.3... --status COMPLETED
 */

#include "pacs/services/mpps_scu.hpp"

#include "pacs/network/association.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

/// Default calling AE title
constexpr const char* default_calling_ae = "MPPS_SCU";

/// Default timeout (30 seconds)
constexpr auto default_timeout = std::chrono::milliseconds{30000};

/**
 * @brief Command type enumeration
 */
enum class mpps_command {
    create,  // N-CREATE (start procedure)
    set      // N-SET (complete/discontinue)
};

/**
 * @brief MPPS status type for command line
 */
enum class cli_status_type {
    completed,
    discontinued
};

/**
 * @brief Command-line options structure
 */
struct options {
    // Connection
    std::string host;
    uint16_t port{0};
    std::string called_ae;
    std::string calling_ae{default_calling_ae};

    // Command
    mpps_command command{mpps_command::create};

    // N-CREATE options (create new MPPS)
    std::string patient_name;
    std::string patient_id;
    std::string modality{"CT"};
    std::string procedure_id;
    std::string study_uid;

    // N-SET options (update existing MPPS)
    std::string mpps_uid;
    cli_status_type status{cli_status_type::completed};
    std::string discontinuation_reason;
    std::string series_uid;

    // Output options
    bool verbose{false};
};

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name) {
    std::cout << R"(
MPPS SCU - Modality Performed Procedure Step Client

Usage: )" << program_name << R"( <host> <port> <called_ae> <command> [options]

Arguments:
  host        Remote host address (IP or hostname)
  port        Remote port number (typically 11112)
  called_ae   Called AE Title (remote MPPS SCP's AE title, e.g., RIS_SCP)
  command     'create' or 'set'

Commands:
  create      Create new MPPS instance with IN PROGRESS status
  set         Update existing MPPS instance to COMPLETED or DISCONTINUED

Create Options (N-CREATE):
  --patient-name <name>   Patient name (format: LAST^FIRST)
  --patient-id <id>       Patient ID (required)
  --modality <mod>        Modality code (CT, MR, US, XR, etc.) [default: CT]
  --procedure-id <id>     Performed Procedure Step ID
  --study-uid <uid>       Study Instance UID (auto-generated if not provided)

Set Options (N-SET):
  --mpps-uid <uid>        MPPS SOP Instance UID (required)
  --status <status>       New status: COMPLETED or DISCONTINUED [default: COMPLETED]
  --reason <text>         Discontinuation reason (for DISCONTINUED status)
  --series-uid <uid>      Performed Series Instance UID

General Options:
  --calling-ae <ae>       Calling AE Title [default: MPPS_SCU]
  --verbose, -v           Show detailed progress
  --help, -h              Show this help message

Examples:
  # Start a new CT procedure
  )" << program_name << R"( localhost 11112 RIS_SCP create \
    --patient-id "12345" \
    --patient-name "Doe^John" \
    --modality CT

  # Complete the procedure
  )" << program_name << R"( localhost 11112 RIS_SCP set \
    --mpps-uid "1.2.3.4.5.6.7.8" \
    --status COMPLETED \
    --series-uid "1.2.3.4.5.6.7.8.9"

  # Discontinue (cancel) the procedure
  )" << program_name << R"( localhost 11112 RIS_SCP set \
    --mpps-uid "1.2.3.4.5.6.7.8" \
    --status DISCONTINUED \
    --reason "Patient refused"

Exit Codes:
  0  Success
  1  MPPS operation failed
  2  Connection or argument error
)";
}

/**
 * @brief Parse command line arguments
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 5) {
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

    // Parse command
    std::string cmd = argv[4];
    if (cmd == "create") {
        opts.command = mpps_command::create;
    } else if (cmd == "set") {
        opts.command = mpps_command::set;
    } else if (cmd == "--help" || cmd == "-h") {
        return false;
    } else {
        std::cerr << "Error: Unknown command '" << cmd << "'. Use 'create' or 'set'\n";
        return false;
    }

    // Parse optional arguments
    for (int i = 5; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--calling-ae" && i + 1 < argc) {
            opts.calling_ae = argv[++i];
            if (opts.calling_ae.length() > 16) {
                std::cerr << "Error: Calling AE title exceeds 16 characters\n";
                return false;
            }
        }
        // N-CREATE options
        else if (arg == "--patient-name" && i + 1 < argc) {
            opts.patient_name = argv[++i];
        } else if (arg == "--patient-id" && i + 1 < argc) {
            opts.patient_id = argv[++i];
        } else if (arg == "--modality" && i + 1 < argc) {
            opts.modality = argv[++i];
        } else if (arg == "--procedure-id" && i + 1 < argc) {
            opts.procedure_id = argv[++i];
        } else if (arg == "--study-uid" && i + 1 < argc) {
            opts.study_uid = argv[++i];
        }
        // N-SET options
        else if (arg == "--mpps-uid" && i + 1 < argc) {
            opts.mpps_uid = argv[++i];
        } else if (arg == "--status" && i + 1 < argc) {
            std::string status_str = argv[++i];
            if (status_str == "COMPLETED") {
                opts.status = cli_status_type::completed;
            } else if (status_str == "DISCONTINUED") {
                opts.status = cli_status_type::discontinued;
            } else {
                std::cerr << "Error: Invalid status '" << status_str
                          << "'. Use COMPLETED or DISCONTINUED\n";
                return false;
            }
        } else if (arg == "--reason" && i + 1 < argc) {
            opts.discontinuation_reason = argv[++i];
        } else if (arg == "--series-uid" && i + 1 < argc) {
            opts.series_uid = argv[++i];
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    // Validate required options
    if (opts.command == mpps_command::create) {
        if (opts.patient_id.empty()) {
            std::cerr << "Error: --patient-id is required for 'create' command\n";
            return false;
        }
    } else if (opts.command == mpps_command::set) {
        if (opts.mpps_uid.empty()) {
            std::cerr << "Error: --mpps-uid is required for 'set' command\n";
            return false;
        }
    }

    return true;
}

/**
 * @brief Perform MPPS N-CREATE operation using the library
 */
int perform_mpps_create(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::services;

    if (opts.verbose) {
        std::cout << "=== MPPS N-CREATE (Start Procedure) ===\n";
        std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
        std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
        std::cout << "  Called AE:   " << opts.called_ae << "\n";
        std::cout << "  Patient ID:  " << opts.patient_id << "\n";
        std::cout << "  Modality:    " << opts.modality << "\n\n";
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "MPPS_SCU_001";

    // Propose MPPS SOP Class
    config.proposed_contexts.push_back({
        1,  // Context ID
        std::string(mpps_sop_class_uid),
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

    if (opts.verbose) {
        auto connect_time = std::chrono::steady_clock::now();
        auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            connect_time - start_time);
        std::cout << "Association established in " << connect_duration.count() << " ms\n";
    }

    // Check if context was accepted
    if (!assoc.has_accepted_context(mpps_sop_class_uid)) {
        std::cerr << "Error: MPPS SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    // Create MPPS SCU instance and prepare data
    mpps_scu scu;

    mpps_create_data create_data;
    create_data.patient_name = opts.patient_name;
    create_data.patient_id = opts.patient_id;
    create_data.modality = opts.modality;
    create_data.station_ae_title = opts.calling_ae;
    create_data.scheduled_procedure_step_id = opts.procedure_id;
    create_data.study_instance_uid = opts.study_uid;

    if (opts.verbose) {
        std::cout << "Sending N-CREATE request...\n";
    }

    // Perform N-CREATE
    auto create_result = scu.create(assoc, create_data);

    if (create_result.is_err()) {
        std::cerr << "N-CREATE failed: " << create_result.error().message << "\n";
        assoc.abort();
        return 1;
    }

    const auto& result = create_result.value();

    if (!result.is_success()) {
        std::cerr << "N-CREATE returned error status: 0x"
                  << std::hex << result.status << std::dec << "\n";
        if (!result.error_comment.empty()) {
            std::cerr << "  Error comment: " << result.error_comment << "\n";
        }
        (void)assoc.release(default_timeout);
        return 1;
    }

    // Release association
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

    // Success output
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "      MPPS Created Successfully\n";
    std::cout << "========================================\n";
    std::cout << "  MPPS UID:     " << result.mpps_sop_instance_uid << "\n";
    std::cout << "  Status:       IN PROGRESS\n";
    std::cout << "  Patient ID:   " << opts.patient_id << "\n";
    std::cout << "  Modality:     " << opts.modality << "\n";
    std::cout << "  Total time:   " << total_duration.count() << " ms\n";
    std::cout << "========================================\n";
    std::cout << "\nUse this MPPS UID to update the procedure:\n";
    std::cout << "  " << opts.calling_ae << " " << opts.host << " " << opts.port
              << " " << opts.called_ae << " set \\\n";
    std::cout << "    --mpps-uid \"" << result.mpps_sop_instance_uid << "\" \\\n";
    std::cout << "    --status COMPLETED\n";

    return 0;
}

/**
 * @brief Perform MPPS N-SET operation using the library
 */
int perform_mpps_set(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::services;

    const char* status_str = (opts.status == cli_status_type::completed)
        ? "COMPLETED" : "DISCONTINUED";

    if (opts.verbose) {
        std::cout << "=== MPPS N-SET (Update Status to " << status_str << ") ===\n";
        std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
        std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
        std::cout << "  Called AE:   " << opts.called_ae << "\n";
        std::cout << "  MPPS UID:    " << opts.mpps_uid << "\n";
        std::cout << "  New Status:  " << status_str << "\n\n";
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "MPPS_SCU_001";

    // Propose MPPS SOP Class
    config.proposed_contexts.push_back({
        1,
        std::string(mpps_sop_class_uid),
        {
            "1.2.840.10008.1.2.1",
            "1.2.840.10008.1.2"
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

    if (opts.verbose) {
        auto connect_time = std::chrono::steady_clock::now();
        auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            connect_time - start_time);
        std::cout << "Association established in " << connect_duration.count() << " ms\n";
    }

    // Check if context was accepted
    if (!assoc.has_accepted_context(mpps_sop_class_uid)) {
        std::cerr << "Error: MPPS SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    // Create MPPS SCU instance
    mpps_scu scu;

    if (opts.verbose) {
        std::cout << "Sending N-SET request...\n";
    }

    // Perform N-SET using convenience methods
    network::Result<mpps_result> set_result = [&]() {
        if (opts.status == cli_status_type::completed) {
            // Build performed series info if provided
            std::vector<performed_series_info> performed_series;
            if (!opts.series_uid.empty()) {
                performed_series_info series;
                series.series_uid = opts.series_uid;
                series.modality = opts.modality;
                performed_series.push_back(series);
            }
            return scu.complete(assoc, opts.mpps_uid, performed_series);
        } else {
            return scu.discontinue(assoc, opts.mpps_uid, opts.discontinuation_reason);
        }
    }();

    if (set_result.is_err()) {
        std::cerr << "N-SET failed: " << set_result.error().message << "\n";
        assoc.abort();
        return 1;
    }

    const auto& result = set_result.value();

    if (!result.is_success()) {
        std::cerr << "N-SET returned error status: 0x"
                  << std::hex << result.status << std::dec << "\n";

        // Common error: trying to modify completed/discontinued MPPS
        if (result.status == 0xC310) {
            std::cerr << "  Note: Cannot modify MPPS that is already COMPLETED or DISCONTINUED\n";
        }
        if (!result.error_comment.empty()) {
            std::cerr << "  Error comment: " << result.error_comment << "\n";
        }

        (void)assoc.release(default_timeout);
        return 1;
    }

    // Release association
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

    // Success output
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "      MPPS Updated Successfully\n";
    std::cout << "========================================\n";
    std::cout << "  MPPS UID:     " << opts.mpps_uid << "\n";
    std::cout << "  New Status:   " << status_str << "\n";
    std::cout << "  Total time:   " << total_duration.count() << " ms\n";
    std::cout << "========================================\n";

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  __  __ ____  ____  ____    ____   ____ _   _
 |  \/  |  _ \|  _ \/ ___|  / ___| / ___| | | |
 | |\/| | |_) | |_) \___ \  \___ \| |   | | | |
 | |  | |  __/|  __/ ___) |  ___) | |___| |_| |
 |_|  |_|_|   |_|   |____/  |____/ \____|\___/

     Modality Performed Procedure Step Client
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 2;
    }

    // Execute requested command
    if (opts.command == mpps_command::create) {
        return perform_mpps_create(opts);
    } else {
        return perform_mpps_set(opts);
    }
}
