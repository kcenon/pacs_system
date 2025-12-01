/**
 * @file server_app.cpp
 * @brief PACS Server application implementation
 */

#include "server_app.hpp"

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/storage_status.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace pacs::example {

namespace {

/// Convert string to naming scheme
storage::naming_scheme parse_naming_scheme(const std::string& scheme) {
    if (scheme == "flat") {
        return storage::naming_scheme::flat;
    }
    if (scheme == "date_hierarchical") {
        return storage::naming_scheme::date_hierarchical;
    }
    return storage::naming_scheme::uid_hierarchical;
}

/// Convert string to duplicate policy
storage::duplicate_policy parse_duplicate_policy(const std::string& policy) {
    if (policy == "replace") {
        return storage::duplicate_policy::replace;
    }
    if (policy == "ignore") {
        return storage::duplicate_policy::ignore;
    }
    return storage::duplicate_policy::reject;
}

/// Log prefix with timestamp
std::string log_prefix() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
    return std::string("[") + buf + "] ";
}

}  // namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

pacs_server_app::pacs_server_app(const pacs_server_config& config)
    : config_(config) {}

pacs_server_app::~pacs_server_app() {
    stop();
}

// =============================================================================
// Lifecycle Management
// =============================================================================

bool pacs_server_app::initialize() {
    std::cout << log_prefix() << "Initializing PACS Server...\n";

    if (!setup_storage()) {
        return false;
    }

    if (!setup_database()) {
        return false;
    }

    if (!setup_services()) {
        return false;
    }

    if (!setup_server()) {
        return false;
    }

    initialized_ = true;
    std::cout << log_prefix() << "PACS Server initialized successfully\n";
    return true;
}

bool pacs_server_app::start() {
    if (!initialized_) {
        std::cerr << log_prefix() << "Error: Server not initialized\n";
        return false;
    }

    std::cout << log_prefix() << "Starting DICOM server...\n";
    std::cout << log_prefix() << "  AE Title: " << config_.server.ae_title << "\n";
    std::cout << log_prefix() << "  Port: " << config_.server.port << "\n";
    std::cout << log_prefix() << "  Max Associations: " << config_.server.max_associations << "\n";

    auto result = server_->start();
    if (result.is_err()) {
        std::cerr << log_prefix() << "Error: Failed to start server\n";
        return false;
    }

    std::cout << log_prefix() << "PACS Server started successfully\n";
    std::cout << log_prefix() << "Listening on port " << config_.server.port << "...\n";
    std::cout << log_prefix() << "Press Ctrl+C to stop\n";

    return true;
}

void pacs_server_app::stop() {
    if (server_ && server_->is_running()) {
        std::cout << log_prefix() << "Stopping DICOM server...\n";
        server_->stop();
        std::cout << log_prefix() << "DICOM server stopped\n";
    }
}

void pacs_server_app::wait_for_shutdown() {
    if (server_) {
        server_->wait_for_shutdown();
    }
}

void pacs_server_app::request_shutdown() {
    shutdown_requested_ = true;
    stop();
}

bool pacs_server_app::is_running() const noexcept {
    return server_ && server_->is_running();
}

void pacs_server_app::print_statistics() const {
    if (!server_) {
        return;
    }

    auto stats = server_->get_statistics();
    auto uptime = stats.uptime();

    std::cout << "\n";
    std::cout << "=== PACS Server Statistics ===\n";
    std::cout << "Uptime: " << uptime.count() << " seconds\n";
    std::cout << "Total Associations: " << stats.total_associations << "\n";
    std::cout << "Active Associations: " << stats.active_associations << "\n";
    std::cout << "Rejected Associations: " << stats.rejected_associations << "\n";
    std::cout << "Messages Processed: " << stats.messages_processed << "\n";
    std::cout << "Bytes Received: " << stats.bytes_received << "\n";
    std::cout << "Bytes Sent: " << stats.bytes_sent << "\n";
    std::cout << "==============================\n";
    std::cout << "\n";
}

// =============================================================================
// Private Setup Methods
// =============================================================================

