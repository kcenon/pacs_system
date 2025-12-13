/**
 * @file main.cpp
 * @brief move_scu - DICOM C-MOVE SCU utility (dcmtk-compatible)
 *
 * A command-line utility for retrieving DICOM objects from a PACS by requesting
 * the SCP to send them to a specified destination. Provides dcmtk-compatible
 * interface with -aem move destination and progress tracking.
 *
 * @see Issue #290 - move_scu: Implement C-MOVE SCU utility (Retrieve)
 * @see Issue #286 - [Epic] PACS SCU/SCP Utilities Implementation
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1.3 - C-MOVE Service
 * @see https://support.dcmtk.org/docs/movescu.html
 *
 * Usage:
 *   move_scu [options] <peer> <port>
 *
 * Example:
 *   move_scu -aem WORKSTATION -L STUDY -k "0020,000D=1.2.840..." localhost 11112
 */

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/retrieve_scp.hpp"
#include "pacs/services/storage_scp.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

// =============================================================================
// Constants
// =============================================================================

constexpr const char* version_string = "1.0.0";
constexpr const char* default_calling_ae = "MOVESCU";
constexpr const char* default_called_ae = "ANY-SCP";
constexpr auto default_timeout = std::chrono::seconds{60};
constexpr size_t max_ae_title_length = 16;
constexpr int progress_bar_width = 40;

// =============================================================================
// Query Model and Level
// =============================================================================

enum class query_model {
    patient_root,
    study_root
};

enum class query_level {
    patient,
    study,
    series,
    image
};

// =============================================================================
// Query Key
// =============================================================================

struct query_key {
    pacs::core::dicom_tag tag;
    std::string value;
};

// =============================================================================
// Command Line Options
// =============================================================================

struct options {
    // Network options
    std::string peer_host;
    uint16_t peer_port{0};
    std::string calling_ae_title{default_calling_ae};
    std::string called_ae_title{default_called_ae};
    std::string move_destination;  // Required for C-MOVE

    // Timeout options
    std::chrono::seconds connection_timeout{default_timeout};
    std::chrono::seconds acse_timeout{default_timeout};
    std::chrono::seconds dimse_timeout{0};  // 0 = infinite

    // Query model and level
    query_model model{query_model::patient_root};
    query_level level{query_level::study};

    // Query keys
    std::vector<query_key> keys;
    std::string query_file;

    // Output options (when receiving locally)
    std::filesystem::path output_dir{"./downloads"};
    uint16_t receive_port{0};  // 0 = auto

    // Progress options
    bool show_progress{true};
    bool ignore_pending{false};

    // Verbosity
    bool verbose{false};
    bool debug{false};
    bool quiet{false};

    // Help/version flags
    bool show_help{false};
    bool show_version{false};
};

// =============================================================================
// Progress Tracking
// =============================================================================

struct move_progress {
    std::atomic<uint16_t> remaining{0};
    std::atomic<uint16_t> completed{0};
    std::atomic<uint16_t> failed{0};
    std::atomic<uint16_t> warning{0};
    std::chrono::steady_clock::time_point start_time;

    void reset() {
        remaining = 0;
        completed = 0;
        failed = 0;
        warning = 0;
        start_time = std::chrono::steady_clock::now();
    }

    [[nodiscard]] uint16_t total() const {
        return remaining + completed + failed + warning;
    }
};

// =============================================================================
// Utility Functions
// =============================================================================

std::string_view query_model_to_string(query_model model) {
    switch (model) {
        case query_model::patient_root: return "Patient Root";
        case query_model::study_root: return "Study Root";
        default: return "Unknown";
    }
}

std::string_view query_level_to_string(query_level level) {
    switch (level) {
        case query_level::patient: return "PATIENT";
        case query_level::study: return "STUDY";
        case query_level::series: return "SERIES";
        case query_level::image: return "IMAGE";
        default: return "UNKNOWN";
    }
}

