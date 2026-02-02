/**
 * @file database_cursor.cpp
 * @brief Implementation of database cursor for streaming query results
 *
 * Uses pacs_database_adapter for unified database access, eliminating
 * the previous dual implementation pattern.
 *
 * @see Issue #642 - Migrate database_cursor to unified implementation
 */

#include "pacs/services/cache/database_cursor.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <database/query_builder.h>

#include <iomanip>
#include <sstream>

namespace pacs::services {

// Use common_system's result helpers
using kcenon::common::ok;

// =============================================================================
// Common Helper Functions
// =============================================================================

auto database_cursor::to_like_pattern(std::string_view pattern) -> std::string {
    std::string result;
    result.reserve(pattern.size());

    for (char c : pattern) {
        if (c == '*') {
            result += '%';
        } else if (c == '?') {
            result += '_';
        } else if (c == '%' || c == '_') {
            // Escape SQL wildcards
            result += '\\';
            result += c;
        } else {
            result += c;
        }
    }

    return result;
}

auto database_cursor::contains_dicom_wildcards(std::string_view pattern) -> bool {
    return pattern.find('*') != std::string_view::npos ||
           pattern.find('?') != std::string_view::npos;
}

namespace {

auto parse_timestamp(const std::string& timestamp)
    -> std::chrono::system_clock::time_point {
    // Simple ISO 8601 parsing (YYYY-MM-DD HH:MM:SS)
    if (timestamp.empty()) {
        return std::chrono::system_clock::now();
    }

    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

    if (ss.fail()) {
        return std::chrono::system_clock::now();
    }

    auto time_t_val = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t_val);
}

}  // namespace

// =============================================================================
// Common Cursor Operations
// =============================================================================

auto database_cursor::has_more() const noexcept -> bool {
    return has_more_;
}

auto database_cursor::position() const noexcept -> size_t {
    return position_;
}

auto database_cursor::type() const noexcept -> record_type {
    return type_;
}

auto database_cursor::serialize() const -> std::string {
    std::ostringstream oss;
    oss << static_cast<int>(type_) << ":" << position_;
    return oss.str();
}

// =============================================================================
// Construction / Destruction
// =============================================================================

database_cursor::database_cursor(std::vector<query_record> results, record_type type)
    : results_(std::move(results)), type_(type), has_more_(!results_.empty()) {}

database_cursor::~database_cursor() = default;

database_cursor::database_cursor(database_cursor&& other) noexcept
    : results_(std::move(other.results_)),
      type_(other.type_),
      position_(other.position_),
      has_more_(other.has_more_),
      stepped_(other.stepped_) {
    other.has_more_ = false;
}

auto database_cursor::operator=(database_cursor&& other) noexcept -> database_cursor& {
    if (this != &other) {
        results_ = std::move(other.results_);
        type_ = other.type_;
        position_ = other.position_;
        has_more_ = other.has_more_;
        stepped_ = other.stepped_;

        other.has_more_ = false;
    }
    return *this;
}

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

auto get_string_value(const storage::database_row& row,
                      const std::string& key) -> std::string {
    if (auto it = row.find(key); it != row.end()) {
        return it->second;
    }
    return "";
}

auto get_int64_value(const storage::database_row& row,
                     const std::string& key) -> int64_t {
    auto str_value = get_string_value(row, key);
    if (str_value.empty()) {
        return 0;
    }
    try {
        return std::stoll(str_value);
    } catch (...) {
        return 0;
    }
}

auto get_int_value(const storage::database_row& row,
                   const std::string& key) -> int {
    return static_cast<int>(get_int64_value(row, key));
}

