/**
 * @file query_result_stream.cpp
 * @brief Implementation of streaming query results with pagination
 *
 * When compiled with PACS_WITH_DATABASE_SYSTEM, uses pacs_database_adapter
 * for unified database access.
 *
 * @see Issue #642 - Migrate database_cursor to unified implementation
 */

#include "pacs/services/cache/query_result_stream.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/storage/index_database.hpp"

#include <sstream>

namespace pacs::services {

// =============================================================================
// Construction
// =============================================================================

query_result_stream::query_result_stream(std::unique_ptr<database_cursor> cursor,
                                         query_level level,
                                         const core::dicom_dataset& query_keys,
                                         stream_config config)
    : cursor_(std::move(cursor)),
      level_(level),
      query_keys_(query_keys),
      config_(config) {}

// =============================================================================
// Query Extraction Helpers
// =============================================================================

auto query_result_stream::extract_patient_query(const core::dicom_dataset& keys)
    -> storage::patient_query {
    storage::patient_query query;

    auto patient_id = keys.get_string(core::tags::patient_id);
    if (!patient_id.empty()) {
        query.patient_id = std::string(patient_id);
    }

    auto patient_name = keys.get_string(core::tags::patient_name);
    if (!patient_name.empty()) {
        query.patient_name = std::string(patient_name);
    }

    auto birth_date = keys.get_string(core::tags::patient_birth_date);
    if (!birth_date.empty()) {
        // Handle date range (YYYYMMDD-YYYYMMDD)
        if (birth_date.find('-') != std::string::npos) {
            auto pos = birth_date.find('-');
            if (pos > 0) {
                query.birth_date_from = std::string(birth_date.substr(0, pos));
            }
            if (pos + 1 < birth_date.length()) {
                query.birth_date_to = std::string(birth_date.substr(pos + 1));
            }
        } else {
            query.birth_date = std::string(birth_date);
        }
    }

    auto sex = keys.get_string(core::tags::patient_sex);
    if (!sex.empty()) {
        query.sex = std::string(sex);
    }

    return query;
}

auto query_result_stream::extract_study_query(const core::dicom_dataset& keys)
    -> storage::study_query {
    storage::study_query query;

    auto patient_id = keys.get_string(core::tags::patient_id);
    if (!patient_id.empty()) {
        query.patient_id = std::string(patient_id);
    }

    auto patient_name = keys.get_string(core::tags::patient_name);
    if (!patient_name.empty()) {
        query.patient_name = std::string(patient_name);
    }

    auto study_uid = keys.get_string(core::tags::study_instance_uid);
    if (!study_uid.empty()) {
        query.study_uid = std::string(study_uid);
    }

    auto study_id = keys.get_string(core::tags::study_id);
    if (!study_id.empty()) {
        query.study_id = std::string(study_id);
    }

    auto study_date = keys.get_string(core::tags::study_date);
    if (!study_date.empty()) {
        // Handle date range (YYYYMMDD-YYYYMMDD)
        if (study_date.find('-') != std::string::npos) {
            auto pos = study_date.find('-');
            if (pos > 0) {
                query.study_date_from = std::string(study_date.substr(0, pos));
            }
            if (pos + 1 < study_date.length()) {
                query.study_date_to = std::string(study_date.substr(pos + 1));
            }
        } else {
            query.study_date = std::string(study_date);
        }
    }

    auto accession = keys.get_string(core::tags::accession_number);
    if (!accession.empty()) {
        query.accession_number = std::string(accession);
    }

    auto modality = keys.get_string(core::tags::modality);
    if (!modality.empty()) {
        query.modality = std::string(modality);
    }

    auto referring = keys.get_string(core::tags::referring_physician_name);
    if (!referring.empty()) {
        query.referring_physician = std::string(referring);
    }

    auto description = keys.get_string(core::tags::study_description);
    if (!description.empty()) {
        query.study_description = std::string(description);
    }

    return query;
}

auto query_result_stream::extract_series_query(const core::dicom_dataset& keys)
    -> storage::series_query {
    storage::series_query query;

    auto study_uid = keys.get_string(core::tags::study_instance_uid);
    if (!study_uid.empty()) {
        query.study_uid = std::string(study_uid);
    }

    auto series_uid = keys.get_string(core::tags::series_instance_uid);
    if (!series_uid.empty()) {
        query.series_uid = std::string(series_uid);
    }

    auto modality = keys.get_string(core::tags::modality);
    if (!modality.empty()) {
        query.modality = std::string(modality);
    }

    auto series_number_str = keys.get_string(core::tags::series_number);
    if (!series_number_str.empty()) {
        try {
            query.series_number = std::stoi(std::string(series_number_str));
        } catch (...) {
            // Ignore invalid number
        }
    }

    auto description = keys.get_string(core::tags::series_description);
    if (!description.empty()) {
        query.series_description = std::string(description);
    }

    return query;
}

auto query_result_stream::extract_instance_query(const core::dicom_dataset& keys)
    -> storage::instance_query {
    storage::instance_query query;

    auto series_uid = keys.get_string(core::tags::series_instance_uid);
    if (!series_uid.empty()) {
        query.series_uid = std::string(series_uid);
    }

    auto sop_uid = keys.get_string(core::tags::sop_instance_uid);
    if (!sop_uid.empty()) {
        query.sop_uid = std::string(sop_uid);
    }

    auto sop_class = keys.get_string(core::tags::sop_class_uid);
    if (!sop_class.empty()) {
        query.sop_class_uid = std::string(sop_class);
    }

    auto instance_number_str = keys.get_string(core::tags::instance_number);
    if (!instance_number_str.empty()) {
        try {
            query.instance_number = std::stoi(std::string(instance_number_str));
        } catch (...) {
            // Ignore invalid number
        }
    }

    return query;
}

// =============================================================================
// Factory Methods
// =============================================================================

auto query_result_stream::create(storage::index_database* db, query_level level,
                                 const core::dicom_dataset& query_keys,
                                 const stream_config& config)
    -> Result<std::unique_ptr<query_result_stream>> {
    if (!db || !db->is_open()) {
        return kcenon::common::error_info(std::string("Database is not open"));
    }

    // Create the appropriate cursor based on query level
    Result<std::unique_ptr<database_cursor>> cursor_result = kcenon::common::error_info(
        std::string("Unknown query level"));

    // Use database_system's pacs_database_adapter for SQL injection safe queries
    auto db_adapter = db->db_adapter();
    if (!db_adapter) {
        return kcenon::common::error_info(std::string("Invalid database adapter"));
    }

    switch (level) {
        case query_level::patient: {
            auto query = extract_patient_query(query_keys);
            cursor_result = database_cursor::create_patient_cursor(db_adapter, query);
            break;
        }
        case query_level::study: {
            auto query = extract_study_query(query_keys);
            cursor_result = database_cursor::create_study_cursor(db_adapter, query);
            break;
        }
        case query_level::series: {
            auto query = extract_series_query(query_keys);
            cursor_result = database_cursor::create_series_cursor(db_adapter, query);
            break;
        }
        case query_level::image: {
            auto query = extract_instance_query(query_keys);
            cursor_result = database_cursor::create_instance_cursor(db_adapter, query);
            break;
        }
    }

    if (cursor_result.is_err()) {
        return kcenon::common::error_info(
            std::string("Failed to create cursor: ") + cursor_result.error().message);
    }

    return std::unique_ptr<query_result_stream>(new query_result_stream(
        std::move(cursor_result.value()), level, query_keys, config));
}

auto query_result_stream::from_cursor(storage::index_database* db,
                                      const std::string& cursor_state, query_level level,
                                      const core::dicom_dataset& query_keys,
                                      const stream_config& config)
    -> Result<std::unique_ptr<query_result_stream>> {
    // Parse cursor state
    std::istringstream iss(cursor_state);
    int type_int = 0;
    size_t position = 0;
    char colon;

    if (!(iss >> type_int >> colon >> position) || colon != ':') {
        return kcenon::common::error_info(std::string("Invalid cursor state format"));
    }

    // Create a new stream and skip to the position
    auto stream_result = create(db, level, query_keys, config);
    if (stream_result.is_err()) {
        return stream_result;
    }

    auto stream = std::move(stream_result.value());

    // Skip to the saved position
    for (size_t i = 0; i < position && stream->has_more(); ++i) {
        (void)stream->cursor_->fetch_next();
    }

    return stream;
}

// =============================================================================
// Stream Operations
// =============================================================================

auto query_result_stream::has_more() const noexcept -> bool {
    return cursor_ && cursor_->has_more();
}

auto query_result_stream::next_batch()
    -> std::optional<std::vector<core::dicom_dataset>> {
    if (!cursor_ || !cursor_->has_more()) {
        return std::nullopt;
    }

    auto records = cursor_->fetch_batch(config_.page_size);
    if (records.empty()) {
        return std::nullopt;
    }

    std::vector<core::dicom_dataset> datasets;
    datasets.reserve(records.size());

    for (const auto& record : records) {
        datasets.push_back(record_to_dataset(record));
    }

    return datasets;
}

auto query_result_stream::total_count() const -> std::optional<size_t> {
    return total_count_;
}

auto query_result_stream::position() const noexcept -> size_t {
    return cursor_ ? cursor_->position() : 0;
}

auto query_result_stream::level() const noexcept -> query_level {
    return level_;
}

auto query_result_stream::cursor() const -> std::string {
    if (!cursor_) {
        return "";
    }
    return cursor_->serialize();
}

// =============================================================================
// Record to Dataset Conversion
// =============================================================================

auto query_result_stream::record_to_dataset(const query_record& record) const
    -> core::dicom_dataset {
    return std::visit(
        [this](const auto& rec) -> core::dicom_dataset {
            using T = std::decay_t<decltype(rec)>;
            if constexpr (std::is_same_v<T, storage::patient_record>) {
                return patient_to_dataset(rec);
            } else if constexpr (std::is_same_v<T, storage::study_record>) {
                return study_to_dataset(rec);
            } else if constexpr (std::is_same_v<T, storage::series_record>) {
                return series_to_dataset(rec);
            } else if constexpr (std::is_same_v<T, storage::instance_record>) {
                return instance_to_dataset(rec);
            }
        },
        record);
}

auto query_result_stream::patient_to_dataset(const storage::patient_record& record) const
    -> core::dicom_dataset {
    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "PATIENT");

