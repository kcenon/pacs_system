/**
 * @file main.cpp
 * @brief Query/Retrieve SCP - DICOM Query/Retrieve Server
 *
 * A command-line server for handling DICOM Query/Retrieve operations.
 * Supports C-FIND for querying and C-MOVE/C-GET for retrieving DICOM images.
 *
 * @see Issue #380 - qr_scp: Implement Query/Retrieve SCP utility
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 *
 * Usage:
 *   qr_scp <port> <ae_title> --storage-dir <path> [options]
 *
 * Examples:
 *   qr_scp 11112 MY_PACS --storage-dir ./dicom --index-db ./pacs.db
 *   qr_scp 11112 MY_PACS --storage-dir ./dicom --peer VIEWER:192.168.1.10:11113
 */

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/query_scp.hpp"
#include "pacs/services/retrieve_scp.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/storage/index_database.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// Global pointer to server for signal handling
std::atomic<pacs::network::dicom_server*> g_server{nullptr};

/// Global running flag for signal handling
std::atomic<bool> g_running{true};

/**
 * @brief Signal handler for graceful shutdown
 * @param signal The signal received
 */
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running = false;

    auto* server = g_server.load();
    if (server) {
        server->stop();
    }
}

/**
 * @brief Install signal handlers for graceful shutdown
 */
void install_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifndef _WIN32
    std::signal(SIGHUP, signal_handler);
#endif
}

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Query/Retrieve SCP - DICOM Query/Retrieve Server

Usage: )" << program_name << R"( <port> <ae_title> [options]

Arguments:
  port            Port number to listen on (typically 104 or 11112)
  ae_title        Application Entity Title for this server (max 16 chars)

Required Options:
  --storage-dir <path>    Directory containing DICOM files to serve

Optional Options:
  --index-db <path>       SQLite database for indexing (default: in-memory)
  --peer <spec>           Known peer for C-MOVE (format: AE:host:port)
                          Can be specified multiple times
  --max-assoc <n>         Maximum concurrent associations (default: 10)
  --timeout <sec>         Idle timeout in seconds (default: 300)
  --scan-only             Scan storage and exit (for indexing)
  --help                  Show this help message

Examples:
  )" << program_name << R"( 11112 MY_PACS --storage-dir ./dicom
  )" << program_name << R"( 11112 MY_PACS --storage-dir ./dicom --index-db ./pacs.db
  )" << program_name << R"( 11112 MY_PACS --storage-dir ./dicom --peer VIEWER:192.168.1.10:11113
  )" << program_name << R"( 11112 MY_PACS --storage-dir ./dicom --peer WS1:10.0.0.1:104 --peer WS2:10.0.0.2:104

Notes:
  - Press Ctrl+C to stop the server gracefully
  - Files are indexed on startup from the storage directory
  - C-FIND supports Patient Root and Study Root queries
  - C-MOVE requires known peers to be configured with --peer
  - C-GET sends files directly to the requesting SCU

Exit Codes:
  0  Normal termination
  1  Error - Failed to start server or invalid arguments
)";
}

/**
 * @brief Peer configuration structure
 */
struct peer_config {
    std::string ae_title;
    std::string host;
    uint16_t port;
};

/**
 * @brief Configuration structure for command-line arguments
 */
struct qr_scp_args {
    uint16_t port = 0;
    std::string ae_title;
    std::filesystem::path storage_dir;
    std::filesystem::path index_db;
    std::vector<peer_config> peers;
    size_t max_associations = 10;
    uint32_t idle_timeout = 300;
    bool scan_only = false;
};

/**
 * @brief Parse a peer specification string
 * @param spec Peer spec in format "AE:host:port"
 * @param peer Output peer configuration
 * @return true if parsed successfully
 */
