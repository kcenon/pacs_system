/**
 * @file main.cpp
 * @brief Storage SCU - DICOM Image Sender
 *
 * A command-line utility for sending DICOM files to a remote SCP (PACS server).
 * Supports single file sending, directory batch sending, and various options.
 *
 * @see Issue #100 - Storage SCU Sample
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.7 Section 9.1.1 - C-STORE Service
 *
 * Usage:
 *   store_scu <host> <port> <called_ae> <path> [options]
 *
 * Example:
 *   store_scu localhost 11112 PACS_SCP image.dcm
 *   store_scu localhost 11112 PACS_SCP ./dicom_folder/ --recursive
 */

#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/network/association.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// Default calling AE title when not specified
constexpr const char* default_calling_ae = "STORE_SCU";

/// Default timeout for operations (30 seconds)
constexpr auto default_timeout = std::chrono::milliseconds{30000};

/**
 * @brief Command line options
 */
struct options {
    std::string host;
    uint16_t port{0};
    std::string called_ae;
    std::string calling_ae{default_calling_ae};
    std::filesystem::path path;
    bool recursive{false};
    std::string transfer_syntax{"explicit-le"};  // default
    bool continue_on_error{true};
    bool verbose{false};
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Storage SCU - DICOM Image Sender

Usage: )" << program_name << R"( <host> <port> <called_ae> <path> [options]

Arguments:
  host        Remote host address (IP or hostname)
  port        Remote port number (typically 104 or 11112)
  called_ae   Called AE Title (remote SCP's AE title)
  path        DICOM file or directory to send

Options:
  --calling-ae <ae>    Calling AE Title (default: STORE_SCU)
  --ts <syntax>        Transfer syntax to propose:
                         explicit-le  Explicit VR Little Endian (default)
                         implicit-le  Implicit VR Little Endian
                         explicit-be  Explicit VR Big Endian
  --recursive, -r      Recursively scan directories
  --stop-on-error      Stop on first error (default: continue)
  --verbose, -v        Show detailed progress
  --help, -h           Show this help message

Examples:
  )" << program_name << R"( localhost 11112 PACS_SCP image.dcm
  )" << program_name << R"( localhost 11112 PACS_SCP ./dicom_folder/ --recursive
  )" << program_name << R"( 192.168.1.100 104 REMOTE_PACS study/ --calling-ae MY_SCU --ts explicit-le

Exit Codes:
  0  Success - All files sent successfully
  1  Error - One or more files failed to send
  2  Error - Invalid arguments or connection failure
)";
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param opts Output: parsed options
 * @return true if arguments are valid
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

    opts.path = argv[4];

    // Parse optional arguments
    for (int i = 5; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--calling-ae" && i + 1 < argc) {
            opts.calling_ae = argv[++i];
            if (opts.calling_ae.length() > 16) {
                std::cerr << "Error: Calling AE title exceeds 16 characters\n";
                return false;
            }
        } else if (arg == "--ts" && i + 1 < argc) {
            opts.transfer_syntax = argv[++i];
            if (opts.transfer_syntax != "explicit-le" &&
                opts.transfer_syntax != "implicit-le" &&
                opts.transfer_syntax != "explicit-be") {
                std::cerr << "Error: Unknown transfer syntax '" << opts.transfer_syntax << "'\n";
                return false;
            }
        } else if (arg == "--recursive" || arg == "-r") {
            opts.recursive = true;
        } else if (arg == "--stop-on-error") {
            opts.continue_on_error = false;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            return false;
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

/**
 * @brief Get Transfer Syntax UID from option string
 * @param ts_option Transfer syntax option string
 * @return Transfer Syntax UID
 */
std::string get_transfer_syntax_uid(const std::string& ts_option) {
    if (ts_option == "implicit-le") {
        return "1.2.840.10008.1.2";      // Implicit VR Little Endian
    } else if (ts_option == "explicit-be") {
        return "1.2.840.10008.1.2.2";    // Explicit VR Big Endian
    }
    return "1.2.840.10008.1.2.1";        // Explicit VR Little Endian (default)
}

/**
 * @brief Collect DICOM files from a path
 * @param path File or directory path
 * @param recursive Whether to scan recursively
 * @return Vector of DICOM file paths
 */
std::vector<std::filesystem::path> collect_files(
    const std::filesystem::path& path,
    bool recursive) {

    std::vector<std::filesystem::path> files;

    if (std::filesystem::is_regular_file(path)) {
        files.push_back(path);
    } else if (std::filesystem::is_directory(path)) {
        auto iterator = recursive
            ? std::filesystem::recursive_directory_iterator(path)
            : std::filesystem::recursive_directory_iterator(
                  path, std::filesystem::directory_options::none);

        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    // Simple heuristic: check for .dcm extension or try to read
                    auto ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".dcm" || ext == ".dicom" || ext.empty()) {
                        files.push_back(entry.path());
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".dcm" || ext == ".dicom" || ext.empty()) {
                        files.push_back(entry.path());
                    }
                }
            }
        }
    }

    return files;
}