    // Patient Module
    ds.set_string(tags::patient_id, vr_type::LO, record.patient_id);
    ds.set_string(tags::patient_name, vr_type::PN, record.patient_name);
    ds.set_string(tags::patient_birth_date, vr_type::DA, record.birth_date);
    ds.set_string(tags::patient_sex, vr_type::CS, record.sex);

    return ds;
}

auto query_result_stream::study_to_dataset(const storage::study_record& record) const
    -> core::dicom_dataset {
    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "STUDY");

    // Study Module
    ds.set_string(tags::study_instance_uid, vr_type::UI, record.study_uid);
    ds.set_string(tags::study_id, vr_type::SH, record.study_id);
    ds.set_string(tags::study_date, vr_type::DA, record.study_date);
    ds.set_string(tags::study_time, vr_type::TM, record.study_time);
    ds.set_string(tags::accession_number, vr_type::SH, record.accession_number);
    ds.set_string(tags::referring_physician_name, vr_type::PN,
                  record.referring_physician);
    ds.set_string(tags::study_description, vr_type::LO, record.study_description);
    ds.set_string(tags::modalities_in_study, vr_type::CS, record.modalities_in_study);

    // Study counts
    ds.set_string(tags::number_of_study_related_series, vr_type::IS,
                  std::to_string(record.num_series));
    ds.set_string(tags::number_of_study_related_instances, vr_type::IS,
                  std::to_string(record.num_instances));

    return ds;
}

