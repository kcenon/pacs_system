/**
 * @file main.cpp
 * @brief MPPS SCP - DICOM Modality Performed Procedure Step Server
 *
 * A command-line server for handling MPPS N-CREATE and N-SET requests.
 * Receives procedure status updates from modality devices.
 *
 * @see Issue #382 - mpps_scp: Implement MPPS SCP utility
 * @see DICOM PS3.4 Section F - MPPS SOP Class
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 *
 * Usage:
 *   mpps_scp <port> <ae_title> [options]
 *
 * Examples:
 *   mpps_scp 11112 MY_MPPS --output-dir ./mpps_records
 *   mpps_scp 11112 MY_MPPS --output-file ./mpps.json
 */

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/core/result.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/mpps_scp.hpp"
#include "pacs/services/verification_scp.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

namespace {

// =============================================================================
// Global State for Signal Handling
// =============================================================================

/// Global pointer to server for signal handling
std::atomic<pacs::network::dicom_server*> g_server{nullptr};

/// Global running flag for signal handling
std::atomic<bool> g_running{true};

/**
 * @brief Signal handler for graceful shutdown
 * @param signal The signal received
 */
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running = false;

    auto* server = g_server.load();
    if (server) {
        server->stop();
    }
}

/**
 * @brief Install signal handlers for graceful shutdown
 */
void install_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifndef _WIN32
    std::signal(SIGHUP, signal_handler);
#endif
}

// =============================================================================
// Command Line Parsing
// =============================================================================

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
MPPS SCP - DICOM Modality Performed Procedure Step Server

Usage: )" << program_name << R"( <port> <ae_title> [options]

Arguments:
  port            Port number to listen on (typically 104 or 11112)
  ae_title        Application Entity Title for this server (max 16 chars)

Output Options (optional):
  --output-dir <path>     Directory to store MPPS records as JSON files
  --output-file <path>    Single JSON file to append MPPS records

Server Options:
  --max-assoc <n>         Maximum concurrent associations (default: 10)
  --timeout <sec>         Idle timeout in seconds (default: 300)
  --help                  Show this help message

Examples:
  )" << program_name << R"( 11112 MY_MPPS
  )" << program_name << R"( 11112 MY_MPPS --output-dir ./mpps_records
  )" << program_name << R"( 11112 MY_MPPS --output-file ./mpps.json --max-assoc 20

MPPS Protocol:
  - N-CREATE: Modality starts a procedure (status = IN PROGRESS)
  - N-SET:    Modality completes or discontinues a procedure
              (status = COMPLETED or DISCONTINUED)

Notes:
  - Press Ctrl+C to stop the server gracefully
  - Without output options, MPPS records are logged to console only
  - Each MPPS instance is identified by its SOP Instance UID

Exit Codes:
  0  Normal termination
  1  Error - Failed to start server or invalid arguments
)";
}

/**
 * @brief Configuration structure for command-line arguments
 */
struct mpps_scp_args {
    uint16_t port = 0;
    std::string ae_title;
    std::filesystem::path output_dir;
    std::filesystem::path output_file;
    size_t max_associations = 10;
    uint32_t idle_timeout = 300;
};

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param args Output: parsed arguments
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], mpps_scp_args& args) {
    if (argc < 3) {
        return false;
    }

    // Check for help flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            return false;
        }
    }

    // Parse port
    try {
        int port_int = std::stoi(argv[1]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        args.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[1] << "'\n";
        return false;
    }

    // Parse AE title
    args.ae_title = argv[2];
    if (args.ae_title.length() > 16) {
        std::cerr << "Error: AE title exceeds 16 characters\n";
        return false;
    }

    // Parse optional arguments
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--output-dir" && i + 1 < argc) {
            args.output_dir = argv[++i];
        } else if (arg == "--output-file" && i + 1 < argc) {
            args.output_file = argv[++i];
        } else if (arg == "--max-assoc" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 1) {
                    std::cerr << "Error: max-assoc must be positive\n";
                    return false;
                }
                args.max_associations = static_cast<size_t>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid max-assoc value\n";
                return false;
            }
        } else if (arg == "--timeout" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 0) {
                    std::cerr << "Error: timeout cannot be negative\n";
                    return false;
                }
                args.idle_timeout = static_cast<uint32_t>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid timeout value\n";
                return false;
            }
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Format timestamp for logging
 * @return Current time as formatted string
 */
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