auto get_optional_int(const storage::database_row& row,
                      const std::string& key) -> std::optional<int> {
    auto str_value = get_string_value(row, key);
    if (str_value.empty()) {
        return std::nullopt;
    }
    try {
        return std::stoi(str_value);
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

auto database_cursor::build_dicom_condition(
    const std::string& field,
    const std::string& value) -> std::string {
    if (contains_dicom_wildcards(value)) {
        // Use LIKE with DICOM-to-SQL wildcard conversion
        return field + " LIKE '" + to_like_pattern(value) + "' ESCAPE '\\'";
    } else {
        // Exact match
        return field + " = '" + value + "'";
    }
}

// =============================================================================
// Row Parsing for database_system
// =============================================================================

auto database_cursor::parse_patient_row(const storage::database_row& row)
    -> storage::patient_record {
    storage::patient_record record;

    record.pk = get_int64_value(row, "patient_pk");
    record.patient_id = get_string_value(row, "patient_id");
    record.patient_name = get_string_value(row, "patient_name");
    record.birth_date = get_string_value(row, "birth_date");
    record.sex = get_string_value(row, "sex");
    record.other_ids = get_string_value(row, "other_ids");
    record.ethnic_group = get_string_value(row, "ethnic_group");
    record.comments = get_string_value(row, "comments");
    record.created_at = parse_timestamp(get_string_value(row, "created_at"));
    record.updated_at = parse_timestamp(get_string_value(row, "updated_at"));

    return record;
}

auto database_cursor::parse_study_row(const storage::database_row& row)
    -> storage::study_record {
    storage::study_record record;

    record.pk = get_int64_value(row, "study_pk");
    record.patient_pk = get_int64_value(row, "patient_pk");
    record.study_uid = get_string_value(row, "study_uid");
    record.study_id = get_string_value(row, "study_id");
    record.study_date = get_string_value(row, "study_date");
    record.study_time = get_string_value(row, "study_time");
    record.accession_number = get_string_value(row, "accession_number");
    record.referring_physician = get_string_value(row, "referring_physician");
    record.study_description = get_string_value(row, "study_description");
    record.modalities_in_study = get_string_value(row, "modalities_in_study");
    record.num_series = get_int_value(row, "num_series");
    record.num_instances = get_int_value(row, "num_instances");
    record.created_at = parse_timestamp(get_string_value(row, "created_at"));
    record.updated_at = parse_timestamp(get_string_value(row, "updated_at"));

    return record;
}

auto database_cursor::parse_series_row(const storage::database_row& row)
    -> storage::series_record {
    storage::series_record record;

    record.pk = get_int64_value(row, "series_pk");
    record.study_pk = get_int64_value(row, "study_pk");
    record.series_uid = get_string_value(row, "series_uid");
    record.modality = get_string_value(row, "modality");
    record.series_number = get_optional_int(row, "series_number");
    record.series_description = get_string_value(row, "series_description");
    record.body_part_examined = get_string_value(row, "body_part_examined");
    record.station_name = get_string_value(row, "station_name");
    record.num_instances = get_int_value(row, "num_instances");
    record.created_at = parse_timestamp(get_string_value(row, "created_at"));
    record.updated_at = parse_timestamp(get_string_value(row, "updated_at"));

    return record;
}

auto database_cursor::parse_instance_row(const storage::database_row& row)
    -> storage::instance_record {
    storage::instance_record record;

    record.pk = get_int64_value(row, "instance_pk");
    record.series_pk = get_int64_value(row, "series_pk");
    record.sop_uid = get_string_value(row, "sop_uid");
    record.sop_class_uid = get_string_value(row, "sop_class_uid");
    record.file_path = get_string_value(row, "file_path");
    record.file_size = get_int64_value(row, "file_size");
    record.transfer_syntax = get_string_value(row, "transfer_syntax");
    record.instance_number = get_optional_int(row, "instance_number");
    record.created_at = parse_timestamp(get_string_value(row, "created_at"));

    return record;
}

// =============================================================================
// Factory Methods for database_system
// =============================================================================

auto database_cursor::create_patient_cursor(
    std::shared_ptr<storage::pacs_database_adapter> db,
    const storage::patient_query& query) -> Result<std::unique_ptr<database_cursor>> {
    auto builder = db->create_query_builder();

    builder.select(std::vector<std::string>{
               "patient_pk", "patient_id", "patient_name", "birth_date", "sex",
               "other_ids", "ethnic_group", "comments", "created_at", "updated_at"})
        .from("patients")
        .order_by("patient_name");

    // Apply DICOM wildcard conditions
    if (query.patient_id.has_value()) {
        auto condition = build_dicom_condition("patient_id", *query.patient_id);
        builder.where(database::query_condition(condition));
    }
    if (query.patient_name.has_value()) {
        auto condition = build_dicom_condition("patient_name", *query.patient_name);
        builder.where(database::query_condition(condition));
    }
    if (query.birth_date.has_value()) {
        builder.where("birth_date", "=", *query.birth_date);
    }
    if (query.birth_date_from.has_value()) {
        builder.where("birth_date", ">=", *query.birth_date_from);
    }
    if (query.birth_date_to.has_value()) {
        builder.where("birth_date", "<=", *query.birth_date_to);
    }
    if (query.sex.has_value()) {
        builder.where("sex", "=", *query.sex);
    }

    auto sql = builder.build();
    auto result = db->select(sql);

    if (result.is_err()) {
        return kcenon::common::error_info(
            std::string("Failed to execute patient cursor query: ") +
            result.error().message);
    }

    std::vector<query_record> records;
    for (const auto& row : result.value().rows) {
        records.push_back(parse_patient_row(row));
    }

    return std::unique_ptr<database_cursor>(
        new database_cursor(std::move(records), record_type::patient));
}

auto database_cursor::create_study_cursor(
    std::shared_ptr<storage::pacs_database_adapter> db,
    const storage::study_query& query) -> Result<std::unique_ptr<database_cursor>> {
    auto builder = db->create_query_builder();

    builder.select(std::vector<std::string>{
               "s.study_pk", "s.patient_pk", "s.study_uid", "s.study_id",
               "s.study_date", "s.study_time", "s.accession_number",
               "s.referring_physician", "s.study_description",
               "s.modalities_in_study", "s.num_series", "s.num_instances",
               "s.created_at", "s.updated_at"})
        .from("studies s")
        .join("patients p", "s.patient_pk = p.patient_pk")
        .order_by("s.study_date", database::sort_order::desc)
        .order_by("s.study_time", database::sort_order::desc);

    // Apply DICOM wildcard conditions
    if (query.patient_id.has_value()) {
        auto condition = build_dicom_condition("p.patient_id", *query.patient_id);
        builder.where(database::query_condition(condition));
    }
    if (query.patient_name.has_value()) {
        auto condition = build_dicom_condition("p.patient_name", *query.patient_name);
        builder.where(database::query_condition(condition));
    }
    if (query.study_uid.has_value()) {
        builder.where("s.study_uid", "=", *query.study_uid);
    }
    if (query.study_id.has_value()) {
        auto condition = build_dicom_condition("s.study_id", *query.study_id);
        builder.where(database::query_condition(condition));
    }
    if (query.study_date.has_value()) {
        builder.where("s.study_date", "=", *query.study_date);
    }
    if (query.study_date_from.has_value()) {
        builder.where("s.study_date", ">=", *query.study_date_from);
    }
    if (query.study_date_to.has_value()) {
        builder.where("s.study_date", "<=", *query.study_date_to);
    }
    if (query.accession_number.has_value()) {
        auto condition = build_dicom_condition("s.accession_number", *query.accession_number);
        builder.where(database::query_condition(condition));
    }
    if (query.modality.has_value()) {
        builder.where(database::query_condition(
            "s.modalities_in_study LIKE '%" + *query.modality + "%'"));
    }
    if (query.referring_physician.has_value()) {
        auto condition = build_dicom_condition("s.referring_physician", *query.referring_physician);
        builder.where(database::query_condition(condition));
    }
    if (query.study_description.has_value()) {
        auto condition = build_dicom_condition("s.study_description", *query.study_description);
        builder.where(database::query_condition(condition));
    }

    auto sql = builder.build();
    auto result = db->select(sql);

    if (result.is_err()) {
        return kcenon::common::error_info(
            std::string("Failed to execute study cursor query: ") +
            result.error().message);
    }

    std::vector<query_record> records;
    for (const auto& row : result.value().rows) {
        records.push_back(parse_study_row(row));
    }

    return std::unique_ptr<database_cursor>(
        new database_cursor(std::move(records), record_type::study));
}

auto database_cursor::create_series_cursor(
    std::shared_ptr<storage::pacs_database_adapter> db,
    const storage::series_query& query) -> Result<std::unique_ptr<database_cursor>> {
    auto builder = db->create_query_builder();

    builder.select(std::vector<std::string>{
               "se.series_pk", "se.study_pk", "se.series_uid", "se.modality",
               "se.series_number", "se.series_description", "se.body_part_examined",
               "se.station_name", "se.num_instances", "se.created_at", "se.updated_at"})
        .from("series se")
        .join("studies st", "se.study_pk = st.study_pk")
        .order_by("se.series_number");

    // Apply conditions
    if (query.study_uid.has_value()) {
        builder.where("st.study_uid", "=", *query.study_uid);
    }
    if (query.series_uid.has_value()) {
        builder.where("se.series_uid", "=", *query.series_uid);
    }
    if (query.modality.has_value()) {
        builder.where("se.modality", "=", *query.modality);
    }
    if (query.series_number.has_value()) {
        builder.where("se.series_number", "=", static_cast<int64_t>(*query.series_number));
    }
    if (query.series_description.has_value()) {
        auto condition = build_dicom_condition("se.series_description", *query.series_description);
        builder.where(database::query_condition(condition));
    }

    auto sql = builder.build();
    auto result = db->select(sql);

    if (result.is_err()) {
        return kcenon::common::error_info(
            std::string("Failed to execute series cursor query: ") +
            result.error().message);
    }

    std::vector<query_record> records;
    for (const auto& row : result.value().rows) {
        records.push_back(parse_series_row(row));
    }

    return std::unique_ptr<database_cursor>(
        new database_cursor(std::move(records), record_type::series));
}

auto database_cursor::create_instance_cursor(
    std::shared_ptr<storage::pacs_database_adapter> db,
    const storage::instance_query& query) -> Result<std::unique_ptr<database_cursor>> {
    auto builder = db->create_query_builder();

    builder.select(std::vector<std::string>{
               "i.instance_pk", "i.series_pk", "i.sop_uid", "i.sop_class_uid",
               "i.file_path", "i.file_size", "i.transfer_syntax", "i.instance_number",
               "i.created_at"})
        .from("instances i")
        .join("series se", "i.series_pk = se.series_pk")
        .order_by("i.instance_number");

    // Apply conditions
    if (query.series_uid.has_value()) {
        builder.where("se.series_uid", "=", *query.series_uid);
    }
    if (query.sop_uid.has_value()) {
        builder.where("i.sop_uid", "=", *query.sop_uid);
    }
    if (query.sop_class_uid.has_value()) {
        builder.where("i.sop_class_uid", "=", *query.sop_class_uid);
    }
    if (query.instance_number.has_value()) {
        builder.where("i.instance_number", "=",
                      static_cast<int64_t>(*query.instance_number));
    }

    auto sql = builder.build();
    auto result = db->select(sql);

    if (result.is_err()) {
        return kcenon::common::error_info(
            std::string("Failed to execute instance cursor query: ") +
            result.error().message);
    }

    std::vector<query_record> records;
    for (const auto& row : result.value().rows) {
        records.push_back(parse_instance_row(row));
    }

    return std::unique_ptr<database_cursor>(
        new database_cursor(std::move(records), record_type::instance));
}

// =============================================================================
// Cursor Operations for database_system
// =============================================================================

auto database_cursor::fetch_next() -> std::optional<query_record> {
    if (!has_more_ || position_ >= results_.size()) {
        has_more_ = false;
        return std::nullopt;
    }

    stepped_ = true;
    auto record = results_[position_];
    ++position_;

    if (position_ >= results_.size()) {
        has_more_ = false;
    }

    return record;
}

auto database_cursor::fetch_batch(size_t batch_size) -> std::vector<query_record> {
    std::vector<query_record> batch;
    batch.reserve(batch_size);

    while (batch.size() < batch_size && has_more_) {
        auto record = fetch_next();
        if (record.has_value()) {
            batch.push_back(std::move(record.value()));
        }
    }

    return batch;
}

auto database_cursor::reset() -> VoidResult {
    position_ = 0;
    has_more_ = !results_.empty();
    stepped_ = false;
    return ok();
}

}  // namespace pacs::services

#endif  // PACS_WITH_DATABASE_SYSTEM
