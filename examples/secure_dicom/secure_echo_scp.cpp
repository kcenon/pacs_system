/**
 * @file secure_echo_scp.cpp
 * @brief Secure Echo SCP - TLS-secured DICOM Connectivity Test Server
 *
 * A secure DICOM server demonstrating TLS 1.2/1.3 configuration for
 * encrypted DICOM communication. This sample shows proper certificate
 * handling and secure transport setup per DICOM PS3.15.
 *
 * @see Issue #110 - TLS DICOM Connection Sample
 * @see DICOM PS3.15 - Security and System Management Profiles
 * @see DICOM PS3.7 Section 9.1 - C-ECHO Service
 *
 * Usage:
 *   secure_echo_scp <port> <ae_title> --cert <cert> --key <key> [options]
 *
 * Example:
 *   secure_echo_scp 2762 MY_PACS --cert server.crt --key server.key --ca ca.crt
 */

#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/integration/network_adapter.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
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
 * @brief TLS configuration for the server
 */
struct tls_options {
    std::filesystem::path cert_path;
    std::filesystem::path key_path;
    std::filesystem::path ca_path;
    bool verify_peer = true;
    std::string tls_version = "1.2";
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Secure Echo SCP - TLS-secured DICOM Connectivity Test Server

Usage: )" << program_name << R"( <port> <ae_title> --cert <file> --key <file> [options]

Arguments:
  port        Port number to listen on (typically 2762 for DICOM TLS)
  ae_title    Application Entity Title for this server (max 16 chars)

Required TLS Options:
  --cert <file>       Server certificate file (PEM format)
  --key <file>        Server private key file (PEM format)

Optional TLS Options:
  --ca <file>         CA certificate for client verification (PEM format)
  --no-verify         Disable client certificate verification
  --tls-version <ver> Minimum TLS version: 1.2 or 1.3 (default: 1.2)

Server Options:
  --max-assoc <n>     Maximum concurrent associations (default: 10)
  --timeout <sec>     Idle timeout in seconds (default: 300)
  --help              Show this help message

Examples:
  # Basic TLS server
  )" << program_name << R"( 2762 MY_PACS --cert server.crt --key server.key

  # With client certificate verification (mutual TLS)
  )" << program_name << R"( 2762 MY_PACS --cert server.crt --key server.key --ca ca.crt

  # TLS 1.3 only
  )" << program_name << R"( 2762 MY_PACS --cert server.crt --key server.key --tls-version 1.3

Notes:
  - Standard DICOM TLS port is 2762
  - Press Ctrl+C to stop the server gracefully
  - Use generate_certs.sh to create test certificates

Exit Codes:
  0  Normal termination
  1  Error - Failed to start server or invalid configuration
)";
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param port Output: listening port
 * @param ae_title Output: AE title
 * @param tls Output: TLS configuration
 * @param max_associations Output: max concurrent associations
 * @param idle_timeout Output: idle timeout in seconds
 * @return true if arguments are valid
 */
