/**
 * @file prefetch_repository.cpp
 * @brief Implementation of the prefetch repository for rule and history persistence
 *
 * @see Issue #541 - Implement Prefetch Manager for Proactive Data Loading
 * @see Issue #530 - PACS Client System Support (Parent Epic)
 * @see Issue #651 - Part 4: Migrate sync, viewer_state, prefetch repositories
 */

#include "pacs/storage/prefetch_repository.hpp"

#include <chrono>
#include <cstring>
#include <sstream>

#ifdef PACS_WITH_DATABASE_SYSTEM

// =============================================================================
// pacs_database_adapter Implementation
// =============================================================================

namespace pacs::storage {

namespace {

/// Convert time_point to ISO8601 string
[[nodiscard]] std::string to_timestamp_string(
    std::chrono::system_clock::time_point tp) {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

/// Parse ISO8601 string to time_point
[[nodiscard]] std::chrono::system_clock::time_point from_timestamp_string(
    const std::string& str) {
    if (str.empty()) {
        return {};
    }
    std::tm tm{};
    if (std::sscanf(str.c_str(), "%d-%d-%d %d:%d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return {};
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(time);
}

/// Escape string for JSON
[[nodiscard]] std::string escape_json_string(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c; break;
        }
    }
    return oss.str();
}

/// Unescape JSON string
[[nodiscard]] std::string unescape_json_string(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            ++i;
            switch (str[i]) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

/// Extract JSON string value
[[nodiscard]] std::pair<std::string, size_t> extract_json_string(
    std::string_view json, size_t pos) {
    auto start = json.find('"', pos);
    if (start == std::string_view::npos) return {"", std::string_view::npos};

    size_t end = start + 1;
    while (end < json.size()) {
        if (json[end] == '\\' && end + 1 < json.size()) {
            end += 2;
        } else if (json[end] == '"') {
            break;
        } else {
            ++end;
        }
    }

    if (end >= json.size()) return {"", std::string_view::npos};

    auto value = unescape_json_string(json.substr(start + 1, end - start - 1));
    return {value, end + 1};
}

}  // namespace

// =============================================================================
// JSON Serialization
// =============================================================================

std::string prefetch_repository::serialize_modalities(
    const std::vector<std::string>& modalities) {
    if (modalities.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < modalities.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escape_json_string(modalities[i]) << "\"";
    }
    oss << "]";
    return oss.str();
}

std::vector<std::string> prefetch_repository::deserialize_modalities(
    std::string_view json) {
    std::vector<std::string> result;
    if (json.empty() || json == "[]") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto [value, next_pos] = extract_json_string(json, pos);
        if (next_pos == std::string_view::npos) break;
        if (!value.empty()) {
            result.push_back(value);
        }
        pos = next_pos;
    }

    return result;
}

std::string prefetch_repository::serialize_node_ids(
    const std::vector<std::string>& node_ids) {
    return serialize_modalities(node_ids);
}

std::vector<std::string> prefetch_repository::deserialize_node_ids(
    std::string_view json) {
    return deserialize_modalities(json);
}

// =============================================================================
// Construction / Destruction
// =============================================================================

prefetch_repository::prefetch_repository(std::shared_ptr<pacs_database_adapter> db)
    : db_(std::move(db)) {}

prefetch_repository::~prefetch_repository() = default;

prefetch_repository::prefetch_repository(prefetch_repository&&) noexcept = default;

auto prefetch_repository::operator=(prefetch_repository&&) noexcept
    -> prefetch_repository& = default;

// =============================================================================
// Timestamp Helpers
// =============================================================================

auto prefetch_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    return from_timestamp_string(str);
}

auto prefetch_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
    return to_timestamp_string(tp);
}

// =============================================================================
// Database Initialization
// =============================================================================

