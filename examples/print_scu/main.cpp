/**
 * @file main.cpp
 * @brief Print SCU - DICOM Print Management Client
 *
 * A command-line utility for sending print requests to a Print SCP.
 * Supports Film Session management, Film Box creation, Image Box pixel
 * data setting, print execution, and printer status queries.
 *
 * @see DICOM PS3.4 Annex H - Print Management Service Class
 * @see DICOM PS3.7 Section 10 - DIMSE-N Services
 *
 * Usage:
 *   print_scu <host> <port> <called_ae> <command> [options]
 *
 * Commands:
 *   print   Execute full print workflow (session + box + print + delete)
 *   status  Query printer status
 *
 * Examples:
 *   print_scu localhost 10400 PRINTER print --copies 1 --priority HIGH
 *   print_scu localhost 10400 PRINTER status
 */

#include "pacs/services/print_scu.hpp"

#include "pacs/network/association.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

/// Default calling AE title
constexpr const char* default_calling_ae = "PRINT_SCU";

/// Default timeout (30 seconds)
constexpr auto default_timeout = std::chrono::milliseconds{30000};

/**
 * @brief Command type enumeration
 */
enum class print_command {
    print,   // Full print workflow
    status   // Printer status query
};

/**
 * @brief Command-line options structure
 */
struct options {
    // Connection
    std::string host;
    uint16_t port{0};
    std::string called_ae;
    std::string calling_ae{default_calling_ae};

    // Command
    print_command command{print_command::print};

    // Film Session options
    uint32_t copies{1};
    std::string priority{"MED"};
    std::string medium_type{"BLUE FILM"};
    std::string film_destination{"MAGAZINE"};
    std::string session_label;

    // Film Box options
    std::string display_format{"STANDARD\\1,1"};
    std::string orientation{"PORTRAIT"};
    std::string film_size{"8INX10IN"};
    std::string magnification;

    // Output options
    bool verbose{false};
};

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Print SCU - DICOM Print Management Client

Usage: )" << program_name << R"( <host> <port> <called_ae> <command> [options]

Arguments:
  host        Remote host address (IP or hostname)
  port        Remote port number (typically 10400)
  called_ae   Called AE Title (remote Print SCP's AE title)
  command     'print' or 'status'

Commands:
  print       Execute full print workflow (session + film box + print + cleanup)
  status      Query printer status via N-GET

Film Session Options (for 'print' command):
  --copies <n>          Number of copies [default: 1]
  --priority <p>        Print priority: HIGH, MED, LOW [default: MED]
  --medium <type>       Medium type: PAPER, CLEAR FILM, BLUE FILM [default: BLUE FILM]
  --destination <dest>  Film destination: MAGAZINE, PROCESSOR [default: MAGAZINE]
  --label <text>        Film session label

Film Box Options (for 'print' command):
  --format <fmt>        Image display format [default: STANDARD\1,1]
  --orientation <o>     Film orientation: PORTRAIT, LANDSCAPE [default: PORTRAIT]
  --film-size <size>    Film size: 8INX10IN, 14INX17IN, etc. [default: 8INX10IN]
  --magnification <m>   Magnification type: REPLICATE, BILINEAR, CUBIC, NONE

General Options:
  --calling-ae <ae>     Calling AE Title [default: PRINT_SCU]
  --verbose, -v         Show detailed progress
  --help, -h            Show this help message

Examples:
  # Print with default settings
  )" << program_name << R"( localhost 10400 PRINTER print

  # Print with custom options
  )" << program_name << R"( localhost 10400 PRINTER print \
    --copies 2 --priority HIGH --medium PAPER \
    --format "STANDARD\2,2" --orientation LANDSCAPE

  # Query printer status
  )" << program_name << R"( localhost 10400 PRINTER status

Exit Codes:
  0  Success
  1  Print operation failed
  2  Connection or argument error
)";
}

