/**
 * @file main.cpp
 * @brief Echo SCP - DICOM Connectivity Test Server
 *
 * A simple command-line server for testing DICOM network connectivity.
 * Responds to C-ECHO requests from remote SCUs (equivalent to a "ping server").
 *
 * @see Issue #99 - Echo SCU/SCP Sample
 * @see DICOM PS3.7 Section 9.1 - C-ECHO Service
 *
 * Usage:
 *   echo_scp <port> <ae_title>
 *
 * Example:
 *   echo_scp 11112 MY_PACS
 */

#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/verification_scp.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

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

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Echo SCP - DICOM Connectivity Test Server

Usage: )" << program_name << R"( <port> <ae_title> [options]

Arguments:
  port        Port number to listen on (typically 104 or 11112)
  ae_title    Application Entity Title for this server (max 16 chars)

Options:
  --max-assoc <n>    Maximum concurrent associations (default: 10)
  --timeout <sec>    Idle timeout in seconds (default: 300)
  --help             Show this help message

Examples:
  )" << program_name << R"( 11112 MY_PACS
  )" << program_name << R"( 104 DICOM_SERVER --max-assoc 20

Notes:
  - Press Ctrl+C to stop the server gracefully
  - The server responds to C-ECHO requests from any calling AE

Exit Codes:
  0  Normal termination
  1  Error - Failed to start server
)";
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param port Output: listening port
 * @param ae_title Output: AE title
 * @param max_associations Output: max concurrent associations
 * @param idle_timeout Output: idle timeout in seconds
 * @return true if arguments are valid
 */
bool parse_arguments(
    int argc,
    char* argv[],
    uint16_t& port,
    std::string& ae_title,
    size_t& max_associations,
    uint32_t& idle_timeout) {

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
        port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[1] << "'\n";
        return false;
    }

    // Parse AE title
    ae_title = argv[2];
    if (ae_title.length() > 16) {
        std::cerr << "Error: AE title exceeds 16 characters\n";
        return false;
    }

    // Default values
    max_associations = 10;
    idle_timeout = 300;

    // Parse optional arguments
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--max-assoc" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 1) {
                    std::cerr << "Error: max-assoc must be positive\n";
                    return false;
                }
                max_associations = static_cast<size_t>(val);
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
                idle_timeout = static_cast<uint32_t>(val);
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
 * @brief Run the Echo SCP server
 * @param port Listening port
 * @param ae_title AE title
 * @param max_associations Maximum concurrent associations
 * @param idle_timeout Idle timeout in seconds
 * @return true if server ran successfully
 */
bool run_server(
    uint16_t port,
    const std::string& ae_title,
    size_t max_associations,
    uint32_t idle_timeout) {

    using namespace pacs::network;
    using namespace pacs::services;

    std::cout << "\nStarting Echo SCP...\n";
    std::cout << "  AE Title:           " << ae_title << "\n";
    std::cout << "  Port:               " << port << "\n";
    std::cout << "  Max Associations:   " << max_associations << "\n";
    std::cout << "  Idle Timeout:       " << idle_timeout << " seconds\n\n";

    // Configure server
    server_config config;
    config.ae_title = ae_title;
    config.port = port;
    config.max_associations = max_associations;
    config.idle_timeout = std::chrono::seconds{idle_timeout};
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "ECHO_SCP_001";

    // Create server
    dicom_server server{config};
    g_server = &server;

    // Register verification service (handles C-ECHO)
    server.register_service(std::make_shared<verification_scp>());

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
    std::cout << " Echo SCP is running on port " << port << "\n";
    std::cout << " Press Ctrl+C to stop\n";
    std::cout << "=================================================\n\n";

    // Wait for shutdown
    server.wait_for_shutdown();

    // Print final statistics
    auto stats = server.get_statistics();
    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << " Server Statistics\n";
    std::cout << "=================================================\n";
    std::cout << "  Total Associations:    " << stats.total_associations << "\n";
    std::cout << "  Rejected Associations: " << stats.rejected_associations << "\n";
    std::cout << "  Messages Processed:    " << stats.messages_processed << "\n";
    std::cout << "  Bytes Received:        " << stats.bytes_received << "\n";
    std::cout << "  Bytes Sent:            " << stats.bytes_sent << "\n";
    std::cout << "  Uptime:                " << stats.uptime().count() << " seconds\n";
    std::cout << "=================================================\n";

    g_server = nullptr;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  _____ ____ _   _  ___    ____   ____ ____
 | ____/ ___| | | |/ _ \  / ___| / ___|  _ \
 |  _|| |   | |_| | | | | \___ \| |   | |_) |
 | |__| |___|  _  | |_| |  ___) | |___|  __/
 |_____\____|_| |_|\___/  |____/ \____|_|

        DICOM Connectivity Test Server
)" << "\n";

    uint16_t port = 0;
    std::string ae_title;
    size_t max_associations = 0;
    uint32_t idle_timeout = 0;

    if (!parse_arguments(argc, argv, port, ae_title, max_associations, idle_timeout)) {
        print_usage(argv[0]);
        return 1;
    }

    // Install signal handlers
    install_signal_handlers();

    bool success = run_server(port, ae_title, max_associations, idle_timeout);

    std::cout << "\nEcho SCP terminated\n";
    return success ? 0 : 1;
}
