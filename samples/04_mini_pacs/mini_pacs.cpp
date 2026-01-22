/**
 * @file mini_pacs.cpp
 * @brief Implementation of Mini PACS class
 */

#include "mini_pacs.hpp"

#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace pacs::samples {

namespace tags = pacs::core::tags;

// =============================================================================
// Construction
// =============================================================================

mini_pacs::mini_pacs(const mini_pacs_config& config)
    : config_(config) {
    initialize_storage();
    initialize_services();
    configure_server();
    setup_event_handlers();
}

mini_pacs::~mini_pacs() {
    stop();
}

// =============================================================================
// Lifecycle
// =============================================================================

bool mini_pacs::start() {
    if (running_) {
        return true;
    }

    auto result = server_->start();
    if (result.is_err()) {
        std::cerr << "Failed to start server: " << result.error().message << "\n";
        return false;
    }

    running_ = true;
    return true;
}

void mini_pacs::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (server_) {
        server_->stop();
    }
}

void mini_pacs::wait() {
    if (server_) {
        server_->wait_for_shutdown();
    }
}

bool mini_pacs::is_running() const noexcept {
    return running_;
}

// =============================================================================
// Statistics
// =============================================================================

const mini_pacs_statistics& mini_pacs::statistics() const noexcept {
    return stats_;
}

// =============================================================================
// Worklist Management
// =============================================================================

void mini_pacs::add_worklist_item(const worklist_entry& item) {
    std::lock_guard<std::mutex> lock(worklist_mutex_);
    worklist_items_.push_back(item);
}

void mini_pacs::clear_worklist() {
    std::lock_guard<std::mutex> lock(worklist_mutex_);
    worklist_items_.clear();
}

std::vector<worklist_entry> mini_pacs::get_worklist_items() const {
    std::lock_guard<std::mutex> lock(worklist_mutex_);
    return worklist_items_;
}

// =============================================================================
// Configuration Access
// =============================================================================

const mini_pacs_config& mini_pacs::config() const noexcept {
    return config_;
}

// =============================================================================
// Initialization
// =============================================================================

void mini_pacs::initialize_storage() {
    // Create storage directories
    std::filesystem::create_directories(config_.storage_path);

    // Configure file storage
    storage::file_storage_config fs_config;
    fs_config.root_path = config_.storage_path;
    fs_config.naming = storage::naming_scheme::uid_hierarchical;
    fs_config.duplicate = storage::duplicate_policy::replace;
    fs_config.create_directories = true;
    fs_config.file_extension = ".dcm";

    file_storage_ = std::make_shared<storage::file_storage>(fs_config);

    // Initialize index database
    auto db_path = config_.storage_path / "index.db";
    auto db_result = storage::index_database::open(db_path.string());
    if (db_result.is_err()) {
        throw std::runtime_error("Failed to open index database: " +
                                 db_result.error().message);
    }
    index_db_ = std::move(db_result.value());
}

void mini_pacs::initialize_services() {
    // Verification SCP (C-ECHO)
    verification_scp_ = std::make_shared<services::verification_scp>();

    // Storage SCP (C-STORE)
    services::storage_scp_config scp_config;
    storage_scp_ = std::make_shared<services::storage_scp>(scp_config);
    storage_scp_->set_handler(
        [this](const auto& ds, const auto& ae, const auto& sop_class,
               const auto& sop_instance) {
            return handle_store(ds, ae, sop_class, sop_instance);
        });

    // Query SCP (C-FIND)
    query_scp_ = std::make_shared<services::query_scp>();
    query_scp_->set_handler(
        [this](auto level, const auto& keys, const auto& ae) {
            return handle_query(level, keys, ae);
        });

    // Retrieve SCP (C-MOVE/C-GET)
    retrieve_scp_ = std::make_shared<services::retrieve_scp>();
    retrieve_scp_->set_retrieve_handler(
        [this](const auto& keys) { return handle_retrieve(keys); });
    retrieve_scp_->set_destination_resolver(
        [this](const auto& ae) { return resolve_destination(ae); });

    // Worklist SCP (MWL C-FIND)
    if (config_.enable_worklist) {
        worklist_scp_ = std::make_shared<services::worklist_scp>();
        worklist_scp_->set_handler(
            [this](const auto& keys, const auto& ae) {
                return handle_worklist_query(keys, ae);
            });
    }

    // MPPS SCP (N-CREATE/N-SET)
    if (config_.enable_mpps) {
        mpps_scp_ = std::make_shared<services::mpps_scp>();
        mpps_scp_->set_create_handler(
            [this](const auto& inst) { return handle_mpps_create(inst); });
        mpps_scp_->set_set_handler(
            [this](const auto& uid, const auto& mods, auto status) {
                return handle_mpps_set(uid, mods, status);
            });
    }
}