/**
 * @brief Parse command line arguments
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 5) {
        return false;
    }

    opts.host = argv[1];

    // Parse port
    try {
        int port_int = std::stoi(argv[2]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        opts.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[2] << "'\n";
        return false;
    }

    opts.called_ae = argv[3];
    if (opts.called_ae.length() > 16) {
        std::cerr << "Error: Called AE title exceeds 16 characters\n";
        return false;
    }

    // Parse command
    std::string cmd = argv[4];
    if (cmd == "print") {
        opts.command = print_command::print;
    } else if (cmd == "status") {
        opts.command = print_command::status;
    } else if (cmd == "--help" || cmd == "-h") {
        return false;
    } else {
        std::cerr << "Error: Unknown command '" << cmd << "'. Use 'print' or 'status'\n";
        return false;
    }

    // Parse optional arguments
    for (int i = 5; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--calling-ae" && i + 1 < argc) {
            opts.calling_ae = argv[++i];
            if (opts.calling_ae.length() > 16) {
                std::cerr << "Error: Calling AE title exceeds 16 characters\n";
                return false;
            }
        }
        // Film Session options
        else if (arg == "--copies" && i + 1 < argc) {
            opts.copies = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--priority" && i + 1 < argc) {
            opts.priority = argv[++i];
        } else if (arg == "--medium" && i + 1 < argc) {
            opts.medium_type = argv[++i];
        } else if (arg == "--destination" && i + 1 < argc) {
            opts.film_destination = argv[++i];
        } else if (arg == "--label" && i + 1 < argc) {
            opts.session_label = argv[++i];
        }
        // Film Box options
        else if (arg == "--format" && i + 1 < argc) {
            opts.display_format = argv[++i];
        } else if (arg == "--orientation" && i + 1 < argc) {
            opts.orientation = argv[++i];
        } else if (arg == "--film-size" && i + 1 < argc) {
            opts.film_size = argv[++i];
        } else if (arg == "--magnification" && i + 1 < argc) {
            opts.magnification = argv[++i];
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

/**
 * @brief Create an association configured for Print Management
 */
pacs::network::Result<pacs::network::association> create_print_association(
    const options& opts) {

    using namespace pacs::network;
    using namespace pacs::services;

    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.2";
    config.implementation_version_name = "PRINT_SCU_001";

    // Propose Basic Grayscale Print Management Meta SOP Class
    // (bundles Film Session, Film Box, Image Box, and Printer)
    config.proposed_contexts.push_back({
        1,
        std::string(basic_grayscale_print_meta_sop_class_uid),
        {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        }
    });

    // Also propose individual SOP classes as fallback
    config.proposed_contexts.push_back({
        3,
        std::string(basic_film_session_sop_class_uid),
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });
    config.proposed_contexts.push_back({
        5,
        std::string(basic_film_box_sop_class_uid),
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });
    config.proposed_contexts.push_back({
        7,
        std::string(basic_grayscale_image_box_sop_class_uid),
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });
    config.proposed_contexts.push_back({
        9,
        std::string(printer_sop_class_uid),
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });

    return association::connect(opts.host, opts.port, config, default_timeout);
}

/**
 * @brief Execute the full print workflow
 */