bool parse_arguments(
    int argc,
    char* argv[],
    uint16_t& port,
    std::string& ae_title,
    tls_options& tls,
    size_t& max_associations,
    uint32_t& idle_timeout) {

    if (argc < 5) {
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
    tls.verify_peer = true;
    tls.tls_version = "1.2";

    // Parse options
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--cert" && i + 1 < argc) {
            tls.cert_path = argv[++i];
        } else if (arg == "--key" && i + 1 < argc) {
            tls.key_path = argv[++i];
        } else if (arg == "--ca" && i + 1 < argc) {
            tls.ca_path = argv[++i];
        } else if (arg == "--no-verify") {
            tls.verify_peer = false;
        } else if (arg == "--tls-version" && i + 1 < argc) {
            tls.tls_version = argv[++i];
            if (tls.tls_version != "1.2" && tls.tls_version != "1.3") {
                std::cerr << "Error: Invalid TLS version (use 1.2 or 1.3)\n";
                return false;
            }
        } else if (arg == "--max-assoc" && i + 1 < argc) {
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
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    // Validate required TLS options
    if (tls.cert_path.empty()) {
        std::cerr << "Error: --cert is required\n";
        return false;
    }
    if (tls.key_path.empty()) {
        std::cerr << "Error: --key is required\n";
        return false;
    }

    return true;
}

/**
 * @brief Validate TLS configuration files exist and are readable
 * @param tls TLS options to validate
 * @return true if all files are valid
 */
bool validate_tls_files(const tls_options& tls) {
    // Check certificate file
    if (!std::filesystem::exists(tls.cert_path)) {
        std::cerr << "Error: Certificate file not found: " << tls.cert_path << "\n";
        return false;
    }

    // Check key file
    if (!std::filesystem::exists(tls.key_path)) {
        std::cerr << "Error: Key file not found: " << tls.key_path << "\n";
        return false;
    }

    // Check CA file if specified
    if (!tls.ca_path.empty() && !std::filesystem::exists(tls.ca_path)) {
        std::cerr << "Error: CA certificate file not found: " << tls.ca_path << "\n";
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
 * @brief Run the Secure Echo SCP server
 */
bool run_server(
    uint16_t port,
    const std::string& ae_title,
    const tls_options& tls,
    size_t max_associations,
    uint32_t idle_timeout) {

    using namespace pacs::network;
    using namespace pacs::services;
    using namespace pacs::integration;

    std::cout << "\nStarting Secure Echo SCP...\n";
    std::cout << "  AE Title:           " << ae_title << "\n";
    std::cout << "  Port:               " << port << "\n";
    std::cout << "  Max Associations:   " << max_associations << "\n";
    std::cout << "  Idle Timeout:       " << idle_timeout << " seconds\n";
    std::cout << "\n";
    std::cout << "  TLS Configuration:\n";
    std::cout << "    Certificate:      " << tls.cert_path << "\n";
    std::cout << "    Private Key:      " << tls.key_path << "\n";
    if (!tls.ca_path.empty()) {
        std::cout << "    CA Certificate:   " << tls.ca_path << "\n";
    }
    std::cout << "    Verify Peer:      " << (tls.verify_peer ? "Yes" : "No") << "\n";
    std::cout << "    Min TLS Version:  " << tls.tls_version << "\n\n";

    // Configure TLS
    tls_config tls_cfg;
    tls_cfg.enabled = true;
    tls_cfg.cert_path = tls.cert_path;
    tls_cfg.key_path = tls.key_path;
    tls_cfg.ca_path = tls.ca_path;
    tls_cfg.verify_peer = tls.verify_peer;
    tls_cfg.min_version = (tls.tls_version == "1.3")
        ? tls_config::tls_version::v1_3
        : tls_config::tls_version::v1_2;

    // Validate TLS configuration
    auto tls_result = network_adapter::configure_tls(tls_cfg);
    if (tls_result.is_err()) {
        std::cerr << "TLS configuration error: " << tls_result.error().message << "\n";
        return false;
    }

    // Configure server
    server_config config;
    config.ae_title = ae_title;
    config.port = port;
    config.max_associations = max_associations;
    config.idle_timeout = std::chrono::seconds{idle_timeout};
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "SECURE_SCP_001";

    // Create server with TLS configuration
    auto server_ptr = network_adapter::create_server(config, tls_cfg);
    if (!server_ptr) {
        std::cerr << "Failed to create secure server\n";
        return false;
    }

    auto& server = *server_ptr;
    g_server = &server;

    // Register verification service (handles C-ECHO)
    server.register_service(std::make_shared<verification_scp>());

    // Set up callbacks for logging
    server.on_association_established([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "[TLS] Association established from: " << assoc.calling_ae()
                  << " -> " << assoc.called_ae() << "\n";
    });

    server.on_association_released([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "[TLS] Association released: " << assoc.calling_ae() << "\n";
    });

    server.on_error([](const std::string& error) {
        std::cerr << "[" << current_timestamp() << "] "
                  << "[TLS] Error: " << error << "\n";
    });

    // Start server
    auto result = server.start();
    if (result.is_err()) {
        std::cerr << "Failed to start server: " << result.error().message << "\n";
        g_server = nullptr;
        return false;
    }

    std::cout << "=================================================\n";
    std::cout << " Secure Echo SCP is running on port " << port << " (TLS)\n";
    std::cout << " Press Ctrl+C to stop\n";
    std::cout << "=================================================\n\n";

    // Wait for shutdown
    server.wait_for_shutdown();

    // Print final statistics
    auto stats = server.get_statistics();
    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << " Server Statistics (TLS-Secured)\n";
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
  ____  _____ ____ _   _ ____  _____   _____ ____ ____
 / ___|| ____/ ___| | | |  _ \| ____| | ____/ ___/ ___|
 \___ \|  _|| |   | | | | |_) |  _|   |  _|| |   \___ \
  ___) | |__| |___| |_| |  _ <| |___  | |__| |___ ___) |
 |____/|_____\____|\___/|_| \_\_____| |_____\____|____/
  ____   ____ ____
 / ___| / ___|  _ \
 \___ \| |   | |_) |
  ___) | |___|  __/
 |____/ \____|_|

        TLS-Secured DICOM Connectivity Test Server
)" << "\n";

    uint16_t port = 0;
    std::string ae_title;
    tls_options tls;
    size_t max_associations = 0;
    uint32_t idle_timeout = 0;

    if (!parse_arguments(argc, argv, port, ae_title, tls, max_associations, idle_timeout)) {
        print_usage(argv[0]);
        return 1;
    }

    // Validate TLS files exist
    if (!validate_tls_files(tls)) {
        return 1;
    }

    // Install signal handlers
    install_signal_handlers();

    bool success = run_server(port, ae_title, tls, max_associations, idle_timeout);

    std::cout << "\nSecure Echo SCP terminated\n";
    return success ? 0 : 1;
}