void mini_pacs::configure_server() {
    network::server_config server_config;
    server_config.ae_title = config_.ae_title;
    server_config.port = config_.port;
    server_config.max_associations = config_.max_associations;
    server_config.idle_timeout = std::chrono::seconds(60);
    server_config.max_pdu_size = 65536;
    server_config.implementation_class_uid = "1.2.410.200001.1.4";
    server_config.implementation_version_name = "MINI_PACS_4.0";

    server_ = std::make_unique<network::dicom_server>(server_config);

    // Register all services
    server_->register_service(verification_scp_);
    server_->register_service(storage_scp_);
    server_->register_service(query_scp_);
    server_->register_service(retrieve_scp_);

    if (worklist_scp_) {
        server_->register_service(worklist_scp_);
    }
    if (mpps_scp_) {
        server_->register_service(mpps_scp_);
    }
}

void mini_pacs::setup_event_handlers() {
    server_->on_association_established(
        [this](const network::association& assoc) {
            stats_.associations_total++;
            stats_.associations_active++;
            if (config_.verbose_logging) {
                std::cout << "[CONNECT] " << assoc.calling_ae()
                          << " -> " << assoc.called_ae()
                          << " (active: " << stats_.associations_active << ")\n";
            }
        });

    server_->on_association_released(
        [this](const network::association& assoc) {
            stats_.associations_active--;
            if (config_.verbose_logging) {
                std::cout << "[RELEASE] " << assoc.calling_ae()
                          << " (active: " << stats_.associations_active << ")\n";
            }
        });

    server_->on_error(
        [this](const std::string& msg) {
            std::cerr << "[ERROR] " << msg << "\n";
        });
}

// =============================================================================
// Storage Handlers
// =============================================================================

services::storage_status mini_pacs::handle_store(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae,
    const std::string& /*sop_class_uid*/,
    const std::string& sop_instance_uid) {

    stats_.c_store_count++;

    if (config_.verbose_logging) {
        std::cout << "[C-STORE] From: " << calling_ae
                  << " Patient: " << dataset.get_string(tags::patient_name)
                  << " Modality: " << dataset.get_string(tags::modality) << "\n";
    }

    // Store to filesystem
    auto store_result = file_storage_->store(dataset);
    if (store_result.is_err()) {
        std::cerr << "  Storage failed: " << store_result.error().message << "\n";
        return services::storage_status::storage_error;
    }

    // Get stored file path and update index
    auto file_path = file_storage_->get_file_path(sop_instance_uid);
    if (!update_index(dataset, file_path)) {
        return services::storage_status::storage_error;
    }

    stats_.bytes_received += std::filesystem::exists(file_path)
        ? std::filesystem::file_size(file_path)
        : 0;

    return services::storage_status::success;
}

bool mini_pacs::update_index(
    const core::dicom_dataset& ds,
    const std::filesystem::path& file_path) {

    // Patient
    auto patient_result = index_db_->upsert_patient(
        ds.get_string(tags::patient_id),
        ds.get_string(tags::patient_name),
        ds.get_string(tags::patient_birth_date),
        ds.get_string(tags::patient_sex));

    if (patient_result.is_err()) {
        std::cerr << "Database error (patient): "
                  << patient_result.error().message << "\n";
        return false;
    }
    int64_t patient_pk = patient_result.value();

    // Study
    auto study_result = index_db_->upsert_study(
        patient_pk,
        ds.get_string(tags::study_instance_uid),
        ds.get_string(tags::study_id),
        ds.get_string(tags::study_date),
        ds.get_string(tags::study_time),
        ds.get_string(tags::accession_number),
        ds.get_string(tags::referring_physician_name),
        ds.get_string(tags::study_description));

    if (study_result.is_err()) {
        std::cerr << "Database error (study): "
                  << study_result.error().message << "\n";
        return false;
    }
    int64_t study_pk = study_result.value();

    // Series
    auto series_result = index_db_->upsert_series(
        study_pk,
        ds.get_string(tags::series_instance_uid),
        ds.get_string(tags::modality),
        ds.get_numeric<int>(tags::series_number),
        ds.get_string(tags::series_description),
        "",  // body_part_examined
        ds.get_string(tags::station_name));

    if (series_result.is_err()) {
        std::cerr << "Database error (series): "
                  << series_result.error().message << "\n";
        return false;
    }
    int64_t series_pk = series_result.value();

    // Instance
    auto file_size = std::filesystem::exists(file_path)
        ? static_cast<int64_t>(std::filesystem::file_size(file_path))
        : 0;

    auto instance_result = index_db_->upsert_instance(
        series_pk,
        ds.get_string(tags::sop_instance_uid),
        ds.get_string(tags::sop_class_uid),
        file_path.string(),
        file_size,
        ds.get_string(tags::transfer_syntax_uid),
        ds.get_numeric<int>(tags::instance_number));

    if (instance_result.is_err()) {
        std::cerr << "Database error (instance): "
                  << instance_result.error().message << "\n";
        return false;
    }

    return true;
}