std::string_view get_move_sop_class_uid(query_model model) {
    switch (model) {
        case query_model::patient_root:
            return pacs::services::patient_root_move_sop_class_uid;
        case query_model::study_root:
            return pacs::services::study_root_move_sop_class_uid;
        default:
            return pacs::services::patient_root_move_sop_class_uid;
    }
}

// =============================================================================
// Output Functions
// =============================================================================

void print_banner() {
    std::cout << R"(
  __  __  _____  _   _ _____   ____   ____ _   _
 |  \/  |/ _ \ \| | / / ____| / ___| / ___| | | |
 | |\/| | | | \ V / |  _|    \___ \| |   | | | |
 | |  | | |_| |\ /| | |___    ___) | |___| |_| |
 |_|  |_|\___/  \_/ |_____|  |____/ \____|\___/

        DICOM C-MOVE Client v)" << version_string << R"(
)" << "\n";
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << R"( [options] <peer> <port>

Arguments:
  peer                          Remote host address (IP or hostname)
  port                          Remote port number (typically 104 or 11112)

Options:
  -h, --help                    Show this help message and exit
  -v, --verbose                 Verbose output mode
  -d, --debug                   Debug output mode
  -q, --quiet                   Quiet mode (minimal output)
  --version                     Show version information

Network Options:
  -aet, --aetitle <aetitle>     Calling AE Title (default: MOVESCU)
  -aec, --call <aetitle>        Called AE Title (default: ANY-SCP)
  -aem, --move-dest <aetitle>   Move destination AE Title (REQUIRED)
  -to, --timeout <seconds>      Connection timeout (default: 60)
  -ta, --acse-timeout <seconds> ACSE timeout (default: 60)
  -td, --dimse-timeout <seconds> DIMSE timeout (default: 0=infinite)

Query Model:
  -P, --patient-root            Patient Root Query Model (default)
  -S, --study-root              Study Root Query Model

Query Level:
  -L, --level <level>           Retrieve level (PATIENT|STUDY|SERIES|IMAGE)

Query Keys:
  -k, --key <tag=value>         Query key for retrieval
  -f, --query-file <file>       Read query keys from file

Output Options (when receiving locally):
  -od, --output-dir <dir>       Output directory (default: ./downloads)
  --port <port>                 Port for receiving files (default: auto)

Progress Options:
  -p, --progress                Show progress information (default)
  --no-progress                 Disable progress display
  --ignore-pending              Ignore pending status

Examples:
  # Move study to third party
  )" << program_name << R"( -aem WORKSTATION \
    -L STUDY \
    -k "0020,000D=1.2.840..." \
    pacs.example.com 104

  # Move series to self
  )" << program_name << R"( -aem MOVESCU \
    --port 11113 \
    -od ./received/ \
    -L SERIES \
    -k "0020,000E=1.2.840..." \
    localhost 11112

  # Move patient data with progress
  )" << program_name << R"( -aem ARCHIVE \
    --progress \
    -L PATIENT \
    -k "0010,0020=12345" \
    pacs.example.com 104

Exit Codes:
  0  Success - Move completed
  1  Partial success - Some sub-operations failed
  2  Error - Move failed or invalid arguments
)";
}

void print_version() {
    std::cout << "move_scu version " << version_string << "\n";
    std::cout << "PACS System DICOM Utilities\n";
    std::cout << "Copyright (c) 2024\n";
}