bool pacs_server_app::setup_storage() {
    std::cout << log_prefix() << "Setting up file storage...\n";
    std::cout << log_prefix() << "  Directory: " << config_.storage.directory << "\n";

    // Create storage directory if it doesn't exist
    std::error_code ec;
    std::filesystem::create_directories(config_.storage.directory, ec);
    if (ec) {
        std::cerr << log_prefix() << "Error: Failed to create storage directory: "
                  << ec.message() << "\n";
        return false;
    }

    storage::file_storage_config storage_config;
    storage_config.root_path = config_.storage.directory;
    storage_config.naming = parse_naming_scheme(config_.storage.naming);
    storage_config.duplicate = parse_duplicate_policy(config_.storage.duplicate_policy);
    storage_config.create_directories = true;

    try {
        file_storage_ = std::make_unique<storage::file_storage>(storage_config);
    } catch (const std::exception& e) {
        std::cerr << log_prefix() << "Error: Failed to create file storage: "
                  << e.what() << "\n";
        return false;
    }

    std::cout << log_prefix() << "File storage ready\n";
    return true;
}

bool pacs_server_app::setup_database() {
    std::cout << log_prefix() << "Setting up database...\n";
    std::cout << log_prefix() << "  Path: " << config_.database.path << "\n";

    // Create database directory if needed
    auto db_dir = config_.database.path.parent_path();
    if (!db_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(db_dir, ec);
        if (ec) {
            std::cerr << log_prefix() << "Error: Failed to create database directory: "
                      << ec.message() << "\n";
            return false;
        }
    }

    storage::index_config db_config;
    db_config.wal_mode = config_.database.wal_mode;

    auto result = storage::index_database::open(
        config_.database.path.string(), db_config);

    if (result.is_err()) {
        std::cerr << log_prefix() << "Error: Failed to open database\n";
        return false;
    }

    database_ = std::move(result.value());
    std::cout << log_prefix() << "Database ready\n";
    return true;
}

bool pacs_server_app::setup_services() {
    std::cout << log_prefix() << "Setting up DICOM services...\n";
    std::cout << log_prefix() << "  - Verification SCP (C-ECHO)\n";
    std::cout << log_prefix() << "  - Storage SCP (C-STORE)\n";
    std::cout << log_prefix() << "  - Query SCP (C-FIND)\n";
    std::cout << log_prefix() << "  - Retrieve SCP (C-MOVE/C-GET)\n";
    std::cout << log_prefix() << "  - Worklist SCP (MWL)\n";
    std::cout << log_prefix() << "  - MPPS SCP (N-CREATE/N-SET)\n";
    std::cout << log_prefix() << "All DICOM services configured\n";
    return true;
}

bool pacs_server_app::setup_server() {
    std::cout << log_prefix() << "Setting up DICOM server...\n";

    network::server_config server_config;
    server_config.ae_title = config_.server.ae_title;
    server_config.port = config_.server.port;
    server_config.max_associations = config_.server.max_associations;
    server_config.idle_timeout = config_.server.idle_timeout;

    // Set up AE whitelist if configured
    if (!config_.access_control.allowed_ae_titles.empty()) {
        server_config.ae_whitelist = config_.access_control.allowed_ae_titles;
        server_config.accept_unknown_calling_ae = false;
    }

    server_ = std::make_unique<network::dicom_server>(server_config);

    // Register Verification SCP
    server_->register_service(std::make_shared<services::verification_scp>());

    // Register Storage SCP
    auto storage_scp = std::make_shared<services::storage_scp>();
    storage_scp->set_handler(
        [this](const auto& ds, const auto& ae, const auto& sop_class, const auto& sop_uid) {
            return handle_store(ds, ae, sop_class, sop_uid);
        });
    server_->register_service(storage_scp);

    // Register Query SCP
    auto query_scp = std::make_shared<services::query_scp>();
    query_scp->set_handler(
        [this](auto level, const auto& keys, const auto& ae) {
            return handle_query(level, keys, ae);
        });
    server_->register_service(query_scp);

    // Register Retrieve SCP
    auto retrieve_scp = std::make_shared<services::retrieve_scp>();
    retrieve_scp->set_retrieve_handler(
        [this](const auto& keys) {
            return handle_retrieve(keys);
        });
    server_->register_service(retrieve_scp);

    // Register Worklist SCP
    auto worklist_scp = std::make_shared<services::worklist_scp>();
    worklist_scp->set_handler(
        [this](const auto& keys, const auto& ae) {
            return handle_worklist_query(keys, ae);
        });
    server_->register_service(worklist_scp);

    // Register MPPS SCP
    auto mpps_scp = std::make_shared<services::mpps_scp>();
    mpps_scp->set_create_handler(
        [this](const auto& instance) {
            return handle_mpps_create(instance);
        });
    mpps_scp->set_set_handler(
        [this](const auto& uid, const auto& mods, auto status) {
            return handle_mpps_set(uid, mods, status);
        });
    server_->register_service(mpps_scp);

    // Set up callbacks
    server_->on_association_established([](const network::association& assoc) {
        std::cout << log_prefix() << "Association established: "
                  << assoc.calling_ae() << " -> " << assoc.called_ae() << "\n";
    });

    server_->on_association_released([](const network::association& assoc) {
        std::cout << log_prefix() << "Association released: "
                  << assoc.calling_ae() << "\n";
    });

    server_->on_error([](const std::string& error) {
        std::cerr << log_prefix() << "Server error: " << error << "\n";
    });

    std::cout << log_prefix() << "DICOM server configured\n";
    return true;
}

