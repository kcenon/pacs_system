/**
 * @file main.cpp
 * @brief Echo SCU - DICOM Connectivity Test Client (dcmtk-style)
 *
 * A command-line utility for testing DICOM network connectivity
 * using the C-ECHO service (equivalent to "ping" for DICOM).
 * Provides dcmtk-compatible interface with extended features.
 *
 * @see Issue #287 - echo_scu: Implement C-ECHO SCU utility (Verification)
 * @see Issue #286 - [Epic] PACS SCU/SCP Utilities Implementation
 * @see DICOM PS3.7 Section 9.1 - C-ECHO Service
 * @see https://support.dcmtk.org/docs/echoscu.html
 *
 * Usage:
 *   echo_scu [options] <peer> <port>
 *
 * Example:
 *   echo_scu localhost 11112
 *   echo_scu -aet MYSCU -aec PACS localhost 11112
 *   echo_scu --repeat 10 --repeat-delay 1000 localhost 11112
 */

#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/verification_scp.hpp"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// =============================================================================
// Constants
// =============================================================================

/// Version information
constexpr const char* version_string = "1.0.0";

/// Default calling AE title when not specified
constexpr const char* default_calling_ae = "ECHOSCU";

/// Default called AE title when not specified
constexpr const char* default_called_ae = "ANY-SCP";

/// Default timeout for connections (30 seconds)
constexpr auto default_connection_timeout = std::chrono::seconds{30};

/// Default ACSE timeout (30 seconds)
constexpr auto default_acse_timeout = std::chrono::seconds{30};

/// Default DIMSE timeout (0 = infinite)
constexpr auto default_dimse_timeout = std::chrono::seconds{0};

/// Maximum AE title length
constexpr size_t max_ae_title_length = 16;

// =============================================================================
// Output Modes
// =============================================================================

/**
 * @brief Output verbosity level
 */
enum class verbosity_level {
    quiet,      ///< Minimal output
    normal,     ///< Standard output
    verbose,    ///< Verbose output
    debug       ///< Debug output with all details
};

// =============================================================================
// Command Line Options
// =============================================================================

/**
 * @brief Command line options structure
 */
struct options {
    // Network options
    std::string peer_host;
    uint16_t peer_port{0};
    std::string calling_ae_title{default_calling_ae};
    std::string called_ae_title{default_called_ae};

    // Timeout options
    std::chrono::seconds connection_timeout{default_connection_timeout};
    std::chrono::seconds acse_timeout{default_acse_timeout};
    std::chrono::seconds dimse_timeout{default_dimse_timeout};

    // Repeat options
    int repeat_count{1};
    std::chrono::milliseconds repeat_delay{0};

    // Output options
    verbosity_level verbosity{verbosity_level::normal};

    // TLS options (for future extension)
    bool use_tls{false};
    std::string tls_cert_file;
    std::string tls_key_file;
    std::string tls_ca_file;

    // Help/version flags
    bool show_help{false};
    bool show_version{false};
};

/**
 * @brief Result of a single echo operation
 */
struct echo_result {
    bool success{false};
    uint16_t status_code{0};
    std::chrono::milliseconds association_time{0};
    std::chrono::milliseconds echo_time{0};
    std::chrono::milliseconds total_time{0};
    std::string error_message;
};

/**
 * @brief Statistics for multiple echo operations
 */
struct echo_statistics {
    int total_attempts{0};
    int successful{0};
    int failed{0};
    std::vector<std::chrono::milliseconds> response_times;

    [[nodiscard]] double success_rate() const {
        return total_attempts > 0
            ? (static_cast<double>(successful) / total_attempts) * 100.0
            : 0.0;
    }

    [[nodiscard]] std::chrono::milliseconds min_time() const {
        if (response_times.empty()) return std::chrono::milliseconds{0};
        return *std::min_element(response_times.begin(), response_times.end());
    }

    [[nodiscard]] std::chrono::milliseconds max_time() const {
        if (response_times.empty()) return std::chrono::milliseconds{0};
        return *std::max_element(response_times.begin(), response_times.end());
    }