VoidResult prefetch_repository::initialize_tables() {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    auto result = db_->execute(R"(
        CREATE TABLE IF NOT EXISTS prefetch_rules (
            pk INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id TEXT UNIQUE NOT NULL,
            name TEXT NOT NULL,
            enabled INTEGER DEFAULT 1,
            trigger_type TEXT NOT NULL,
            modality_filter TEXT,
            body_part_filter TEXT,
            station_ae_filter TEXT,
            prior_lookback_hours INTEGER DEFAULT 8760,
            max_prior_studies INTEGER DEFAULT 3,
            prior_modalities_json TEXT,
            source_node_ids_json TEXT NOT NULL,
            schedule_cron TEXT,
            advance_time_minutes INTEGER DEFAULT 60,
            triggered_count INTEGER DEFAULT 0,
            studies_prefetched INTEGER DEFAULT 0,
            last_triggered TIMESTAMP,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");

    if (result.is_err()) {
        return result;
    }

    result = db_->execute(R"(
        CREATE TABLE IF NOT EXISTS prefetch_history (
            pk INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_id TEXT NOT NULL,
            study_uid TEXT NOT NULL,
            rule_id TEXT,
            source_node_id TEXT NOT NULL,
            job_id TEXT,
            status TEXT NOT NULL,
            prefetched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )");

    if (result.is_err()) {
        return result;
    }

    result = db_->execute(R"(
        CREATE INDEX IF NOT EXISTS idx_prefetch_history_patient ON prefetch_history(patient_id);
        CREATE INDEX IF NOT EXISTS idx_prefetch_history_study ON prefetch_history(study_uid);
        CREATE INDEX IF NOT EXISTS idx_prefetch_history_status ON prefetch_history(status);
    )");

    return result;
}

// =============================================================================
// Rule CRUD Operations
// =============================================================================

VoidResult prefetch_repository::save_rule(const client::prefetch_rule& rule) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        INSERT INTO prefetch_rules (
            rule_id, name, enabled, trigger_type,
            modality_filter, body_part_filter, station_ae_filter,
            prior_lookback_hours, max_prior_studies, prior_modalities_json,
            source_node_ids_json, schedule_cron, advance_time_minutes,
            triggered_count, studies_prefetched, last_triggered
        ) VALUES (
            ')" << rule.rule_id << "', "
        << "'" << rule.name << "', "
        << (rule.enabled ? 1 : 0) << ", "
        << "'" << client::to_string(rule.trigger) << "', ";

    if (rule.modality_filter.empty()) {
        sql << "NULL, ";
    } else {
        sql << "'" << rule.modality_filter << "', ";
    }

    if (rule.body_part_filter.empty()) {
        sql << "NULL, ";
    } else {
        sql << "'" << rule.body_part_filter << "', ";
    }

    if (rule.station_ae_filter.empty()) {
        sql << "NULL, ";
    } else {
        sql << "'" << rule.station_ae_filter << "', ";
    }

    sql << rule.prior_lookback.count() << ", "
        << rule.max_prior_studies << ", "
        << "'" << serialize_modalities(rule.prior_modalities) << "', "
        << "'" << serialize_node_ids(rule.source_node_ids) << "', ";

    if (rule.schedule_cron.empty()) {
        sql << "NULL, ";
    } else {
        sql << "'" << rule.schedule_cron << "', ";
    }

    sql << rule.advance_time.count() << ", "
        << rule.triggered_count << ", "
        << rule.studies_prefetched << ", ";

    auto last_triggered_str = format_timestamp(rule.last_triggered);
    if (last_triggered_str.empty()) {
        sql << "NULL";
    } else {
        sql << "'" << last_triggered_str << "'";
    }

    sql << R"()
        ON CONFLICT(rule_id) DO UPDATE SET
            name = excluded.name,
            enabled = excluded.enabled,
            trigger_type = excluded.trigger_type,
            modality_filter = excluded.modality_filter,
            body_part_filter = excluded.body_part_filter,
            station_ae_filter = excluded.station_ae_filter,
            prior_lookback_hours = excluded.prior_lookback_hours,
            max_prior_studies = excluded.max_prior_studies,
            prior_modalities_json = excluded.prior_modalities_json,
            source_node_ids_json = excluded.source_node_ids_json,
            schedule_cron = excluded.schedule_cron,
            advance_time_minutes = excluded.advance_time_minutes,
            updated_at = CURRENT_TIMESTAMP
    )";

    auto result = db_->insert(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

std::optional<client::prefetch_rule> prefetch_repository::find_rule_by_id(
    std::string_view rule_id) const {
    if (!db_ || !db_->is_connected()) return std::nullopt;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, rule_id, name, enabled, trigger_type,
               modality_filter, body_part_filter, station_ae_filter,
               prior_lookback_hours, max_prior_studies, prior_modalities_json,
               source_node_ids_json, schedule_cron, advance_time_minutes,
               triggered_count, studies_prefetched, last_triggered
        FROM prefetch_rules WHERE rule_id = ')" << rule_id << "'";

    auto result = db_->select(sql.str());
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_rule(result.value()[0]);
}

std::optional<client::prefetch_rule> prefetch_repository::find_rule_by_pk(
    int64_t pk) const {
    if (!db_ || !db_->is_connected()) return std::nullopt;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, rule_id, name, enabled, trigger_type,
               modality_filter, body_part_filter, station_ae_filter,
               prior_lookback_hours, max_prior_studies, prior_modalities_json,
               source_node_ids_json, schedule_cron, advance_time_minutes,
               triggered_count, studies_prefetched, last_triggered
        FROM prefetch_rules WHERE pk = )" << pk;

    auto result = db_->select(sql.str());
    if (result.is_err() || result.value().empty()) {
        return std::nullopt;
    }

    return map_row_to_rule(result.value()[0]);
}

std::vector<client::prefetch_rule> prefetch_repository::find_rules(
    const prefetch_rule_query_options& options) const {
    std::vector<client::prefetch_rule> rules;
    if (!db_ || !db_->is_connected()) return rules;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, rule_id, name, enabled, trigger_type,
               modality_filter, body_part_filter, station_ae_filter,
               prior_lookback_hours, max_prior_studies, prior_modalities_json,
               source_node_ids_json, schedule_cron, advance_time_minutes,
               triggered_count, studies_prefetched, last_triggered
        FROM prefetch_rules WHERE 1=1
    )";

    if (options.enabled_only.has_value()) {
        sql << " AND enabled = " << (options.enabled_only.value() ? "1" : "0");
    }

    if (options.trigger.has_value()) {
        sql << " AND trigger_type = '" << client::to_string(options.trigger.value()) << "'";
    }

    sql << " ORDER BY created_at DESC";
    sql << " LIMIT " << options.limit << " OFFSET " << options.offset;

    auto result = db_->select(sql.str());
    if (result.is_err()) return rules;

    rules.reserve(result.value().size());
    for (const auto& row : result.value()) {
        rules.push_back(map_row_to_rule(row));
    }

    return rules;
}

std::vector<client::prefetch_rule> prefetch_repository::find_enabled_rules() const {
    prefetch_rule_query_options options;
    options.enabled_only = true;
    return find_rules(options);
}

VoidResult prefetch_repository::remove_rule(std::string_view rule_id) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << "DELETE FROM prefetch_rules WHERE rule_id = '" << rule_id << "'";

    auto result = db_->remove(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

bool prefetch_repository::rule_exists(std::string_view rule_id) const {
    if (!db_ || !db_->is_connected()) return false;

    std::ostringstream sql;
    sql << "SELECT 1 FROM prefetch_rules WHERE rule_id = '" << rule_id << "'";

    auto result = db_->select(sql.str());
    return result.is_ok() && !result.value().empty();
}

// =============================================================================
// Rule Statistics
// =============================================================================

VoidResult prefetch_repository::increment_triggered(std::string_view rule_id) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        UPDATE prefetch_rules SET
            triggered_count = triggered_count + 1,
            last_triggered = CURRENT_TIMESTAMP
        WHERE rule_id = ')" << rule_id << "'";

    auto result = db_->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

VoidResult prefetch_repository::increment_studies_prefetched(
    std::string_view rule_id, size_t count) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        UPDATE prefetch_rules SET
            studies_prefetched = studies_prefetched + )" << count
        << " WHERE rule_id = '" << rule_id << "'";

    auto result = db_->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

VoidResult prefetch_repository::enable_rule(std::string_view rule_id) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        UPDATE prefetch_rules SET
            enabled = 1,
            updated_at = CURRENT_TIMESTAMP
        WHERE rule_id = ')" << rule_id << "'";

    auto result = db_->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

VoidResult prefetch_repository::disable_rule(std::string_view rule_id) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        UPDATE prefetch_rules SET
            enabled = 0,
            updated_at = CURRENT_TIMESTAMP
        WHERE rule_id = ')" << rule_id << "'";

    auto result = db_->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

// =============================================================================
// History Operations
// =============================================================================

VoidResult prefetch_repository::save_history(const client::prefetch_history& history) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << R"(
        INSERT INTO prefetch_history (
            patient_id, study_uid, rule_id, source_node_id, job_id, status
        ) VALUES (
            ')" << history.patient_id << "', "
        << "'" << history.study_uid << "', ";

    if (history.rule_id.empty()) {
        sql << "NULL, ";
    } else {
        sql << "'" << history.rule_id << "', ";
    }

    sql << "'" << history.source_node_id << "', ";

    if (history.job_id.empty()) {
        sql << "NULL, ";
    } else {
        sql << "'" << history.job_id << "', ";
    }

    sql << "'" << history.status << "')";

    auto result = db_->insert(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

std::vector<client::prefetch_history> prefetch_repository::find_history(
    const prefetch_history_query_options& options) const {
    std::vector<client::prefetch_history> histories;
    if (!db_ || !db_->is_connected()) return histories;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, patient_id, study_uid, rule_id, source_node_id, job_id, status, prefetched_at
        FROM prefetch_history WHERE 1=1
    )";

    if (options.patient_id.has_value()) {
        sql << " AND patient_id = '" << options.patient_id.value() << "'";
    }

    if (options.rule_id.has_value()) {
        sql << " AND rule_id = '" << options.rule_id.value() << "'";
    }

    if (options.status.has_value()) {
        sql << " AND status = '" << options.status.value() << "'";
    }

    sql << " ORDER BY prefetched_at DESC";
    sql << " LIMIT " << options.limit << " OFFSET " << options.offset;

    auto result = db_->select(sql.str());
    if (result.is_err()) return histories;

    histories.reserve(result.value().size());
    for (const auto& row : result.value()) {
        histories.push_back(map_row_to_history(row));
    }

    return histories;
}

bool prefetch_repository::is_study_prefetched(std::string_view study_uid) const {
    if (!db_ || !db_->is_connected()) return false;

    std::ostringstream sql;
    sql << R"(
        SELECT 1 FROM prefetch_history
        WHERE study_uid = ')" << study_uid << "' AND status IN ('completed', 'pending')";

    auto result = db_->select(sql.str());
    return result.is_ok() && !result.value().empty();
}

size_t prefetch_repository::count_completed_today() const {
    if (!db_ || !db_->is_connected()) return 0;

    auto result = db_->select(R"(
        SELECT COUNT(*) as count FROM prefetch_history
        WHERE status = 'completed'
        AND date(prefetched_at) = date('now')
    )");

    if (result.is_err() || result.value().empty()) return 0;
    return std::stoull(result.value()[0].at("count"));
}

size_t prefetch_repository::count_failed_today() const {
    if (!db_ || !db_->is_connected()) return 0;

    auto result = db_->select(R"(
        SELECT COUNT(*) as count FROM prefetch_history
        WHERE status = 'failed'
        AND date(prefetched_at) = date('now')
    )");

    if (result.is_err() || result.value().empty()) return 0;
    return std::stoull(result.value()[0].at("count"));
}

VoidResult prefetch_repository::update_history_status(
    std::string_view study_uid,
    std::string_view status) {
    if (!db_ || !db_->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    std::ostringstream sql;
    sql << "UPDATE prefetch_history SET status = '" << status
        << "' WHERE study_uid = '" << study_uid << "'";

    auto result = db_->update(sql.str());
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

Result<size_t> prefetch_repository::cleanup_old_history(std::chrono::hours max_age) {
    if (!db_ || !db_->is_connected()) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not connected", "prefetch_repository"});
    }

    auto cutoff = std::chrono::system_clock::now() - max_age;
    auto cutoff_str = format_timestamp(cutoff);

    std::ostringstream sql;
    sql << "DELETE FROM prefetch_history WHERE prefetched_at < '" << cutoff_str << "'";

    auto result = db_->remove(sql.str());
    if (result.is_err()) {
        return Result<size_t>(result.error());
    }

    return kcenon::common::ok(static_cast<size_t>(result.value()));
}

// =============================================================================
// Statistics
// =============================================================================

size_t prefetch_repository::rule_count() const {
    if (!db_ || !db_->is_connected()) return 0;

    auto result = db_->select("SELECT COUNT(*) as count FROM prefetch_rules");
    if (result.is_err() || result.value().empty()) return 0;
    return std::stoull(result.value()[0].at("count"));
}

size_t prefetch_repository::enabled_rule_count() const {
    if (!db_ || !db_->is_connected()) return 0;

    auto result = db_->select(
        "SELECT COUNT(*) as count FROM prefetch_rules WHERE enabled = 1");
    if (result.is_err() || result.value().empty()) return 0;
    return std::stoull(result.value()[0].at("count"));
}

size_t prefetch_repository::history_count() const {
    if (!db_ || !db_->is_connected()) return 0;

    auto result = db_->select("SELECT COUNT(*) as count FROM prefetch_history");
    if (result.is_err() || result.value().empty()) return 0;
    return std::stoull(result.value()[0].at("count"));
}

// =============================================================================
// Database Information
// =============================================================================

bool prefetch_repository::is_valid() const noexcept {
    return db_ && db_->is_connected();
}

// =============================================================================
// Row Mapping
// =============================================================================

client::prefetch_rule prefetch_repository::map_row_to_rule(
    const database_row& row) const {
    client::prefetch_rule rule;

    rule.pk = std::stoll(row.at("pk"));
    rule.rule_id = row.at("rule_id");
    rule.name = row.at("name");
    rule.enabled = (row.at("enabled") == "1");
    rule.trigger = client::prefetch_trigger_from_string(row.at("trigger_type"));

    auto modality_it = row.find("modality_filter");
    if (modality_it != row.end()) {
        rule.modality_filter = modality_it->second;
    }

    auto body_part_it = row.find("body_part_filter");
    if (body_part_it != row.end()) {
        rule.body_part_filter = body_part_it->second;
    }

    auto station_it = row.find("station_ae_filter");
    if (station_it != row.end()) {
        rule.station_ae_filter = station_it->second;
    }

    rule.prior_lookback = std::chrono::hours{
        std::stoll(row.at("prior_lookback_hours"))};
    rule.max_prior_studies = std::stoull(row.at("max_prior_studies"));
    rule.prior_modalities = deserialize_modalities(row.at("prior_modalities_json"));
    rule.source_node_ids = deserialize_node_ids(row.at("source_node_ids_json"));

    auto schedule_it = row.find("schedule_cron");
    if (schedule_it != row.end()) {
        rule.schedule_cron = schedule_it->second;
    }

    rule.advance_time = std::chrono::minutes{
        std::stoll(row.at("advance_time_minutes"))};
    rule.triggered_count = std::stoull(row.at("triggered_count"));
    rule.studies_prefetched = std::stoull(row.at("studies_prefetched"));

    auto last_triggered_it = row.find("last_triggered");
    if (last_triggered_it != row.end() && !last_triggered_it->second.empty()) {
        rule.last_triggered = parse_timestamp(last_triggered_it->second);
    }

    return rule;
}

client::prefetch_history prefetch_repository::map_row_to_history(
    const database_row& row) const {
    client::prefetch_history history;

    history.pk = std::stoll(row.at("pk"));
    history.patient_id = row.at("patient_id");
    history.study_uid = row.at("study_uid");

    auto rule_id_it = row.find("rule_id");
    if (rule_id_it != row.end()) {
        history.rule_id = rule_id_it->second;
    }

    history.source_node_id = row.at("source_node_id");

    auto job_id_it = row.find("job_id");
    if (job_id_it != row.end()) {
        history.job_id = job_id_it->second;
    }

    history.status = row.at("status");
    history.prefetched_at = parse_timestamp(row.at("prefetched_at"));

    return history;
}

}  // namespace pacs::storage

#else  // !PACS_WITH_DATABASE_SYSTEM

// =============================================================================
// Legacy SQLite Implementation
// =============================================================================

#include <sqlite3.h>

namespace pacs::storage {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/// Convert time_point to ISO8601 string
[[nodiscard]] std::string to_timestamp_string(
    std::chrono::system_clock::time_point tp) {
    if (tp == std::chrono::system_clock::time_point{}) {
        return "";
    }
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

/// Parse ISO8601 string to time_point
[[nodiscard]] std::chrono::system_clock::time_point from_timestamp_string(
    const char* str) {
    if (!str || str[0] == '\0') {
        return {};
    }
    std::tm tm{};
    if (std::sscanf(str, "%d-%d-%d %d:%d:%d",
                    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
        return {};
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
#ifdef _WIN32
    auto time = _mkgmtime(&tm);
#else
    auto time = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(time);
}

/// Get text column safely (returns empty string if NULL)
[[nodiscard]] std::string get_text_column(sqlite3_stmt* stmt, int col) {
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? text : "";
}

/// Get int column with default
[[nodiscard]] int get_int_column(sqlite3_stmt* stmt, int col, int default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int(stmt, col);
}

/// Get int64 column with default
[[nodiscard]] int64_t get_int64_column(sqlite3_stmt* stmt, int col, int64_t default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int64(stmt, col);
}

/// Escape string for JSON
[[nodiscard]] std::string escape_json_string(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:   oss << c; break;
        }
    }
    return oss.str();
}

/// Unescape JSON string
[[nodiscard]] std::string unescape_json_string(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            ++i;
            switch (str[i]) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

/// Extract JSON string value
[[nodiscard]] std::pair<std::string, size_t> extract_json_string(
    std::string_view json, size_t pos) {
    auto start = json.find('"', pos);
    if (start == std::string_view::npos) return {"", std::string_view::npos};

    size_t end = start + 1;
    while (end < json.size()) {
        if (json[end] == '\\' && end + 1 < json.size()) {
            end += 2;
        } else if (json[end] == '"') {
            break;
        } else {
            ++end;
        }
    }

    if (end >= json.size()) return {"", std::string_view::npos};

    auto value = unescape_json_string(json.substr(start + 1, end - start - 1));
    return {value, end + 1};
}

}  // namespace

// =============================================================================
// JSON Serialization for String Arrays
// =============================================================================

std::string prefetch_repository::serialize_modalities(
    const std::vector<std::string>& modalities) {
    if (modalities.empty()) return "[]";

    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < modalities.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << escape_json_string(modalities[i]) << "\"";
    }
    oss << "]";
    return oss.str();
}

std::vector<std::string> prefetch_repository::deserialize_modalities(
    std::string_view json) {
    std::vector<std::string> result;
    if (json.empty() || json == "[]") return result;

    size_t pos = 0;
    while (pos < json.size()) {
        auto [value, next_pos] = extract_json_string(json, pos);
        if (next_pos == std::string_view::npos) break;
        if (!value.empty()) {
            result.push_back(value);
        }
        pos = next_pos;
    }

    return result;
}

std::string prefetch_repository::serialize_node_ids(
    const std::vector<std::string>& node_ids) {
    return serialize_modalities(node_ids);  // Same format
}

std::vector<std::string> prefetch_repository::deserialize_node_ids(
    std::string_view json) {
    return deserialize_modalities(json);  // Same format
}

// =============================================================================
// Construction / Destruction
// =============================================================================

prefetch_repository::prefetch_repository(sqlite3* db) : db_(db) {}

prefetch_repository::~prefetch_repository() = default;

prefetch_repository::prefetch_repository(prefetch_repository&&) noexcept = default;

auto prefetch_repository::operator=(prefetch_repository&&) noexcept -> prefetch_repository& = default;

// =============================================================================
// Database Initialization
// =============================================================================

VoidResult prefetch_repository::initialize_tables() {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    // Create prefetch_rules table
    static constexpr const char* create_rules_sql = R"(
        CREATE TABLE IF NOT EXISTS prefetch_rules (
            pk INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_id TEXT UNIQUE NOT NULL,
            name TEXT NOT NULL,
            enabled INTEGER DEFAULT 1,
            trigger_type TEXT NOT NULL,
            modality_filter TEXT,
            body_part_filter TEXT,
            station_ae_filter TEXT,
            prior_lookback_hours INTEGER DEFAULT 8760,
            max_prior_studies INTEGER DEFAULT 3,
            prior_modalities_json TEXT,
            source_node_ids_json TEXT NOT NULL,
            schedule_cron TEXT,
            advance_time_minutes INTEGER DEFAULT 60,
            triggered_count INTEGER DEFAULT 0,
            studies_prefetched INTEGER DEFAULT 0,
            last_triggered TIMESTAMP,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, create_rules_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to create prefetch_rules table: " + error,
            "prefetch_repository"});
    }

    // Create prefetch_history table
    static constexpr const char* create_history_sql = R"(
        CREATE TABLE IF NOT EXISTS prefetch_history (
            pk INTEGER PRIMARY KEY AUTOINCREMENT,
            patient_id TEXT NOT NULL,
            study_uid TEXT NOT NULL,
            rule_id TEXT,
            source_node_id TEXT NOT NULL,
            job_id TEXT,
            status TEXT NOT NULL,
            prefetched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    rc = sqlite3_exec(db_, create_history_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to create prefetch_history table: " + error,
            "prefetch_repository"});
    }

    // Create indexes
    static constexpr const char* create_indexes_sql = R"(
        CREATE INDEX IF NOT EXISTS idx_prefetch_history_patient ON prefetch_history(patient_id);
        CREATE INDEX IF NOT EXISTS idx_prefetch_history_study ON prefetch_history(study_uid);
        CREATE INDEX IF NOT EXISTS idx_prefetch_history_status ON prefetch_history(status);
    )";

    rc = sqlite3_exec(db_, create_indexes_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to create indexes: " + error,
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

// =============================================================================
// Rule CRUD Operations
// =============================================================================

VoidResult prefetch_repository::save_rule(const client::prefetch_rule& rule) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO prefetch_rules (
            rule_id, name, enabled, trigger_type,
            modality_filter, body_part_filter, station_ae_filter,
            prior_lookback_hours, max_prior_studies, prior_modalities_json,
            source_node_ids_json, schedule_cron, advance_time_minutes,
            triggered_count, studies_prefetched, last_triggered
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(rule_id) DO UPDATE SET
            name = excluded.name,
            enabled = excluded.enabled,
            trigger_type = excluded.trigger_type,
            modality_filter = excluded.modality_filter,
            body_part_filter = excluded.body_part_filter,
            station_ae_filter = excluded.station_ae_filter,
            prior_lookback_hours = excluded.prior_lookback_hours,
            max_prior_studies = excluded.max_prior_studies,
            prior_modalities_json = excluded.prior_modalities_json,
            source_node_ids_json = excluded.source_node_ids_json,
            schedule_cron = excluded.schedule_cron,
            advance_time_minutes = excluded.advance_time_minutes,
            updated_at = CURRENT_TIMESTAMP
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, rule.rule_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, rule.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, rule.enabled ? 1 : 0);
    sqlite3_bind_text(stmt, idx++, client::to_string(rule.trigger), -1, SQLITE_STATIC);

    if (rule.modality_filter.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, rule.modality_filter.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (rule.body_part_filter.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, rule.body_part_filter.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (rule.station_ae_filter.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, rule.station_ae_filter.c_str(), -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_int64(stmt, idx++, rule.prior_lookback.count());
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(rule.max_prior_studies));

    auto modalities_json = serialize_modalities(rule.prior_modalities);
    sqlite3_bind_text(stmt, idx++, modalities_json.c_str(), -1, SQLITE_TRANSIENT);

    auto node_ids_json = serialize_node_ids(rule.source_node_ids);
    sqlite3_bind_text(stmt, idx++, node_ids_json.c_str(), -1, SQLITE_TRANSIENT);

    if (rule.schedule_cron.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, rule.schedule_cron.c_str(), -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_int64(stmt, idx++, rule.advance_time.count());
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(rule.triggered_count));
    sqlite3_bind_int64(stmt, idx++, static_cast<int64_t>(rule.studies_prefetched));

    auto last_triggered_str = to_timestamp_string(rule.last_triggered);
    if (last_triggered_str.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, last_triggered_str.c_str(), -1, SQLITE_TRANSIENT);
    }

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save rule: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

std::optional<client::prefetch_rule> prefetch_repository::find_rule_by_id(
    std::string_view rule_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, rule_id, name, enabled, trigger_type,
               modality_filter, body_part_filter, station_ae_filter,
               prior_lookback_hours, max_prior_studies, prior_modalities_json,
               source_node_ids_json, schedule_cron, advance_time_minutes,
               triggered_count, studies_prefetched, last_triggered
        FROM prefetch_rules WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    std::optional<client::prefetch_rule> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_rule_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<client::prefetch_rule> prefetch_repository::find_rule_by_pk(int64_t pk) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, rule_id, name, enabled, trigger_type,
               modality_filter, body_part_filter, station_ae_filter,
               prior_lookback_hours, max_prior_studies, prior_modalities_json,
               source_node_ids_json, schedule_cron, advance_time_minutes,
               triggered_count, studies_prefetched, last_triggered
        FROM prefetch_rules WHERE pk = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<client::prefetch_rule> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_rule_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::prefetch_rule> prefetch_repository::find_rules(
    const prefetch_rule_query_options& options) const {
    std::vector<client::prefetch_rule> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, rule_id, name, enabled, trigger_type,
               modality_filter, body_part_filter, station_ae_filter,
               prior_lookback_hours, max_prior_studies, prior_modalities_json,
               source_node_ids_json, schedule_cron, advance_time_minutes,
               triggered_count, studies_prefetched, last_triggered
        FROM prefetch_rules WHERE 1=1
    )";

    if (options.enabled_only.has_value()) {
        sql << " AND enabled = " << (options.enabled_only.value() ? "1" : "0");
    }

    if (options.trigger.has_value()) {
        sql << " AND trigger_type = '" << client::to_string(options.trigger.value()) << "'";
    }

    sql << " ORDER BY created_at DESC";
    sql << " LIMIT " << options.limit << " OFFSET " << options.offset;

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_rule_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::prefetch_rule> prefetch_repository::find_enabled_rules() const {
    prefetch_rule_query_options options;
    options.enabled_only = true;
    return find_rules(options);
}

VoidResult prefetch_repository::remove_rule(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = "DELETE FROM prefetch_rules WHERE rule_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete rule: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

bool prefetch_repository::rule_exists(std::string_view rule_id) const {
    if (!db_) return false;

    static constexpr const char* sql = "SELECT 1 FROM prefetch_rules WHERE rule_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

// =============================================================================
// Rule Statistics
// =============================================================================

VoidResult prefetch_repository::increment_triggered(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE prefetch_rules SET
            triggered_count = triggered_count + 1,
            last_triggered = CURRENT_TIMESTAMP
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to increment triggered: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

VoidResult prefetch_repository::increment_studies_prefetched(
    std::string_view rule_id, size_t count) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE prefetch_rules SET
            studies_prefetched = studies_prefetched + ?
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(count));
    sqlite3_bind_text(stmt, 2, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to increment studies prefetched: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

VoidResult prefetch_repository::enable_rule(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE prefetch_rules SET
            enabled = 1,
            updated_at = CURRENT_TIMESTAMP
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to enable rule: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

VoidResult prefetch_repository::disable_rule(std::string_view rule_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE prefetch_rules SET
            enabled = 0,
            updated_at = CURRENT_TIMESTAMP
        WHERE rule_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    sqlite3_bind_text(stmt, 1, rule_id.data(), static_cast<int>(rule_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to disable rule: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

// =============================================================================
// History Operations
// =============================================================================

VoidResult prefetch_repository::save_history(const client::prefetch_history& history) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO prefetch_history (
            patient_id, study_uid, rule_id, source_node_id, job_id, status
        ) VALUES (?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, history.patient_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, history.study_uid.c_str(), -1, SQLITE_TRANSIENT);

    if (history.rule_id.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, history.rule_id.c_str(), -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_text(stmt, idx++, history.source_node_id.c_str(), -1, SQLITE_TRANSIENT);

    if (history.job_id.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, history.job_id.c_str(), -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_text(stmt, idx++, history.status.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save history: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

std::vector<client::prefetch_history> prefetch_repository::find_history(
    const prefetch_history_query_options& options) const {
    std::vector<client::prefetch_history> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, patient_id, study_uid, rule_id, source_node_id, job_id, status, prefetched_at
        FROM prefetch_history WHERE 1=1
    )";

    if (options.patient_id.has_value()) {
        sql << " AND patient_id = '" << options.patient_id.value() << "'";
    }

    if (options.rule_id.has_value()) {
        sql << " AND rule_id = '" << options.rule_id.value() << "'";
    }

    if (options.status.has_value()) {
        sql << " AND status = '" << options.status.value() << "'";
    }

    sql << " ORDER BY prefetched_at DESC";
    sql << " LIMIT " << options.limit << " OFFSET " << options.offset;

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_history_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

bool prefetch_repository::is_study_prefetched(std::string_view study_uid) const {
    if (!db_) return false;

    static constexpr const char* sql = R"(
        SELECT 1 FROM prefetch_history
        WHERE study_uid = ? AND status IN ('completed', 'pending')
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, study_uid.data(), static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

size_t prefetch_repository::count_completed_today() const {
    if (!db_) return 0;

    static constexpr const char* sql = R"(
        SELECT COUNT(*) FROM prefetch_history
        WHERE status = 'completed'
        AND date(prefetched_at) = date('now')
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

size_t prefetch_repository::count_failed_today() const {
    if (!db_) return 0;

    static constexpr const char* sql = R"(
        SELECT COUNT(*) FROM prefetch_history
        WHERE status = 'failed'
        AND date(prefetched_at) = date('now')
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult prefetch_repository::update_history_status(
    std::string_view study_uid,
    std::string_view status) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE prefetch_history SET status = ? WHERE study_uid = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    sqlite3_bind_text(stmt, 1, status.data(), static_cast<int>(status.size()), SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, study_uid.data(), static_cast<int>(study_uid.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update status: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok();
}

Result<size_t> prefetch_repository::cleanup_old_history(std::chrono::hours max_age) {
    if (!db_) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Database not initialized", "prefetch_repository"});
    }

    // Calculate cutoff timestamp
    auto cutoff = std::chrono::system_clock::now() - max_age;
    auto cutoff_str = to_timestamp_string(cutoff);

    static constexpr const char* sql = R"(
        DELETE FROM prefetch_history WHERE prefetched_at < ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    sqlite3_bind_text(stmt, 1, cutoff_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return Result<size_t>(kcenon::common::error_info{
            -1, "Failed to cleanup history: " + std::string(sqlite3_errmsg(db_)),
            "prefetch_repository"});
    }

    return kcenon::common::ok(static_cast<size_t>(sqlite3_changes(db_)));
}

// =============================================================================
// Statistics
// =============================================================================

size_t prefetch_repository::rule_count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM prefetch_rules";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

size_t prefetch_repository::enabled_rule_count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM prefetch_rules WHERE enabled = 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

size_t prefetch_repository::history_count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM prefetch_history";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

// =============================================================================
// Database Information
// =============================================================================

bool prefetch_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

// =============================================================================
// Private Implementation
// =============================================================================

client::prefetch_rule prefetch_repository::parse_rule_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::prefetch_rule rule;

    int col = 0;
    rule.pk = get_int64_column(stmt, col++);
    rule.rule_id = get_text_column(stmt, col++);
    rule.name = get_text_column(stmt, col++);
    rule.enabled = (get_int_column(stmt, col++) != 0);
    rule.trigger = client::prefetch_trigger_from_string(get_text_column(stmt, col++));

    rule.modality_filter = get_text_column(stmt, col++);
    rule.body_part_filter = get_text_column(stmt, col++);
    rule.station_ae_filter = get_text_column(stmt, col++);

    rule.prior_lookback = std::chrono::hours{get_int64_column(stmt, col++)};
    rule.max_prior_studies = static_cast<size_t>(get_int64_column(stmt, col++));

    auto modalities_json = get_text_column(stmt, col++);
    rule.prior_modalities = deserialize_modalities(modalities_json);

    auto node_ids_json = get_text_column(stmt, col++);
    rule.source_node_ids = deserialize_node_ids(node_ids_json);

    rule.schedule_cron = get_text_column(stmt, col++);
    rule.advance_time = std::chrono::minutes{get_int64_column(stmt, col++)};

    rule.triggered_count = static_cast<size_t>(get_int64_column(stmt, col++));
    rule.studies_prefetched = static_cast<size_t>(get_int64_column(stmt, col++));

    auto last_triggered_str = get_text_column(stmt, col++);
    rule.last_triggered = from_timestamp_string(last_triggered_str.c_str());

    return rule;
}

client::prefetch_history prefetch_repository::parse_history_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::prefetch_history history;

    int col = 0;
    history.pk = get_int64_column(stmt, col++);
    history.patient_id = get_text_column(stmt, col++);
    history.study_uid = get_text_column(stmt, col++);
    history.rule_id = get_text_column(stmt, col++);
    history.source_node_id = get_text_column(stmt, col++);
    history.job_id = get_text_column(stmt, col++);
    history.status = get_text_column(stmt, col++);

    auto prefetched_str = get_text_column(stmt, col++);
    history.prefetched_at = from_timestamp_string(prefetched_str.c_str());

    return history;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