// =============================================================================
// Service Handlers
// =============================================================================

services::storage_status pacs_server_app::handle_store(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae,
    const std::string& sop_class_uid,
    const std::string& sop_instance_uid) {

    std::cout << log_prefix() << "C-STORE from " << calling_ae
              << ": " << sop_instance_uid << "\n";

    // Store to filesystem
    auto store_result = file_storage_->store(dataset);
    if (store_result.is_err()) {
        std::cerr << log_prefix() << "Storage error\n";
        return services::storage_status::storage_error;
    }

    // Index in database
    auto patient_id = dataset.get_string(core::tags::patient_id);
    auto patient_name = dataset.get_string(core::tags::patient_name);
    auto study_uid = dataset.get_string(core::tags::study_instance_uid);
    auto series_uid = dataset.get_string(core::tags::series_instance_uid);

    if (patient_id.empty()) {
        std::cerr << log_prefix() << "Warning: Missing PatientID\n";
        return services::storage_status::success;
    }

    // Upsert patient and get pk
    auto birth_date = dataset.get_string(core::tags::patient_birth_date);
    auto sex = dataset.get_string(core::tags::patient_sex);
    auto patient_result = database_->upsert_patient(
        patient_id,
        patient_name,
        birth_date,
        sex
    );
    if (patient_result.is_err()) {
        std::cerr << log_prefix() << "Database error (patient)\n";
        return services::storage_status::success;
    }
    int64_t patient_pk = patient_result.value();

    // Upsert study if we have study_uid
    int64_t study_pk = 0;
    if (!study_uid.empty()) {
        auto study_date = dataset.get_string(core::tags::study_date);
        auto study_time = dataset.get_string(core::tags::study_time);
        auto accession = dataset.get_string(core::tags::accession_number);
        auto study_desc = dataset.get_string(core::tags::study_description);
        auto referring = dataset.get_string(core::tags::referring_physician_name);
        auto study_id = dataset.get_string(core::tags::study_id);

        auto study_result = database_->upsert_study(
            patient_pk,
            study_uid,
            study_id,
            study_date,
            study_time,
            accession,
            referring,
            study_desc
        );
        if (study_result.is_err()) {
            std::cerr << log_prefix() << "Database error (study)\n";
            return services::storage_status::success;
        }
        study_pk = study_result.value();
    }

    // Upsert series if we have series_uid and study_pk
    int64_t series_pk = 0;
    if (!series_uid.empty() && study_pk > 0) {
        auto modality = dataset.get_string(core::tags::modality);
        auto series_number_str = dataset.get_string(core::tags::series_number);
        auto series_desc = dataset.get_string(core::tags::series_description);

        std::optional<int> series_number;
        if (!series_number_str.empty()) {
            try {
                series_number = std::stoi(series_number_str);
            } catch (...) {}
        }

        auto series_result = database_->upsert_series(
            study_pk,
            series_uid,
            modality,
            series_number,
            series_desc,
            "",  // body_part_examined
            ""   // station_name
        );
        if (series_result.is_err()) {
            std::cerr << log_prefix() << "Database error (series)\n";
            return services::storage_status::success;
        }
        series_pk = series_result.value();
    }

    // Upsert instance if we have series_pk
    if (series_pk > 0) {
        auto instance_number_str = dataset.get_string(core::tags::instance_number);
        auto file_path = file_storage_->get_file_path(sop_instance_uid);

        int64_t file_size = 0;
        if (std::filesystem::exists(file_path)) {
            file_size = static_cast<int64_t>(std::filesystem::file_size(file_path));
        }

        std::optional<int> instance_number;
        if (!instance_number_str.empty()) {
            try {
                instance_number = std::stoi(instance_number_str);
            } catch (...) {}
        }

        auto instance_result = database_->upsert_instance(
            series_pk,
            sop_instance_uid,
            sop_class_uid,
            file_path.string(),
            file_size,
            "",  // transfer_syntax
            instance_number
        );
        if (instance_result.is_err()) {
            std::cerr << log_prefix() << "Database error (instance)\n";
        }
    }

    return services::storage_status::success;
}

