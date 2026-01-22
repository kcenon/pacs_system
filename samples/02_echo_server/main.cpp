/**
 * @file main.cpp
 * @brief Level 2 Sample: Echo Server - DICOM network communication fundamentals
 *
 * This sample demonstrates DICOM networking concepts:
 * - Server configuration (AE Title, port, timeouts)
 * - Association management (connection, negotiation, release)
 * - Service Class Provider (SCP) pattern
 * - C-ECHO operation (Verification service)
 *
 * After completing this sample, you will understand how to:
 * 1. Configure a DICOM server with proper network parameters
 * 2. Register SCP services to handle DIMSE requests
 * 3. Set up callbacks for association lifecycle events
 * 4. Implement graceful shutdown handling
 *
 * @see DICOM PS3.7 Section 9.1 - C-ECHO Service
 * @see DICOM PS3.8 - Network Communication Support
 */

#include "console_utils.hpp"
#include "signal_handler.hpp"

#include <pacs/network/dicom_server.hpp>
#include <pacs/network/server_config.hpp>
#include <pacs/network/association.hpp>
#include <pacs/services/verification_scp.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace net = pacs::network;
namespace svc = pacs::services;

namespace {

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
    oss << std::put_time(std::localtime(&time_t_now), "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

}  // namespace

int main(int argc, char* argv[]) {
    pacs::samples::print_header("Echo Server - Level 2 Sample");

    // ===========================================================================
    // Part 1: Server Configuration
    // ===========================================================================
    // DICOM servers require several network parameters:
    // - AE Title: Application Entity identifier (max 16 characters)
    //   This is like a hostname in DICOM networking
    // - Port: TCP port to listen on (11112 is a common non-privileged port)
    // - Max PDU Length: Maximum Protocol Data Unit size for data transfer
    // - Timeouts: Connection and idle timeouts for resource management
    // - Max Associations: Limit concurrent connections to prevent overload

    pacs::samples::print_section("Part 1: Server Configuration");

    std::cout << "DICOM servers use these key parameters:\n";
    std::cout << "  - AE Title:    Application Entity identifier (like hostname)\n";
    std::cout << "  - Port:        TCP listening port\n";
    std::cout << "  - Max PDU:     Maximum data unit size per transfer\n";
    std::cout << "  - Timeouts:    Connection and idle timeouts\n\n";

    // Parse optional port argument
    uint16_t port = 11112;  // Default port
    if (argc > 1) {
        try {
            int port_arg = std::stoi(argv[1]);
            if (port_arg > 0 && port_arg < 65536) {
                port = static_cast<uint16_t>(port_arg);
            }
        } catch (const std::exception&) {
            // Keep default port if parsing fails
        }
    }

    // Create server configuration
    net::server_config config;
    config.ae_title = "ECHO_SCP";                          // Our AE Title
    config.port = port;                                     // Listening port
    config.max_associations = 10;                           // Concurrent connections
    config.idle_timeout = std::chrono::seconds(60);        // Idle disconnect
    config.max_pdu_size = 16384;                           // 16KB PDU size

    // Implementation identification (optional but recommended for interoperability)
    // The Implementation Class UID uniquely identifies your software implementation
    config.implementation_class_uid = "1.2.410.200001.1.1";
    config.implementation_version_name = "PACS_SAMPLE_2.0";

    pacs::samples::print_table("Server Configuration", {
        {"AE Title", config.ae_title},
        {"Port", std::to_string(config.port)},
        {"Max Associations", std::to_string(config.max_associations)},
        {"Idle Timeout", std::to_string(config.idle_timeout.count()) + " seconds"},
        {"Max PDU Size", std::to_string(config.max_pdu_size) + " bytes"}
    });

    pacs::samples::print_success("Part 1 complete - Configuration ready!");

    // ===========================================================================
    // Part 2: Create Server and Register Services
    // ===========================================================================
    // DICOM servers host one or more Service Class Providers (SCPs).
    // Each SCP handles specific DIMSE (DICOM Message Service Element) operations:
    // - Verification SCP: Handles C-ECHO (connectivity test)
    // - Storage SCP: Handles C-STORE (image storage)
    // - Query SCP: Handles C-FIND (data query)
    // - Retrieve SCP: Handles C-GET/C-MOVE (data retrieval)
    //
    // For this sample, we only register the Verification SCP to handle C-ECHO.
    // C-ECHO is the simplest DICOM service - like a "ping" for DICOM.

    pacs::samples::print_section("Part 2: Server & Service Setup");

    std::cout << "DICOM Service Class Provider (SCP) types:\n";
    std::cout << "  - Verification SCP: C-ECHO (connectivity test)\n";
    std::cout << "  - Storage SCP:      C-STORE (receive images)\n";
    std::cout << "  - Query SCP:        C-FIND (search data)\n";
    std::cout << "  - Retrieve SCP:     C-GET/C-MOVE (retrieve data)\n\n";

    // Create server instance
    auto server = std::make_unique<net::dicom_server>(config);

    // Register Verification SCP (C-ECHO handler)
    // The Verification SOP Class UID is: 1.2.840.10008.1.1
    // This is the standard UID for the Verification service
    auto verification_service = std::make_shared<svc::verification_scp>();
    server->register_service(verification_service);

    std::cout << "Registered services:\n";
    for (const auto& sop_class : server->supported_sop_classes()) {
        if (sop_class == "1.2.840.10008.1.1") {
            std::cout << "  - Verification SCP (1.2.840.10008.1.1)\n";
        } else {
            std::cout << "  - " << sop_class << "\n";
        }
    }

    pacs::samples::print_success("Part 2 complete - Verification SCP registered!");

    // ===========================================================================
    // Part 3: Event Callbacks
    // ===========================================================================
    // DICOM associations go through a lifecycle:
    // 1. Association Request (A-ASSOCIATE-RQ): Client requests connection
    // 2. Association Accept (A-ASSOCIATE-AC): Server accepts connection
    // 3. DIMSE Exchange: Client sends C-ECHO-RQ, server responds C-ECHO-RSP
    // 4. Association Release (A-RELEASE): Clean disconnection
    //
    // The server provides callbacks for monitoring these events.
    // This is useful for logging, statistics, and debugging.

    pacs::samples::print_section("Part 3: Event Handlers");

    std::cout << "DICOM Association lifecycle:\n";
    std::cout << "  1. A-ASSOCIATE-RQ  -> Client requests connection\n";
    std::cout << "  2. A-ASSOCIATE-AC  -> Server accepts\n";
    std::cout << "  3. DIMSE exchange  -> C-ECHO, C-STORE, etc.\n";
    std::cout << "  4. A-RELEASE       -> Clean disconnection\n\n";

    // Track connection statistics
    std::atomic<int> connection_count{0};
    std::atomic<int> total_connections{0};

    // Set up association established callback
    // Called when a new association is successfully negotiated
    server->on_association_established(
        [&connection_count, &total_connections](const net::association& assoc) {
            connection_count++;
            total_connections++;
            std::cout << "[" << current_timestamp() << "] "
                      << pacs::samples::colors::green << "[CONNECT]"
                      << pacs::samples::colors::reset << " "
                      << assoc.calling_ae() << " -> " << assoc.called_ae()
                      << " (active: " << connection_count << ")\n";
        }
    );

    // Set up association released callback
    // Called when an association is gracefully released
    server->on_association_released(
        [&connection_count](const net::association& assoc) {
            connection_count--;
            std::cout << "[" << current_timestamp() << "] "
                      << pacs::samples::colors::cyan << "[RELEASE]"
                      << pacs::samples::colors::reset << " "
                      << assoc.calling_ae() << " disconnected"
                      << " (active: " << connection_count << ")\n";
        }
    );

    // Set up error callback
    // Called when network or protocol errors occur
    server->on_error(
        [](const std::string& error_msg) {
            std::cerr << "[" << current_timestamp() << "] "
                      << pacs::samples::colors::red << "[ERROR]"
                      << pacs::samples::colors::reset << " "
                      << error_msg << "\n";
        }
    );

    pacs::samples::print_success("Part 3 complete - Event handlers registered!");

    // ===========================================================================
    // Part 4: Start Server
    // ===========================================================================
    // The server starts listening on the configured port.
    // It runs in the background, handling associations in a thread pool.
    // The main thread waits for a shutdown signal (Ctrl+C).

    pacs::samples::print_section("Part 4: Running Server");

    // Install signal handler for graceful shutdown (Ctrl+C)
    // scoped_signal_handler automatically cleans up when destroyed
    pacs::samples::scoped_signal_handler sig_handler([&server]() {
        std::cout << "\n" << pacs::samples::colors::yellow
                  << "Shutdown signal received..."
                  << pacs::samples::colors::reset << "\n";
        server->stop();
    });

    // Start the server (non-blocking)
    auto start_result = server->start();
    if (start_result.is_err()) {
        pacs::samples::print_error("Failed to start server: " +
                                   start_result.error().message);
        return 1;
    }

    // Print server running banner
    pacs::samples::print_box({
        "Echo Server Running",
        "",
        "Test with DCMTK:",
        "  echoscu -aec ECHO_SCP localhost " + std::to_string(config.port),
        "",
        "Test multiple connections:",
        "  for i in {1..5}; do echoscu -aec ECHO_SCP localhost " +
            std::to_string(config.port) + "; done",
        "",
        "Press Ctrl+C to stop"
    });

    // Wait for shutdown signal
    // This blocks until Ctrl+C is pressed or server->stop() is called
    sig_handler.wait();

    // ===========================================================================
    // Part 5: Statistics and Cleanup
    // ===========================================================================
    // After shutdown, we can retrieve server statistics for monitoring.
    // The server automatically cleans up all resources.

    pacs::samples::print_section("Final Statistics");

    auto stats = server->get_statistics();

    pacs::samples::print_table("Server Statistics", {
        {"Total Associations", std::to_string(stats.total_associations)},
        {"Rejected Associations", std::to_string(stats.rejected_associations)},
        {"Messages Processed", std::to_string(stats.messages_processed)},
        {"Bytes Received", std::to_string(stats.bytes_received)},
        {"Bytes Sent", std::to_string(stats.bytes_sent)},
        {"Uptime", std::to_string(stats.uptime().count()) + " seconds"}
    });

    // Summary
    pacs::samples::print_box({
        "Congratulations! You have learned:",
        "",
        "1. Server Configuration - AE Title, port, timeouts",
        "2. Service Registration - SCP services for DIMSE handling",
        "3. Association Lifecycle - Connect, exchange, release",
        "4. Event Callbacks - Monitoring and logging",
        "5. Graceful Shutdown - Clean resource cleanup",
        "",
        "Next step: Level 3 - Storage Server (C-STORE SCP)"
    });

    pacs::samples::print_success("Echo Server terminated successfully.");

    return 0;
}
