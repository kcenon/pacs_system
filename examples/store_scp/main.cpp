/**
 * @file main.cpp
 * @brief Storage SCP - DICOM Image Receiver
 *
 * A command-line server for receiving and storing DICOM images from modalities.
 * Supports hierarchical file storage with optional database indexing.
 *
 * @see Issue #101 - Storage SCP Sample
 * @see DICOM PS3.4 Section B - Storage Service Class
 *
 * Usage:
 *   store_scp <port> <ae_title> [options]
 *
 * Examples:
 *   store_scp 11112 MY_PACS --storage-dir ./received
 *   store_scp 11112 MY_PACS --storage-dir ./received --index-db ./pacs.db
 *   store_scp 11112 MY_PACS --storage-dir ./received --accept "CT,MR,US"
 */

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/storage/file_storage.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// Global pointer to server for signal handling
std::atomic<pacs::network::dicom_server*> g_server{nullptr};

/// Global running flag for signal handling
std::atomic<bool> g_running{true};

/// Global pointer to file storage for statistics
std::atomic<pacs::storage::file_storage*> g_file_storage{nullptr};

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

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Storage SCP - DICOM Image Receiver

Usage: )" << program_name << R"( <port> <ae_title> [options]

Arguments:
  port            Port number to listen on (typically 104 or 11112)
  ae_title        Application Entity Title for this server (max 16 chars)

Required Options:
  --storage-dir <path>    Directory to store received DICOM files

Optional Options:
  --index-db <path>       SQLite database for indexing (optional)
  --accept <modalities>   Comma-separated list of accepted modalities
                          (CT,MR,US,XR,CR,DX,NM,PT,SC,SR)
  --naming <scheme>       File naming scheme: hierarchical (default),
                          date, flat
  --duplicate <policy>    Duplicate handling: reject (default), replace, ignore
  --max-assoc <n>         Maximum concurrent associations (default: 10)
  --timeout <sec>         Idle timeout in seconds (default: 300)
  --help                  Show this help message

Examples:
  )" << program_name << R"( 11112 MY_PACS --storage-dir ./received
  )" << program_name << R"( 11112 MY_PACS --storage-dir ./received --index-db ./pacs.db
  )" << program_name << R"( 11112 MY_PACS --storage-dir ./archive --accept "CT,MR"

Notes:
  - Press Ctrl+C to stop the server gracefully
  - Files are stored in hierarchical structure: StudyUID/SeriesUID/SOPUID.dcm
  - Without --accept, all standard storage SOP classes are accepted

Exit Codes:
  0  Normal termination
  1  Error - Failed to start server or invalid arguments
)";
}

/**
 * @brief Configuration structure for command-line arguments
 */
struct store_scp_args {
    uint16_t port = 0;
    std::string ae_title;
    std::filesystem::path storage_dir;
    std::filesystem::path index_db;
    std::vector<std::string> accepted_modalities;
    pacs::storage::naming_scheme naming = pacs::storage::naming_scheme::uid_hierarchical;
    pacs::storage::duplicate_policy duplicate = pacs::storage::duplicate_policy::reject;
    size_t max_associations = 10;
    uint32_t idle_timeout = 300;
};

/**
 * @brief Map modality names to SOP Class UIDs
 */
std::vector<std::string> modality_to_sop_classes(const std::string& modality) {
    static const std::map<std::string, std::vector<std::string>> modality_map = {
        {"CT", {"1.2.840.10008.5.1.4.1.1.2", "1.2.840.10008.5.1.4.1.1.2.1"}},
        {"MR", {"1.2.840.10008.5.1.4.1.1.4", "1.2.840.10008.5.1.4.1.1.4.1"}},
        {"US", {"1.2.840.10008.5.1.4.1.1.6.1"}},
        {"CR", {"1.2.840.10008.5.1.4.1.1.1"}},
        {"DX", {"1.2.840.10008.5.1.4.1.1.1.1", "1.2.840.10008.5.1.4.1.1.1.1.1"}},
        {"XR", {"1.2.840.10008.5.1.4.1.1.12.1", "1.2.840.10008.5.1.4.1.1.12.2"}},
        {"NM", {"1.2.840.10008.5.1.4.1.1.20"}},
        {"PT", {"1.2.840.10008.5.1.4.1.1.128", "1.2.840.10008.5.1.4.1.1.130"}},
        {"SC", {"1.2.840.10008.5.1.4.1.1.7"}},
        {"SR", {"1.2.840.10008.5.1.4.1.1.88.11", "1.2.840.10008.5.1.4.1.1.88.22",
                "1.2.840.10008.5.1.4.1.1.88.33"}}
    };

    auto it = modality_map.find(modality);
    if (it != modality_map.end()) {
        return it->second;
    }
    return {};
}