std::vector<core::dicom_dataset> pacs_server_app::handle_query(
    services::query_level level,
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae) {

    std::cout << log_prefix() << "C-FIND from " << calling_ae
              << " at level " << services::to_string(level) << "\n";

    std::vector<core::dicom_dataset> results;

    switch (level) {
        case services::query_level::patient: {
            storage::patient_query query;
            auto id = query_keys.get_string(core::tags::patient_id);
            if (!id.empty()) {
                query.patient_id = id;
            }
            auto name = query_keys.get_string(core::tags::patient_name);
            if (!name.empty()) {
                query.patient_name = name;
            }

            auto patients = database_->search_patients(query);
            for (const auto& patient : patients) {
                core::dicom_dataset ds;
                ds.set_string(core::tags::patient_id, encoding::vr_type::LO, patient.patient_id);
                ds.set_string(core::tags::patient_name, encoding::vr_type::PN, patient.patient_name);
                ds.set_string(core::tags::patient_birth_date, encoding::vr_type::DA, patient.birth_date);
                ds.set_string(core::tags::patient_sex, encoding::vr_type::CS, patient.sex);
                results.push_back(std::move(ds));
            }
            break;
        }

        case services::query_level::study: {
            storage::study_query query;
            auto id = query_keys.get_string(core::tags::patient_id);
            if (!id.empty()) {
                query.patient_id = id;
            }
            auto uid = query_keys.get_string(core::tags::study_instance_uid);
            if (!uid.empty()) {
                query.study_uid = uid;
            }
            auto date = query_keys.get_string(core::tags::study_date);
            if (!date.empty()) {
                query.study_date = date;
            }

            auto studies = database_->search_studies(query);
            for (const auto& study : studies) {
                core::dicom_dataset ds;
                ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, study.study_uid);
                ds.set_string(core::tags::study_date, encoding::vr_type::DA, study.study_date);
                ds.set_string(core::tags::study_time, encoding::vr_type::TM, study.study_time);
                ds.set_string(core::tags::accession_number, encoding::vr_type::SH, study.accession_number);
                ds.set_string(core::tags::study_description, encoding::vr_type::LO, study.study_description);
                results.push_back(std::move(ds));
            }
            break;
        }

        case services::query_level::series: {
            storage::series_query query;
            auto uid = query_keys.get_string(core::tags::study_instance_uid);
            if (!uid.empty()) {
                query.study_uid = uid;
            }
            auto mod = query_keys.get_string(core::tags::modality);
            if (!mod.empty()) {
                query.modality = mod;
            }

            auto series_list = database_->search_series(query);
            for (const auto& series : series_list) {
                core::dicom_dataset ds;
                ds.set_string(core::tags::series_instance_uid, encoding::vr_type::UI, series.series_uid);
                ds.set_string(core::tags::modality, encoding::vr_type::CS, series.modality);
                if (series.series_number.has_value()) {
                    ds.set_string(core::tags::series_number, encoding::vr_type::IS,
                                  std::to_string(series.series_number.value()));
                }
                ds.set_string(core::tags::series_description, encoding::vr_type::LO, series.series_description);
                results.push_back(std::move(ds));
            }
            break;
        }

        case services::query_level::image: {
            storage::instance_query query;
            auto uid = query_keys.get_string(core::tags::series_instance_uid);
            if (!uid.empty()) {
                query.series_uid = uid;
            }

            auto instances = database_->search_instances(query);
            for (const auto& instance : instances) {
                core::dicom_dataset ds;
                ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, instance.sop_uid);
                ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, instance.sop_class_uid);
                if (instance.instance_number.has_value()) {
                    ds.set_string(core::tags::instance_number, encoding::vr_type::IS,
                                  std::to_string(instance.instance_number.value()));
                }
                results.push_back(std::move(ds));
            }
            break;
        }
    }

    std::cout << log_prefix() << "  Found " << results.size() << " matches\n";
    return results;
}