/**
 * @brief Escape a string for JSON output
 */
std::string json_escape(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// =============================================================================
// MPPS Record Storage
// =============================================================================

/**
 * @brief MPPS record structure for storage
 */
struct mpps_record {
    std::string sop_instance_uid;
    std::string status;
    std::string station_ae;
    std::string patient_id;
    std::string patient_name;
    std::string modality;
    std::string procedure_step_id;
    std::string start_date;
    std::string start_time;
    std::string end_date;
    std::string end_time;
    std::string created_at;
    std::string updated_at;
};

/**
 * @brief Convert MPPS record to JSON string
 */
std::string to_json(const mpps_record& record, bool pretty = true) {
    std::ostringstream oss;
    std::string indent = pretty ? "  " : "";
    std::string nl = pretty ? "\n" : "";

    oss << "{" << nl;
    oss << indent << "\"sopInstanceUid\": \"" << json_escape(record.sop_instance_uid) << "\"," << nl;
    oss << indent << "\"status\": \"" << json_escape(record.status) << "\"," << nl;
    oss << indent << "\"stationAeTitle\": \"" << json_escape(record.station_ae) << "\"," << nl;
    oss << indent << "\"patientId\": \"" << json_escape(record.patient_id) << "\"," << nl;
    oss << indent << "\"patientName\": \"" << json_escape(record.patient_name) << "\"," << nl;
    oss << indent << "\"modality\": \"" << json_escape(record.modality) << "\"," << nl;
    oss << indent << "\"procedureStepId\": \"" << json_escape(record.procedure_step_id) << "\"," << nl;
    oss << indent << "\"startDate\": \"" << json_escape(record.start_date) << "\"," << nl;
    oss << indent << "\"startTime\": \"" << json_escape(record.start_time) << "\"," << nl;
    oss << indent << "\"endDate\": \"" << json_escape(record.end_date) << "\"," << nl;
    oss << indent << "\"endTime\": \"" << json_escape(record.end_time) << "\"," << nl;
    oss << indent << "\"createdAt\": \"" << json_escape(record.created_at) << "\"," << nl;
    oss << indent << "\"updatedAt\": \"" << json_escape(record.updated_at) << "\"" << nl;
    oss << "}";

    return oss.str();
}

/**
 * @brief Thread-safe MPPS repository for storing records
 */
class mpps_repository {
public:
    explicit mpps_repository(const mpps_scp_args& args)
        : output_dir_(args.output_dir)
        , output_file_(args.output_file) {

        // Create output directory if specified
        if (!output_dir_.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(output_dir_, ec);
            if (ec) {
                std::cerr << "Warning: Could not create output directory: "
                          << output_dir_ << " - " << ec.message() << "\n";
            }
        }
    }

    /**
     * @brief Handle N-CREATE: store new MPPS record
     */
    pacs::network::Result<std::monostate> on_create(
        const pacs::services::mpps_instance& instance) {

        std::lock_guard<std::mutex> lock(mutex_);

        // Extract data from the MPPS instance
        mpps_record record;
        record.sop_instance_uid = instance.sop_instance_uid;
        record.status = std::string(pacs::services::to_string(instance.status));
        record.station_ae = instance.station_ae;
        record.created_at = current_timestamp();
        record.updated_at = record.created_at;

        // Extract patient information from dataset
        namespace tags = pacs::core::tags;
        record.patient_id = instance.data.get_string(tags::patient_id, "");
        record.patient_name = instance.data.get_string(tags::patient_name, "");
        record.modality = instance.data.get_string(tags::modality, "");
        record.start_date = instance.data.get_string(
            tags::performed_procedure_step_start_date, "");
        record.start_time = instance.data.get_string(
            tags::performed_procedure_step_start_time, "");

        // Extract procedure step ID from MPPS-specific tags
        record.procedure_step_id = instance.data.get_string(
            pacs::services::mpps_tags::performed_procedure_step_id, "");

        // Store record
        records_[record.sop_instance_uid] = record;

        // Log the event
        std::cout << "[" << current_timestamp() << "] "
                  << "N-CREATE: MPPS instance created\n"
                  << "  UID:        " << record.sop_instance_uid << "\n"
                  << "  Status:     " << record.status << "\n"
                  << "  Station:    " << record.station_ae << "\n"
                  << "  Patient:    " << record.patient_id << " / " << record.patient_name << "\n"
                  << "  Modality:   " << record.modality << "\n";

        // Save to file if configured
        save_record(record);

        return pacs::network::Result<std::monostate>::ok({});
    }

    /**
     * @brief Handle N-SET: update existing MPPS record
     */
    pacs::network::Result<std::monostate> on_set(
        const std::string& sop_instance_uid,
        const pacs::core::dicom_dataset& modifications,
        pacs::services::mpps_status new_status) {

        std::lock_guard<std::mutex> lock(mutex_);

        // Find existing record
        auto it = records_.find(sop_instance_uid);
        if (it == records_.end()) {
            // If not found, create a minimal record
            mpps_record record;
            record.sop_instance_uid = sop_instance_uid;
            record.created_at = current_timestamp();
            records_[sop_instance_uid] = record;
            it = records_.find(sop_instance_uid);
        }

        auto& record = it->second;

        // Check if already in final state
        if (record.status == "COMPLETED" || record.status == "DISCONTINUED") {
            std::cerr << "[" << current_timestamp() << "] "
                      << "Warning: Cannot modify MPPS in final state: "
                      << record.sop_instance_uid << "\n";
            return pacs::pacs_error<std::monostate>(
                pacs::error_codes::mpps_invalid_state,
                "Cannot modify MPPS in final state");
        }

        // Update status
        record.status = std::string(pacs::services::to_string(new_status));
        record.updated_at = current_timestamp();

        // Extract end date/time from modifications
        record.end_date = modifications.get_string(
            pacs::services::mpps_tags::performed_procedure_step_end_date, "");
        record.end_time = modifications.get_string(
            pacs::services::mpps_tags::performed_procedure_step_end_time, "");

        // Log the event
        std::cout << "[" << current_timestamp() << "] "
                  << "N-SET: MPPS instance updated\n"
                  << "  UID:        " << record.sop_instance_uid << "\n"
                  << "  New Status: " << record.status << "\n";

        if (!record.end_date.empty() || !record.end_time.empty()) {
            std::cout << "  End Time:   " << record.end_date << " " << record.end_time << "\n";
        }

        // Save updated record
        save_record(record);

        return pacs::network::Result<std::monostate>::ok({});
    }

    /**
     * @brief Get count of stored records
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_.size();
    }

    /**
     * @brief Get count by status
     */
    std::map<std::string, size_t> count_by_status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::string, size_t> counts;
        for (const auto& [uid, record] : records_) {
            counts[record.status]++;
        }
        return counts;
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, mpps_record> records_;
    std::filesystem::path output_dir_;
    std::filesystem::path output_file_;

    /**
     * @brief Save record to file(s)
     */
    void save_record(const mpps_record& record) {
        // Save to individual file in output directory
        if (!output_dir_.empty()) {
            auto filename = output_dir_ / (record.sop_instance_uid + ".json");
            std::ofstream file(filename);
            if (file) {
                file << to_json(record, true);
                file.close();
            } else {
                std::cerr << "Warning: Could not write to " << filename << "\n";
            }
        }

        // Append to output file
        if (!output_file_.empty()) {
            std::ofstream file(output_file_, std::ios::app);
            if (file) {
                file << to_json(record, false) << "\n";
                file.close();
            } else {
                std::cerr << "Warning: Could not write to " << output_file_ << "\n";
            }
        }
    }
};

// =============================================================================
// Server Implementation
// =============================================================================

/**
 * @brief Run the MPPS SCP server
 * @param args Parsed command-line arguments
 * @return true if server ran successfully
 */
bool run_server(const mpps_scp_args& args) {
    using namespace pacs::network;
    using namespace pacs::services;

    std::cout << "\nStarting MPPS SCP...\n";
    std::cout << "  AE Title:         " << args.ae_title << "\n";
    std::cout << "  Port:             " << args.port << "\n";
    if (!args.output_dir.empty()) {
        std::cout << "  Output Directory: " << args.output_dir << "\n";
    }
    if (!args.output_file.empty()) {
        std::cout << "  Output File:      " << args.output_file << "\n";
    }
    std::cout << "  Max Associations: " << args.max_associations << "\n";
    std::cout << "  Idle Timeout:     " << args.idle_timeout << " seconds\n";
    std::cout << "\n";

    // Create MPPS repository
    mpps_repository repository(args);

    // Configure server
    server_config config;
    config.ae_title = args.ae_title;
    config.port = args.port;
    config.max_associations = args.max_associations;
    config.idle_timeout = std::chrono::seconds{args.idle_timeout};
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.3";
    config.implementation_version_name = "MPPS_SCP_001";

    // Create server
    dicom_server server{config};
    g_server = &server;

    // Register Verification service (C-ECHO)
    server.register_service(std::make_shared<verification_scp>());

    // Configure MPPS SCP
    auto mpps_service = std::make_shared<mpps_scp>();

    // Set N-CREATE handler
    mpps_service->set_create_handler(
        [&repository](const mpps_instance& instance) {
            return repository.on_create(instance);
        });

    // Set N-SET handler
    mpps_service->set_set_handler(
        [&repository](const std::string& uid,
                      const pacs::core::dicom_dataset& mods,
                      mpps_status status) {
            return repository.on_set(uid, mods, status);
        });

    server.register_service(mpps_service);

    // Set up callbacks for logging
    server.on_association_established([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "Association established from: " << assoc.calling_ae()
                  << " -> " << assoc.called_ae() << "\n";
    });

    server.on_association_released([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "Association released: " << assoc.calling_ae() << "\n";
    });

    server.on_error([](const std::string& error) {
        std::cerr << "[" << current_timestamp() << "] "
                  << "Error: " << error << "\n";
    });

    // Start server
    auto result = server.start();
    if (result.is_err()) {
        std::cerr << "Failed to start server: " << result.error().message << "\n";
        g_server = nullptr;
        return false;
    }

    std::cout << "=================================================\n";
    std::cout << " MPPS SCP is running on port " << args.port << "\n";
    std::cout << " Waiting for MPPS requests...\n";
    std::cout << " Press Ctrl+C to stop\n";
    std::cout << "=================================================\n\n";

    // Wait for shutdown
    server.wait_for_shutdown();

    // Print final statistics
    auto server_stats = server.get_statistics();
    auto status_counts = repository.count_by_status();

    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << " Server Statistics\n";
    std::cout << "=================================================\n";
    std::cout << "  Total Associations:    " << server_stats.total_associations << "\n";
    std::cout << "  Rejected Associations: " << server_stats.rejected_associations << "\n";
    std::cout << "  Messages Processed:    " << server_stats.messages_processed << "\n";
    std::cout << "  N-CREATE Processed:    " << mpps_service->creates_processed() << "\n";
    std::cout << "  N-SET Processed:       " << mpps_service->sets_processed() << "\n";
    std::cout << "  MPPS Completed:        " << mpps_service->mpps_completed() << "\n";
    std::cout << "  MPPS Discontinued:     " << mpps_service->mpps_discontinued() << "\n";
    std::cout << "  Total MPPS Records:    " << repository.size() << "\n";

    if (!status_counts.empty()) {
        std::cout << "  Records by Status:\n";
        for (const auto& [status, count] : status_counts) {
            std::cout << "    - " << status << ": " << count << "\n";
        }
    }

    std::cout << "  Uptime:                " << server_stats.uptime().count() << " seconds\n";
    std::cout << "=================================================\n";

    g_server = nullptr;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  __  __ ____  ____  ____    ____   ____ ____
 |  \/  |  _ \|  _ \/ ___|  / ___| / ___|  _ \
 | |\/| | |_) | |_) \___ \  \___ \| |   | |_) |
 | |  | |  __/|  __/ ___) |  ___) | |___|  __/
 |_|  |_|_|   |_|   |____/  |____/ \____|_|

     DICOM Modality Performed Procedure Step Server
)" << "\n";

    mpps_scp_args args;

    if (!parse_arguments(argc, argv, args)) {
        print_usage(argv[0]);
        return 1;
    }

    // Install signal handlers
    install_signal_handlers();

    bool success = run_server(args);

    std::cout << "\nMPPS SCP terminated\n";
    return success ? 0 : 1;
}