auto query_result_stream::series_to_dataset(const storage::series_record& record) const
    -> core::dicom_dataset {
    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "SERIES");

    // Series Module
    ds.set_string(tags::series_instance_uid, vr_type::UI, record.series_uid);
    ds.set_string(tags::modality, vr_type::CS, record.modality);
    ds.set_string(tags::series_description, vr_type::LO, record.series_description);

    if (record.series_number.has_value()) {
        ds.set_string(tags::series_number, vr_type::IS,
                      std::to_string(record.series_number.value()));
    }

    // Series counts
    ds.set_string(tags::number_of_series_related_instances, vr_type::IS,
                  std::to_string(record.num_instances));

    return ds;
}

auto query_result_stream::instance_to_dataset(const storage::instance_record& record) const
    -> core::dicom_dataset {
    using namespace core;
    using namespace encoding;

    dicom_dataset ds;

    // Query/Retrieve Level
    ds.set_string(tags::query_retrieve_level, vr_type::CS, "IMAGE");

    // Instance Module
    ds.set_string(tags::sop_instance_uid, vr_type::UI, record.sop_uid);
    ds.set_string(tags::sop_class_uid, vr_type::UI, record.sop_class_uid);

    if (record.instance_number.has_value()) {
        ds.set_string(tags::instance_number, vr_type::IS,
                      std::to_string(record.instance_number.value()));
    }

    return ds;
}

}  // namespace pacs::services

#endif  // PACS_WITH_DATABASE_SYSTEM
