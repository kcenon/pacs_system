/**
 * @file server_app.cpp
 * @brief PACS Server application implementation
 */

#include "server_app.h"

#include "kcenon/pacs/core/utf8_text.h"
#include "kcenon/pacs/services/qr_index.h"
#include "kcenon/pacs/services/qr_matching.h"

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/services/storage_status.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace kcenon::pacs::example {

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

    if (ups_index_) {
        // Delivery runs on its own thread, so this does not hold up serving.
        ups_index_->report_restart();
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
    std::cout << log_prefix() << "  - Storage Commitment SCP (N-ACTION/N-EVENT-REPORT)\n";
    std::cout << log_prefix() << "  - UPS SCP (Push, Pull, Watch, Query, Event)\n";
    std::cout << log_prefix() << "  - Print SCP (Film Session/Box, Image Box, Printer)\n";
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

    server_ = std::make_unique<network::v2::dicom_server_v2>(server_config);

    // Register Verification SCP
    server_->register_service(std::make_shared<services::verification_scp>());

    // Register Storage SCP
    auto storage_scp = std::make_shared<services::storage_scp>();
    storage_scp->set_context_handler(
        [this](const services::storage_context& context) {
            return handle_store(context.dataset, context.calling_ae, context.sop_class_uid,
                                context.sop_instance_uid, context.transfer_syntax);
        });
    server_->register_service(storage_scp);

    // Register Query SCP
    auto query_scp = std::make_shared<services::query_scp>();
    query_scp->set_find_handler(
        [this](const services::find_request& request) { return services::find_in_index(*database_, request); });
    server_->register_service(query_scp);

    // Register Retrieve SCP
    auto retrieve_scp = std::make_shared<services::retrieve_scp>();
    retrieve_scp->set_retrieve_selector(
        [this](const services::retrieve_request& request) { return services::select_from_index(*database_, request); });
    retrieve_scp->set_destination_resolver(
        [this](const std::string& ae_title) -> std::optional<std::pair<std::string, uint16_t>> {
            for (const auto& destination : config_.move_destinations) {
                if (destination.ae_title == ae_title) {
                    return std::make_pair(destination.host, destination.port);
                }
            }
            return std::nullopt;
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

    // Reverse-direction reports, to the peers named on the command line
    services::event_sender_config event_config;
    event_config.local_ae_title = config_.server.ae_title;
    event_config.attempts = config_.event_attempts;
    event_config.retry_delay = config_.event_retry_delay;
    events_ = std::make_shared<services::event_sender>(
        event_config,
        [this](const std::string& ae_title) -> std::optional<std::pair<std::string, uint16_t>> {
            for (const auto& destination : config_.move_destinations) {
                if (destination.ae_title == ae_title) {
                    return std::make_pair(destination.host, destination.port);
                }
            }
            return std::nullopt;
        });
    events_->set_observer([](const services::event_report& report, std::optional<uint16_t> status, int attempts) {
        if (status) {
            std::cout << log_prefix() << "N-EVENT-REPORT type " << report.event_type_id << " to " << report.receiving_ae
                      << " answered with status " << *status << " after " << attempts << " attempt(s)\n";
        } else {
            std::cerr << log_prefix() << "N-EVENT-REPORT type " << report.event_type_id << " to " << report.receiving_ae
                      << " not delivered after " << attempts << " attempt(s)\n";
        }
    });

    // Register Storage Commitment SCP
    auto commitment_scp = std::make_shared<services::storage_commitment_scp>(nullptr);
    commitment_scp->set_event_sender(events_);
    commitment_scp->set_instance_verifier(
        [this](const services::sop_reference& reference) { return verify_commitment(reference); });
    server_->register_service(commitment_scp);

    // Register UPS SCP
    auto ups_directory = config_.ups_directory;
    if (ups_directory.empty()) {
        ups_directory = config_.storage.directory.string() + ".ups";
    }
    ups_index_ = std::make_unique<services::ups_index>(*database_, ups_directory, events_);
    server_->register_service(ups_index_->make_service());

    // Register Print SCP
    auto print_scp = std::make_shared<services::print_scp>();
    print_scp->set_job_handler(
        [this](const services::print_job& job) { return spool_print_job(job); });
    server_->register_service(print_scp);

    // Set up callbacks
    server_->on_association_established([](const std::string& /* session_id */,
                                          const std::string& calling_ae,
                                          const std::string& called_ae) {
        std::cout << log_prefix() << "Association established: "
                  << calling_ae << " -> " << called_ae << "\n";
    });

    server_->on_association_closed([](const std::string& session_id, bool graceful) {
        std::cout << log_prefix() << "Association closed: "
                  << session_id << (graceful ? " (released)" : " (aborted)") << "\n";
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
    const std::string& sop_instance_uid,
    const encoding::transfer_syntax& syntax) {

    std::cout << log_prefix() << "C-STORE from " << calling_ae
              << ": " << sop_instance_uid << "\n";

    // Store to filesystem
    auto store_result = file_storage_->store(dataset, syntax);
    if (store_result.is_err()) {
        std::cerr << log_prefix() << "Storage error\n";
        return services::storage_status::storage_error;
    }

    // Index in database. The index holds UTF-8, whatever character set each object uses.
    const auto index_text = core::utf8_text_copy(dataset);
    auto patient_id = index_text.get_string(core::tags::patient_id);
    auto patient_name = index_text.get_string(core::tags::patient_name);
    auto study_uid = index_text.get_string(core::tags::study_instance_uid);
    auto series_uid = index_text.get_string(core::tags::series_instance_uid);

    if (patient_id.empty()) {
        std::cerr << log_prefix() << "Warning: Missing PatientID\n";
        return services::storage_status::success;
    }

    // Upsert patient and get pk
    auto birth_date = index_text.get_string(core::tags::patient_birth_date);
    auto sex = index_text.get_string(core::tags::patient_sex);
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
        auto study_date = index_text.get_string(core::tags::study_date);
        auto study_time = index_text.get_string(core::tags::study_time);
        auto accession = index_text.get_string(core::tags::accession_number);
        auto study_desc = index_text.get_string(core::tags::study_description);
        auto referring = index_text.get_string(core::tags::referring_physician_name);
        auto study_id = index_text.get_string(core::tags::study_id);

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
        auto modality = index_text.get_string(core::tags::modality);
        auto series_number_str = index_text.get_string(core::tags::series_number);
        auto series_desc = index_text.get_string(core::tags::series_description);

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
        auto instance_number_str = index_text.get_string(core::tags::instance_number);
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

namespace {

/// One attribute of a worklist item as PS3.4 K.6.1.2.2 has it: how a key matches it, and its value.
struct worklist_attribute {
    core::dicom_tag tag;
    encoding::vr_type vr;
    services::key_matching matching;
    std::string value;
};

/// Keys are honored and returned as asked; an attribute nobody keeps comes back empty.
bool answer_keys(const std::vector<worklist_attribute>& attributes, const core::dicom_dataset& keys,
                 core::dicom_dataset& row) {
    for (const auto& [tag, key] : keys) {
        if (key.is_sequence() || tag == core::tags::specific_character_set) {
            continue;
        }
        const auto found = std::find_if(attributes.begin(), attributes.end(),
                                        [&](const worklist_attribute& attribute) { return attribute.tag == tag; });
        const std::string wanted = keys.get_string(tag);
        if (found == attributes.end()) {
            if (!wanted.empty()) {
                return false;  // A key this SCP does not keep cannot match a value.
            }
            row.insert(core::dicom_element(tag, key.vr()));
            continue;
        }
        if (!wanted.empty() && !services::key_matches(found->matching, wanted, found->value)) {
            return false;
        }
        row.set_string(tag, found->vr, found->value);
    }
    return true;
}

}  // namespace

std::optional<core::dicom_dataset> pacs_server_app::worklist_row(
    const storage::worklist_item& item, const core::dicom_dataset& keys) const {

    using services::key_matching;
    using encoding::vr_type;

    const std::vector<worklist_attribute> patient{
        {core::tags::patient_id, vr_type::LO, key_matching::text, item.patient_id},
        {core::tags::patient_name, vr_type::PN, key_matching::person_name, item.patient_name},
        {core::tags::patient_birth_date, vr_type::DA, key_matching::date, item.birth_date},
        {core::tags::patient_sex, vr_type::CS, key_matching::text, item.sex},
        {core::tags::accession_number, vr_type::SH, key_matching::text, item.accession_no},
        {core::tags::study_instance_uid, vr_type::UI, key_matching::uid, item.study_uid},
        {core::tags::referring_physician_name, vr_type::PN, key_matching::person_name, item.referring_phys},
        {core::dicom_tag{0x0040, 0x1001}, vr_type::SH, key_matching::text, item.requested_proc_id},
    };
    const std::vector<worklist_attribute> step{
        {core::tags::modality, vr_type::CS, key_matching::text, item.modality},
        {core::dicom_tag{0x0040, 0x0001}, vr_type::AE, key_matching::text, item.station_ae},
        {core::dicom_tag{0x0040, 0x0010}, vr_type::SH, key_matching::text, item.station_name},
        {core::dicom_tag{0x0040, 0x0002}, vr_type::DA, key_matching::date, item.scheduled_datetime.substr(0, 8)},
        {core::dicom_tag{0x0040, 0x0003}, vr_type::TM, key_matching::time,
         item.scheduled_datetime.size() > 8 ? item.scheduled_datetime.substr(8) : std::string{}},
        {core::dicom_tag{0x0040, 0x0009}, vr_type::SH, key_matching::text, item.step_id},
        {core::dicom_tag{0x0040, 0x0007}, vr_type::LO, key_matching::text, item.procedure_desc},
    };

    core::dicom_dataset row;
    if (!answer_keys(patient, keys, row)) {
        return std::nullopt;
    }

    // worklist_scp has refused an identifier without this sequence before this runs.
    const auto* step_keys = keys.get_sequence(core::tags::scheduled_procedure_step_sequence);
    core::dicom_dataset step_row;
    if (step_keys != nullptr && !step_keys->empty() && !answer_keys(step, step_keys->front(), step_row)) {
        return std::nullopt;
    }
    row.get_or_create_sequence(core::tags::scheduled_procedure_step_sequence).push_back(std::move(step_row));

    const auto non_ascii = [](const std::string& text) {
        return std::any_of(text.begin(), text.end(), [](char c) { return static_cast<unsigned char>(c) >= 0x80; });
    };
    if (non_ascii(item.patient_name) || non_ascii(item.procedure_desc) || non_ascii(item.referring_phys)) {
        row.set_string(core::tags::specific_character_set, vr_type::CS, "ISO_IR 192");
    }
    return row;
}

std::optional<services::commitment_failure_reason> pacs_server_app::verify_commitment(
    const services::sop_reference& reference) const {

    using reason = services::commitment_failure_reason;

    auto instance = database_->find_instance(reference.sop_instance_uid);
    if (!instance) {
        return reason::no_such_object_instance;
    }
    if (instance->sop_class_uid != reference.sop_class_uid) {
        return reason::class_instance_conflict;
    }
    // Commitment promises the object can be retrieved, so the file itself has to be there.
    std::error_code error;
    if (!std::filesystem::is_regular_file(instance->file_path, error) || error) {
        return reason::no_such_object_instance;
    }
    return std::nullopt;
}

network::Result<std::monostate> pacs_server_app::spool_print_job(const services::print_job& job) {
    auto spool = config_.print_spool_directory;
    if (spool.empty()) {
        spool = config_.storage.directory.string() + ".print-spool";
    }
    // The UID was checked at N-CREATE, and it arrived from the network, so it stays a plain name.
    const auto& box_uid = job.box.sop_instance_uid;
    if (box_uid.empty() || !std::all_of(box_uid.begin(), box_uid.end(), [](char c) { return (c >= '0' && c <= '9') || c == '.'; })) {
        return kcenon::common::make_error<std::monostate>(1, "Film Box UID cannot name a job", "print");
    }
    const auto directory = spool / box_uid;
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) {
        return kcenon::common::make_error<std::monostate>(1, "Cannot create " + directory.string(), "print");
    }

    const auto quoted = [](std::string text) {
        std::string out = "\"";
        for (const char c : text) {
            if (c == '"' || c == '\\') {
                out += '\\';
            }
            if (static_cast<unsigned char>(c) >= 0x20) {
                out += c;
            }
        }
        return out + "\"";
    };

    std::ostringstream manifest;
    manifest << "{\n  \"film_box\": " << quoted(box_uid)
             << ",\n  \"film_session\": " << quoted(job.session.sop_instance_uid)
             << ",\n  \"copies\": " << job.session.number_of_copies
             << ",\n  \"priority\": " << quoted(job.session.print_priority)
             << ",\n  \"medium_type\": " << quoted(job.session.medium_type)
             << ",\n  \"film_destination\": " << quoted(job.session.film_destination)
             << ",\n  \"display_format\": " << quoted(job.box.image_display_format)
             << ",\n  \"orientation\": " << quoted(job.box.film_orientation)
             << ",\n  \"film_size\": " << quoted(job.box.film_size_id)
             << ",\n  \"images\": [";
    bool first = true;
    for (const auto& image : job.images) {
        const auto* gray = image.data.get_sequence(services::print_tags::basic_grayscale_image_sequence);
        const auto* color = image.data.get_sequence(services::print_tags::basic_color_image_sequence);
        const auto* sequence = gray != nullptr ? gray : color;
        if (sequence == nullptr || sequence->empty()) {
            continue;
        }
        const auto& content = sequence->front();
        const auto* pixels = content.get(services::print_tags::pixel_data);
        if (pixels == nullptr) {
            continue;
        }
        const auto name = "image-" + std::to_string(image.image_position) + ".pixels";
        const auto bytes = pixels->raw_data();
        std::ofstream output(directory / name, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            return kcenon::common::make_error<std::monostate>(1, "Cannot write " + name, "print");
        }
        manifest << (first ? "" : ",") << "\n    {\"position\": " << image.image_position
                 << ", \"rows\": " << content.get_numeric<uint16_t>(core::tags::rows).value_or(0)
                 << ", \"columns\": " << content.get_numeric<uint16_t>(core::tags::columns).value_or(0)
                 << ", \"photometric\": " << quoted(content.get_string(core::tags::photometric_interpretation))
                 << ", \"pixels\": " << quoted(name) << "}";
        first = false;
    }
    manifest << "\n  ]\n}\n";

    // The manifest appears whole and last: a job with a manifest has all its images.
    const auto manifest_path = directory / "job.json";
    auto temporary = manifest_path;
    temporary += ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary);
        output << manifest.str();
        if (!output) {
            return kcenon::common::make_error<std::monostate>(1, "Cannot write the job manifest", "print");
        }
    }
    std::filesystem::rename(temporary, manifest_path, error);
    if (error) {
        return kcenon::common::make_error<std::monostate>(1, "Cannot finish the job manifest", "print");
    }
    std::cout << log_prefix() << "Print job " << box_uid << " spooled to " << directory.string() << "\n";
    return std::monostate{};
}

std::vector<core::dicom_dataset> pacs_server_app::handle_worklist_query(
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae) {

    std::cout << log_prefix() << "MWL query from " << calling_ae << "\n";

    std::vector<core::dicom_dataset> results;

    auto items_result = database_->query_worklist(storage::worklist_query{});

    if (items_result.is_ok()) {
        for (const auto& item : items_result.value()) {
            if (auto row = worklist_row(item, query_keys)) {
                results.push_back(std::move(*row));
            }
        }
    }

    std::cout << log_prefix() << "  Found " << results.size() << " worklist items\n";
    return results;
}

network::Result<std::monostate> pacs_server_app::handle_mpps_create(
    const services::mpps_instance& instance) {

    std::cout << log_prefix() << "MPPS N-CREATE: " << instance.sop_instance_uid << "\n";

    if (database_->find_mpps(instance.sop_instance_uid)) {
        return kcenon::common::make_error<std::monostate>(
            network::dimse::status_error_duplicate_sop_instance, "Duplicate MPPS instance", "mpps");
    }

    // PS3.4 Table F.7.2-1: the study is named inside the Scheduled Step Attributes Sequence.
    std::string study_uid = instance.data.get_string(core::tags::study_instance_uid);
    std::string accession = instance.data.get_string(core::tags::accession_number);
    if (const auto* scheduled = instance.data.get_sequence(core::dicom_tag{0x0040, 0x0270});
        scheduled != nullptr && !scheduled->empty()) {
        study_uid = scheduled->front().get_string(core::tags::study_instance_uid);
        accession = scheduled->front().get_string(core::tags::accession_number);
    }

    auto result = database_->create_mpps(
        instance.sop_instance_uid,
        instance.station_ae,
        instance.data.get_string(core::tags::modality),
        study_uid,
        accession,
        instance.data.get_string(core::tags::performed_procedure_step_start_date) +
            instance.data.get_string(core::tags::performed_procedure_step_start_time)
    );

    if (result.is_err()) {
        return kcenon::common::make_error<std::monostate>(1, "MPPS creation failed");
    }

    return network::Result<std::monostate>(std::monostate{});
}

network::Result<std::monostate> pacs_server_app::handle_mpps_set(
    const std::string& sop_instance_uid,
    const core::dicom_dataset& modifications,
    services::mpps_status new_status) {

    std::cout << log_prefix() << "MPPS N-SET: " << sop_instance_uid
              << " -> " << services::to_string(new_status) << "\n";

    // PS3.4 F.7.2.2: 0112H for an unknown instance, 0110H once it may no longer be updated.
    const auto existing = database_->find_mpps(sop_instance_uid);
    if (!existing) {
        return kcenon::common::make_error<std::monostate>(
            network::dimse::status_error_no_such_object_instance, "No such MPPS instance", "mpps");
    }
    if (existing->status != "IN PROGRESS") {
        return kcenon::common::make_error<std::monostate>(
            network::dimse::status_error_processing_failure, "MPPS may no longer be updated", "mpps");
    }

    auto result = database_->update_mpps(
        sop_instance_uid,
        std::string(services::to_string(new_status)),
        modifications.get_string(services::mpps_tags::performed_procedure_step_end_date) +
            modifications.get_string(services::mpps_tags::performed_procedure_step_end_time)
    );

    if (result.is_err()) {
        return kcenon::common::make_error<std::monostate>(1, "MPPS update failed");
    }

    return network::Result<std::monostate>(std::monostate{});
}

}  // namespace kcenon::pacs::example