    [[nodiscard]] std::chrono::milliseconds avg_time() const {
        if (response_times.empty()) return std::chrono::milliseconds{0};
        auto sum = std::accumulate(
            response_times.begin(), response_times.end(),
            std::chrono::milliseconds{0});
        return sum / static_cast<int>(response_times.size());
    }
};

// =============================================================================
// Output Functions
// =============================================================================

/**
 * @brief Print banner
 */
void print_banner() {
    std::cout << R"(
  _____ ____ _   _  ___    ____   ____ _   _
 | ____/ ___| | | |/ _ \  / ___| / ___| | | |
 |  _|| |   | |_| | | | | \___ \| |   | | | |
 | |__| |___|  _  | |_| |  ___) | |___| |_| |
 |_____\____|_| |_|\___/  |____/ \____|\___/

        DICOM Connectivity Test Client v)" << version_string << R"(
)" << "\n";
}

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << R"( [options] <peer> <port>

Arguments:
  peer                          Remote host address (IP or hostname)
  port                          Remote port number (typically 104 or 11112)

Options:
  -h, --help                    Show this help message and exit
  -v, --verbose                 Verbose output mode
  -d, --debug                   Debug output mode (more details than verbose)
  -q, --quiet                   Quiet mode (minimal output)
  --version                     Show version information

Network Options:
  -aet, --aetitle <aetitle>     Calling AE Title (default: ECHOSCU)
  -aec, --call <aetitle>        Called AE Title (default: ANY-SCP)
  -to, --timeout <seconds>      Connection timeout (default: 30)
  -ta, --acse-timeout <seconds> ACSE timeout (default: 30)
  -td, --dimse-timeout <seconds> DIMSE timeout (default: 0=infinite)

Repeat Options:
  -r, --repeat <count>          Repeat echo request n times (default: 1)
  --repeat-delay <ms>           Delay between repeats in milliseconds (default: 0)

TLS Options (not yet implemented):
  --tls                         Enable TLS connection
  --tls-cert <file>             TLS certificate file
  --tls-key <file>              TLS private key file
  --tls-ca <file>               TLS CA certificate file

Examples:
  # Basic echo test
  )" << program_name << R"( localhost 11112

  # With custom AE Titles
  )" << program_name << R"( -aet MYSCU -aec PACS localhost 11112

  # Repeat test for connectivity monitoring
  )" << program_name << R"( -r 10 --repeat-delay 1000 localhost 11112

  # Verbose output with timeout
  )" << program_name << R"( -v -to 60 192.168.1.100 104

Exit Codes:
  0  Success - All echo responses received
  1  Error - Echo failed or partial failure
  2  Error - Invalid arguments
)";
}

/**
 * @brief Print version information
 */
void print_version() {
    std::cout << "echo_scu version " << version_string << "\n";
    std::cout << "PACS System DICOM Utilities\n";
    std::cout << "Copyright (c) 2024\n";
}

// =============================================================================
// Argument Parsing
// =============================================================================

/**
 * @brief Parse timeout value from string
 * @param value String value
 * @param result Output timeout value
 * @param option_name Name of the option for error messages
 * @return true if parsing succeeded
 */
bool parse_timeout(const std::string& value, std::chrono::seconds& result,
                   const std::string& option_name) {
    try {
        int seconds = std::stoi(value);
        if (seconds < 0) {
            std::cerr << "Error: " << option_name << " must be non-negative\n";
            return false;
        }
        result = std::chrono::seconds{seconds};
        return true;
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid value for " << option_name << ": '"
                  << value << "'\n";
        return false;
    }
}

/**
 * @brief Parse integer value from string
 * @param value String value
 * @param result Output integer value
 * @param option_name Name of the option for error messages
 * @param min_value Minimum allowed value
 * @return true if parsing succeeded
 */
bool parse_int(const std::string& value, int& result,
               const std::string& option_name, int min_value = 0) {
    try {
        result = std::stoi(value);
        if (result < min_value) {
            std::cerr << "Error: " << option_name << " must be at least "
                      << min_value << "\n";
            return false;
        }
        return true;
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid value for " << option_name << ": '"
                  << value << "'\n";
        return false;
    }
}

