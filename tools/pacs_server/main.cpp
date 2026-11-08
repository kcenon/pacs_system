/**
 * @file main.cpp
 * @brief Entry point for PACS Server sample application
 * @example pacs_server/main.cpp
 *
 * A complete DICOM archive server demonstrating all PACS functionality
 * including storage, query/retrieve, worklist, and MPPS services.
 *
 * @see Issue #106 - Full PACS Server Sample
 *
 * @code{.unparsed}
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
 *   --peer <AE:host:port>   C-MOVE destination and N-EVENT-REPORT receiver (repeatable)
 *   --event-attempts <n>    Attempts per N-EVENT-REPORT on a new association
 *   --event-retry-delay <s> Seconds between those attempts
 *   --print-spool <path>    Print job directory
 *   --ups-dir <path>        UPS instance directory
 *   --http-port <port>      REST and DICOMweb port (default: off)
 *   --http-bind <address>   Address the HTTP API listens on
 *   --http-max-body-mb <n>  Largest HTTP request body in MiB
 *   --http-cors-origin <o>  Origin named in Access-Control-Allow-Origin
 *   --http-rest-api         Also serve the REST API under /api; needs OAuth 2.0
 *   --oauth2-issuer <url>   Require OAuth 2.0 bearer tokens from this issuer on DICOMweb
 *   --oauth2-audience <id>  Audience those tokens must name
 *   --oauth2-jwks <path>    JWKS file holding the keys that sign them
 *   --help                  Show help message
 * @endcode
 */

#include "config.h"
#include "server_app.h"
#include "../common/scp_lifecycle.h"

#include <atomic>
#include <csignal>
#include <iostream>

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
    auto config = kcenon::pacs::example::pacs_server_config::parse_args(argc, argv);
    if (!config) {
        return 1;
    }

    // Install signal handlers
    kcenon::pacs::cli::install_shutdown_signals();

    // Create and initialize server
    kcenon::pacs::example::pacs_server_app server(config.value());

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
    kcenon::pacs::cli::wait_for_shutdown(server);

    // Print final statistics
    server.print_statistics();

    std::cout << "PACS Server terminated\n";
    return 0;
}
