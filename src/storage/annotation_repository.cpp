/**
 * @file annotation_repository.cpp
 * @brief Implementation of the annotation repository
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#include "pacs/storage/annotation_repository.hpp"

#ifdef PACS_WITH_DATABASE_SYSTEM


#include <sqlite3.h>

#include <chrono>
#include <sstream>

namespace pacs::storage {

namespace {

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

[[nodiscard]] std::string get_text_column(sqlite3_stmt* stmt, int col) {
    auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, col));
    return text ? text : "";
}

[[nodiscard]] [[maybe_unused]] int get_int_column(sqlite3_stmt* stmt, int col, int default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int(stmt, col);
}

[[nodiscard]] int64_t get_int64_column(sqlite3_stmt* stmt, int col, int64_t default_val = 0) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return default_val;
    }
    return sqlite3_column_int64(stmt, col);
}

[[nodiscard]] std::optional<int> get_optional_int(sqlite3_stmt* stmt, int col) {
    if (sqlite3_column_type(stmt, col) == SQLITE_NULL) {
        return std::nullopt;
    }
    return sqlite3_column_int(stmt, col);
}

void bind_optional_int(sqlite3_stmt* stmt, int idx, const std::optional<int>& value) {
    if (value.has_value()) {
        sqlite3_bind_int(stmt, idx, value.value());
    } else {
        sqlite3_bind_null(stmt, idx);
    }
}

[[nodiscard]] std::string json_escape(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

}  // namespace

std::string annotation_repository::serialize_style(const annotation_style& style) {
    std::ostringstream oss;
    oss << "{"
        << R"("color":")" << json_escape(style.color) << "\","
        << R"("line_width":)" << style.line_width << ","
        << R"("fill_color":")" << json_escape(style.fill_color) << "\","
        << R"("fill_opacity":)" << style.fill_opacity << ","
        << R"("font_family":")" << json_escape(style.font_family) << "\","
        << R"("font_size":)" << style.font_size
        << "}";
    return oss.str();
}

annotation_style annotation_repository::deserialize_style(std::string_view json) {
    annotation_style style;
    if (json.empty() || json == "{}") return style;

    auto find_string_value = [&](const char* key) -> std::string {
        std::string search = std::string("\"") + key + "\":\"";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return "";
        pos += search.size();
        auto end = json.find('"', pos);
        if (end == std::string_view::npos) return "";
        return std::string(json.substr(pos, end - pos));
    };

    auto find_int_value = [&](const char* key) -> int {
        std::string search = std::string("\"") + key + "\":";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return 0;
        pos += search.size();
        return std::atoi(json.data() + pos);
    };

    auto find_float_value = [&](const char* key) -> float {
        std::string search = std::string("\"") + key + "\":";
        auto pos = json.find(search);
        if (pos == std::string_view::npos) return 0.0f;
        pos += search.size();
        return static_cast<float>(std::atof(json.data() + pos));
    };

    auto color = find_string_value("color");
    if (!color.empty()) style.color = color;

    style.line_width = find_int_value("line_width");
    if (style.line_width == 0) style.line_width = 2;

    style.fill_color = find_string_value("fill_color");
    style.fill_opacity = find_float_value("fill_opacity");

    auto font_family = find_string_value("font_family");
    if (!font_family.empty()) style.font_family = font_family;

    style.font_size = find_int_value("font_size");
    if (style.font_size == 0) style.font_size = 14;

    return style;
}

annotation_repository::annotation_repository(sqlite3* db) : db_(db) {}

annotation_repository::~annotation_repository() = default;

annotation_repository::annotation_repository(annotation_repository&&) noexcept = default;

auto annotation_repository::operator=(annotation_repository&&) noexcept
    -> annotation_repository& = default;

VoidResult annotation_repository::save(const annotation_record& record) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "annotation_repository"});
    }

    static constexpr const char* sql = R"(
        INSERT INTO annotations (
            annotation_id, study_uid, series_uid, sop_instance_uid, frame_number,
            user_id, annotation_type, geometry_json, text, style_json,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(annotation_id) DO UPDATE SET
            geometry_json = excluded.geometry_json,
            text = excluded.text,
            style_json = excluded.style_json,
            updated_at = excluded.updated_at
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "annotation_repository"});
    }

    auto now = std::chrono::system_clock::now();
    auto now_str = to_timestamp_string(now);
    auto style_json = serialize_style(record.style);

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, record.annotation_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.study_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.series_uid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.sop_instance_uid.c_str(), -1, SQLITE_TRANSIENT);
    bind_optional_int(stmt, idx++, record.frame_number);
    sqlite3_bind_text(stmt, idx++, record.user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, to_string(record.type).c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.geometry_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, style_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to save annotation: " + std::string(sqlite3_errmsg(db_)),
            "annotation_repository"});
    }

    return kcenon::common::ok();
}

std::optional<annotation_record> annotation_repository::find_by_id(
    std::string_view annotation_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, annotation_id, study_uid, series_uid, sop_instance_uid, frame_number,
               user_id, annotation_type, geometry_json, text, style_json,
               created_at, updated_at
        FROM annotations WHERE annotation_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, annotation_id.data(),
                      static_cast<int>(annotation_id.size()), SQLITE_TRANSIENT);

    std::optional<annotation_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<annotation_record> annotation_repository::find_by_pk(int64_t pk) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, annotation_id, study_uid, series_uid, sop_instance_uid, frame_number,
               user_id, annotation_type, geometry_json, text, style_json,
               created_at, updated_at
        FROM annotations WHERE pk = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<annotation_record> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<annotation_record> annotation_repository::find_by_instance(
    std::string_view sop_instance_uid) const {
    annotation_query query;
    query.sop_instance_uid = std::string(sop_instance_uid);
    return search(query);
}

std::vector<annotation_record> annotation_repository::find_by_study(
    std::string_view study_uid) const {
    annotation_query query;
    query.study_uid = std::string(study_uid);
    return search(query);
}

std::vector<annotation_record> annotation_repository::search(
    const annotation_query& query) const {
    std::vector<annotation_record> result;
    if (!db_) return result;

    std::ostringstream sql;
    sql << R"(
        SELECT pk, annotation_id, study_uid, series_uid, sop_instance_uid, frame_number,
               user_id, annotation_type, geometry_json, text, style_json,
               created_at, updated_at
        FROM annotations WHERE 1=1
    )";

    std::vector<std::pair<int, std::string>> bindings;
    int param_idx = 1;

    if (query.study_uid.has_value()) {
        sql << " AND study_uid = ?";
        bindings.emplace_back(param_idx++, query.study_uid.value());
    }

    if (query.series_uid.has_value()) {
        sql << " AND series_uid = ?";
        bindings.emplace_back(param_idx++, query.series_uid.value());
    }

    if (query.sop_instance_uid.has_value()) {
        sql << " AND sop_instance_uid = ?";
        bindings.emplace_back(param_idx++, query.sop_instance_uid.value());
    }

    if (query.user_id.has_value()) {
        sql << " AND user_id = ?";
        bindings.emplace_back(param_idx++, query.user_id.value());
    }

    if (query.type.has_value()) {
        sql << " AND annotation_type = ?";
        bindings.emplace_back(param_idx++, to_string(query.type.value()));
    }

    sql << " ORDER BY created_at DESC";

    if (query.limit > 0) {
        sql << " LIMIT " << query.limit << " OFFSET " << query.offset;
    }

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    for (const auto& [idx, value] : bindings) {
        sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult annotation_repository::update(const annotation_record& record) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "annotation_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE annotations SET
            geometry_json = ?,
            text = ?,
            style_json = ?,
            updated_at = ?
        WHERE annotation_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "annotation_repository"});
    }

    auto now_str = to_timestamp_string(std::chrono::system_clock::now());
    auto style_json = serialize_style(record.style);

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, record.geometry_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, style_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, now_str.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, record.annotation_id.c_str(), -1, SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update annotation: " + std::string(sqlite3_errmsg(db_)),
            "annotation_repository"});
    }

    return kcenon::common::ok();
}

VoidResult annotation_repository::remove(std::string_view annotation_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not initialized", "annotation_repository"});
    }

    static constexpr const char* sql = "DELETE FROM annotations WHERE annotation_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "annotation_repository"});
    }

    sqlite3_bind_text(stmt, 1, annotation_id.data(),
                      static_cast<int>(annotation_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete annotation: " + std::string(sqlite3_errmsg(db_)),
            "annotation_repository"});
    }

    return kcenon::common::ok();
}

bool annotation_repository::exists(std::string_view annotation_id) const {
    if (!db_) return false;

    static constexpr const char* sql =
        "SELECT 1 FROM annotations WHERE annotation_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, annotation_id.data(),
                      static_cast<int>(annotation_id.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

size_t annotation_repository::count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM annotations";

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

size_t annotation_repository::count(const annotation_query& query) const {
    if (!db_) return 0;

    std::ostringstream sql;
    sql << "SELECT COUNT(*) FROM annotations WHERE 1=1";

    std::vector<std::pair<int, std::string>> bindings;
    int param_idx = 1;

    if (query.study_uid.has_value()) {
        sql << " AND study_uid = ?";
        bindings.emplace_back(param_idx++, query.study_uid.value());
    }

    if (query.series_uid.has_value()) {
        sql << " AND series_uid = ?";
        bindings.emplace_back(param_idx++, query.series_uid.value());
    }

    if (query.sop_instance_uid.has_value()) {
        sql << " AND sop_instance_uid = ?";
        bindings.emplace_back(param_idx++, query.sop_instance_uid.value());
    }

    if (query.user_id.has_value()) {
        sql << " AND user_id = ?";
        bindings.emplace_back(param_idx++, query.user_id.value());
    }

    if (query.type.has_value()) {
        sql << " AND annotation_type = ?";
        bindings.emplace_back(param_idx++, to_string(query.type.value()));
    }

    sqlite3_stmt* stmt = nullptr;
    auto sql_str = sql.str();
    if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    for (const auto& [idx, value] : bindings) {
        sqlite3_bind_text(stmt, idx, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    size_t result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return result;
}

bool annotation_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

annotation_record annotation_repository::parse_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    annotation_record record;

    int col = 0;
    record.pk = get_int64_column(stmt, col++);
    record.annotation_id = get_text_column(stmt, col++);
    record.study_uid = get_text_column(stmt, col++);
    record.series_uid = get_text_column(stmt, col++);
    record.sop_instance_uid = get_text_column(stmt, col++);
    record.frame_number = get_optional_int(stmt, col++);
    record.user_id = get_text_column(stmt, col++);

    auto type_str = get_text_column(stmt, col++);
    record.type = annotation_type_from_string(type_str).value_or(annotation_type::text);

    record.geometry_json = get_text_column(stmt, col++);
    record.text = get_text_column(stmt, col++);

    auto style_json = get_text_column(stmt, col++);
    record.style = deserialize_style(style_json);

    auto created_str = get_text_column(stmt, col++);
    record.created_at = from_timestamp_string(created_str.c_str());

    auto updated_str = get_text_column(stmt, col++);
    record.updated_at = from_timestamp_string(updated_str.c_str());

    return record;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
