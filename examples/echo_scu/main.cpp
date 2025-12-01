/**
 * @file main.cpp
 * @brief Echo SCU - DICOM Connectivity Test Client
 *
 * A simple command-line utility for testing DICOM network connectivity
 * using the C-ECHO service (equivalent to "ping" for DICOM).
 *
 * @see Issue #99 - Echo SCU/SCP Sample
 * @see DICOM PS3.7 Section 9.1 - C-ECHO Service
 *
 * Usage:
 *   echo_scu <host> <port> <called_ae> [calling_ae]
 *
 * Example:
 *   echo_scu localhost 11112 PACS_SCP MY_SCU
 */

#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/verification_scp.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

/// Default calling AE title when not specified
constexpr const char* default_calling_ae = "ECHO_SCU";

/// Default timeout for operations (30 seconds)
constexpr auto default_timeout = std::chrono::milliseconds{30000};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Echo SCU - DICOM Connectivity Test Client

Usage: )" << program_name << R"( <host> <port> <called_ae> [calling_ae]

Arguments:
  host        Remote host address (IP or hostname)
  port        Remote port number (typically 104 or 11112)
  called_ae   Called AE Title (remote SCP's AE title)
  calling_ae  Calling AE Title (optional, default: ECHO_SCU)

Examples:
  )" << program_name << R"( localhost 11112 PACS_SCP
  )" << program_name << R"( 192.168.1.100 104 REMOTE_PACS MY_WORKSTATION

Exit Codes:
  0  Success - Echo response received
  1  Error - Connection or echo failed
)";
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param host Output: remote host
 * @param port Output: remote port
 * @param called_ae Output: called AE title
 * @param calling_ae Output: calling AE title
 * @return true if arguments are valid
 */
bool parse_arguments(
    int argc,
    char* argv[],
    std::string& host,
    uint16_t& port,
    std::string& called_ae,
    std::string& calling_ae) {

    if (argc < 4 || argc > 5) {
        return false;
    }

    host = argv[1];

    // Parse port
    try {
        int port_int = std::stoi(argv[2]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[2] << "'\n";
        return false;
    }

    called_ae = argv[3];

    // Validate AE title length (max 16 characters)
    if (called_ae.length() > 16) {
        std::cerr << "Error: Called AE title exceeds 16 characters\n";
        return false;
    }

    // Optional calling AE
    if (argc == 5) {
        calling_ae = argv[4];
        if (calling_ae.length() > 16) {
            std::cerr << "Error: Calling AE title exceeds 16 characters\n";
            return false;
        }
    } else {
        calling_ae = default_calling_ae;
    }

    return true;
}

/**
 * @brief Perform C-ECHO to remote SCP
 * @param host Remote host
 * @param port Remote port
 * @param called_ae Called AE title
 * @param calling_ae Calling AE title
 * @return true if echo succeeded
 */
bool perform_echo(
    const std::string& host,
    uint16_t port,
    const std::string& called_ae,
    const std::string& calling_ae) {

    using namespace pacs::network;
    using namespace pacs::network::dimse;
    using namespace pacs::services;

    std::cout << "Connecting to " << host << ":" << port << "...\n";
    std::cout << "  Calling AE:  " << calling_ae << "\n";
    std::cout << "  Called AE:   " << called_ae << "\n\n";

    // Configure association
    association_config config;
    config.calling_ae_title = calling_ae;
    config.called_ae_title = called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "ECHO_SCU_001";

    // Propose Verification SOP Class with Explicit VR Little Endian
    config.proposed_contexts.push_back({
        1,  // Context ID (must be odd: 1, 3, 5, ...)
        std::string(verification_sop_class_uid),
        {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        }
    });

    // Establish association
    auto start_time = std::chrono::steady_clock::now();
    auto connect_result = association::connect(host, port, config, default_timeout);

    if (connect_result.is_err()) {
        std::cerr << "Failed to establish association: "
                  << connect_result.error().message << "\n";

        // Check for rejection info
        return false;
    }

    auto& assoc = connect_result.value();
    auto connect_time = std::chrono::steady_clock::now();
    auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        connect_time - start_time);

    std::cout << "Association established in " << connect_duration.count() << " ms\n";

    // Verify we have an accepted context for Verification
    if (!assoc.has_accepted_context(verification_sop_class_uid)) {
        std::cerr << "Error: Verification SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return false;
    }

    // Get the accepted context ID
    auto context_id_opt = assoc.accepted_context_id(verification_sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return false;
    }
    uint8_t context_id = *context_id_opt;

    // Create C-ECHO request
    auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);

    std::cout << "Sending C-ECHO request...\n";

    // Send C-ECHO request
    auto send_result = assoc.send_dimse(context_id, echo_rq);
    if (send_result.is_err()) {
        std::cerr << "Failed to send C-ECHO: " << send_result.error().message << "\n";
        assoc.abort();
        return false;
    }

    // Receive C-ECHO response
    auto recv_result = assoc.receive_dimse(default_timeout);
    if (recv_result.is_err()) {
        std::cerr << "Failed to receive C-ECHO response: "
                  << recv_result.error().message << "\n";
        assoc.abort();
        return false;
    }

    auto& [recv_context_id, echo_rsp] = recv_result.value();

    auto echo_time = std::chrono::steady_clock::now();
    auto echo_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        echo_time - connect_time);

    // Check response
    if (echo_rsp.command() != command_field::c_echo_rsp) {
        std::cerr << "Error: Unexpected response (expected C-ECHO-RSP)\n";
        assoc.abort();
        return false;
    }

    auto status = echo_rsp.status();
    if (status != status_success) {
        std::cerr << "C-ECHO failed with status: 0x"
                  << std::hex << static_cast<uint16_t>(status) << std::dec << "\n";
        (void)assoc.release();
        return false;
    }

    std::cout << "C-ECHO successful! Round-trip time: " << echo_duration.count() << " ms\n";

    // Release association gracefully
    std::cout << "Releasing association...\n";
    auto release_result = assoc.release(default_timeout);
    if (release_result.is_err()) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
        // Still consider this a success since echo worked
    }

    auto total_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        total_time - start_time);

    std::cout << "\nSummary:\n";
    std::cout << "  Remote AE:        " << called_ae << "\n";
    std::cout << "  Connection time:  " << connect_duration.count() << " ms\n";
    std::cout << "  Echo time:        " << echo_duration.count() << " ms\n";
    std::cout << "  Total time:       " << total_duration.count() << " ms\n";
    std::cout << "  Status:           SUCCESS\n";

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  _____ ____ _   _  ___    ____   ____ _   _
 | ____/ ___| | | |/ _ \  / ___| / ___| | | |
 |  _|| |   | |_| | | | | \___ \| |   | | | |
 | |__| |___|  _  | |_| |  ___) | |___| |_| |
 |_____\____|_| |_|\___/  |____/ \____|\___/

        DICOM Connectivity Test Client
)" << "\n";

    std::string host;
    uint16_t port = 0;
    std::string called_ae;
    std::string calling_ae;

    if (!parse_arguments(argc, argv, host, port, called_ae, calling_ae)) {
        print_usage(argv[0]);
        return 1;
    }

    bool success = perform_echo(host, port, called_ae, calling_ae);

    return success ? 0 : 1;
}
