/**
 * @file main.cpp
 * @brief MPPS SCU - Modality Performed Procedure Step Client
 *
 * A command-line utility for reporting procedure status to an MPPS SCP (RIS).
 * Supports creating new MPPS instances (N-CREATE) and updating status (N-SET).
 *
 * @see Issue #105 - MPPS SCU Sample
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

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/mpps_scp.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

namespace {

/// Default calling AE title
constexpr const char* default_calling_ae = "MPPS_SCU";

/// Default timeout (30 seconds)
constexpr auto default_timeout = std::chrono::milliseconds{30000};

/// MPPS SOP Class UID
constexpr std::string_view mpps_sop_class_uid = "1.2.840.10008.3.1.2.3.3";

/**
 * @brief Command type enumeration
 */
enum class mpps_command {
    create,  // N-CREATE (start procedure)
    set      // N-SET (complete/discontinue)
};

/**
 * @brief MPPS status type
 */
enum class mpps_status_type {
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
    mpps_status_type status{mpps_status_type::completed};
    std::string discontinuation_reason;
    std::string series_uid;

    // Output options
    bool verbose{false};
};

/**
 * @brief Generate a unique UID
 * @param root Root UID prefix
 * @return Unique UID string
 */
std::string generate_uid(const std::string& root = "1.2.826.0.1.3680043.2.1545") {
    static std::mt19937_64 gen{std::random_device{}()};
    static std::uniform_int_distribution<uint64_t> dist;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return root + "." + std::to_string(timestamp) + "." + std::to_string(dist(gen) % 100000);
}

/**
 * @brief Get current date in DICOM format (YYYYMMDD)
 */
std::string get_current_date() {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y%m%d");
    return oss.str();
}

/**
 * @brief Get current time in DICOM format (HHMMSS)
 */