std::vector<core::dicom_file> pacs_server_app::handle_retrieve(
    const core::dicom_dataset& query_keys) {

    std::cout << log_prefix() << "C-MOVE/C-GET retrieve request\n";

    std::vector<core::dicom_file> files;
    std::vector<std::string> file_paths;

    // Determine query level and get file paths
    auto sop_uid = query_keys.get_string(core::tags::sop_instance_uid);
    if (!sop_uid.empty()) {
        // Instance level
        if (auto path = database_->get_file_path(sop_uid)) {
            file_paths.push_back(path.value());
        }
    } else {
        auto series_uid = query_keys.get_string(core::tags::series_instance_uid);
        if (!series_uid.empty()) {
            // Series level
            file_paths = database_->get_series_files(series_uid);
        } else {
            auto study_uid = query_keys.get_string(core::tags::study_instance_uid);
            if (!study_uid.empty()) {
                // Study level
                file_paths = database_->get_study_files(study_uid);
            }
        }
    }

    // Load files
    for (const auto& path : file_paths) {
        auto file_result = core::dicom_file::open(path);
        if (file_result.has_value()) {
            files.push_back(std::move(file_result.value()));
        }
    }

    std::cout << log_prefix() << "  Found " << files.size() << " files to transfer\n";
    return files;
}

std::vector<core::dicom_dataset> pacs_server_app::handle_worklist_query(
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae) {

    std::cout << log_prefix() << "MWL query from " << calling_ae << "\n";

    std::vector<core::dicom_dataset> results;

    storage::worklist_query query;
    auto id = query_keys.get_string(core::tags::patient_id);
    if (!id.empty()) {
        query.patient_id = id;
    }

    auto items = database_->query_worklist(query);

    for (const auto& item : items) {
        core::dicom_dataset ds;
        ds.set_string(core::tags::patient_id, encoding::vr_type::LO, item.patient_id);
        ds.set_string(core::tags::patient_name, encoding::vr_type::PN, item.patient_name);
        ds.set_string(core::tags::accession_number, encoding::vr_type::SH, item.accession_no);
        ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, item.study_uid);
        results.push_back(std::move(ds));
    }

    std::cout << log_prefix() << "  Found " << results.size() << " worklist items\n";
    return results;
}

network::Result<std::monostate> pacs_server_app::handle_mpps_create(
    const services::mpps_instance& instance) {

    std::cout << log_prefix() << "MPPS N-CREATE: " << instance.sop_instance_uid << "\n";

    auto result = database_->create_mpps(
        instance.sop_instance_uid,
        instance.station_ae
    );

    if (result.is_err()) {
        return kcenon::common::make_error<std::monostate>(1, "MPPS creation failed");
    }

    return network::Result<std::monostate>(std::monostate{});
}

network::Result<std::monostate> pacs_server_app::handle_mpps_set(
    const std::string& sop_instance_uid,
    [[maybe_unused]] const core::dicom_dataset& modifications,
    services::mpps_status new_status) {

    std::cout << log_prefix() << "MPPS N-SET: " << sop_instance_uid
              << " -> " << services::to_string(new_status) << "\n";

    auto result = database_->update_mpps(
        sop_instance_uid,
        std::string(services::to_string(new_status))
    );

    if (result.is_err()) {
        return kcenon::common::make_error<std::monostate>(1, "MPPS update failed");
    }

    return network::Result<std::monostate>(std::monostate{});
}

}  // namespace pacs::example