// =============================================================================
// Query Handlers
// =============================================================================

std::vector<core::dicom_dataset> mini_pacs::handle_query(
    services::query_level level,
    const core::dicom_dataset& query_keys,
    const std::string& /*calling_ae*/) {

    stats_.c_find_count++;

    std::vector<core::dicom_dataset> results;

    switch (level) {
        case services::query_level::patient: {
            storage::patient_query pq;
            auto pid = query_keys.get_string(tags::patient_id);
            auto pname = query_keys.get_string(tags::patient_name);
            if (!pid.empty()) pq.patient_id = pid;
            if (!pname.empty()) pq.patient_name = pname;

            auto patients = index_db_->search_patients(pq);

            if (patients.is_ok()) {
                for (const auto& p : patients.value()) {
                    core::dicom_dataset ds;
                    ds.set_string(tags::patient_id, encoding::vr_type::LO, p.patient_id);
                    ds.set_string(tags::patient_name, encoding::vr_type::PN, p.patient_name);
                    ds.set_string(tags::patient_birth_date, encoding::vr_type::DA, p.birth_date);
                    ds.set_string(tags::patient_sex, encoding::vr_type::CS, p.sex);
                    results.push_back(std::move(ds));
                }
            }
            break;
        }

        case services::query_level::study: {
            storage::study_query sq;
            auto pid = query_keys.get_string(tags::patient_id);
            auto study_uid = query_keys.get_string(tags::study_instance_uid);
            auto study_date = query_keys.get_string(tags::study_date);
            auto accession = query_keys.get_string(tags::accession_number);
            if (!pid.empty()) sq.patient_id = pid;
            if (!study_uid.empty()) sq.study_uid = study_uid;
            if (!study_date.empty()) sq.study_date = study_date;
            if (!accession.empty()) sq.accession_number = accession;

            auto studies = index_db_->search_studies(sq);

            if (studies.is_ok()) {
                for (const auto& s : studies.value()) {
                    core::dicom_dataset ds;
                    ds.set_string(tags::study_instance_uid, encoding::vr_type::UI, s.study_uid);
                    ds.set_string(tags::study_id, encoding::vr_type::SH, s.study_id);
                    ds.set_string(tags::study_date, encoding::vr_type::DA, s.study_date);
                    ds.set_string(tags::study_time, encoding::vr_type::TM, s.study_time);
                    ds.set_string(tags::accession_number, encoding::vr_type::SH, s.accession_number);
                    ds.set_string(tags::study_description, encoding::vr_type::LO, s.study_description);
                    ds.set_string(tags::referring_physician_name, encoding::vr_type::PN, s.referring_physician);
                    results.push_back(std::move(ds));
                }
            }
            break;
        }

        case services::query_level::series: {
            storage::series_query sq;
            auto study_uid = query_keys.get_string(tags::study_instance_uid);
            auto series_uid = query_keys.get_string(tags::series_instance_uid);
            auto modality = query_keys.get_string(tags::modality);
            if (!study_uid.empty()) sq.study_uid = study_uid;
            if (!series_uid.empty()) sq.series_uid = series_uid;
            if (!modality.empty()) sq.modality = modality;

            auto series = index_db_->search_series(sq);

            if (series.is_ok()) {
                for (const auto& ser : series.value()) {
                    core::dicom_dataset ds;
                    ds.set_string(tags::series_instance_uid, encoding::vr_type::UI, ser.series_uid);
                    ds.set_string(tags::modality, encoding::vr_type::CS, ser.modality);
                    if (ser.series_number.has_value()) {
                        ds.set_numeric(tags::series_number, encoding::vr_type::IS, ser.series_number.value());
                    }
                    ds.set_string(tags::series_description, encoding::vr_type::LO, ser.series_description);
                    ds.set_string(tags::station_name, encoding::vr_type::SH, ser.station_name);
                    results.push_back(std::move(ds));
                }
            }
            break;
        }

        case services::query_level::image: {
            storage::instance_query iq;
            auto series_uid = query_keys.get_string(tags::series_instance_uid);
            auto sop_uid = query_keys.get_string(tags::sop_instance_uid);
            if (!series_uid.empty()) iq.series_uid = series_uid;
            if (!sop_uid.empty()) iq.sop_uid = sop_uid;

            auto instances = index_db_->search_instances(iq);

            if (instances.is_ok()) {
                for (const auto& inst : instances.value()) {
                    core::dicom_dataset ds;
                    ds.set_string(tags::sop_instance_uid, encoding::vr_type::UI, inst.sop_uid);
                    ds.set_string(tags::sop_class_uid, encoding::vr_type::UI, inst.sop_class_uid);
                    if (inst.instance_number.has_value()) {
                        ds.set_numeric(tags::instance_number, encoding::vr_type::IS, inst.instance_number.value());
                    }
                    results.push_back(std::move(ds));
                }
            }
            break;
        }
    }

    if (config_.verbose_logging) {
        std::cout << "[C-FIND] Level: " << to_string(level)
                  << " Results: " << results.size() << "\n";
    }

    return results;
}

