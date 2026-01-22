/**
 * @file main.cpp
 * @brief Level 3 Sample: Storage Server - DICOM image reception and archiving
 *
 * This sample demonstrates DICOM storage concepts:
 * - Storage SCP configuration (accepting images from modalities)
 * - File storage with hierarchical organization
 * - Index database for fast metadata queries
 * - Pre/post storage hooks for validation and workflow
 *
 * After completing this sample, you will understand how to:
 * 1. Configure a DICOM storage server to receive images
 * 2. Organize DICOM files in a hierarchical directory structure
 * 3. Index metadata in SQLite for efficient queries
 * 4. Implement validation and post-processing hooks
 *
 * @see DICOM PS3.4 Section B - Storage Service Class
 * @see DICOM PS3.7 Section 9.1.1 - C-STORE Service
 */

#include "console_utils.hpp"
#include "signal_handler.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/network/dicom_server.hpp>
#include <pacs/network/server_config.hpp>
#include <pacs/services/storage_scp.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/storage/file_storage.hpp>
#include <pacs/storage/index_database.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

namespace net = pacs::network;
namespace svc = pacs::services;
namespace stg = pacs::storage;
namespace core = pacs::core;
namespace tags = pacs::core::tags;

namespace {

/**
 * @brief Format timestamp for logging
 * @return Current time as formatted string
 */
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

/**
 * @brief Update the index database with dataset information
 *
 * This function demonstrates the database indexing workflow:
 * 1. Upsert patient record
 * 2. Upsert study record
 * 3. Upsert series record
 * 4. Insert instance record
 *
 * @param db The index database instance
 * @param ds The DICOM dataset to index
 * @param file_path Path where the file is stored
 * @return true on success, false on failure
 */
bool update_index(stg::index_database& db,
                  const core::dicom_dataset& ds,
                  const std::filesystem::path& file_path) {
    // Patient information
    auto patient_result = db.upsert_patient(
        ds.get_string(tags::patient_id),
        ds.get_string(tags::patient_name),
        ds.get_string(tags::patient_birth_date),
        ds.get_string(tags::patient_sex)
    );

    if (patient_result.is_err()) {
        std::cerr << "  Database error (patient): "
                  << patient_result.error().message << "\n";
        return false;
    }
    int64_t patient_pk = patient_result.value();

    // Study information
    auto study_result = db.upsert_study(
        patient_pk,
        ds.get_string(tags::study_instance_uid),
        ds.get_string(tags::study_id),
        ds.get_string(tags::study_date),
        ds.get_string(tags::study_time),
        ds.get_string(tags::accession_number),
        ds.get_string(tags::referring_physician_name),
        ds.get_string(tags::study_description)
    );

    if (study_result.is_err()) {
        std::cerr << "  Database error (study): "
                  << study_result.error().message << "\n";
        return false;
    }
    int64_t study_pk = study_result.value();

    // Series information
    // Note: body_part_examined tag not defined in current core library,
    // pass empty string as placeholder
    auto series_result = db.upsert_series(
        study_pk,
        ds.get_string(tags::series_instance_uid),
        ds.get_string(tags::modality),
        ds.get_numeric<int>(tags::series_number),
        ds.get_string(tags::series_description),
        "",  // body_part_examined - not available in current tag definitions
        ds.get_string(tags::station_name)
    );

    if (series_result.is_err()) {
        std::cerr << "  Database error (series): "
                  << series_result.error().message << "\n";
        return false;
    }
    int64_t series_pk = series_result.value();

    // Instance information
    auto file_size = std::filesystem::exists(file_path)
        ? static_cast<int64_t>(std::filesystem::file_size(file_path))
        : 0;

    auto instance_result = db.upsert_instance(
        series_pk,
        ds.get_string(tags::sop_instance_uid),
        ds.get_string(tags::sop_class_uid),
        file_path.string(),
        file_size,
        ds.get_string(tags::transfer_syntax_uid),
        ds.get_numeric<int>(tags::instance_number)
    );

    if (instance_result.is_err()) {
        std::cerr << "  Database error (instance): "
                  << instance_result.error().message << "\n";
        return false;
    }

    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    pacs::samples::print_header("Storage Server - Level 3 Sample");

    // ===========================================================================
    // Part 1: Storage Configuration
    // ===========================================================================
    // DICOM storage systems need several components:
    // - Root directory for storing DICOM files
    // - Naming scheme for file organization (UID-based hierarchy is common)
    // - Duplicate handling policy (reject, replace, or ignore)
    // - File extension for saved files

    pacs::samples::print_section("Part 1: Storage Configuration");

    std::cout << "DICOM storage systems organize files hierarchically:\n";
    std::cout << "  - UID-based:  {root}/{StudyUID}/{SeriesUID}/{SOPUID}.dcm\n";
    std::cout << "  - Date-based: {root}/YYYY/MM/DD/{StudyUID}/{SOPUID}.dcm\n";
    std::cout << "  - Flat:       {root}/{SOPUID}.dcm\n\n";

    // Create storage root directory
    const std::filesystem::path storage_root = "./dicom_archive";
    std::filesystem::create_directories(storage_root);

    // Configure file storage
    stg::file_storage_config fs_config;
    fs_config.root_path = storage_root;
    fs_config.naming = stg::naming_scheme::uid_hierarchical;
    fs_config.duplicate = stg::duplicate_policy::replace;
    fs_config.create_directories = true;
    fs_config.file_extension = ".dcm";

    pacs::samples::print_table("File Storage Configuration", {
        {"Root Path", storage_root.string()},
        {"Naming Scheme", "uid_hierarchical"},
        {"Duplicate Policy", "replace"},
        {"File Extension", ".dcm"}
    });

    // Create file storage backend
    auto file_storage = std::make_shared<stg::file_storage>(fs_config);

    pacs::samples::print_success("Part 1 complete - File storage configured!");

    // ===========================================================================
    // Part 2: Index Database
    // ===========================================================================
    // The index database enables fast queries without reading every DICOM file.
    // It stores:
    // - Patient demographics (name, ID, birth date, sex)
    // - Study metadata (date, description, accession number)
    // - Series metadata (modality, series number, description)
    // - Instance references (SOP UID, file path, file size)

    pacs::samples::print_section("Part 2: Index Database");

    std::cout << "Index database stores metadata for fast queries:\n";
    std::cout << "  - Patient demographics\n";
    std::cout << "  - Study/Series/Instance metadata\n";
    std::cout << "  - File paths for retrieval\n\n";

    const auto db_path = storage_root / "index.db";

    auto db_result = stg::index_database::open(db_path.string());
    if (db_result.is_err()) {
        pacs::samples::print_error("Failed to open database: " +
                                   db_result.error().message);
        return 1;
    }
    auto index_db = std::move(db_result.value());

    pacs::samples::print_table("Database Configuration", {
        {"Database Path", db_path.string()},
        {"Storage Engine", "SQLite"},
        {"Mode", "WAL (Write-Ahead Logging)"}
    });

    pacs::samples::print_success("Part 2 complete - Database initialized!");

    // ===========================================================================
    // Part 3: Storage SCP Setup
    // ===========================================================================
    // Storage SCP (Service Class Provider) handles incoming C-STORE requests.
    // Key configuration options:
    // - Accepted SOP Classes: Which image types to accept (CT, MR, US, etc.)
    // - Duplicate Policy: How to handle images that already exist
    // - Handler callbacks: Custom logic for storage, validation, post-processing

    pacs::samples::print_section("Part 3: Storage SCP Setup");

    std::cout << "Storage SCP handles C-STORE requests from modalities:\n";
    std::cout << "  1. Pre-store handler: Validate incoming data\n";
    std::cout << "  2. Storage handler: Store to filesystem + index\n";
    std::cout << "  3. Post-store handler: Trigger workflows\n\n";

    // Configure Storage SCP (empty = accept all standard storage SOP classes)
    svc::storage_scp_config scp_config;
    // scp_config.accepted_sop_classes = { ... }; // Leave empty to accept all

    auto storage_scp = std::make_shared<svc::storage_scp>(scp_config);

    // Statistics tracking
    std::atomic<int> store_count{0};
    std::atomic<int> reject_count{0};

    // Main storage handler - called for each received image
    storage_scp->set_handler(
        [&](const core::dicom_dataset& dataset,
            const std::string& calling_ae,
            const std::string& sop_class_uid,
            const std::string& sop_instance_uid) -> svc::storage_status {

            std::cout << "\n[" << current_timestamp() << "] "
                      << pacs::samples::colors::cyan << "[C-STORE]"
                      << pacs::samples::colors::reset << " From: " << calling_ae << "\n";

            // Extract key metadata for logging
            std::cout << "  Patient:  " << dataset.get_string(tags::patient_name) << "\n";
            std::cout << "  Study:    " << dataset.get_string(tags::study_description) << "\n";
            std::cout << "  Modality: " << dataset.get_string(tags::modality) << "\n";
            std::cout << "  SOP Class: " << sop_class_uid << "\n";

            // Store to filesystem
            auto store_result = file_storage->store(dataset);
            if (store_result.is_err()) {
                std::cout << "  " << pacs::samples::colors::red << "Storage failed: "
                          << pacs::samples::colors::reset
                          << store_result.error().message << "\n";
                reject_count++;
                return svc::storage_status::storage_error;
            }

            // Get the stored file path for database indexing
            auto file_path = file_storage->get_file_path(sop_instance_uid);

            // Update index database
            if (!update_index(*index_db, dataset, file_path)) {
                reject_count++;
                return svc::storage_status::storage_error;
            }

            store_count++;
            std::cout << "  " << pacs::samples::colors::green << "Stored"
                      << pacs::samples::colors::reset << " (#" << store_count
                      << ") -> " << file_path.filename().string() << "\n";

            return svc::storage_status::success;
        }
    );

    // Pre-store validation handler (optional)
    // Return false to reject the storage request
    storage_scp->set_pre_store_handler(
        [&reject_count](const core::dicom_dataset& dataset) -> bool {
            // Example validation: Require Patient ID
            if (dataset.get_string(tags::patient_id).empty()) {
                std::cout << "  " << pacs::samples::colors::yellow << "[REJECTED]"
                          << pacs::samples::colors::reset
                          << " Missing Patient ID\n";
                reject_count++;
                return false;
            }

            // Example validation: Require Study Instance UID
            if (dataset.get_string(tags::study_instance_uid).empty()) {
                std::cout << "  " << pacs::samples::colors::yellow << "[REJECTED]"
                          << pacs::samples::colors::reset
                          << " Missing Study Instance UID\n";
                reject_count++;
                return false;
            }

            return true;  // Accept the image
        }
    );

    // Post-store notification handler (optional)
    // Called after successful storage for workflow integration
    storage_scp->set_post_store_handler(
        [](const core::dicom_dataset& /*dataset*/,
           const std::string& /*patient_id*/,
           const std::string& /*study_uid*/,
           const std::string& /*series_uid*/,
           const std::string& /*sop_instance_uid*/) {
            // Example: Trigger downstream processing
            // - Send to AI analysis service
            // - Notify worklist system
            // - Queue for auto-routing
            // For this sample, we just acknowledge the callback
        }
    );

    pacs::samples::print_table("Storage SCP Configuration", {
        {"SOP Classes", "All standard storage classes"},
        {"Duplicate Policy", "Replace existing"},
        {"Pre-store Handler", "Validate Patient ID and Study UID"},
        {"Post-store Handler", "Workflow notification"}
    });

    pacs::samples::print_success("Part 3 complete - Storage SCP configured!");

    // ===========================================================================
    // Part 4: Server Startup
    // ===========================================================================
    // Create the DICOM server and register services:
    // - Verification SCP (C-ECHO) for connectivity testing
    // - Storage SCP (C-STORE) for receiving images

    pacs::samples::print_section("Part 4: Running Server");

    // Parse optional port argument
    uint16_t port = 11112;
    if (argc > 1) {
        try {
            int port_arg = std::stoi(argv[1]);
            if (port_arg > 0 && port_arg < 65536) {
                port = static_cast<uint16_t>(port_arg);
            }
        } catch (const std::exception&) {
            // Keep default port
        }
    }

    // Configure server
    net::server_config server_config;
    server_config.ae_title = "STORE_SCP";
    server_config.port = port;
    server_config.max_associations = 20;
    server_config.idle_timeout = std::chrono::seconds(60);
    server_config.max_pdu_size = 65536;  // 64KB for efficient image transfer
    server_config.implementation_class_uid = "1.2.410.200001.1.1";
    server_config.implementation_version_name = "PACS_SAMPLE_3.0";

    // Create server and register services
    auto server = std::make_unique<net::dicom_server>(server_config);
    server->register_service(std::make_shared<svc::verification_scp>());
    server->register_service(storage_scp);

    // Connection tracking
    std::atomic<int> active_connections{0};

    server->on_association_established(
        [&active_connections](const net::association& assoc) {
            active_connections++;
            std::cout << "[" << current_timestamp() << "] "
                      << pacs::samples::colors::green << "[CONNECT]"
                      << pacs::samples::colors::reset << " "
                      << assoc.calling_ae() << " -> " << assoc.called_ae()
                      << " (active: " << active_connections << ")\n";
        }
    );

    server->on_association_released(
        [&active_connections](const net::association& assoc) {
            active_connections--;
            std::cout << "[" << current_timestamp() << "] "
                      << pacs::samples::colors::cyan << "[RELEASE]"
                      << pacs::samples::colors::reset << " "
                      << assoc.calling_ae() << " disconnected"
                      << " (active: " << active_connections << ")\n";
        }
    );

    server->on_error(
        [](const std::string& error_msg) {
            std::cerr << "[" << current_timestamp() << "] "
                      << pacs::samples::colors::red << "[ERROR]"
                      << pacs::samples::colors::reset << " "
                      << error_msg << "\n";
        }
    );

    // Install signal handler for graceful shutdown
    pacs::samples::scoped_signal_handler sig_handler([&server]() {
        std::cout << "\n" << pacs::samples::colors::yellow
                  << "Shutdown signal received..."
                  << pacs::samples::colors::reset << "\n";
        server->stop();
    });

    // Start the server
    auto start_result = server->start();
    if (start_result.is_err()) {
        pacs::samples::print_error("Failed to start server: " +
                                   start_result.error().message);
        return 1;
    }

    // Print server running banner
    pacs::samples::print_box({
        "Storage Server Running",
        "",
        "Test with DCMTK:",
        "  storescu -aec STORE_SCP localhost " + std::to_string(port) + " image.dcm",
        "",
        "Generate test data (from Level 1):",
        "  ./hello_dicom  # Creates hello_dicom_output.dcm",
        "  storescu -aec STORE_SCP localhost " + std::to_string(port) +
            " hello_dicom_output.dcm",
        "",
        "Verify connectivity:",
        "  echoscu -aec STORE_SCP localhost " + std::to_string(port),
        "",
        "Press Ctrl+C to stop"
    });

    // Wait for shutdown signal
    sig_handler.wait();

    // ===========================================================================
    // Part 5: Statistics and Cleanup
    // ===========================================================================
    // Display final statistics and verify database contents.

    pacs::samples::print_section("Final Statistics");

    // Server statistics
    auto stats = server->get_statistics();
    pacs::samples::print_table("Server Statistics", {
        {"Total Associations", std::to_string(stats.total_associations)},
        {"Messages Processed", std::to_string(stats.messages_processed)},
        {"Bytes Received", std::to_string(stats.bytes_received)},
        {"Uptime", std::to_string(stats.uptime().count()) + " seconds"}
    });

    // Storage statistics
    pacs::samples::print_table("Storage Statistics", {
        {"Images Stored", std::to_string(store_count.load())},
        {"Images Rejected", std::to_string(reject_count.load())},
        {"Storage Path", storage_root.string()}
    });

    // Database statistics
    auto db_stats_result = index_db->get_storage_stats();
    if (db_stats_result.is_ok()) {
        auto db_stats = db_stats_result.value();
        pacs::samples::print_table("Database Statistics", {
            {"Patients", std::to_string(db_stats.total_patients)},
            {"Studies", std::to_string(db_stats.total_studies)},
            {"Series", std::to_string(db_stats.total_series)},
            {"Instances", std::to_string(db_stats.total_instances)},
            {"Total File Size", std::to_string(db_stats.total_file_size) + " bytes"}
        });
    }

    // Summary
    pacs::samples::print_box({
        "Congratulations! You have learned:",
        "",
        "1. File Storage - Hierarchical DICOM file organization",
        "2. Index Database - SQLite-based metadata indexing",
        "3. Storage SCP - Receiving images via C-STORE",
        "4. Validation - Pre-store data validation hooks",
        "5. Workflow - Post-store notification handlers",
        "",
        "Query the database:",
        "  sqlite3 " + db_path.string() + " \"SELECT * FROM patients;\"",
        "",
        "Next step: Level 4 - Mini PACS (Query/Retrieve)"
    });

    pacs::samples::print_success("Storage Server terminated successfully.");

    return 0;
}