bool parse_peer(const std::string& spec, peer_config& peer) {
    size_t first_colon = spec.find(':');
    if (first_colon == std::string::npos) {
        return false;
    }

    size_t last_colon = spec.rfind(':');
    if (last_colon == first_colon || last_colon == spec.length() - 1) {
        return false;
    }

    peer.ae_title = spec.substr(0, first_colon);
    peer.host = spec.substr(first_colon + 1, last_colon - first_colon - 1);

    try {
        int port_int = std::stoi(spec.substr(last_colon + 1));
        if (port_int < 1 || port_int > 65535) {
            return false;
        }
        peer.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        return false;
    }

    if (peer.ae_title.empty() || peer.ae_title.length() > 16 || peer.host.empty()) {
        return false;
    }

    return true;
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param args Output: parsed arguments
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], qr_scp_args& args) {
    if (argc < 3) {
        return false;
    }

    // Check for help flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            return false;
        }
    }

    // Parse port
    try {
        int port_int = std::stoi(argv[1]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        args.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[1] << "'\n";
        return false;
    }

    // Parse AE title
    args.ae_title = argv[2];
    if (args.ae_title.length() > 16) {
        std::cerr << "Error: AE title exceeds 16 characters\n";
        return false;
    }

    // Parse optional arguments
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--storage-dir" && i + 1 < argc) {
            args.storage_dir = argv[++i];
        } else if (arg == "--index-db" && i + 1 < argc) {
            args.index_db = argv[++i];
        } else if (arg == "--peer" && i + 1 < argc) {
            peer_config peer;
            if (!parse_peer(argv[++i], peer)) {
                std::cerr << "Error: Invalid peer format. Use AE:host:port\n";
                return false;
            }
            args.peers.push_back(peer);
        } else if (arg == "--max-assoc" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 1) {
                    std::cerr << "Error: max-assoc must be positive\n";
                    return false;
                }
                args.max_associations = static_cast<size_t>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid max-assoc value\n";
                return false;
            }
        } else if (arg == "--timeout" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 0) {
                    std::cerr << "Error: timeout cannot be negative\n";
                    return false;
                }
                args.idle_timeout = static_cast<uint32_t>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid timeout value\n";
                return false;
            }
        } else if (arg == "--scan-only") {
            args.scan_only = true;
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    // Validate required arguments
    if (args.storage_dir.empty()) {
        std::cerr << "Error: --storage-dir is required\n";
        return false;
    }

    return true;
}

/**
 * @brief Format timestamp for logging
 * @return Current time as formatted string
 *
 * Uses thread-safe time conversion for multi-association handling.
 */
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

/**
 * @brief Format byte count for display
 */
std::string format_bytes(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        ++unit_index;
    }

    std::ostringstream oss;
    if (unit_index == 0) {
        oss << bytes << " " << units[unit_index];
    } else {
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    }
    return oss.str();
}

/**
 * @brief Scan storage directory and index DICOM files
 * @param storage_dir Directory to scan
 * @param db Index database
 * @return Number of files indexed
 */