void display_progress(const move_progress& progress, bool verbose) {
    auto total = progress.total();
    if (total == 0) return;

    uint16_t done = progress.completed + progress.failed + progress.warning;
    float pct = static_cast<float>(done) / total;

    auto elapsed = std::chrono::steady_clock::now() - progress.start_time;
    auto elapsed_sec = std::chrono::duration<double>(elapsed).count();

    std::cout << "\r[";
    int filled = static_cast<int>(pct * progress_bar_width);
    for (int i = 0; i < progress_bar_width; ++i) {
        if (i < filled) std::cout << "=";
        else if (i == filled) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] ";

    std::cout << std::fixed << std::setprecision(1) << (pct * 100) << "% ";
    std::cout << "(" << done << "/" << total << ") ";

    if (verbose) {
        std::cout << std::setprecision(1) << elapsed_sec << "s ";
        if (progress.failed > 0) {
            std::cout << "[" << progress.failed.load() << " failed] ";
        }
    }

    std::cout << std::flush;
}

// =============================================================================
// Argument Parsing
// =============================================================================

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

std::optional<query_level> parse_level(const std::string& level_str) {
    std::string upper = level_str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "PATIENT") return query_level::patient;
    if (upper == "STUDY") return query_level::study;
    if (upper == "SERIES") return query_level::series;
    if (upper == "IMAGE" || upper == "INSTANCE") return query_level::image;
    return std::nullopt;
}

bool parse_query_key(const std::string& key_str, query_key& key) {
    std::regex key_regex(R"(\(?([0-9A-Fa-f]{4}),([0-9A-Fa-f]{4})\)?=?(.*))");
    std::smatch match;

    if (!std::regex_match(key_str, match, key_regex)) {
        std::cerr << "Error: Invalid query key format: '" << key_str << "'\n";
        return false;
    }

    uint16_t group = static_cast<uint16_t>(std::stoul(match[1].str(), nullptr, 16));
    uint16_t element = static_cast<uint16_t>(std::stoul(match[2].str(), nullptr, 16));

    key.tag = pacs::core::dicom_tag{group, element};
    key.value = match[3].str();

    return true;
}

bool load_query_file(const std::string& filename, std::vector<query_key>& keys) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open query file: " << filename << "\n";
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        auto pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos || line[pos] == '#') {
            continue;
        }

        query_key key;
        if (!parse_query_key(line, key)) {
            return false;
        }
        keys.push_back(key);
    }

    return true;
}