/**
 * @brief Display a progress bar
 * @param current Current progress
 * @param total Total items
 * @param width Width of the progress bar
 */
void show_progress(size_t current, size_t total, int width = 40) {
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
    std::cout << "] " << std::setw(3) << static_cast<int>(progress * 100) << "% "
              << "(" << current << "/" << total << ")" << std::flush;
}

/**
 * @brief Format file size for display
 * @param bytes Size in bytes
 * @return Formatted string
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
 * @brief Send DICOM files to remote SCP
 * @param opts Command line options
 * @return Exit code (0 = success, 1 = partial failure, 2 = fatal error)
 */
int send_files(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::services;
    using namespace pacs::core;

    // Collect files to send
    std::cout << "Scanning for DICOM files...\n";
    auto files = collect_files(opts.path, opts.recursive);

    if (files.empty()) {
        std::cerr << "Error: No DICOM files found at '" << opts.path.string() << "'\n";
        return 2;
    }

    std::cout << "Found " << files.size() << " file(s) to send\n\n";

    // Pre-scan files to get SOP Classes for presentation contexts
    std::cout << "Analyzing files...\n";
    std::vector<std::string> sop_classes;
    std::vector<std::pair<std::filesystem::path, std::string>> valid_files;  // path, sop_class

    for (const auto& file_path : files) {
        auto result = dicom_file::open(file_path);
        if (result.has_value()) {
            auto sop_class = result.value().sop_class_uid();
            if (!sop_class.empty()) {
                valid_files.emplace_back(file_path, sop_class);
                if (std::find(sop_classes.begin(), sop_classes.end(), sop_class) ==
                    sop_classes.end()) {
                    sop_classes.push_back(sop_class);
                }
            }
        } else if (opts.verbose) {
            std::cerr << "Warning: Skipping invalid file: " << file_path.string() << "\n";
        }
    }

    if (valid_files.empty()) {
        std::cerr << "Error: No valid DICOM files found\n";
        return 2;
    }

    std::cout << "Valid DICOM files: " << valid_files.size() << "\n";
    std::cout << "SOP Classes found: " << sop_classes.size() << "\n\n";

    // Configure association
    std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
    std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
    std::cout << "  Called AE:   " << opts.called_ae << "\n\n";

    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "STORE_SCU_001";

    // Build presentation contexts for each SOP Class
    std::string ts_uid = get_transfer_syntax_uid(opts.transfer_syntax);
    uint8_t context_id = 1;

    for (const auto& sop_class : sop_classes) {
        config.proposed_contexts.push_back({
            context_id,
            sop_class,
            {
                ts_uid,
                "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
                "1.2.840.10008.1.2"     // Implicit VR Little Endian
            }
        });
        context_id += 2;  // Context IDs must be odd
    }

    // Establish association
    auto start_time = std::chrono::steady_clock::now();
    auto connect_result = association::connect(opts.host, opts.port, config, default_timeout);

    if (connect_result.is_err()) {
        std::cerr << "Failed to establish association: "
                  << connect_result.error().message << "\n";
        return 2;
    }

    auto& assoc = connect_result.value();
    auto connect_time = std::chrono::steady_clock::now();
    auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        connect_time - start_time);

    std::cout << "Association established in " << connect_duration.count() << " ms\n\n";

    // Create storage SCU
    storage_scu_config scu_config;
    scu_config.continue_on_error = opts.continue_on_error;
    scu_config.response_timeout = default_timeout;

    storage_scu scu{scu_config};

    // Send files
    std::cout << "Sending files...\n";
    size_t success_count = 0;
    size_t failure_count = 0;
    size_t warning_count = 0;

    for (size_t i = 0; i < valid_files.size(); ++i) {
        const auto& [file_path, sop_class] = valid_files[i];

        show_progress(i + 1, valid_files.size());

        auto result = scu.store_file(assoc, file_path);

        if (result.is_ok()) {
            const auto& store_result = result.value();
            if (store_result.is_success()) {
                ++success_count;
                if (opts.verbose) {
                    std::cout << "\n  OK: " << file_path.filename().string();
                }
            } else if (store_result.is_warning()) {
                ++warning_count;
                ++success_count;  // Warnings are still considered successful
                if (opts.verbose) {
                    std::cout << "\n  WARN: " << file_path.filename().string()
                              << " (0x" << std::hex << store_result.status << std::dec << ")";
                }
            } else {
                ++failure_count;
                if (opts.verbose) {
                    std::cout << "\n  FAIL: " << file_path.filename().string()
                              << " - " << store_result.error_comment;
                }
                if (!opts.continue_on_error) {
                    std::cout << "\n";
                    break;
                }
            }
        } else {
            ++failure_count;
            if (opts.verbose) {
                std::cout << "\n  FAIL: " << file_path.filename().string()
                          << " - " << result.error().message;
            }
            if (!opts.continue_on_error) {
                std::cout << "\n";
                break;
            }
        }
    }

    std::cout << "\n\n";

    // Release association gracefully
    std::cout << "Releasing association...\n";
    auto release_result = assoc.release(default_timeout);
    if (release_result.is_err()) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Print summary
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "              Summary\n";
    std::cout << "========================================\n";
    std::cout << "  Files processed:  " << valid_files.size() << "\n";
    std::cout << "  Successful:       " << success_count << "\n";
    if (warning_count > 0) {
        std::cout << "  Warnings:         " << warning_count << "\n";
    }
    std::cout << "  Failed:           " << failure_count << "\n";
    std::cout << "  Data sent:        " << format_size(scu.bytes_sent()) << "\n";
    std::cout << "  Total time:       " << total_duration.count() << " ms\n";

    if (valid_files.size() > 0) {
        auto avg_time = total_duration.count() / valid_files.size();
        std::cout << "  Avg time/file:    " << avg_time << " ms\n";
    }

    std::cout << "========================================\n";

    if (failure_count > 0) {
        std::cout << "  Status: PARTIAL FAILURE\n";
        return 1;
    }

    std::cout << "  Status: SUCCESS\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____ _____ ___  ____  _____   ____   ____ _   _
 / ___|_   _/ _ \|  _ \| ____| / ___| / ___| | | |
 \___ \ | || | | | |_) |  _|   \___ \| |   | | | |
  ___) || || |_| |  _ <| |___   ___) | |___| |_| |
 |____/ |_| \___/|_| \_\_____| |____/ \____|\___/

          DICOM Image Sender
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 2;
    }

    // Check if path exists
    if (!std::filesystem::exists(opts.path)) {
        std::cerr << "Error: Path does not exist: " << opts.path.string() << "\n";
        return 2;
    }

    return send_files(opts);
}
