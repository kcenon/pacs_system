/**
 * @file main.cpp
 * @brief Store SCU - DICOM Image Sender (dcmtk-style)
 *
 * A command-line utility for sending DICOM files to a remote SCP (PACS server).
 * Provides dcmtk-compatible interface with extended features including batch
 * operations, progress display, and transfer report generation.
 *
 * @see Issue #288 - store_scu: Implement C-STORE SCU utility (Storage)
 * @see Issue #286 - [Epic] PACS SCU/SCP Utilities Implementation
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.7 Section 9.1.1 - C-STORE Service
 * @see https://support.dcmtk.org/docs/storescu.html
 *
 * Usage:
 *   store_scu [options] <peer> <port> <dcmfile-in> [dcmfile-in...]
 *
 * Example:
 *   store_scu localhost 11112 image.dcm
 *   store_scu -aet MYSCU -aec PACS localhost 11112 ./dicom_folder/ -r
 *   store_scu --progress --report-file transfer.log localhost 11112 *.dcm
 */

#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/network/association.hpp"
#include "pacs/services/storage_scu.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

// =============================================================================
// Constants
// =============================================================================

/// Version information
constexpr const char* version_string = "1.0.0";

/// Default calling AE title when not specified
constexpr const char* default_calling_ae = "STORESCU";

/// Default called AE title when not specified
constexpr const char* default_called_ae = "ANY-SCP";

/// Default connection timeout (30 seconds)
constexpr auto default_connection_timeout = std::chrono::seconds{30};

/// Default ACSE timeout (30 seconds)
constexpr auto default_acse_timeout = std::chrono::seconds{30};

/// Default DIMSE timeout (0 = infinite)
constexpr auto default_dimse_timeout = std::chrono::seconds{0};

/// Maximum AE title length
constexpr size_t max_ae_title_length = 16;

/// Maximum PDU size
constexpr size_t default_max_pdu_size = 16384;

// =============================================================================
// Transfer Syntax Constants
// =============================================================================

/// Transfer Syntax UIDs
namespace ts {
constexpr const char* implicit_vr_le = "1.2.840.10008.1.2";
constexpr const char* explicit_vr_le = "1.2.840.10008.1.2.1";
constexpr const char* explicit_vr_be = "1.2.840.10008.1.2.2";
constexpr const char* jpeg_baseline = "1.2.840.10008.1.2.4.50";
constexpr const char* jpeg_extended = "1.2.840.10008.1.2.4.51";
constexpr const char* jpeg_lossless = "1.2.840.10008.1.2.4.70";
constexpr const char* jpeg2000_lossless = "1.2.840.10008.1.2.4.90";
constexpr const char* jpeg2000_lossy = "1.2.840.10008.1.2.4.91";
constexpr const char* rle = "1.2.840.10008.1.2.5";
}  // namespace ts

// =============================================================================
// Output Modes
// =============================================================================

/**
 * @brief Output verbosity level
 */
enum class verbosity_level {
    quiet,    ///< Minimal output
    normal,   ///< Standard output
    verbose,  ///< Verbose output
    debug     ///< Debug output with all details
};

/**
 * @brief Transfer syntax proposal mode
 */
enum class transfer_syntax_mode {
    prefer_lossless,   ///< Prefer lossless transfer syntaxes
    propose_implicit,  ///< Propose only Implicit VR LE
    propose_explicit,  ///< Propose only Explicit VR LE
    propose_all        ///< Propose all available transfer syntaxes
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

    // Input files/directories
    std::vector<std::filesystem::path> input_paths;
    bool recursive{false};
    std::string scan_pattern{"*.dcm"};

    // Transfer syntax options
    transfer_syntax_mode ts_mode{transfer_syntax_mode::propose_all};

    // Batch options
    bool continue_on_error{true};
    size_t max_pdu_size{default_max_pdu_size};

    // Progress options
    bool show_progress{false};
    std::string report_file;

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
 * @brief Result of a single store operation
 */
struct file_store_result {
    std::filesystem::path file_path;
    std::string sop_class_uid;
    std::string sop_instance_uid;
    bool success{false};
    uint16_t status_code{0};
    std::string error_message;
    size_t file_size{0};
    std::chrono::milliseconds transfer_time{0};
};

/**
 * @brief Statistics for batch store operations
 */
struct store_statistics {
    size_t total_files{0};
    size_t successful{0};
    size_t warnings{0};
    size_t failed{0};
    size_t total_bytes{0};
    std::chrono::milliseconds total_time{0};
    std::chrono::milliseconds association_time{0};