int perform_print(const options& opts) {
    using namespace pacs::services;

    if (opts.verbose) {
        std::cout << "=== Print Workflow ===\n";
        std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
        std::cout << "  Calling AE:    " << opts.calling_ae << "\n";
        std::cout << "  Called AE:     " << opts.called_ae << "\n";
        std::cout << "  Copies:        " << opts.copies << "\n";
        std::cout << "  Priority:      " << opts.priority << "\n";
        std::cout << "  Medium:        " << opts.medium_type << "\n";
        std::cout << "  Format:        " << opts.display_format << "\n";
        std::cout << "  Orientation:   " << opts.orientation << "\n";
        std::cout << "  Film Size:     " << opts.film_size << "\n\n";
    }

    auto start_time = std::chrono::steady_clock::now();

    // Establish association
    auto connect_result = create_print_association(opts);
    if (connect_result.is_err()) {
        std::cerr << "Failed to establish association: "
                  << connect_result.error().message << "\n";
        return 2;
    }
    auto& assoc = connect_result.value();

    if (opts.verbose) {
        auto connect_time = std::chrono::steady_clock::now();
        auto connect_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            connect_time - start_time);
        std::cout << "Association established in " << connect_ms.count() << " ms\n";
    }

    print_scu scu;

    // Step 1: Create Film Session
    if (opts.verbose) {
        std::cout << "\n[1/4] Creating Film Session...\n";
    }

    print_session_data session_data;
    session_data.number_of_copies = opts.copies;
    session_data.print_priority = opts.priority;
    session_data.medium_type = opts.medium_type;
    session_data.film_destination = opts.film_destination;
    session_data.film_session_label = opts.session_label;

    auto session_result = scu.create_film_session(assoc, session_data);
    if (session_result.is_err()) {
        std::cerr << "Failed to create Film Session: "
                  << session_result.error().message << "\n";
        assoc.abort();
        return 1;
    }
    if (!session_result.value().is_success()) {
        std::cerr << "Film Session creation returned status: 0x"
                  << std::hex << session_result.value().status << std::dec << "\n";
        (void)assoc.release(default_timeout);
        return 1;
    }

    auto session_uid = session_result.value().sop_instance_uid;
    if (opts.verbose) {
        std::cout << "  Film Session UID: " << session_uid << "\n";
    }

    // Step 2: Create Film Box
    if (opts.verbose) {
        std::cout << "\n[2/4] Creating Film Box...\n";
    }

    print_film_box_data box_data;
    box_data.image_display_format = opts.display_format;
    box_data.film_orientation = opts.orientation;
    box_data.film_size_id = opts.film_size;
    box_data.magnification_type = opts.magnification;
    box_data.film_session_uid = session_uid;

    auto box_result = scu.create_film_box(assoc, box_data);
    if (box_result.is_err()) {
        std::cerr << "Failed to create Film Box: "
                  << box_result.error().message << "\n";
        (void)scu.delete_film_session(assoc, session_uid);
        (void)assoc.release(default_timeout);
        return 1;
    }
    if (!box_result.value().is_success()) {
        std::cerr << "Film Box creation returned status: 0x"
                  << std::hex << box_result.value().status << std::dec << "\n";
        (void)scu.delete_film_session(assoc, session_uid);
        (void)assoc.release(default_timeout);
        return 1;
    }

    auto film_box_uid = box_result.value().sop_instance_uid;
    if (opts.verbose) {
        std::cout << "  Film Box UID: " << film_box_uid << "\n";
    }

    // Step 3: Print Film Box (N-ACTION)
    if (opts.verbose) {
        std::cout << "\n[3/4] Printing Film Box...\n";
    }

    auto print_result_val = scu.print_film_box(assoc, film_box_uid);
    if (print_result_val.is_err()) {
        std::cerr << "Failed to print Film Box: "
                  << print_result_val.error().message << "\n";
        (void)scu.delete_film_session(assoc, session_uid);
        (void)assoc.release(default_timeout);
        return 1;
    }
    if (!print_result_val.value().is_success()) {
        std::cerr << "Print action returned status: 0x"
                  << std::hex << print_result_val.value().status << std::dec << "\n";
        if (!print_result_val.value().error_comment.empty()) {
            std::cerr << "  Error: " << print_result_val.value().error_comment << "\n";
        }
        (void)scu.delete_film_session(assoc, session_uid);
        (void)assoc.release(default_timeout);
        return 1;
    }

    // Step 4: Clean up - Delete Film Session
    if (opts.verbose) {
        std::cout << "\n[4/4] Cleaning up (deleting Film Session)...\n";
    }

    auto delete_result = scu.delete_film_session(assoc, session_uid);
    if (delete_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Film Session delete failed: "
                  << delete_result.error().message << "\n";
    }

    // Release association
    if (opts.verbose) {
        std::cout << "\nReleasing association...\n";
    }
    auto release_result = assoc.release(default_timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Success output
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "      Print Completed Successfully\n";
    std::cout << "========================================\n";
    std::cout << "  Session UID:  " << session_uid << "\n";
    std::cout << "  Film Box UID: " << film_box_uid << "\n";
    std::cout << "  Copies:       " << opts.copies << "\n";
    std::cout << "  Priority:     " << opts.priority << "\n";
    std::cout << "  Medium:       " << opts.medium_type << "\n";
    std::cout << "  Film Size:    " << opts.film_size << "\n";
    std::cout << "  Total time:   " << total_ms.count() << " ms\n";
    std::cout << "========================================\n";

    return 0;
}

