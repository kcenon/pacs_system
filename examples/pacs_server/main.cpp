/**
 * @file main.cpp
 * @brief Entry point for PACS Server sample application
 *
 * A complete DICOM archive server demonstrating all PACS functionality
 * including storage, query/retrieve, worklist, and MPPS services.
 *
 * @see Issue #106 - Full PACS Server Sample
 *
 * Usage:
 *   pacs_server [OPTIONS]
 *
 * Options:
 *   --port <port>           Port to listen on (default: 11112)
 *   --ae-title <title>      AE title (default: MY_PACS)
 *   --storage-dir <path>    Storage directory (default: ./archive)
 *   --db-path <path>        Database path (default: ./pacs.db)
 *   --log-level <level>     Log level (default: info)
 *   --max-associations <n>  Max concurrent associations (default: 50)
 *   --help                  Show help message
 */

#include "config.hpp"
#include "server_app.hpp"

#include <atomic>
#include <csignal>
#include <iostream>

namespace {

/// Global pointer to server app for signal handling
std::atomic<pacs::example::pacs_server_app*> g_server{nullptr};

/// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";

    auto* server = g_server.load();
    if (server) {
        server->request_shutdown();
    }
}

/// Install signal handlers
void install_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifndef _WIN32
    std::signal(SIGHUP, signal_handler);
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____   _    ____ ____    ____
 |  _ \ / \  / ___/ ___|  / ___|  ___ _ ____   _____ _ __
 | |_) / _ \| |   \___ \  \___ \ / _ \ '__\ \ / / _ \ '__|
 |  __/ ___ \ |___ ___) |  ___) |  __/ |   \ V /  __/ |
 |_| /_/   \_\____|____/  |____/ \___|_|    \_/ \___|_|

                Complete DICOM Archive Server
)" << "\n";

    // Parse command line arguments
    auto config = pacs::example::pacs_server_config::parse_args(argc, argv);
    if (!config) {
        return 1;
    }

    // Install signal handlers
    install_signal_handlers();

    // Create and initialize server
    pacs::example::pacs_server_app server(config.value());
    g_server = &server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize PACS server\n";
        return 1;
    }

    // Start server
    if (!server.start()) {
        std::cerr << "Failed to start PACS server\n";
        return 1;
    }

    // Wait for shutdown
    server.wait_for_shutdown();

    // Print final statistics
    server.print_statistics();

    g_server = nullptr;

    std::cout << "PACS Server terminated\n";
    return 0;
}