bool parse_arguments(int argc, char* argv[], options& opts) {
    std::vector<std::string> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

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
            opts.verbose = true;
            continue;
        }
        if (arg == "-d" || arg == "--debug") {
            opts.debug = true;
            opts.verbose = true;
            continue;
        }
        if (arg == "-q" || arg == "--quiet") {
            opts.quiet = true;
            continue;
        }

        // Network options
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
        if ((arg == "-aem" || arg == "--move-dest") && i + 1 < argc) {
            opts.move_destination = argv[++i];
            if (!validate_ae_title(opts.move_destination, "Move Destination")) {
                return false;
            }
            continue;
        }

        // Timeout options
        if ((arg == "-to" || arg == "--timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], opts.connection_timeout, "timeout")) {
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

        // Query model
        if (arg == "-P" || arg == "--patient-root") {
            opts.model = query_model::patient_root;
            continue;
        }
        if (arg == "-S" || arg == "--study-root") {
            opts.model = query_model::study_root;
            continue;
        }

        // Query level
        if ((arg == "-L" || arg == "--level") && i + 1 < argc) {
            auto level = parse_level(argv[++i]);
            if (!level) {
                std::cerr << "Error: Invalid query level: '" << argv[i] << "'\n";
                return false;
            }
            opts.level = *level;
            continue;
        }

        // Query keys
        if ((arg == "-k" || arg == "--key") && i + 1 < argc) {
            query_key key;
            if (!parse_query_key(argv[++i], key)) {
                return false;
            }
            opts.keys.push_back(key);
            continue;
        }
        if ((arg == "-f" || arg == "--query-file") && i + 1 < argc) {
            opts.query_file = argv[++i];
            continue;
        }

        // Output options
        if ((arg == "-od" || arg == "--output-dir") && i + 1 < argc) {
            opts.output_dir = argv[++i];
            continue;
        }
        if (arg == "--port" && i + 1 < argc) {
            try {
                int port = std::stoi(argv[++i]);
                if (port < 1 || port > 65535) {
                    std::cerr << "Error: Port must be between 1 and 65535\n";
                    return false;
                }
                opts.receive_port = static_cast<uint16_t>(port);
            } catch (...) {
                std::cerr << "Error: Invalid port number\n";
                return false;
            }
            continue;
        }

        // Progress options
        if (arg == "-p" || arg == "--progress") {
            opts.show_progress = true;
            continue;
        }
        if (arg == "--no-progress") {
            opts.show_progress = false;
            continue;
        }
        if (arg == "--ignore-pending") {
            opts.ignore_pending = true;
            continue;
        }

        if (arg.starts_with("-")) {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }

        positional_args.push_back(arg);
    }

    if (positional_args.size() != 2) {
        std::cerr << "Error: Expected <peer> <port> arguments\n";
        return false;
    }

    opts.peer_host = positional_args[0];

    try {
        int port_int = std::stoi(positional_args[1]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        opts.peer_port = static_cast<uint16_t>(port_int);
    } catch (...) {
        std::cerr << "Error: Invalid port number '" << positional_args[1] << "'\n";
        return false;
    }

    // Validate move destination
    if (opts.move_destination.empty()) {
        std::cerr << "Error: Move destination (-aem) is required\n";
        return false;
    }

    // Load query file if specified
    if (!opts.query_file.empty()) {
        if (!load_query_file(opts.query_file, opts.keys)) {
            return false;
        }
    }

    // Validate at least one key
    if (opts.keys.empty()) {
        std::cerr << "Error: At least one query key (-k) is required\n";
        return false;
    }

    return true;
}

// =============================================================================
// Query Dataset Building
// =============================================================================

pacs::core::dicom_dataset build_query_dataset(const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    dicom_dataset ds;

    std::string level_str{query_level_to_string(opts.level)};
    ds.set_string(tags::query_retrieve_level, vr_type::CS, level_str);

    for (const auto& key : opts.keys) {
        ds.set_string(key.tag, vr_type::UN, key.value);
    }

    return ds;
}

// =============================================================================
// Move Implementation
// =============================================================================

pacs::network::dimse::dimse_message make_c_move_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view move_destination) {

    using namespace pacs::network::dimse;

    dimse_message msg{command_field::c_move_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_priority(priority_medium);

    msg.command_set().set_string(
        tag_move_destination,
        pacs::encoding::vr_type::AE,
        std::string(move_destination));

    return msg;
}

int perform_move(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;
    using namespace pacs::services;

    auto sop_class_uid = get_move_sop_class_uid(opts.model);

    if (!opts.quiet) {
        std::cout << "Requesting Association\n";
        if (opts.verbose) {
            std::cout << "  Peer:        " << opts.peer_host << ":"
                      << opts.peer_port << "\n";
            std::cout << "  Calling AE:  " << opts.calling_ae_title << "\n";
            std::cout << "  Called AE:   " << opts.called_ae_title << "\n";
            std::cout << "  Move Dest:   " << opts.move_destination << "\n";
            std::cout << "  Query Model: " << query_model_to_string(opts.model)
                      << "\n";
            std::cout << "  Query Level: " << query_level_to_string(opts.level)
                      << "\n\n";
        }
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae_title;
    config.called_ae_title = opts.called_ae_title;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "MOVE_SCU_100";

    config.proposed_contexts.push_back({
        1,
        std::string(sop_class_uid),
        {
            "1.2.840.10008.1.2.1",
            "1.2.840.10008.1.2"
        }
    });

    // Establish association
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        opts.connection_timeout);
    auto connect_result = association::connect(opts.peer_host, opts.peer_port,
                                               config, timeout);

    if (connect_result.is_err()) {
        std::cerr << "Association Failed: " << connect_result.error().message
                  << "\n";
        return 2;
    }

    auto& assoc = connect_result.value();

    if (!opts.quiet) {
        std::cout << "Association Accepted\n";
    }

    if (!assoc.has_accepted_context(sop_class_uid)) {
        std::cerr << "Error: C-MOVE SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    auto context_id_opt = assoc.accepted_context_id(sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t context_id = *context_id_opt;

    auto query_ds = build_query_dataset(opts);

    auto move_rq = make_c_move_rq(1, sop_class_uid, opts.move_destination);
    move_rq.set_dataset(std::move(query_ds));

    if (!opts.quiet) {
        std::cout << "Initiating C-MOVE to " << opts.move_destination << "...\n";
    }

    auto send_result = assoc.send_dimse(context_id, move_rq);
    if (send_result.is_err()) {
        std::cerr << "Send Failed: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Progress tracking
    move_progress progress;
    progress.reset();

    bool move_complete = false;
    uint16_t final_completed = 0;
    uint16_t final_failed = 0;
    uint16_t final_warning = 0;

    auto dimse_timeout = opts.dimse_timeout.count() > 0
        ? std::chrono::duration_cast<std::chrono::milliseconds>(opts.dimse_timeout)
        : std::chrono::milliseconds{60000};

    while (!move_complete) {
        auto recv_result = assoc.receive_dimse(dimse_timeout);
        if (recv_result.is_err()) {
            std::cerr << "\nReceive Failed: " << recv_result.error().message
                      << "\n";
            assoc.abort();
            return 2;
        }

        auto& [recv_context_id, msg] = recv_result.value();

        if (msg.command() != command_field::c_move_rsp) {
            std::cerr << "\nError: Unexpected response (expected C-MOVE-RSP)\n";
            assoc.abort();
            return 2;
        }

        auto status = msg.status();

        if (auto remaining = msg.remaining_subops()) {
            progress.remaining = *remaining;
        }
        if (auto completed = msg.completed_subops()) {
            progress.completed = *completed;
            final_completed = *completed;
        }
        if (auto failed = msg.failed_subops()) {
            progress.failed = *failed;
            final_failed = *failed;
        }
        if (auto warning = msg.warning_subops()) {
            progress.warning = *warning;
            final_warning = *warning;
        }

        if (opts.show_progress && !opts.quiet) {
            display_progress(progress, opts.verbose);
        }

        if (status == status_success ||
            status == status_cancel ||
            (status & 0xF000) == 0xA000 ||
            (status & 0xF000) == 0xC000) {

            move_complete = true;

            if (status != status_success && status != status_cancel &&
                !opts.quiet) {
                std::cerr << "\nC-MOVE failed with status: 0x" << std::hex
                          << status << std::dec << "\n";
            }
        }
    }

    if (opts.show_progress && !opts.quiet) {
        std::cout << "\n";
    }

    if (!opts.quiet && opts.verbose) {
        std::cout << "Releasing Association\n";
    }

    auto release_result = assoc.release(timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: "
                  << release_result.error().message << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Print summary
    if (!opts.quiet) {
        std::cout << "\n========================================\n";
        std::cout << "           Move Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Destination:     " << opts.move_destination << "\n";
        std::cout << "  Level:           " << query_level_to_string(opts.level)
                  << "\n";
        std::cout << "  ----------------------------------------\n";
        std::cout << "  Completed:       " << final_completed << "\n";
        if (final_warning > 0) {
            std::cout << "  Warnings:        " << final_warning << "\n";
        }
        if (final_failed > 0) {
            std::cout << "  Failed:          " << final_failed << "\n";
        }
        std::cout << "  Total Time:      " << total_duration.count() << " ms\n";
        std::cout << "========================================\n";
    }

    if (final_failed > 0 && final_completed == 0) {
        return 2;
    } else if (final_failed > 0) {
        return 1;
    }
    return 0;
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

    if (!opts.quiet) {
        print_banner();
    }

    return perform_move(opts);
}