// =============================================================================
// Retrieve Handlers
// =============================================================================

std::vector<core::dicom_file> mini_pacs::handle_retrieve(
    const core::dicom_dataset& query_keys) {

    std::vector<core::dicom_file> files;

    // Determine query level from keys
    auto study_uid = query_keys.get_string(tags::study_instance_uid);
    auto series_uid = query_keys.get_string(tags::series_instance_uid);
    auto sop_uid = query_keys.get_string(tags::sop_instance_uid);

    std::vector<storage::instance_record> instances;

    if (!sop_uid.empty()) {
        // Instance level retrieve
        storage::instance_query iq;
        iq.sop_uid = sop_uid;
        auto result = index_db_->search_instances(iq);
        if (result.is_ok()) {
            instances = result.value();
        }
    } else if (!series_uid.empty()) {
        // Series level retrieve
        storage::instance_query iq;
        iq.series_uid = series_uid;
        auto result = index_db_->search_instances(iq);
        if (result.is_ok()) {
            instances = result.value();
        }
    } else if (!study_uid.empty()) {
        // Study level retrieve - get all series first
        storage::series_query sq;
        sq.study_uid = study_uid;
        auto series_result = index_db_->search_series(sq);
        if (series_result.is_ok()) {
            for (const auto& ser : series_result.value()) {
                storage::instance_query iq;
                iq.series_uid = ser.series_uid;
                auto inst_result = index_db_->search_instances(iq);
                if (inst_result.is_ok()) {
                    for (const auto& inst : inst_result.value()) {
                        instances.push_back(inst);
                    }
                }
            }
        }
    }

    // Load files from disk
    for (const auto& inst : instances) {
        auto file_result = core::dicom_file::open(inst.file_path);
        if (file_result.is_ok()) {
            files.push_back(std::move(file_result.value()));
        }
    }

    stats_.c_move_count++;

    if (config_.verbose_logging) {
        std::cout << "[C-MOVE/C-GET] Files: " << files.size() << "\n";
    }

    return files;
}

std::optional<std::pair<std::string, uint16_t>> mini_pacs::resolve_destination(
    const std::string& ae_title) {
    // Simple in-memory AE resolution
    // In a production system, this would query a configuration database
    if (ae_title == config_.ae_title) {
        return std::make_pair("localhost", config_.port);
    }
    // Return nullopt for unknown destinations
    return std::nullopt;
}

// =============================================================================
// Worklist Handlers
// =============================================================================