    [[nodiscard]] double success_rate() const {
        return total_files > 0
                   ? (static_cast<double>(successful) / total_files) * 100.0
                   : 0.0;
    }

    [[nodiscard]] double throughput_mbps() const {
        if (total_time.count() == 0) return 0.0;
        double bytes_per_sec =
            static_cast<double>(total_bytes) / (total_time.count() / 1000.0);
        return bytes_per_sec / (1024.0 * 1024.0);
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
  ____ _____ ___  ____  _____   ____   ____ _   _
 / ___|_   _/ _ \|  _ \| ____| / ___| / ___| | | |
 \___ \ | || | | | |_) |  _|   \___ \| |   | | | |
  ___) || || |_| |  _ <| |___   ___) | |___| |_| |
 |____/ |_| \___/|_| \_\_____| |____/ \____|\___/

          DICOM Image Sender v)" << version_string
              << R"(
)" << "\n";
}

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name
              << R"( [options] <peer> <port> <dcmfile-in> [dcmfile-in...]

Arguments:
  peer                          Remote host address (IP or hostname)
  port                          Remote port number (typically 104 or 11112)
  dcmfile-in                    DICOM file(s) or directory to send

Options:
  -h, --help                    Show this help message and exit
  -v, --verbose                 Verbose output mode
  -d, --debug                   Debug output mode (more details than verbose)
  -q, --quiet                   Quiet mode (minimal output)
  --version                     Show version information

Network Options:
  -aet, --aetitle <aetitle>     Calling AE Title (default: STORESCU)
  -aec, --call <aetitle>        Called AE Title (default: ANY-SCP)
  -to, --timeout <seconds>      Connection timeout (default: 30)
  -ta, --acse-timeout <seconds> ACSE timeout (default: 30)
  -td, --dimse-timeout <seconds> DIMSE timeout (default: 0=infinite)

Transfer Options:
  -r, --recursive               Recursively process directories
  -xs, --prefer-lossless        Prefer lossless transfer syntaxes
  -xv, --propose-implicit       Propose only Implicit VR Little Endian
  -xe, --propose-explicit       Propose only Explicit VR Little Endian
  +xa, --propose-all            Propose all transfer syntaxes (default)

Batch Options:
  --scan-pattern <pattern>      File pattern for directory scan (default: *.dcm)
  --continue-on-error           Continue after failures (default)
  --stop-on-error               Stop on first error
  --max-pdu <size>              Maximum PDU size (default: 16384)

Progress Options:
  -p, --progress                Show progress bar
  --report-file <file>          Write transfer report to file

TLS Options (not yet implemented):
  --tls                         Enable TLS connection
  --tls-cert <file>             TLS certificate file
  --tls-key <file>              TLS private key file
  --tls-ca <file>               TLS CA certificate file

Examples:
  # Send single file
  )" << program_name << R"( localhost 11112 image.dcm

  # Send with custom AE Titles
  )" << program_name << R"( -aet MYSCU -aec PACS localhost 11112 image.dcm

  # Send directory recursively with progress
  )" << program_name << R"( -r --progress localhost 11112 ./patient_data/

  # Send with report file
  )" << program_name << R"( --report-file transfer.log localhost 11112 *.dcm

  # Prefer lossless transfer syntax
  )" << program_name << R"( --prefer-lossless localhost 11112 *.dcm

Exit Codes:
  0  Success - All files sent successfully
  1  Error - One or more files failed to send
  2  Error - Invalid arguments or connection failure
)";
}

/**
 * @brief Print version information
 */
void print_version() {
    std::cout << "store_scu version " << version_string << "\n";
    std::cout << "PACS System DICOM Utilities\n";
    std::cout << "Copyright (c) 2024\n";
}

// =============================================================================
// Argument Parsing Helpers
// =============================================================================

/**
 * @brief Parse timeout value from string
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
 * @brief Parse size value from string
 */
