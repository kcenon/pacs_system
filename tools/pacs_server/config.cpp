/**
 * @file config.cpp
 * @brief Configuration management implementation for PACS Server sample
 */

#include "config.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace kcenon::pacs::example {

void pacs_server_config::print_help() {
    std::cout << R"(
PACS Server - Complete DICOM Archive

Usage: pacs_server [OPTIONS]

Options:
  --port <port>           Port to listen on (default: 11112)
  --ae-title <title>      Application Entity title (default: MY_PACS)
  --storage-dir <path>    Storage directory for DICOM files (default: ./archive)
  --db-path <path>        SQLite database path (default: ./pacs.db)
  --log-level <level>     Log level: trace, debug, info, warning, error, critical
                          (default: info)
  --max-associations <n>  Maximum concurrent associations (default: 50)
  --peer <AE:host:port>   C-MOVE destination and N-EVENT-REPORT receiver (repeatable)
  --event-attempts <n>    Attempts per N-EVENT-REPORT on a new association (default: 3)
  --event-retry-delay <s> Seconds between those attempts (default: 5)
  --print-spool <path>    Print job directory (default: <storage-dir>.print-spool)
  --ups-dir <path>        UPS instance directory (default: <storage-dir>.ups)
  --help, -h              Show this help message

Supported DICOM Services:
  - C-ECHO (Verification)
  - C-STORE (Storage)
  - C-FIND (Query - Patient/Study Root)
  - C-MOVE/C-GET (Retrieve)
  - MWL (Modality Worklist)
  - MPPS (Modality Performed Procedure Step)

Examples:
  # Start with default settings
  pacs_server

  # Start on custom port with custom AE title
  pacs_server --port 104 --ae-title MAIN_PACS

  # Specify storage and database locations
  pacs_server --storage-dir /data/dicom --db-path /data/pacs.db

)";
}

auto pacs_server_config::parse_args(int argc, char* argv[])
    -> std::optional<pacs_server_config> {

    pacs_server_config config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_help();
            return std::nullopt;
        }

        if (arg == "--port") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --port requires a value\n";
                return std::nullopt;
            }
            try {
                config.server.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid port number\n";
                return std::nullopt;
            }
            continue;
        }

        if (arg == "--ae-title") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --ae-title requires a value\n";
                return std::nullopt;
            }
            config.server.ae_title = argv[++i];
            if (config.server.ae_title.length() > 16) {
                std::cerr << "Error: AE title must be 16 characters or less\n";
                return std::nullopt;
            }
            continue;
        }

        if (arg == "--storage-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --storage-dir requires a value\n";
                return std::nullopt;
            }
            config.storage.directory = argv[++i];
            continue;
        }

        if (arg == "--db-path") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --db-path requires a value\n";
                return std::nullopt;
            }
            config.database.path = argv[++i];
            continue;
        }

        if (arg == "--log-level") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --log-level requires a value\n";
                return std::nullopt;
            }
            config.logging.level = argv[++i];
            // Validate log level
            const std::string_view level = config.logging.level;
            if (level != "trace" && level != "debug" && level != "info" &&
                level != "warning" && level != "error" && level != "critical") {
                std::cerr << "Error: Invalid log level: " << level << "\n";
                std::cerr << "Valid levels: trace, debug, info, warning, error, critical\n";
                return std::nullopt;
            }
            continue;
        }

        if (arg == "--max-associations") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --max-associations requires a value\n";
                return std::nullopt;
            }
            try {
                config.server.max_associations = static_cast<size_t>(std::stoi(argv[++i]));
            } catch (const std::exception& e) {
                std::cerr << "Error: Invalid max-associations value\n";
                return std::nullopt;
            }
            continue;
        }

        if (arg == "--peer") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --peer requires a value\n";
                return std::nullopt;
            }
            const std::string spec = argv[++i];
            const auto first = spec.find(':');
            const auto last = spec.rfind(':');
            move_destination destination;
            if (first == std::string::npos || first == 0 || last == first || last + 1 == spec.size()) {
                std::cerr << "Error: Invalid peer format. Use AE:host:port\n";
                return std::nullopt;
            }
            destination.ae_title = spec.substr(0, first);
            destination.host = spec.substr(first + 1, last - first - 1);
            try {
                const int port = std::stoi(spec.substr(last + 1));
                if (port < 1 || port > 65535) {
                    throw std::out_of_range("port");
                }
                destination.port = static_cast<uint16_t>(port);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid peer port\n";
                return std::nullopt;
            }
            config.move_destinations.push_back(std::move(destination));
            continue;
        }

        if (arg == "--event-attempts" || arg == "--event-retry-delay") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return std::nullopt;
            }
            try {
                const int value = std::stoi(argv[++i]);
                if (value < (arg == "--event-attempts" ? 1 : 0)) {
                    throw std::out_of_range("value");
                }
                if (arg == "--event-attempts") {
                    config.event_attempts = value;
                } else {
                    config.event_retry_delay = std::chrono::seconds{value};
                }
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid value for " << arg << "\n";
                return std::nullopt;
            }
            continue;
        }

        if (arg == "--print-spool" || arg == "--ups-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a value\n";
                return std::nullopt;
            }
            (arg == "--print-spool" ? config.print_spool_directory : config.ups_directory) = argv[++i];
            continue;
        }

        std::cerr << "Error: Unknown option: " << arg << "\n";
        std::cerr << "Use --help for usage information\n";
        return std::nullopt;
    }

    return config;
}

}  // namespace kcenon::pacs::example