/**
 * @brief Query printer status
 */
int perform_status_query(const options& opts) {
    using namespace pacs::services;

    if (opts.verbose) {
        std::cout << "=== Printer Status Query ===\n";
        std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
        std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
        std::cout << "  Called AE:   " << opts.called_ae << "\n\n";
    }

    auto start_time = std::chrono::steady_clock::now();

    auto connect_result = create_print_association(opts);
    if (connect_result.is_err()) {
        std::cerr << "Failed to establish association: "
                  << connect_result.error().message << "\n";
        return 2;
    }
    auto& assoc = connect_result.value();

    if (opts.verbose) {
        auto connect_time = std::chrono::steady_clock::now();
        auto connect_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            connect_time - start_time);
        std::cout << "Association established in " << connect_ms.count() << " ms\n";
        std::cout << "Sending N-GET Printer Status...\n";
    }

    print_scu scu;

    auto status_result = scu.query_printer_status(assoc);
    if (status_result.is_err()) {
        std::cerr << "Failed to query printer status: "
                  << status_result.error().message << "\n";
        assoc.abort();
        return 1;
    }

    if (!status_result.value().is_success()) {
        std::cerr << "Printer status query returned status: 0x"
                  << std::hex << status_result.value().status << std::dec << "\n";
        (void)assoc.release(default_timeout);
        return 1;
    }

    // Extract printer status from response dataset
    const auto& response = status_result.value().response_data;
    std::string printer_status_str = "UNKNOWN";
    std::string printer_status_info;
    std::string printer_name_str;

    if (response.contains(print_tags::printer_status_tag)) {
        printer_status_str = response.get_string(print_tags::printer_status_tag);
    }
    if (response.contains(print_tags::printer_status_info)) {
        printer_status_info = response.get_string(print_tags::printer_status_info);
    }
    if (response.contains(print_tags::printer_name)) {
        printer_name_str = response.get_string(print_tags::printer_name);
    }

    // Release association
    if (opts.verbose) {
        std::cout << "Releasing association...\n";
    }
    auto release_result = assoc.release(default_timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Output
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "         Printer Status\n";
    std::cout << "========================================\n";
    std::cout << "  Status:       " << printer_status_str << "\n";
    if (!printer_status_info.empty()) {
        std::cout << "  Status Info:  " << printer_status_info << "\n";
    }
    if (!printer_name_str.empty()) {
        std::cout << "  Printer Name: " << printer_name_str << "\n";
    }
    std::cout << "  Query time:   " << total_ms.count() << " ms\n";
    std::cout << "========================================\n";

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____       _       _     ____   ____ _   _
 |  _ \ _ __(_)_ __ | |_  / ___| / ___| | | |
 | |_) | '__| | '_ \| __| \___ \| |   | | | |
 |  __/| |  | | | | | |_   ___) | |___| |_| |
 |_|   |_|  |_|_| |_|\__| |____/ \____|\___/

          DICOM Print Management Client
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 2;
    }

    if (opts.command == print_command::print) {
        return perform_print(opts);
    } else {
        return perform_status_query(opts);
    }
}