bool parse_size(const std::string& value, size_t& result,
                const std::string& option_name, size_t min_value = 0) {
    try {
        long long val = std::stoll(value);
        if (val < 0 || static_cast<size_t>(val) < min_value) {
            std::cerr << "Error: " << option_name << " must be at least "
                      << min_value << "\n";
            return false;
        }
        result = static_cast<size_t>(val);
        return true;
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid value for " << option_name << ": '"
                  << value << "'\n";
        return false;
    }
}

/**
 * @brief Validate AE title
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

        // Transfer options
        if (arg == "-r" || arg == "--recursive") {
            opts.recursive = true;
            continue;
        }
        if (arg == "-xs" || arg == "--prefer-lossless") {
            opts.ts_mode = transfer_syntax_mode::prefer_lossless;
            continue;
        }
        if (arg == "-xv" || arg == "--propose-implicit") {
            opts.ts_mode = transfer_syntax_mode::propose_implicit;
            continue;
        }
        if (arg == "-xe" || arg == "--propose-explicit") {
            opts.ts_mode = transfer_syntax_mode::propose_explicit;
            continue;
        }
        if (arg == "+xa" || arg == "--propose-all") {
            opts.ts_mode = transfer_syntax_mode::propose_all;
            continue;
        }

        // Batch options
        if (arg == "--scan-pattern" && i + 1 < argc) {
            opts.scan_pattern = argv[++i];
            continue;
        }
        if (arg == "--continue-on-error") {
            opts.continue_on_error = true;
            continue;
        }
        if (arg == "--stop-on-error") {
            opts.continue_on_error = false;
            continue;
        }
        if (arg == "--max-pdu" && i + 1 < argc) {
            if (!parse_size(argv[++i], opts.max_pdu_size, "Max PDU size", 4096)) {
                return false;
            }
            continue;
        }

        // Progress options
        if (arg == "-p" || arg == "--progress") {
            opts.show_progress = true;
            continue;
        }
        if (arg == "--report-file" && i + 1 < argc) {
            opts.report_file = argv[++i];
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
        if (arg.starts_with("-") && arg != "-") {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }

        // Positional arguments
        positional_args.push_back(arg);
    }

    // Validate positional arguments (need at least peer, port, and one file)
    if (positional_args.size() < 3) {
        std::cerr
            << "Error: Expected <peer> <port> <dcmfile-in> [dcmfile-in...]\n";
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

    // Collect input paths
    for (size_t i = 2; i < positional_args.size(); ++i) {
        opts.input_paths.emplace_back(positional_args[i]);
    }

    // TLS validation
    if (opts.use_tls) {
        std::cerr << "Warning: TLS support is not yet implemented\n";
    }

    return true;
}

// =============================================================================
// File Collection Helpers
// =============================================================================

/**
 * @brief Check if a file is a potential DICOM file
 */
bool is_dicom_file_candidate(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".dcm" || ext == ".dicom" || ext.empty();
}

/**
 * @brief Collect files from input paths
 */
std::vector<std::filesystem::path> collect_files(
    const std::vector<std::filesystem::path>& input_paths, bool recursive) {
    std::vector<std::filesystem::path> files;

    for (const auto& path : input_paths) {
        if (!std::filesystem::exists(path)) {
            std::cerr << "Warning: Path does not exist: " << path.string()
                      << "\n";
            continue;
        }

        if (std::filesystem::is_regular_file(path)) {
            files.push_back(path);
        } else if (std::filesystem::is_directory(path)) {
            if (recursive) {
                for (const auto& entry :
                     std::filesystem::recursive_directory_iterator(path)) {
                    if (entry.is_regular_file() &&
                        is_dicom_file_candidate(entry.path())) {
                        files.push_back(entry.path());
                    }
                }
            } else {
                for (const auto& entry :
                     std::filesystem::directory_iterator(path)) {
                    if (entry.is_regular_file() &&
                        is_dicom_file_candidate(entry.path())) {
                        files.push_back(entry.path());
                    }
                }
            }
        }
    }

    return files;
}

// =============================================================================
// Progress Display
// =============================================================================

/**
 * @brief Display a progress bar
 */
void show_progress_bar(size_t current, size_t total, int width = 40) {
    if (total == 0) return;

    float progress = static_cast<float>(current) / static_cast<float>(total);
    int filled = static_cast<int>(progress * width);

    std::cout << "\r[";
    for (int i = 0; i < width; ++i) {
        if (i < filled) {
            std::cout << "=";
        } else if (i == filled) {
            std::cout << ">";
        } else {
            std::cout << " ";
        }
    }
    std::cout << "] " << std::setw(3) << static_cast<int>(progress * 100)
              << "% "
              << "(" << current << "/" << total << ")" << std::flush;
}

