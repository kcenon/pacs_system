/**
 * @file main.cpp
 * @brief get_scu - DICOM C-GET SCU utility (dcmtk-compatible)
 *
 * A command-line utility for retrieving DICOM objects directly from a PACS.
 * Unlike C-MOVE, C-GET retrieves objects directly to the requesting SCU
 * without requiring a separate storage SCP.
 *
 * @see Issue #291 - get_scu: Implement C-GET SCU utility (Retrieve)
 * @see Issue #286 - [Epic] PACS SCU/SCP Utilities Implementation
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1.4 - C-GET Service
 * @see https://support.dcmtk.org/docs/getscu.html
 *
 * Usage:
 *   get_scu [options] <peer> <port>
 *
 * Example:
 *   get_scu -L STUDY -k "0020,000D=1.2.840..." -od ./downloads localhost 11112
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
#include <vector>

namespace {

// =============================================================================
// Constants
// =============================================================================

constexpr const char* version_string = "1.0.0";
constexpr const char* default_calling_ae = "GETSCU";
constexpr const char* default_called_ae = "ANY-SCP";
constexpr auto default_timeout = std::chrono::seconds{60};
constexpr size_t max_ae_title_length = 16;
constexpr int progress_bar_width = 40;

// Storage SOP Classes for role negotiation
static const std::vector<std::string_view> storage_sop_classes = {
    "1.2.840.10008.5.1.4.1.1.2",      // CT Image Storage
    "1.2.840.10008.5.1.4.1.1.4",      // MR Image Storage
    "1.2.840.10008.5.1.4.1.1.7",      // Secondary Capture Image Storage
    "1.2.840.10008.5.1.4.1.1.1",      // CR Image Storage
    "1.2.840.10008.5.1.4.1.1.1.1",    // Digital X-Ray Image Storage
    "1.2.840.10008.5.1.4.1.1.12.1",   // X-Ray Angiographic Image Storage
    "1.2.840.10008.5.1.4.1.1.6.1",    // US Image Storage
    "1.2.840.10008.5.1.4.1.1.88.11",  // Basic Text SR
    "1.2.840.10008.5.1.4.1.1.88.22",  // Enhanced SR
    "1.2.840.10008.5.1.4.1.1.2.1",    // Enhanced CT Image Storage
    "1.2.840.10008.5.1.4.1.1.4.1",    // Enhanced MR Image Storage
    "1.2.840.10008.5.1.4.1.1.128",    // PET Image Storage
};

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

    // Timeout options
    std::chrono::seconds connection_timeout{default_timeout};
    std::chrono::seconds acse_timeout{default_timeout};
    std::chrono::seconds dimse_timeout{0};

    // Query model and level
    query_model model{query_model::study_root};
    query_level level{query_level::study};

    // Query keys
    std::vector<query_key> keys;
    std::string query_file;

    // Output options
    std::filesystem::path output_dir{"."};
    std::string output_format;  // Filename format

    // Transfer syntax preferences
    bool prefer_lossless{false};
    bool prefer_explicit{false};
    bool accept_all{false};

    // Progress options
    bool show_progress{true};

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

struct get_progress {
    std::atomic<uint16_t> remaining{0};
    std::atomic<uint16_t> completed{0};
    std::atomic<uint16_t> failed{0};
    std::atomic<uint16_t> warning{0};
    std::atomic<size_t> bytes_received{0};
    std::chrono::steady_clock::time_point start_time;

    void reset() {
        remaining = 0;
        completed = 0;
        failed = 0;
        warning = 0;
        bytes_received = 0;
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

std::string_view get_get_sop_class_uid(query_model model) {
    switch (model) {
        case query_model::patient_root:
            return pacs::services::patient_root_get_sop_class_uid;
        case query_model::study_root:
            return pacs::services::study_root_get_sop_class_uid;
        default:
            return pacs::services::study_root_get_sop_class_uid;
    }
}

std::string format_size(size_t bytes) {
    constexpr size_t KB = 1024;
    constexpr size_t MB = KB * 1024;
    constexpr size_t GB = MB * 1024;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (bytes >= GB) {
        oss << static_cast<double>(bytes) / GB << " GB";
    } else if (bytes >= MB) {
        oss << static_cast<double>(bytes) / MB << " MB";
    } else if (bytes >= KB) {
        oss << static_cast<double>(bytes) / KB << " KB";
    } else {
        oss << bytes << " B";
    }

    return oss.str();
}

// =============================================================================
// Output Functions
// =============================================================================

void print_banner() {
    std::cout << R"(
   ____ _____ _____   ____   ____ _   _
  / ___| ____|_   _| / ___| / ___| | | |
 | |  _|  _|   | |   \___ \| |   | | | |
 | |_| | |___  | |    ___) | |___| |_| |
  \____|_____| |_|   |____/ \____|\___/

        DICOM C-GET Client v)" << version_string << R"(
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
  -aet, --aetitle <aetitle>     Calling AE Title (default: GETSCU)
  -aec, --call <aetitle>        Called AE Title (default: ANY-SCP)
  -to, --timeout <seconds>      Connection timeout (default: 60)
  -ta, --acse-timeout <seconds> ACSE timeout (default: 60)
  -td, --dimse-timeout <seconds> DIMSE timeout (default: 0=infinite)

Query Model:
  -P, --patient-root            Patient Root Query Model
  -S, --study-root              Study Root Query Model (default)

Query Level:
  -L, --level <level>           Retrieve level (PATIENT|STUDY|SERIES|IMAGE)

Query Keys:
  -k, --key <tag=value>         Query key for retrieval
  -f, --query-file <file>       Read query keys from file

Output Options:
  -od, --output-dir <dir>       Output directory (default: current)
  --output-format <format>      Filename format

Storage Options:
  -xs, --prefer-lossless        Prefer lossless transfer syntax
  -xe, --prefer-explicit        Prefer Explicit VR LE
  +xa, --accept-all             Accept all transfer syntaxes

Progress Options:
  -p, --progress                Show progress information (default)
  --no-progress                 Disable progress display

C-GET vs C-MOVE:
  C-GET retrieves objects directly to the calling SCU, eliminating
  the need for a separate storage SCP. This makes it firewall-friendly
  but requires SCP support for C-GET (less common than C-MOVE).

Examples:
  # Get single instance
  )" << program_name << R"( -L IMAGE \
    -k "0008,0018=1.2.840..." \
    -od ./retrieved/ \
    localhost 11112

  # Get entire study
  )" << program_name << R"( -L STUDY \
    -k "0020,000D=1.2.840..." \
    --progress \
    -od ./study_data/ \
    pacs.example.com 104

  # Get with lossless preference
  )" << program_name << R"( --prefer-lossless \
    -L SERIES \
    -k "0020,000E=1.2.840..." \
    localhost 11112

Exit Codes:
  0  Success - All objects retrieved
  1  Partial success - Some sub-operations failed
  2  Error - Retrieve failed or invalid arguments
)";
}

void print_version() {
    std::cout << "get_scu version " << version_string << "\n";
    std::cout << "PACS System DICOM Utilities\n";
    std::cout << "Copyright (c) 2024\n";
}

void display_progress(const get_progress& progress, bool verbose) {
    auto total = progress.total();
    if (total == 0) return;

    uint16_t done = progress.completed + progress.failed + progress.warning;
    float pct = static_cast<float>(done) / total;

    auto elapsed = std::chrono::steady_clock::now() - progress.start_time;
    auto elapsed_sec = std::chrono::duration<double>(elapsed).count();
    double speed = elapsed_sec > 0
        ? static_cast<double>(progress.bytes_received) / elapsed_sec / 1024.0
        : 0;

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
        std::cout << std::setprecision(1) << speed << " KB/s ";
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

        if (arg == "-P" || arg == "--patient-root") {
            opts.model = query_model::patient_root;
            continue;
        }
        if (arg == "-S" || arg == "--study-root") {
            opts.model = query_model::study_root;
            continue;
        }

        if ((arg == "-L" || arg == "--level") && i + 1 < argc) {
            auto level = parse_level(argv[++i]);
            if (!level) {
                std::cerr << "Error: Invalid query level: '" << argv[i] << "'\n";
                return false;
            }
            opts.level = *level;
            continue;
        }

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

        if ((arg == "-od" || arg == "--output-dir") && i + 1 < argc) {
            opts.output_dir = argv[++i];
            continue;
        }
        if (arg == "--output-format" && i + 1 < argc) {
            opts.output_format = argv[++i];
            continue;
        }

        if (arg == "-xs" || arg == "--prefer-lossless") {
            opts.prefer_lossless = true;
            continue;
        }
        if (arg == "-xe" || arg == "--prefer-explicit") {
            opts.prefer_explicit = true;
            continue;
        }
        if (arg == "+xa" || arg == "--accept-all") {
            opts.accept_all = true;
            continue;
        }

        if (arg == "-p" || arg == "--progress") {
            opts.show_progress = true;
            continue;
        }
        if (arg == "--no-progress") {
            opts.show_progress = false;
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

    if (!opts.query_file.empty()) {
        if (!load_query_file(opts.query_file, opts.keys)) {
            return false;
        }
    }

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
// File Path Generation
// =============================================================================

std::filesystem::path generate_file_path(
    const std::filesystem::path& output_dir,
    const pacs::core::dicom_dataset& dataset) {

    using namespace pacs::core;

    auto sop_uid = dataset.get_string(tags::sop_instance_uid, "UNKNOWN");

    // Replace any characters that might be invalid in filenames
    std::string filename = sop_uid + ".dcm";
    std::replace(filename.begin(), filename.end(), '/', '_');
    std::replace(filename.begin(), filename.end(), '\\', '_');

    return output_dir / filename;
}

bool save_dicom_file(const std::filesystem::path& path,
                     const pacs::core::dicom_dataset& dataset) {
    std::filesystem::create_directories(path.parent_path());

    auto file = pacs::core::dicom_file::create(
        dataset,
        pacs::encoding::transfer_syntax::explicit_vr_little_endian);

    auto result = file.save(path);
    return result.has_value();
}

// =============================================================================
// Get Implementation
// =============================================================================

pacs::network::dimse::dimse_message make_c_get_rq(
    uint16_t message_id,
    std::string_view sop_class_uid) {

    using namespace pacs::network::dimse;

    dimse_message msg{command_field::c_get_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_priority(priority_medium);

    return msg;
}

int perform_get(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;
    using namespace pacs::services;

    auto sop_class_uid = get_get_sop_class_uid(opts.model);

    if (!opts.quiet) {
        std::cout << "Requesting Association\n";
        if (opts.verbose) {
            std::cout << "  Peer:        " << opts.peer_host << ":"
                      << opts.peer_port << "\n";
            std::cout << "  Calling AE:  " << opts.calling_ae_title << "\n";
            std::cout << "  Called AE:   " << opts.called_ae_title << "\n";
            std::cout << "  Query Model: " << query_model_to_string(opts.model)
                      << "\n";
            std::cout << "  Query Level: " << query_level_to_string(opts.level)
                      << "\n";
            std::cout << "  Output:      " << opts.output_dir << "\n\n";
        }
    }

    std::filesystem::create_directories(opts.output_dir);

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae_title;
    config.called_ae_title = opts.called_ae_title;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "GET_SCU_100";

    // Propose C-GET SOP Class
    config.proposed_contexts.push_back({
        1,
        std::string(sop_class_uid),
        {
            "1.2.840.10008.1.2.1",
            "1.2.840.10008.1.2"
        }
    });

    // Propose storage SOP classes for receiving
    uint8_t context_id = 3;
    for (auto storage_sop : storage_sop_classes) {
        config.proposed_contexts.push_back({
            context_id,
            std::string(storage_sop),
            {
                "1.2.840.10008.1.2.1",
                "1.2.840.10008.1.2"
            }
        });
        context_id += 2;
    }

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
        std::cerr << "Error: C-GET SOP Class not accepted by remote SCP\n";
        std::cerr << "Note: C-GET is less commonly supported than C-MOVE\n";
        assoc.abort();
        return 2;
    }

    auto context_id_opt = assoc.accepted_context_id(sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t get_context_id = *context_id_opt;

    auto query_ds = build_query_dataset(opts);

    auto get_rq = make_c_get_rq(1, sop_class_uid);
    get_rq.set_dataset(std::move(query_ds));

    if (!opts.quiet) {
        std::cout << "Initiating C-GET...\n";
    }

    auto send_result = assoc.send_dimse(get_context_id, get_rq);
    if (send_result.is_err()) {
        std::cerr << "Send Failed: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Progress tracking
    get_progress progress;
    progress.reset();

    std::vector<std::filesystem::path> received_files;

    bool retrieve_complete = false;
    uint16_t final_completed = 0;
    uint16_t final_failed = 0;
    uint16_t final_warning = 0;

    auto dimse_timeout = opts.dimse_timeout.count() > 0
        ? std::chrono::duration_cast<std::chrono::milliseconds>(opts.dimse_timeout)
        : std::chrono::milliseconds{60000};

    while (!retrieve_complete) {
        auto recv_result = assoc.receive_dimse(dimse_timeout);
        if (recv_result.is_err()) {
            std::cerr << "\nReceive Failed: " << recv_result.error().message
                      << "\n";
            assoc.abort();
            return 2;
        }

        auto& [recv_context_id, msg] = recv_result.value();
        auto cmd = msg.command();

        if (cmd == command_field::c_get_rsp) {
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

                retrieve_complete = true;

                if (status != status_success && status != status_cancel &&
                    !opts.quiet) {
                    std::cerr << "\nC-GET failed with status: 0x" << std::hex
                              << status << std::dec << "\n";
                }
            }

        } else if (cmd == command_field::c_store_rq) {
            // Incoming C-STORE sub-operation
            if (msg.has_dataset()) {
                const auto& dataset = msg.dataset();

                auto file_path = generate_file_path(opts.output_dir, dataset);
                bool saved = save_dicom_file(file_path, dataset);

                if (saved) {
                    received_files.push_back(file_path);
                }

                // Approximate bytes received
                progress.bytes_received += 1024;

                auto sop_class = msg.affected_sop_class_uid();
                auto sop_instance = msg.affected_sop_instance_uid();

                auto store_rsp = make_c_store_rsp(
                    msg.message_id(),
                    sop_class,
                    sop_instance,
                    saved ? status_success : 0xA700
                );

                auto send_rsp_result = assoc.send_dimse(recv_context_id, store_rsp);
                if (send_rsp_result.is_err() && opts.verbose) {
                    std::cerr << "\nWarning: Failed to send C-STORE response\n";
                }

                if (!saved && opts.verbose) {
                    std::cerr << "\nWarning: Failed to save " << file_path << "\n";
                }
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
        std::cout << "           Get Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Mode:            C-GET\n";
        std::cout << "  Level:           " << query_level_to_string(opts.level)
                  << "\n";
        std::cout << "  Output:          " << opts.output_dir << "\n";
        std::cout << "  ----------------------------------------\n";
        std::cout << "  Received:        " << received_files.size() << " files\n";
        std::cout << "  Data Size:       " << format_size(progress.bytes_received)
                  << "\n";
        std::cout << "  Completed:       " << final_completed << "\n";
        if (final_warning > 0) {
            std::cout << "  Warnings:        " << final_warning << "\n";
        }
        if (final_failed > 0) {
            std::cout << "  Failed:          " << final_failed << "\n";
        }
        std::cout << "  Total Time:      " << total_duration.count() << " ms\n";

        if (total_duration.count() > 0) {
            double speed = static_cast<double>(progress.bytes_received) /
                           (total_duration.count() / 1000.0) / (1024.0 * 1024.0);
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "  Average Speed:   " << speed << " MB/s\n";
        }

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

    return perform_get(opts);
}