/**
 * @brief Parse comma-separated modalities
 */
std::vector<std::string> parse_modalities(const std::string& input) {
    std::vector<std::string> sop_classes;
    std::istringstream ss(input);
    std::string modality;

    while (std::getline(ss, modality, ',')) {
        // Trim whitespace
        modality.erase(0, modality.find_first_not_of(" \t"));
        modality.erase(modality.find_last_not_of(" \t") + 1);

        // Convert to uppercase
        for (auto& c : modality) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        auto classes = modality_to_sop_classes(modality);
        sop_classes.insert(sop_classes.end(), classes.begin(), classes.end());
    }

    return sop_classes;
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param args Output: parsed arguments
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], store_scp_args& args) {
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

        if (arg == "--storage-dir" && i + 1 < argc) {
            args.storage_dir = argv[++i];
        } else if (arg == "--index-db" && i + 1 < argc) {
            args.index_db = argv[++i];
        } else if (arg == "--accept" && i + 1 < argc) {
            args.accepted_modalities = parse_modalities(argv[++i]);
        } else if (arg == "--naming" && i + 1 < argc) {
            std::string scheme = argv[++i];
            if (scheme == "hierarchical") {
                args.naming = pacs::storage::naming_scheme::uid_hierarchical;
            } else if (scheme == "date") {
                args.naming = pacs::storage::naming_scheme::date_hierarchical;
            } else if (scheme == "flat") {
                args.naming = pacs::storage::naming_scheme::flat;
            } else {
                std::cerr << "Error: Unknown naming scheme '" << scheme << "'\n";
                return false;
            }
        } else if (arg == "--duplicate" && i + 1 < argc) {
            std::string policy = argv[++i];
            if (policy == "reject") {
                args.duplicate = pacs::storage::duplicate_policy::reject;
            } else if (policy == "replace") {
                args.duplicate = pacs::storage::duplicate_policy::replace;
            } else if (policy == "ignore") {
                args.duplicate = pacs::storage::duplicate_policy::ignore;
            } else {
                std::cerr << "Error: Unknown duplicate policy '" << policy << "'\n";
                return false;
            }
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

    // Validate required arguments
    if (args.storage_dir.empty()) {
        std::cerr << "Error: --storage-dir is required\n";
        return false;
    }

    return true;
}

/**
 * @brief Format timestamp for logging
 * @return Current time as formatted string
 */
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

/**
 * @brief Format byte count for display
 */
std::string format_bytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        ++unit_index;
    }

    std::ostringstream oss;
    if (unit_index == 0) {
        oss << bytes << " " << units[unit_index];
    } else {
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    }
    return oss.str();
}

/**
 * @brief Run the Storage SCP server
 * @param args Parsed command-line arguments
 * @return true if server ran successfully
 */