/**
 * @brief Format file size for display
 */
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

/**
 * @brief Format duration for display
 */
std::string format_duration(std::chrono::milliseconds duration) {
    auto ms = duration.count();

    if (ms < 1000) {
        return std::to_string(ms) + " ms";
    }

    auto seconds = ms / 1000;
    auto minutes = seconds / 60;
    seconds %= 60;

    std::ostringstream oss;
    if (minutes > 0) {
        oss << minutes << "m ";
    }
    oss << seconds << "s";
    return oss.str();
}

// =============================================================================
// Transfer Syntax Helpers
// =============================================================================

/**
 * @brief Get transfer syntaxes based on mode
 */
std::vector<std::string> get_transfer_syntaxes(transfer_syntax_mode mode) {
    switch (mode) {
        case transfer_syntax_mode::propose_implicit:
            return {ts::implicit_vr_le};
        case transfer_syntax_mode::propose_explicit:
            return {ts::explicit_vr_le};
        case transfer_syntax_mode::prefer_lossless:
            return {ts::jpeg_lossless, ts::jpeg2000_lossless, ts::rle,
                    ts::explicit_vr_le, ts::implicit_vr_le};
        case transfer_syntax_mode::propose_all:
        default:
            return {ts::explicit_vr_le,       ts::implicit_vr_le,
                    ts::explicit_vr_be,       ts::jpeg_baseline,
                    ts::jpeg_extended,        ts::jpeg_lossless,
                    ts::jpeg2000_lossless,    ts::jpeg2000_lossy,
                    ts::rle};
    }
}

// =============================================================================
// Report Generation
// =============================================================================

/**
 * @brief Generate transfer report
 */