/**
 * @brief Validate AE title
 * @param ae_title AE title to validate
 * @param option_name Name of the option for error messages
 * @return true if valid
 */
bool validate_ae_title(const std::string& ae_title,
                       const std::string& option_name) {
    if (ae_title.empty()) {
        std::cerr << "Error: " << option_name << " cannot be empty\n";
        return false;
    }
    if (ae_title.length() > max_ae_title_length) {
        std::cerr << "Error: " << option_name << " exceeds "
                  << max_ae_title_length << " characters\n";
        return false;
    }
    return true;
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param opts Output: parsed options
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    std::vector<std::string> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Help options
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return true;
        }
        if (arg == "--version") {
            opts.show_version = true;
            return true;
        }

        // Verbosity options
        if (arg == "-v" || arg == "--verbose") {
            opts.verbosity = verbosity_level::verbose;
            continue;
        }
        if (arg == "-d" || arg == "--debug") {
            opts.verbosity = verbosity_level::debug;
            continue;
        }
        if (arg == "-q" || arg == "--quiet") {
            opts.verbosity = verbosity_level::quiet;
            continue;
        }

        // Network options with values
        if ((arg == "-aet" || arg == "--aetitle") && i + 1 < argc) {
            opts.calling_ae_title = argv[++i];
            if (!validate_ae_title(opts.calling_ae_title, "Calling AE Title")) {
                return false;
            }
            continue;
        }
        if ((arg == "-aec" || arg == "--call") && i + 1 < argc) {
            opts.called_ae_title = argv[++i];
            if (!validate_ae_title(opts.called_ae_title, "Called AE Title")) {
                return false;
            }
            continue;
        }

        // Timeout options
        if ((arg == "-to" || arg == "--timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], opts.connection_timeout,
                               "Connection timeout")) {
                return false;
            }
            continue;
        }
        if ((arg == "-ta" || arg == "--acse-timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], opts.acse_timeout, "ACSE timeout")) {
                return false;
            }
            continue;
        }
        if ((arg == "-td" || arg == "--dimse-timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], opts.dimse_timeout, "DIMSE timeout")) {
                return false;
            }
            continue;
        }

        // Repeat options
        if ((arg == "-r" || arg == "--repeat") && i + 1 < argc) {
            if (!parse_int(argv[++i], opts.repeat_count, "Repeat count", 1)) {
                return false;
            }
            continue;
        }
        if (arg == "--repeat-delay" && i + 1 < argc) {
            int delay_ms = 0;
            if (!parse_int(argv[++i], delay_ms, "Repeat delay", 0)) {
                return false;
            }
            opts.repeat_delay = std::chrono::milliseconds{delay_ms};
            continue;
        }

        // TLS options
        if (arg == "--tls") {
            opts.use_tls = true;
            continue;
        }
        if (arg == "--tls-cert" && i + 1 < argc) {
            opts.tls_cert_file = argv[++i];
            continue;
        }
        if (arg == "--tls-key" && i + 1 < argc) {
            opts.tls_key_file = argv[++i];
            continue;
        }
        if (arg == "--tls-ca" && i + 1 < argc) {
            opts.tls_ca_file = argv[++i];
            continue;
        }

        // Check for unknown options
        if (arg.starts_with("-")) {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }

        // Positional arguments
        positional_args.push_back(arg);
    }

    // Validate positional arguments
    if (positional_args.size() != 2) {
        std::cerr << "Error: Expected <peer> <port> arguments\n";
        return false;
    }

    opts.peer_host = positional_args[0];

    // Parse port
    try {
        int port_int = std::stoi(positional_args[1]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        opts.peer_port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << positional_args[1]
                  << "'\n";
        return false;
    }

    // TLS validation
    if (opts.use_tls) {
        std::cerr << "Warning: TLS support is not yet implemented\n";
    }

    return true;
}

// =============================================================================
// Echo Implementation
// =============================================================================