bool run_server(const store_scp_args& args) {
    using namespace pacs::network;
    using namespace pacs::services;
    using namespace pacs::storage;
    using namespace pacs::core;

    std::cout << "\nStarting Storage SCP...\n";
    std::cout << "  AE Title:           " << args.ae_title << "\n";
    std::cout << "  Port:               " << args.port << "\n";
    std::cout << "  Storage Directory:  " << args.storage_dir << "\n";
    if (!args.index_db.empty()) {
        std::cout << "  Index Database:     " << args.index_db << "\n";
    }
    std::cout << "  Max Associations:   " << args.max_associations << "\n";
    std::cout << "  Idle Timeout:       " << args.idle_timeout << " seconds\n";
    if (!args.accepted_modalities.empty()) {
        std::cout << "  Accepted Classes:   " << args.accepted_modalities.size()
                  << " SOP class(es)\n";
    } else {
        std::cout << "  Accepted Classes:   All standard storage classes\n";
    }
    std::cout << "\n";

    // Create storage directory if it doesn't exist
    std::error_code ec;
    if (!std::filesystem::exists(args.storage_dir)) {
        if (!std::filesystem::create_directories(args.storage_dir, ec)) {
            std::cerr << "Failed to create storage directory: " << ec.message() << "\n";
            return false;
        }
        std::cout << "Created storage directory: " << args.storage_dir << "\n";
    }

    // Configure file storage
    file_storage_config storage_config;
    storage_config.root_path = args.storage_dir;
    storage_config.naming = args.naming;
    storage_config.duplicate = args.duplicate;
    storage_config.create_directories = true;

    // Create file storage
    file_storage storage{storage_config};
    g_file_storage = &storage;

    // Configure server
    server_config config;
    config.ae_title = args.ae_title;
    config.port = args.port;
    config.max_associations = args.max_associations;
    config.idle_timeout = std::chrono::seconds{args.idle_timeout};
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "STORE_SCP_001";

    // Create server
    dicom_server server{config};
    g_server = &server;

    // Configure Storage SCP service
    storage_scp_config scp_config;
    if (!args.accepted_modalities.empty()) {
        scp_config.accepted_sop_classes = args.accepted_modalities;
    }
    scp_config.duplicate_policy =
        args.duplicate == pacs::storage::duplicate_policy::reject ? pacs::services::duplicate_policy::reject :
        args.duplicate == pacs::storage::duplicate_policy::replace ? pacs::services::duplicate_policy::replace :
        pacs::services::duplicate_policy::ignore;

    auto storage_service = std::make_shared<storage_scp>(scp_config);

    // Set storage handler
    storage_service->set_handler(
        [&storage](const dicom_dataset& dataset,
                   const std::string& calling_ae,
                   [[maybe_unused]] const std::string& sop_class_uid,
                   [[maybe_unused]] const std::string& sop_instance_uid) -> storage_status {

            // Log incoming image
            auto patient_name = dataset.get_string(tags::patient_name, "Unknown");
            auto study_desc = dataset.get_string(tags::study_description, "");
            auto modality = dataset.get_string(tags::modality, "??");

            std::cout << "[" << current_timestamp() << "] "
                      << "C-STORE from " << calling_ae << ": "
                      << modality << " - " << patient_name;
            if (!study_desc.empty()) {
                std::cout << " (" << study_desc << ")";
            }
            std::cout << "\n";

            // Store the dataset
            auto result = storage.store(dataset);
            if (result.is_err()) {
                std::cerr << "[" << current_timestamp() << "] "
                          << "Storage failed: " << result.error().message << "\n";
                return storage_status::out_of_resources;
            }

            return storage_status::success;
        });

    // Optional: pre-store validation
    storage_service->set_pre_store_handler([](const dicom_dataset& dataset) {
        // Validate required attributes
        if (!dataset.contains(tags::study_instance_uid) ||
            !dataset.contains(tags::series_instance_uid) ||
            !dataset.contains(tags::sop_instance_uid)) {
            std::cerr << "[" << current_timestamp() << "] "
                      << "Rejected: Missing required UID attributes\n";
            return false;
        }
        return true;
    });

    // Register storage service
    server.register_service(storage_service);

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
        g_file_storage = nullptr;
        return false;
    }

    std::cout << "=================================================\n";
    std::cout << " Storage SCP is running on port " << args.port << "\n";
    std::cout << " Storage: " << args.storage_dir << "\n";
    std::cout << " Press Ctrl+C to stop\n";
    std::cout << "=================================================\n\n";

    // Wait for shutdown
    server.wait_for_shutdown();

    // Print final statistics
    auto server_stats = server.get_statistics();
    auto storage_stats = storage.get_statistics();

    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << " Server Statistics\n";
    std::cout << "=================================================\n";
    std::cout << "  Total Associations:    " << server_stats.total_associations << "\n";
    std::cout << "  Rejected Associations: " << server_stats.rejected_associations << "\n";
    std::cout << "  Messages Processed:    " << server_stats.messages_processed << "\n";
    std::cout << "  Images Received:       " << storage_service->images_received() << "\n";
    std::cout << "  Bytes Received:        " << format_bytes(storage_service->bytes_received()) << "\n";
    std::cout << "  Uptime:                " << server_stats.uptime().count() << " seconds\n";
    std::cout << "=================================================\n";
    std::cout << " Storage Statistics\n";
    std::cout << "=================================================\n";
    std::cout << "  Total Instances:       " << storage_stats.total_instances << "\n";
    std::cout << "  Total Size:            " << format_bytes(storage_stats.total_bytes) << "\n";
    std::cout << "=================================================\n";

    g_server = nullptr;
    g_file_storage = nullptr;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____ _____ ___  ____  _____   ____   ____ ____
 / ___|_   _/ _ \|  _ \| ____| / ___| / ___|  _ \
 \___ \ | || | | | |_) |  _|   \___ \| |   | |_) |
  ___) || || |_| |  _ <| |___   ___) | |___|  __/
 |____/ |_| \___/|_| \_\_____| |____/ \____|_|

           DICOM Image Receiver Server
)" << "\n";

    store_scp_args args;

    if (!parse_arguments(argc, argv, args)) {
        print_usage(argv[0]);
        return 1;
    }

    // Install signal handlers
    install_signal_handlers();

    bool success = run_server(args);

    std::cout << "\nStorage SCP terminated\n";
    return success ? 0 : 1;
}