void generate_report(const std::string& report_file,
                     const std::vector<file_store_result>& results,
                     const store_statistics& stats, const options& opts) {
    std::ofstream ofs(report_file);
    if (!ofs.is_open()) {
        std::cerr << "Warning: Could not open report file: " << report_file
                  << "\n";
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    ofs << "========================================\n";
    ofs << "    DICOM Store SCU Transfer Report\n";
    ofs << "========================================\n";
    ofs << "Generated: " << std::ctime(&time);
    ofs << "\n";

    ofs << "Connection Info:\n";
    ofs << "  Peer:           " << opts.peer_host << ":" << opts.peer_port
        << "\n";
    ofs << "  Calling AE:     " << opts.calling_ae_title << "\n";
    ofs << "  Called AE:      " << opts.called_ae_title << "\n";
    ofs << "\n";

    ofs << "Summary:\n";
    ofs << "  Total Files:    " << stats.total_files << "\n";
    ofs << "  Successful:     " << stats.successful << "\n";
    ofs << "  Warnings:       " << stats.warnings << "\n";
    ofs << "  Failed:         " << stats.failed << "\n";
    ofs << "  Data Sent:      " << format_size(stats.total_bytes) << "\n";
    ofs << "  Duration:       " << format_duration(stats.total_time) << "\n";
    ofs << std::fixed << std::setprecision(2);
    ofs << "  Throughput:     " << stats.throughput_mbps() << " MB/s\n";
    ofs << "  Success Rate:   " << stats.success_rate() << "%\n";
    ofs << "\n";

    if (stats.failed > 0) {
        ofs << "Failed Transfers:\n";
        ofs << "----------------------------------------\n";
        for (const auto& result : results) {
            if (!result.success) {
                ofs << "  File: " << result.file_path.filename().string()
                    << "\n";
                ofs << "  Error: " << result.error_message << "\n";
                ofs << "  Status: 0x" << std::hex << result.status_code
                    << std::dec << "\n";
                ofs << "\n";
            }
        }
    }

    ofs << "All Transfers:\n";
    ofs << "----------------------------------------\n";
    for (const auto& result : results) {
        ofs << (result.success ? "[OK]  " : "[FAIL]") << " ";
        ofs << result.file_path.filename().string();
        if (result.success) {
            ofs << " (" << format_size(result.file_size) << ", "
                << result.transfer_time.count() << "ms)";
        } else {
            ofs << " - " << result.error_message;
        }
        ofs << "\n";
    }

    ofs.close();
}

// =============================================================================
// Main Store Implementation
// =============================================================================

/**
 * @brief Analyze files and collect SOP classes
 */
std::vector<std::pair<std::filesystem::path, std::string>> analyze_files(
    const std::vector<std::filesystem::path>& files, bool verbose) {
    using namespace pacs::core;

    std::vector<std::pair<std::filesystem::path, std::string>> valid_files;

    for (const auto& file_path : files) {
        auto result = dicom_file::open(file_path);
        if (result.has_value()) {
            auto sop_class = result.value().sop_class_uid();
            if (!sop_class.empty()) {
                valid_files.emplace_back(file_path, sop_class);
            } else if (verbose) {
                std::cerr << "Warning: No SOP Class UID in file: "
                          << file_path.string() << "\n";
            }
        } else if (verbose) {
            std::cerr << "Warning: Skipping invalid file: "
                      << file_path.string() << "\n";
        }
    }

    return valid_files;
}

/**
 * @brief Perform store operations
 */
int perform_store(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::services;
    using namespace pacs::core;

    bool is_quiet = opts.verbosity == verbosity_level::quiet;
    bool is_verbose = opts.verbosity == verbosity_level::verbose ||
                      opts.verbosity == verbosity_level::debug;

    store_statistics stats;
    std::vector<file_store_result> results;
    auto start_time = std::chrono::steady_clock::now();

    // Collect files
    if (!is_quiet) {
        std::cout << "Scanning for DICOM files...\n";
    }
    auto files = collect_files(opts.input_paths, opts.recursive);

    if (files.empty()) {
        std::cerr << "Error: No DICOM files found\n";
        return 2;
    }

    if (!is_quiet) {
        std::cout << "Found " << files.size() << " file(s) to analyze\n";
    }

    // Analyze files
    if (!is_quiet) {
        std::cout << "Analyzing files...\n";
    }
    auto valid_files = analyze_files(files, is_verbose);

    if (valid_files.empty()) {
        std::cerr << "Error: No valid DICOM files found\n";
        return 2;
    }

    // Collect unique SOP classes
    std::vector<std::string> sop_classes;
    for (const auto& [path, sop_class] : valid_files) {
        if (std::find(sop_classes.begin(), sop_classes.end(), sop_class) ==
            sop_classes.end()) {
            sop_classes.push_back(sop_class);
        }
    }

    if (!is_quiet) {
        std::cout << "Valid DICOM files: " << valid_files.size() << "\n";
        std::cout << "SOP Classes found: " << sop_classes.size() << "\n\n";
    }

    stats.total_files = valid_files.size();

    // Print connection info
    if (!is_quiet) {
        std::cout << "Connecting to " << opts.peer_host << ":"
                  << opts.peer_port << "\n";
        std::cout << "  Calling AE Title: " << opts.calling_ae_title << "\n";
        std::cout << "  Called AE Title:  " << opts.called_ae_title << "\n";

        if (is_verbose) {
            std::cout << "  Connection Timeout: "
                      << opts.connection_timeout.count() << "s\n";
            std::cout << "  Max PDU Size: " << opts.max_pdu_size << "\n";
        }
        std::cout << "\n";
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae_title;
    config.called_ae_title = opts.called_ae_title;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "STORE_SCU_100";

    // Build presentation contexts
    auto transfer_syntaxes = get_transfer_syntaxes(opts.ts_mode);
    uint8_t context_id = 1;

    for (const auto& sop_class : sop_classes) {
        config.proposed_contexts.push_back(
            {context_id, sop_class, transfer_syntaxes});
        context_id += 2;  // Context IDs must be odd
    }

    // Establish association
    auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        opts.connection_timeout);
    auto connect_result =
        association::connect(opts.peer_host, opts.peer_port, config, timeout);

    if (connect_result.is_err()) {
        std::cerr << "Error: Failed to establish association: "
                  << connect_result.error().message << "\n";
        return 2;
    }

    auto& assoc = connect_result.value();
    auto connect_time = std::chrono::steady_clock::now();
    stats.association_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(connect_time -
                                                              start_time);

    if (!is_quiet) {
        std::cout << "Association established in "
                  << stats.association_time.count() << " ms\n\n";
    }

    // Create storage SCU
    storage_scu_config scu_config;
    scu_config.continue_on_error = opts.continue_on_error;
    scu_config.response_timeout =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            opts.dimse_timeout.count() > 0 ? opts.dimse_timeout
                                           : std::chrono::seconds{30});

    storage_scu scu{scu_config};

    // Send files
    if (!is_quiet) {
        std::cout << "Sending files...\n";
    }

    for (size_t i = 0; i < valid_files.size(); ++i) {
        const auto& [file_path, sop_class] = valid_files[i];

        file_store_result file_result;
        file_result.file_path = file_path;
        file_result.sop_class_uid = sop_class;

        // Get file size
        try {
            file_result.file_size = std::filesystem::file_size(file_path);
        } catch (...) {
            file_result.file_size = 0;
        }

        if (opts.show_progress && !is_quiet) {
            show_progress_bar(i + 1, valid_files.size());
        }

        auto file_start = std::chrono::steady_clock::now();
        auto result = scu.store_file(assoc, file_path);
        auto file_end = std::chrono::steady_clock::now();

        file_result.transfer_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(file_end -
                                                                  file_start);

        if (result.is_ok()) {
            const auto& store_result = result.value();
            file_result.sop_instance_uid = store_result.sop_instance_uid;
            file_result.status_code = store_result.status;

            if (store_result.is_success()) {
                file_result.success = true;
                stats.successful++;
                stats.total_bytes += file_result.file_size;

                if (is_verbose && !opts.show_progress) {
                    std::cout << "  [OK] " << file_path.filename().string()
                              << " (" << format_size(file_result.file_size)
                              << ")\n";
                }
            } else if (store_result.is_warning()) {
                file_result.success = true;
                stats.warnings++;
                stats.successful++;
                stats.total_bytes += file_result.file_size;

                if (is_verbose && !opts.show_progress) {
                    std::cout << "  [WARN] " << file_path.filename().string()
                              << " (Status: 0x" << std::hex
                              << store_result.status << std::dec << ")\n";
                }
            } else {
                file_result.success = false;
                file_result.error_message = store_result.error_comment;
                stats.failed++;

                if (is_verbose && !opts.show_progress) {
                    std::cout << "  [FAIL] " << file_path.filename().string()
                              << " - " << store_result.error_comment << "\n";
                }

                if (!opts.continue_on_error) {
                    results.push_back(file_result);
                    break;
                }
            }
        } else {
            file_result.success = false;
            file_result.error_message = result.error().message;
            stats.failed++;

            if (is_verbose && !opts.show_progress) {
                std::cout << "  [FAIL] " << file_path.filename().string()
                          << " - " << result.error().message << "\n";
            }

            if (!opts.continue_on_error) {
                results.push_back(file_result);
                break;
            }
        }

        results.push_back(file_result);
    }

    if (opts.show_progress && !is_quiet) {
        std::cout << "\n";
    }

    // Release association
    if (!is_quiet) {
        std::cout << "\nReleasing association...\n";
    }
    auto release_result = assoc.release(timeout);
    if (release_result.is_err() && is_verbose) {
        std::cerr << "Warning: Release failed: " << release_result.error().message
                  << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    stats.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Print summary
    if (!is_quiet) {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "              Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Files processed:  " << stats.total_files << "\n";
        std::cout << "  Successful:       " << stats.successful << "\n";
        if (stats.warnings > 0) {
            std::cout << "  Warnings:         " << stats.warnings << "\n";
        }
        std::cout << "  Failed:           " << stats.failed << "\n";
        std::cout << "  Data sent:        " << format_size(stats.total_bytes)
                  << "\n";
        std::cout << "  Total time:       " << format_duration(stats.total_time)
                  << "\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  Throughput:       " << stats.throughput_mbps()
                  << " MB/s\n";

        if (stats.total_files > 0) {
            auto avg_time = stats.total_time.count() / stats.total_files;
            std::cout << "  Avg time/file:    " << avg_time << " ms\n";
        }

        std::cout << "========================================\n";
    }

    // Generate report file if requested
    if (!opts.report_file.empty()) {
        generate_report(opts.report_file, results, stats, opts);
        if (!is_quiet) {
            std::cout << "Report written to: " << opts.report_file << "\n";
        }
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

    return perform_store(opts);
}