/**
 * @brief Perform a single C-ECHO operation
 * @param opts Command line options
 * @return Echo result
 */
echo_result perform_single_echo(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;
    using namespace pacs::services;

    echo_result result;
    auto start_time = std::chrono::steady_clock::now();

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae_title;
    config.called_ae_title = opts.called_ae_title;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "ECHO_SCU_100";

    // Propose Verification SOP Class
    config.proposed_contexts.push_back({
        1,  // Context ID (must be odd: 1, 3, 5, ...)
        std::string(verification_sop_class_uid),
        {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        }
    });

    // Establish association
    auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        opts.connection_timeout);
    auto connect_result = association::connect(
        opts.peer_host, opts.peer_port, config, timeout);

    if (connect_result.is_err()) {
        result.error_message = "Connection failed: " +
                               std::string(connect_result.error().message);
        return result;
    }

    auto& assoc = connect_result.value();
    auto connect_time = std::chrono::steady_clock::now();
    result.association_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        connect_time - start_time);

    // Verify we have an accepted context for Verification
    if (!assoc.has_accepted_context(verification_sop_class_uid)) {
        result.error_message = "Verification SOP Class not accepted by remote SCP";
        assoc.abort();
        return result;
    }

    // Get the accepted context ID
    auto context_id_opt = assoc.accepted_context_id(verification_sop_class_uid);
    if (!context_id_opt) {
        result.error_message = "Could not get presentation context ID";
        assoc.abort();
        return result;
    }
    uint8_t context_id = *context_id_opt;

    // Create and send C-ECHO request
    auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);

    auto send_result = assoc.send_dimse(context_id, echo_rq);
    if (send_result.is_err()) {
        result.error_message = "Send failed: " +
                               std::string(send_result.error().message);
        assoc.abort();
        return result;
    }

    // Receive C-ECHO response
    auto dimse_timeout = opts.dimse_timeout.count() > 0
        ? std::chrono::duration_cast<std::chrono::milliseconds>(opts.dimse_timeout)
        : std::chrono::milliseconds{30000};  // Default if 0

    auto recv_result = assoc.receive_dimse(dimse_timeout);
    if (recv_result.is_err()) {
        result.error_message = "Receive failed: " +
                               std::string(recv_result.error().message);
        assoc.abort();
        return result;
    }

    auto echo_time = std::chrono::steady_clock::now();
    result.echo_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        echo_time - connect_time);

    auto& [recv_context_id, echo_rsp] = recv_result.value();

    // Check response
    if (echo_rsp.command() != command_field::c_echo_rsp) {
        result.error_message = "Unexpected response (expected C-ECHO-RSP)";
        assoc.abort();
        return result;
    }

    result.status_code = static_cast<uint16_t>(echo_rsp.status());

    if (echo_rsp.status() != status_success) {
        std::ostringstream oss;
        oss << "C-ECHO failed with status: 0x" << std::hex << result.status_code;
        result.error_message = oss.str();
        (void)assoc.release();
        return result;
    }

    // Release association gracefully
    (void)assoc.release(timeout);

    auto end_time = std::chrono::steady_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    result.success = true;
    return result;
}

/**
 * @brief Perform echo operations with repeat support
 * @param opts Command line options
 * @return Exit code (0 = success, 1 = failure)
 */