size_t scan_storage(
    const std::filesystem::path& storage_dir,
    pacs::storage::index_database& db) {

    using namespace pacs::core;
    namespace fs = std::filesystem;

    size_t count = 0;
    size_t errors = 0;

    std::cout << "Scanning " << storage_dir << "...\n";

    for (const auto& entry : fs::recursive_directory_iterator(storage_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        // Try common DICOM extensions
        auto ext = entry.path().extension().string();
        bool is_dcm = ext == ".dcm" || ext == ".DCM" || ext.empty();
        if (!is_dcm) {
            continue;
        }

        try {
            auto file = dicom_file::open(entry.path());
            if (file.is_err()) {
                ++errors;
                continue;
            }

            const auto& dataset = file.value().dataset();

            // Extract patient info
            auto patient_id = dataset.get_string(tags::patient_id, "");
            auto patient_name = dataset.get_string(tags::patient_name, "");
            auto birth_date = dataset.get_string(tags::patient_birth_date, "");
            auto sex = dataset.get_string(tags::patient_sex, "");

            // Insert/update patient
            auto patient_pk = db.upsert_patient(patient_id, patient_name, birth_date, sex);
            if (patient_pk.is_err()) {
                ++errors;
                continue;
            }

            // Extract study info
            auto study_uid = dataset.get_string(tags::study_instance_uid, "");
            auto study_id = dataset.get_string(tags::study_id, "");
            auto study_date = dataset.get_string(tags::study_date, "");
            auto study_time = dataset.get_string(tags::study_time, "");
            auto accession = dataset.get_string(tags::accession_number, "");
            auto ref_phys = dataset.get_string(tags::referring_physician_name, "");
            auto study_desc = dataset.get_string(tags::study_description, "");

            // Insert/update study
            auto study_pk = db.upsert_study(
                patient_pk.value(), study_uid, study_id, study_date, study_time,
                accession, ref_phys, study_desc);
            if (study_pk.is_err()) {
                ++errors;
                continue;
            }

            // Extract series info
            auto series_uid = dataset.get_string(tags::series_instance_uid, "");
            auto modality = dataset.get_string(tags::modality, "");
            auto series_num_str = dataset.get_string(tags::series_number, "");
            auto series_desc = dataset.get_string(tags::series_description, "");
            auto body_part = std::string{}; // body_part_examined not in tag constants
            auto station = dataset.get_string(tags::station_name, "");

            std::optional<int> series_num;
            if (!series_num_str.empty()) {
                try {
                    series_num = std::stoi(series_num_str);
                } catch (...) {}
            }

            // Insert/update series
            auto series_pk = db.upsert_series(
                study_pk.value(), series_uid, modality, series_num,
                series_desc, body_part, station);
            if (series_pk.is_err()) {
                ++errors;
                continue;
            }

            // Extract instance info
            auto sop_uid = dataset.get_string(tags::sop_instance_uid, "");
            auto sop_class = dataset.get_string(tags::sop_class_uid, "");
            auto inst_num_str = dataset.get_string(tags::instance_number, "");
            auto transfer_syntax_uid = file.value().transfer_syntax().uid();

            std::optional<int> inst_num;
            if (!inst_num_str.empty()) {
                try {
                    inst_num = std::stoi(inst_num_str);
                } catch (...) {}
            }

            auto file_size = static_cast<int64_t>(fs::file_size(entry.path()));

            // Insert/update instance
            auto instance_pk = db.upsert_instance(
                series_pk.value(), sop_uid, sop_class,
                entry.path().string(), file_size, transfer_syntax_uid, inst_num);

            if (instance_pk.is_ok()) {
                ++count;
                if (count % 100 == 0) {
                    std::cout << "  Indexed " << count << " files...\n";
                }
            } else {
                ++errors;
            }
        } catch (const std::exception& e) {
            ++errors;
        }
    }

    std::cout << "Scan complete: " << count << " files indexed";
    if (errors > 0) {
        std::cout << " (" << errors << " errors)";
    }
    std::cout << "\n";

    return count;
}

/**
 * @brief Build query response datasets from database records
 */
std::vector<pacs::core::dicom_dataset> handle_query(
    pacs::services::query_level level,
    const pacs::core::dicom_dataset& query_keys,
    [[maybe_unused]] const std::string& calling_ae,
    pacs::storage::index_database& db) {

    using namespace pacs::core;
    using namespace pacs::storage;
    using namespace pacs::services;
    using namespace pacs::encoding;

    std::vector<dicom_dataset> results;

    try {
        switch (level) {
            case query_level::patient: {
                patient_query pq;
                pq.patient_id = query_keys.get_string(tags::patient_id, "");
                pq.patient_name = query_keys.get_string(tags::patient_name, "");

                auto patients = db.search_patients(pq);
                if (patients.is_ok()) {
                    for (const auto& p : patients.value()) {
                        dicom_dataset ds;
                        ds.set_string(tags::query_retrieve_level, vr_type::CS, "PATIENT");
                        ds.set_string(tags::patient_id, vr_type::LO, p.patient_id);
                        ds.set_string(tags::patient_name, vr_type::PN, p.patient_name);
                        ds.set_string(tags::patient_birth_date, vr_type::DA, p.birth_date);
                        ds.set_string(tags::patient_sex, vr_type::CS, p.sex);
                        results.push_back(std::move(ds));
                    }
                }
                break;
            }

            case query_level::study: {
                study_query sq;
                sq.patient_id = query_keys.get_string(tags::patient_id, "");
                sq.patient_name = query_keys.get_string(tags::patient_name, "");
                sq.study_uid = query_keys.get_string(tags::study_instance_uid, "");
                sq.study_date = query_keys.get_string(tags::study_date, "");
                sq.accession_number = query_keys.get_string(tags::accession_number, "");
                sq.study_description = query_keys.get_string(tags::study_description, "");

                auto studies = db.search_studies(sq);
                if (studies.is_ok()) {
                    for (const auto& s : studies.value()) {
                        // Get patient info
                        auto patient = db.find_patient_by_pk(s.patient_pk);

                        dicom_dataset ds;
                        ds.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");
                        if (patient) {
                            ds.set_string(tags::patient_id, vr_type::LO, patient->patient_id);
                            ds.set_string(tags::patient_name, vr_type::PN, patient->patient_name);
                            ds.set_string(tags::patient_birth_date, vr_type::DA, patient->birth_date);
                            ds.set_string(tags::patient_sex, vr_type::CS, patient->sex);
                        }
                        ds.set_string(tags::study_instance_uid, vr_type::UI, s.study_uid);
                        ds.set_string(tags::study_id, vr_type::SH, s.study_id);
                        ds.set_string(tags::study_date, vr_type::DA, s.study_date);
                        ds.set_string(tags::study_time, vr_type::TM, s.study_time);
                        ds.set_string(tags::accession_number, vr_type::SH, s.accession_number);
                        ds.set_string(tags::referring_physician_name, vr_type::PN, s.referring_physician);
                        ds.set_string(tags::study_description, vr_type::LO, s.study_description);
                        ds.set_string(tags::modalities_in_study, vr_type::CS, s.modalities_in_study);
                        results.push_back(std::move(ds));
                    }
                }
                break;
            }

            case query_level::series: {
                series_query serq;
                serq.study_uid = query_keys.get_string(tags::study_instance_uid, "");
                serq.series_uid = query_keys.get_string(tags::series_instance_uid, "");
                serq.modality = query_keys.get_string(tags::modality, "");
                serq.series_description = query_keys.get_string(tags::series_description, "");

                auto series_list = db.search_series(serq);
                if (series_list.is_ok()) {
                    for (const auto& ser : series_list.value()) {
                        // Get study info
                        auto study = db.find_study_by_pk(ser.study_pk);

                        dicom_dataset ds;
                        ds.set_string(tags::query_retrieve_level, vr_type::CS, "SERIES");
                        if (study) {
                            ds.set_string(tags::study_instance_uid, vr_type::UI, study->study_uid);
                        }
                        ds.set_string(tags::series_instance_uid, vr_type::UI, ser.series_uid);
                        ds.set_string(tags::modality, vr_type::CS, ser.modality);
                        if (ser.series_number.has_value()) {
                            ds.set_string(tags::series_number, vr_type::IS,
                                std::to_string(ser.series_number.value()));
                        }
                        ds.set_string(tags::series_description, vr_type::LO, ser.series_description);
                        // body_part_examined tag not in constants, skip
                        results.push_back(std::move(ds));
                    }
                }
                break;
            }

            case query_level::image: {
                instance_query iq;
                iq.series_uid = query_keys.get_string(tags::series_instance_uid, "");
                iq.sop_uid = query_keys.get_string(tags::sop_instance_uid, "");
                iq.sop_class_uid = query_keys.get_string(tags::sop_class_uid, "");

                auto instances = db.search_instances(iq);
                if (instances.is_ok()) {
                    for (const auto& inst : instances.value()) {
                        // Get series info
                        auto series = db.find_series_by_pk(inst.series_pk);

                        dicom_dataset ds;
                        ds.set_string(tags::query_retrieve_level, vr_type::CS, "IMAGE");
                        if (series) {
                            ds.set_string(tags::series_instance_uid, vr_type::UI, series->series_uid);

                            // Get study info
                            auto study = db.find_study_by_pk(series->study_pk);
                            if (study) {
                                ds.set_string(tags::study_instance_uid, vr_type::UI, study->study_uid);
                            }
                        }
                        ds.set_string(tags::sop_instance_uid, vr_type::UI, inst.sop_uid);
                        ds.set_string(tags::sop_class_uid, vr_type::UI, inst.sop_class_uid);
                        if (inst.instance_number.has_value()) {
                            ds.set_string(tags::instance_number, vr_type::IS,
                                std::to_string(inst.instance_number.value()));
                        }
                        results.push_back(std::move(ds));
                    }
                }
                break;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[" << current_timestamp() << "] Query error: " << e.what() << "\n";
    }

    return results;
}

/**
 * @brief Handle retrieve request - find matching DICOM files
 */
std::vector<pacs::core::dicom_file> handle_retrieve(
    const pacs::core::dicom_dataset& query_keys,
    pacs::storage::index_database& db) {

    using namespace pacs::core;
    using namespace pacs::storage;

    std::vector<dicom_file> files;

    try {
        // Determine query level from keys
        auto sop_uid = query_keys.get_string(tags::sop_instance_uid, "");
        auto series_uid = query_keys.get_string(tags::series_instance_uid, "");
        auto study_uid = query_keys.get_string(tags::study_instance_uid, "");
        auto patient_id = query_keys.get_string(tags::patient_id, "");

        std::vector<instance_record> instances;

        if (!sop_uid.empty()) {
            // Instance level retrieve
            auto inst = db.find_instance(sop_uid);
            if (inst) {
                instances.push_back(*inst);
            }
        } else if (!series_uid.empty()) {
            // Series level retrieve
            auto result = db.list_instances(series_uid);
            if (result.is_ok()) {
                instances = std::move(result.value());
            }
        } else if (!study_uid.empty()) {
            // Study level retrieve - get all series first
            auto series_result = db.list_series(study_uid);
            if (series_result.is_ok()) {
                for (const auto& ser : series_result.value()) {
                    auto inst_result = db.list_instances(ser.series_uid);
                    if (inst_result.is_ok()) {
                        for (auto& inst : inst_result.value()) {
                            instances.push_back(std::move(inst));
                        }
                    }
                }
            }
        } else if (!patient_id.empty()) {
            // Patient level retrieve - get all studies first
            auto studies_result = db.list_studies(patient_id);
            if (studies_result.is_ok()) {
                for (const auto& study : studies_result.value()) {
                    auto series_result = db.list_series(study.study_uid);
                    if (series_result.is_ok()) {
                        for (const auto& ser : series_result.value()) {
                            auto inst_result = db.list_instances(ser.series_uid);
                            if (inst_result.is_ok()) {
                                for (auto& inst : inst_result.value()) {
                                    instances.push_back(std::move(inst));
                                }
                            }
                        }
                    }
                }
            }
        }

        // Load files
        for (const auto& inst : instances) {
            auto file = dicom_file::open(inst.file_path);
            if (file.is_ok()) {
                files.push_back(std::move(file.value()));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[" << current_timestamp() << "] Retrieve error: " << e.what() << "\n";
    }

    return files;
}

/**
 * @brief Run the Query/Retrieve SCP server
 * @param args Parsed command-line arguments
 * @return true if server ran successfully
 */
bool run_server(const qr_scp_args& args) {
    using namespace pacs::network;
    using namespace pacs::services;
    using namespace pacs::storage;
    using namespace pacs::core;

    std::cout << "\nStarting Query/Retrieve SCP...\n";
    std::cout << "  AE Title:           " << args.ae_title << "\n";
    std::cout << "  Port:               " << args.port << "\n";
    std::cout << "  Storage Directory:  " << args.storage_dir << "\n";
    if (!args.index_db.empty()) {
        std::cout << "  Index Database:     " << args.index_db << "\n";
    } else {
        std::cout << "  Index Database:     (in-memory)\n";
    }
    std::cout << "  Max Associations:   " << args.max_associations << "\n";
    std::cout << "  Idle Timeout:       " << args.idle_timeout << " seconds\n";
    if (!args.peers.empty()) {
        std::cout << "  Known Peers:\n";
        for (const auto& peer : args.peers) {
            std::cout << "    - " << peer.ae_title << " -> "
                      << peer.host << ":" << peer.port << "\n";
        }
    }
    std::cout << "\n";

    // Verify storage directory exists
    std::error_code ec;
    if (!std::filesystem::exists(args.storage_dir, ec)) {
        std::cerr << "Error: Storage directory does not exist: " << args.storage_dir << "\n";
        return false;
    }

    // Open index database
    std::string db_path = args.index_db.empty() ? ":memory:" : args.index_db.string();
    auto db_result = index_database::open(db_path);
    if (db_result.is_err()) {
        std::cerr << "Failed to open database: " << db_result.error().message << "\n";
        return false;
    }
    auto db = std::move(db_result.value());

    // Scan storage and build index
    auto indexed = scan_storage(args.storage_dir, *db);

    if (args.scan_only) {
        std::cout << "\nScan complete. Exiting.\n";
        return true;
    }

    if (indexed == 0) {
        std::cout << "\nWarning: No DICOM files found in storage directory.\n";
        std::cout << "         Server will start but queries will return no results.\n\n";
    }

    // Build peer map for destination resolution
    std::map<std::string, std::pair<std::string, uint16_t>> peer_map;
    for (const auto& peer : args.peers) {
        peer_map[peer.ae_title] = {peer.host, peer.port};
    }

    // Configure server
    server_config config;
    config.ae_title = args.ae_title;
    config.port = args.port;
    config.max_associations = args.max_associations;
    config.idle_timeout = std::chrono::seconds{args.idle_timeout};
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "QR_SCP_001";

    // Create server
    dicom_server server{config};
    g_server = &server;

    // Register Verification service (C-ECHO)
    server.register_service(std::make_shared<verification_scp>());

    // Configure Query SCP
    auto query_service = std::make_shared<query_scp>();
    query_service->set_handler(
        [&db](query_level level, const dicom_dataset& keys, const std::string& ae) {
            return handle_query(level, keys, ae, *db);
        });

    server.register_service(query_service);

    // Configure Retrieve SCP
    auto retrieve_service = std::make_shared<retrieve_scp>();
    retrieve_service->set_retrieve_handler(
        [&db](const dicom_dataset& keys) {
            return handle_retrieve(keys, *db);
        });

    // Set destination resolver for C-MOVE
    retrieve_service->set_destination_resolver(
        [&peer_map](const std::string& ae_title)
            -> std::optional<std::pair<std::string, uint16_t>> {
            auto it = peer_map.find(ae_title);
            if (it != peer_map.end()) {
                return it->second;
            }
            return std::nullopt;
        });

    server.register_service(retrieve_service);

    // Set up callbacks for logging
    server.on_association_established([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "Association established from: " << assoc.calling_ae()
                  << " -> " << assoc.called_ae() << "\n";
    });

    server.on_association_released([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "Association released: " << assoc.calling_ae() << "\n";
    });

    server.on_error([](const std::string& error) {
        std::cerr << "[" << current_timestamp() << "] "
                  << "Error: " << error << "\n";
    });

    // Start server
    auto result = server.start();
    if (result.is_err()) {
        std::cerr << "Failed to start server: " << result.error().message << "\n";
        g_server = nullptr;
        return false;
    }

    std::cout << "=================================================\n";
    std::cout << " Query/Retrieve SCP is running on port " << args.port << "\n";
    std::cout << " Storage: " << args.storage_dir << "\n";
    std::cout << " Indexed: " << indexed << " DICOM files\n";
    std::cout << " Press Ctrl+C to stop\n";
    std::cout << "=================================================\n\n";

    // Wait for shutdown
    server.wait_for_shutdown();

    // Print final statistics
    auto server_stats = server.get_statistics();

    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << " Server Statistics\n";
    std::cout << "=================================================\n";
    std::cout << "  Total Associations:    " << server_stats.total_associations << "\n";
    std::cout << "  Rejected Associations: " << server_stats.rejected_associations << "\n";
    std::cout << "  Messages Processed:    " << server_stats.messages_processed << "\n";
    std::cout << "  Queries Processed:     " << query_service->queries_processed() << "\n";
    std::cout << "  C-MOVE Operations:     " << retrieve_service->move_operations() << "\n";
    std::cout << "  C-GET Operations:      " << retrieve_service->get_operations() << "\n";
    std::cout << "  Images Transferred:    " << retrieve_service->images_transferred() << "\n";
    std::cout << "  Bytes Received:        " << format_bytes(server_stats.bytes_received) << "\n";
    std::cout << "  Bytes Sent:            " << format_bytes(server_stats.bytes_sent) << "\n";
    std::cout << "  Uptime:                " << server_stats.uptime().count() << " seconds\n";
    std::cout << "=================================================\n";

    g_server = nullptr;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
   ___  ____    ____   ____ ____
  / _ \|  _ \  / ___| / ___|  _ \
 | | | | |_) | \___ \| |   | |_) |
 | |_| |  _ <   ___) | |___|  __/
  \__\_\_| \_\ |____/ \____|_|

     DICOM Query/Retrieve Server
)" << "\n";

    qr_scp_args args;

    if (!parse_arguments(argc, argv, args)) {
        print_usage(argv[0]);
        return 1;
    }

    // Install signal handlers
    install_signal_handlers();

    bool success = run_server(args);

    std::cout << "\nQuery/Retrieve SCP terminated\n";
    return success ? 0 : 1;
}