std::vector<core::dicom_dataset> mini_pacs::handle_worklist_query(
    const core::dicom_dataset& query_keys,
    const std::string& /*calling_ae*/) {

    stats_.mwl_count++;

    std::vector<core::dicom_dataset> results;

    // Extract filter criteria
    auto filter_modality = query_keys.get_string(tags::modality);
    auto filter_date = query_keys.get_string(tags::scheduled_procedure_step_start_date);
    auto filter_patient_id = query_keys.get_string(tags::patient_id);

    std::lock_guard<std::mutex> lock(worklist_mutex_);

    for (const auto& item : worklist_items_) {
        // Apply filters
        if (!filter_modality.empty() && item.modality != filter_modality) {
            continue;
        }
        if (!filter_date.empty() && item.scheduled_date != filter_date) {
            continue;
        }
        if (!filter_patient_id.empty() && item.patient_id != filter_patient_id) {
            continue;
        }

        results.push_back(create_worklist_response(item));
    }

    if (config_.verbose_logging) {
        std::cout << "[MWL] Results: " << results.size() << "\n";
    }

    return results;
}

core::dicom_dataset mini_pacs::create_worklist_response(const worklist_entry& item) {
    core::dicom_dataset ds;

    // Patient level
    ds.set_string(tags::patient_id, encoding::vr_type::LO, item.patient_id);
    ds.set_string(tags::patient_name, encoding::vr_type::PN, item.patient_name);
    ds.set_string(tags::patient_birth_date, encoding::vr_type::DA, item.patient_birth_date);
    ds.set_string(tags::patient_sex, encoding::vr_type::CS, item.patient_sex);

    // Study level
    ds.set_string(tags::study_instance_uid, encoding::vr_type::UI, item.study_uid);
    ds.set_string(tags::accession_number, encoding::vr_type::SH, item.accession_number);
    ds.set_string(tags::referring_physician_name, encoding::vr_type::PN, item.referring_physician);

    // Scheduled Procedure Step Sequence would be set here
    // For simplicity, we set flat attributes
    ds.set_string(tags::modality, encoding::vr_type::CS, item.modality);
    ds.set_string(tags::scheduled_procedure_step_start_date, encoding::vr_type::DA, item.scheduled_date);
    ds.set_string(tags::scheduled_procedure_step_start_time, encoding::vr_type::TM, item.scheduled_time);
    ds.set_string(tags::scheduled_procedure_step_id, encoding::vr_type::SH, item.step_id);
    ds.set_string(tags::scheduled_procedure_step_description, encoding::vr_type::LO, item.procedure_description);
    ds.set_string(tags::scheduled_station_ae_title, encoding::vr_type::AE, item.scheduled_station_ae);

    return ds;
}

// =============================================================================
// MPPS Handlers
// =============================================================================

network::Result<std::monostate> mini_pacs::handle_mpps_create(
    const services::mpps_instance& instance) {

    stats_.mpps_create_count++;

    mpps_entry entry;
    entry.sop_instance_uid = instance.sop_instance_uid;
    entry.status = "IN PROGRESS";
    entry.station_ae = instance.station_ae;
    entry.data = instance.data;

    {
        std::lock_guard<std::mutex> lock(mpps_mutex_);
        mpps_instances_.push_back(std::move(entry));
    }

    if (config_.verbose_logging) {
        std::cout << "[MPPS N-CREATE] UID: " << instance.sop_instance_uid
                  << " Station: " << instance.station_ae << "\n";
    }

    return network::Result<std::monostate>::ok({});
}

network::Result<std::monostate> mini_pacs::handle_mpps_set(
    const std::string& sop_instance_uid,
    const core::dicom_dataset& /*modifications*/,
    services::mpps_status new_status) {

    stats_.mpps_set_count++;

    std::lock_guard<std::mutex> lock(mpps_mutex_);

    for (auto& entry : mpps_instances_) {
        if (entry.sop_instance_uid == sop_instance_uid) {
            entry.status = std::string(services::to_string(new_status));

            if (config_.verbose_logging) {
                std::cout << "[MPPS N-SET] UID: " << sop_instance_uid
                          << " Status: " << entry.status << "\n";
            }

            return network::Result<std::monostate>::ok({});
        }
    }

    return pacs::pacs_error<std::monostate>(
        pacs::error_codes::instance_not_found,
        "MPPS instance not found: " + sop_instance_uid);
}

}  // namespace pacs::samples