int perform_echo(const options& opts) {
    echo_statistics stats;
    bool is_quiet = opts.verbosity == verbosity_level::quiet;
    bool is_verbose = opts.verbosity == verbosity_level::verbose ||
                      opts.verbosity == verbosity_level::debug;

    // Print connection info
    if (!is_quiet) {
        std::cout << "Requesting Association with "
                  << opts.peer_host << ":" << opts.peer_port << "\n";
        std::cout << "  Calling AE Title: " << opts.calling_ae_title << "\n";
        std::cout << "  Called AE Title:  " << opts.called_ae_title << "\n";

        if (is_verbose) {
            std::cout << "  Connection Timeout: "
                      << opts.connection_timeout.count() << "s\n";
            std::cout << "  ACSE Timeout:       "
                      << opts.acse_timeout.count() << "s\n";
            std::cout << "  DIMSE Timeout:      "
                      << (opts.dimse_timeout.count() == 0
                              ? "infinite"
                              : std::to_string(opts.dimse_timeout.count()) + "s")
                      << "\n";
        }

        if (opts.repeat_count > 1) {
            std::cout << "  Repeat Count:       " << opts.repeat_count << "\n";
            std::cout << "  Repeat Delay:       "
                      << opts.repeat_delay.count() << " ms\n";
        }
        std::cout << "\n";
    }

    // Perform echo operations
    for (int i = 0; i < opts.repeat_count; ++i) {
        stats.total_attempts++;

        if (!is_quiet && opts.repeat_count > 1) {
            std::cout << "Echo " << (i + 1) << "/" << opts.repeat_count << ": ";
            std::cout.flush();
        }

        auto result = perform_single_echo(opts);

        if (result.success) {
            stats.successful++;
            stats.response_times.push_back(result.echo_time);

            if (!is_quiet) {
                if (opts.repeat_count > 1) {
                    std::cout << "Success (";
                    std::cout << result.echo_time.count() << " ms)\n";
                } else {
                    std::cout << "Association Accepted\n";
                    std::cout << "Sending Echo Request (Message ID: 1)\n";
                    std::cout << "Received Echo Response (Status: Success)\n";
                    std::cout << "Releasing Association\n";
                    std::cout << "Echo Successful\n";
                }

                if (is_verbose && opts.repeat_count == 1) {
                    std::cout << "\nStatistics:\n";
                    std::cout << "  Association Time: "
                              << result.association_time.count() << " ms\n";
                    std::cout << "  Echo Response Time: "
                              << result.echo_time.count() << " ms\n";
                    std::cout << "  Total Time: "
                              << result.total_time.count() << " ms\n";
                }
            }
        } else {
            stats.failed++;

            if (!is_quiet) {
                if (opts.repeat_count > 1) {
                    std::cout << "Failed: " << result.error_message << "\n";
                } else {
                    std::cerr << "Echo Failed: " << result.error_message << "\n";
                }
            }
        }

        // Delay between repeats
        if (i < opts.repeat_count - 1 && opts.repeat_delay.count() > 0) {
            std::this_thread::sleep_for(opts.repeat_delay);
        }
    }

    // Print summary for multiple echo operations
    if (!is_quiet && opts.repeat_count > 1) {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "              Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Total Attempts:  " << stats.total_attempts << "\n";
        std::cout << "  Successful:      " << stats.successful << "\n";
        std::cout << "  Failed:          " << stats.failed << "\n";
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "  Success Rate:    " << stats.success_rate() << "%\n";

        if (stats.successful > 0) {
            std::cout << "\nResponse Times:\n";
            std::cout << "  Min:             " << stats.min_time().count() << " ms\n";
            std::cout << "  Max:             " << stats.max_time().count() << " ms\n";
            std::cout << "  Avg:             " << stats.avg_time().count() << " ms\n";
        }
        std::cout << "========================================\n";
    }

    // Return appropriate exit code
    if (stats.failed == 0) {
        if (!is_quiet) {
            std::cout << "Status: SUCCESS\n";
        }
        return 0;
    } else if (stats.successful > 0) {
        if (!is_quiet) {
            std::cout << "Status: PARTIAL FAILURE\n";
        }
        return 1;
    } else {
        if (!is_quiet) {
            std::cout << "Status: FAILURE\n";
        }
        return 1;
    }
}

}  // namespace

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        if (!opts.show_help && !opts.show_version) {
            std::cerr << "\nUse --help for usage information.\n";
            return 2;
        }
    }

    if (opts.show_version) {
        print_version();
        return 0;
    }

    if (opts.show_help) {
        print_banner();
        print_usage(argv[0]);
        return 0;
    }

    // Print banner unless quiet mode
    if (opts.verbosity != verbosity_level::quiet) {
        print_banner();
    }

    return perform_echo(opts);
}