std::string get_current_time() {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(tm, "%H%M%S");
    return oss.str();
}

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
                opts.status = mpps_status_type::completed;
            } else if (status_str == "DISCONTINUED") {
                opts.status = mpps_status_type::discontinued;
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
 * @brief Create N-CREATE request message for MPPS
 *
 * Constructs an N-CREATE-RQ message to start a new Modality Performed
 * Procedure Step with IN PROGRESS status.
 *
 * @param message_id The message ID for this request
 * @param sop_instance_uid The SOP Instance UID for the new MPPS
 * @param mpps_dataset The MPPS attributes dataset
 * @return Constructed N-CREATE-RQ message
 */
pacs::network::dimse::dimse_message make_n_create_rq(
    uint16_t message_id,
    std::string_view sop_instance_uid,
    pacs::core::dicom_dataset mpps_dataset) {

    using namespace pacs::network::dimse;

    dimse_message msg{command_field::n_create_rq, message_id};
    msg.set_affected_sop_class_uid(mpps_sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_dataset(std::move(mpps_dataset));

    return msg;
}

/**
 * @brief Create N-SET request message for MPPS
 *
 * Constructs an N-SET-RQ message to update an existing MPPS instance
 * to COMPLETED or DISCONTINUED status.
 *
 * @param message_id The message ID for this request
 * @param sop_instance_uid The SOP Instance UID of the MPPS to update
 * @param modifications The dataset containing the modifications
 * @return Constructed N-SET-RQ message
 */
pacs::network::dimse::dimse_message make_n_set_rq(
    uint16_t message_id,
    std::string_view sop_instance_uid,
    pacs::core::dicom_dataset modifications) {

    using namespace pacs::network::dimse;
    using namespace pacs::encoding;

    dimse_message msg{command_field::n_set_rq, message_id};
    msg.set_affected_sop_class_uid(mpps_sop_class_uid);

    // For N-SET, use Requested SOP Instance UID in command set
    msg.command_set().set_string(tag_requested_sop_instance_uid, vr_type::UI, std::string(sop_instance_uid));
    msg.set_dataset(std::move(modifications));

    return msg;
}

/**
 * @brief Build MPPS creation dataset
 */
pacs::core::dicom_dataset build_mpps_create_dataset(const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;
    using namespace pacs::services::mpps_tags;

    dicom_dataset ds;

    // Performed Procedure Step Status - always IN PROGRESS for N-CREATE
    ds.set_string(performed_procedure_step_status, vr_type::CS, "IN PROGRESS");

    // Performed Procedure Step Timing
    ds.set_string(tags::performed_procedure_step_start_date, vr_type::DA, get_current_date());
    ds.set_string(tags::performed_procedure_step_start_time, vr_type::TM, get_current_time());

    // Modality and Station
    ds.set_string(tags::modality, vr_type::CS, opts.modality);
    ds.set_string(performed_station_ae_title, vr_type::AE, opts.calling_ae);

    // Procedure Step ID
    std::string pps_id = opts.procedure_id.empty()
        ? "PPS_" + opts.patient_id + "_" + get_current_time()
        : opts.procedure_id;
    ds.set_string(performed_procedure_step_id, vr_type::SH, pps_id);

    // Patient module (required)
    std::string patient_name = opts.patient_name.empty() ? "ANONYMOUS" : opts.patient_name;
    ds.set_string(tags::patient_name, vr_type::PN, patient_name);
    ds.set_string(tags::patient_id, vr_type::LO, opts.patient_id);

    // Study reference
    std::string study_uid = opts.study_uid.empty() ? generate_uid() : opts.study_uid;
    ds.set_string(tags::study_instance_uid, vr_type::UI, study_uid);

    // Empty sequence placeholders (required by MPPS IOD)
    // Performed Series Sequence - will be populated on N-SET COMPLETED
    // Scheduled Step Attributes Sequence - links to worklist

    return ds;
}

/**
 * @brief Build MPPS update dataset for N-SET
 */
pacs::core::dicom_dataset build_mpps_set_dataset(const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;
    using namespace pacs::services::mpps_tags;

    dicom_dataset ds;

    // Update status
    if (opts.status == mpps_status_type::completed) {
        ds.set_string(performed_procedure_step_status, vr_type::CS, "COMPLETED");
    } else {
        ds.set_string(performed_procedure_step_status, vr_type::CS, "DISCONTINUED");

        // Discontinuation reason code would go in a sequence
        // Simplified: just note the reason in the comments
        // In production, use Performed Procedure Step Discontinuation Reason Code Sequence
    }

    // End date/time
    ds.set_string(performed_procedure_step_end_date, vr_type::DA, get_current_date());
    ds.set_string(performed_procedure_step_end_time, vr_type::TM, get_current_time());

    // Series information (if completing)
    if (opts.status == mpps_status_type::completed && !opts.series_uid.empty()) {
        // Note: In production, this would be a Performed Series Sequence
        // with proper nested structure. Simplified here for the sample.
    }

    return ds;
}

/**
 * @brief Perform MPPS N-CREATE operation
 */
int perform_mpps_create(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;

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

    auto context_id_opt = assoc.accepted_context_id(mpps_sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t context_id = *context_id_opt;

    // Generate MPPS SOP Instance UID
    std::string mpps_uid = generate_uid();

    // Build MPPS dataset
    auto mpps_dataset = build_mpps_create_dataset(opts);

    // Create N-CREATE request
    auto n_create_rq = make_n_create_rq(1, mpps_uid, std::move(mpps_dataset));

    if (opts.verbose) {
        std::cout << "Sending N-CREATE request...\n";
        std::cout << "  MPPS UID: " << mpps_uid << "\n";
    }

    // Send N-CREATE request
    auto send_result = assoc.send_dimse(context_id, n_create_rq);
    if (send_result.is_err()) {
        std::cerr << "Failed to send N-CREATE: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Receive N-CREATE response
    auto recv_result = assoc.receive_dimse(default_timeout);
    if (recv_result.is_err()) {
        std::cerr << "Failed to receive N-CREATE response: "
                  << recv_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    auto& [recv_context_id, n_create_rsp] = recv_result.value();

    // Check response
    if (n_create_rsp.command() != command_field::n_create_rsp) {
        std::cerr << "Error: Unexpected response (expected N-CREATE-RSP)\n";
        assoc.abort();
        return 1;
    }

    auto status = n_create_rsp.status();
    if (status != status_success) {
        std::cerr << "N-CREATE failed with status: 0x"
                  << std::hex << static_cast<uint16_t>(status) << std::dec << "\n";
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
    std::cout << "  MPPS UID:     " << mpps_uid << "\n";
    std::cout << "  Status:       IN PROGRESS\n";
    std::cout << "  Patient ID:   " << opts.patient_id << "\n";
    std::cout << "  Modality:     " << opts.modality << "\n";
    std::cout << "  Total time:   " << total_duration.count() << " ms\n";
    std::cout << "========================================\n";
    std::cout << "\nUse this MPPS UID to update the procedure:\n";
    std::cout << "  " << opts.calling_ae << " " << opts.host << " " << opts.port
              << " " << opts.called_ae << " set \\\n";
    std::cout << "    --mpps-uid \"" << mpps_uid << "\" \\\n";
    std::cout << "    --status COMPLETED\n";

    return 0;
}

/**
 * @brief Perform MPPS N-SET operation
 */
int perform_mpps_set(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;

    const char* status_str = (opts.status == mpps_status_type::completed)
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

    auto context_id_opt = assoc.accepted_context_id(mpps_sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t context_id = *context_id_opt;

    // Build modification dataset
    auto mod_dataset = build_mpps_set_dataset(opts);

    // Create N-SET request
    auto n_set_rq = make_n_set_rq(1, opts.mpps_uid, std::move(mod_dataset));

    if (opts.verbose) {
        std::cout << "Sending N-SET request...\n";
    }

    // Send N-SET request
    auto send_result = assoc.send_dimse(context_id, n_set_rq);
    if (send_result.is_err()) {
        std::cerr << "Failed to send N-SET: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Receive N-SET response
    auto recv_result = assoc.receive_dimse(default_timeout);
    if (recv_result.is_err()) {
        std::cerr << "Failed to receive N-SET response: "
                  << recv_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    auto& [recv_context_id, n_set_rsp] = recv_result.value();

    // Check response
    if (n_set_rsp.command() != command_field::n_set_rsp) {
        std::cerr << "Error: Unexpected response (expected N-SET-RSP)\n";
        assoc.abort();
        return 1;
    }

    auto status = n_set_rsp.status();
    if (status != status_success) {
        std::cerr << "N-SET failed with status: 0x"
                  << std::hex << static_cast<uint16_t>(status) << std::dec << "\n";

        // Common error: trying to modify completed/discontinued MPPS
        if (static_cast<uint16_t>(status) == 0xC310) {
            std::cerr << "  Note: Cannot modify MPPS that is already COMPLETED or DISCONTINUED\n";
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
