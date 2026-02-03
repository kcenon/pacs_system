/**
 * @file node_repository.cpp
 * @brief Implementation of the node repository for remote node persistence
 *
 * @see Issue #535 - Implement Remote Node Manager
 * @see Issue #610 - Phase 4: Repository Migrations
 * @see Issue #650 - Part 2: Migrate annotation, routing, node repositories
 */

#include "pacs/storage/node_repository.hpp"

#include <chrono>
#include <cstring>

#ifdef PACS_WITH_DATABASE_SYSTEM

namespace pacs::storage {

// =============================================================================
// Constructor
// =============================================================================

node_repository::node_repository(std::shared_ptr<pacs_database_adapter> db)
    : base_repository(std::move(db), "remote_nodes", "node_id") {}

// =============================================================================
// Timestamp Helpers
// =============================================================================

auto node_repository::parse_timestamp(const std::string& str) const
    -> std::chrono::system_clock::time_point {
    if (str.empty()) {
        return {};
    }

    std::tm tm{};
    if (std::sscanf(str.c_str(), "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon,
                    &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
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

auto node_repository::format_timestamp(
    std::chrono::system_clock::time_point tp) const -> std::string {
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

// =============================================================================
// Domain-Specific Operations
// =============================================================================

auto node_repository::find_by_pk(int64_t pk) -> result_type {
    if (!db() || !db()->is_connected()) {
        return result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("pk", "=", pk)
        .limit(1);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return result_type(result.error());
    }

    if (result.value().empty()) {
        return result_type(kcenon::common::error_info{
            -1, "Node not found with pk=" + std::to_string(pk), "storage"});
    }

    return result_type(map_row_to_entity(result.value()[0]));
}

auto node_repository::find_all_nodes() -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select(select_columns())
        .from(table_name())
        .order_by("name", database::sort_order::asc);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::remote_node> nodes;
    nodes.reserve(result.value().size());
    for (const auto& row : result.value()) {
        nodes.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(nodes));
}

auto node_repository::find_by_status(client::node_status status)
    -> list_result_type {
    if (!db() || !db()->is_connected()) {
        return list_result_type(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto builder = query_builder();
    builder.select(select_columns())
        .from(table_name())
        .where("status", "=", std::string(client::to_string(status)))
        .order_by("name", database::sort_order::asc);

    auto result = db()->select(builder.build());
    if (result.is_err()) {
        return list_result_type(result.error());
    }

    std::vector<client::remote_node> nodes;
    nodes.reserve(result.value().size());
    for (const auto& row : result.value()) {
        nodes.push_back(map_row_to_entity(row));
    }

    return list_result_type(std::move(nodes));
}

// =============================================================================
// Status Updates
// =============================================================================

auto node_repository::update_status(
    std::string_view node_id,
    client::node_status status,
    std::string_view error_message) -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    if (status == client::node_status::error ||
        status == client::node_status::offline) {
        // Use raw SQL for CURRENT_TIMESTAMP
        auto sql = "UPDATE " + table_name() +
                   " SET status = '" + std::string(client::to_string(status)) +
                   "', last_error = CURRENT_TIMESTAMP, "
                   "last_error_message = '" + std::string(error_message) +
                   "', updated_at = CURRENT_TIMESTAMP "
                   "WHERE node_id = '" + std::string(node_id) + "'";
        auto result = db()->execute(sql);
        if (result.is_err()) {
            return VoidResult(result.error());
        }
    } else {
        auto builder = query_builder();
        builder.update(table_name())
            .set("status", std::string(client::to_string(status)))
            .where("node_id", "=", std::string(node_id));

        auto result = db()->execute(builder.build());
        if (result.is_err()) {
            return VoidResult(result.error());
        }
    }

    return kcenon::common::ok();
}

auto node_repository::update_last_verified(std::string_view node_id)
    -> VoidResult {
    if (!db() || !db()->is_connected()) {
        return VoidResult(kcenon::common::error_info{
            -1, "Database not connected", "storage"});
    }

    auto sql = "UPDATE " + table_name() +
               " SET last_verified = CURRENT_TIMESTAMP, "
               "updated_at = CURRENT_TIMESTAMP WHERE node_id = '" +
               std::string(node_id) + "'";

    auto result = db()->execute(sql);
    if (result.is_err()) {
        return VoidResult(result.error());
    }

    return kcenon::common::ok();
}

// =============================================================================
// base_repository overrides
// =============================================================================

auto node_repository::map_row_to_entity(const database_row& row) const
    -> client::remote_node {
    client::remote_node node;

    // Parse pk if present
    auto pk_it = row.find("pk");
    if (pk_it != row.end() && !pk_it->second.empty()) {
        node.pk = std::stoll(pk_it->second);
    }

    node.node_id = row.at("node_id");
    node.name = row.at("name");
    node.ae_title = row.at("ae_title");
    node.host = row.at("host");

    auto port_it = row.find("port");
    if (port_it != row.end() && !port_it->second.empty()) {
        node.port = static_cast<uint16_t>(std::stoi(port_it->second));
    }

    auto find_it = row.find("supports_find");
    if (find_it != row.end() && !find_it->second.empty()) {
        node.supports_find = (std::stoi(find_it->second) != 0);
    }

    auto move_it = row.find("supports_move");
    if (move_it != row.end() && !move_it->second.empty()) {
        node.supports_move = (std::stoi(move_it->second) != 0);
    }

    auto get_it = row.find("supports_get");
    if (get_it != row.end() && !get_it->second.empty()) {
        node.supports_get = (std::stoi(get_it->second) != 0);
    }

    auto store_it = row.find("supports_store");
    if (store_it != row.end() && !store_it->second.empty()) {
        node.supports_store = (std::stoi(store_it->second) != 0);
    }

    auto worklist_it = row.find("supports_worklist");
    if (worklist_it != row.end() && !worklist_it->second.empty()) {
        node.supports_worklist = (std::stoi(worklist_it->second) != 0);
    }

    auto conn_timeout_it = row.find("connection_timeout_sec");
    if (conn_timeout_it != row.end() && !conn_timeout_it->second.empty()) {
        node.connection_timeout = std::chrono::seconds(std::stoi(conn_timeout_it->second));
    }

    auto dimse_timeout_it = row.find("dimse_timeout_sec");
    if (dimse_timeout_it != row.end() && !dimse_timeout_it->second.empty()) {
        node.dimse_timeout = std::chrono::seconds(std::stoi(dimse_timeout_it->second));
    }

    auto max_assoc_it = row.find("max_associations");
    if (max_assoc_it != row.end() && !max_assoc_it->second.empty()) {
        node.max_associations = static_cast<size_t>(std::stoll(max_assoc_it->second));
    }

    auto status_it = row.find("status");
    if (status_it != row.end() && !status_it->second.empty()) {
        node.status = client::node_status_from_string(status_it->second);
    }

    auto last_verified_it = row.find("last_verified");
    if (last_verified_it != row.end() && !last_verified_it->second.empty()) {
        node.last_verified = parse_timestamp(last_verified_it->second);
    }

    auto last_error_it = row.find("last_error");
    if (last_error_it != row.end() && !last_error_it->second.empty()) {
        node.last_error = parse_timestamp(last_error_it->second);
    }

    auto last_error_msg_it = row.find("last_error_message");
    if (last_error_msg_it != row.end()) {
        node.last_error_message = last_error_msg_it->second;
    }

    auto created_it = row.find("created_at");
    if (created_it != row.end() && !created_it->second.empty()) {
        node.created_at = parse_timestamp(created_it->second);
    }

    auto updated_it = row.find("updated_at");
    if (updated_it != row.end() && !updated_it->second.empty()) {
        node.updated_at = parse_timestamp(updated_it->second);
    }

    return node;
}

auto node_repository::entity_to_row(const client::remote_node& entity) const
    -> std::map<std::string, database_value> {
    std::map<std::string, database_value> row;

    row["node_id"] = entity.node_id;
    row["name"] = entity.name;
    row["ae_title"] = entity.ae_title;
    row["host"] = entity.host;
    row["port"] = static_cast<int64_t>(entity.port);
    row["supports_find"] = static_cast<int64_t>(entity.supports_find ? 1 : 0);
    row["supports_move"] = static_cast<int64_t>(entity.supports_move ? 1 : 0);
    row["supports_get"] = static_cast<int64_t>(entity.supports_get ? 1 : 0);
    row["supports_store"] = static_cast<int64_t>(entity.supports_store ? 1 : 0);
    row["supports_worklist"] = static_cast<int64_t>(entity.supports_worklist ? 1 : 0);
    row["connection_timeout_sec"] = static_cast<int64_t>(entity.connection_timeout.count());
    row["dimse_timeout_sec"] = static_cast<int64_t>(entity.dimse_timeout.count());
    row["max_associations"] = static_cast<int64_t>(entity.max_associations);
    row["status"] = std::string(client::to_string(entity.status));
    row["last_verified"] = format_timestamp(entity.last_verified);
    row["last_error"] = format_timestamp(entity.last_error);
    row["last_error_message"] = entity.last_error_message;
    row["created_at"] = format_timestamp(entity.created_at);
    row["updated_at"] = format_timestamp(entity.updated_at);

    return row;
}

auto node_repository::get_pk(const client::remote_node& entity) const
    -> std::string {
    return entity.node_id;
}

auto node_repository::has_pk(const client::remote_node& entity) const -> bool {
    return !entity.node_id.empty();
}

auto node_repository::select_columns() const -> std::vector<std::string> {
    return {
        "pk", "node_id", "name", "ae_title", "host", "port",
        "supports_find", "supports_move", "supports_get",
        "supports_store", "supports_worklist",
        "connection_timeout_sec", "dimse_timeout_sec", "max_associations",
        "status", "last_verified", "last_error", "last_error_message",
        "created_at", "updated_at"
    };
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

}  // namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

node_repository::node_repository(sqlite3* db) : db_(db) {}

node_repository::~node_repository() = default;

node_repository::node_repository(node_repository&&) noexcept = default;

auto node_repository::operator=(node_repository&&) noexcept -> node_repository& = default;

// =============================================================================
// CRUD Operations
// =============================================================================

Result<int64_t> node_repository::upsert(const client::remote_node& node) {
    if (!db_) {
        return kcenon::common::make_error<int64_t>(
            -1, "Database not initialized", "node_repository");
    }

    static constexpr const char* sql = R"(
        INSERT INTO remote_nodes (
            node_id, name, ae_title, host, port,
            supports_find, supports_move, supports_get, supports_store, supports_worklist,
            connection_timeout_sec, dimse_timeout_sec, max_associations,
            status, last_verified, last_error, last_error_message,
            created_at, updated_at
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)
        ON CONFLICT(node_id) DO UPDATE SET
            name = excluded.name,
            ae_title = excluded.ae_title,
            host = excluded.host,
            port = excluded.port,
            supports_find = excluded.supports_find,
            supports_move = excluded.supports_move,
            supports_get = excluded.supports_get,
            supports_store = excluded.supports_store,
            supports_worklist = excluded.supports_worklist,
            connection_timeout_sec = excluded.connection_timeout_sec,
            dimse_timeout_sec = excluded.dimse_timeout_sec,
            max_associations = excluded.max_associations,
            updated_at = CURRENT_TIMESTAMP
        RETURNING pk
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return kcenon::common::make_error<int64_t>(
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "node_repository");
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, node.node_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, node.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, node.ae_title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, node.host.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, idx++, node.port);
    sqlite3_bind_int(stmt, idx++, node.supports_find ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, node.supports_move ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, node.supports_get ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, node.supports_store ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, node.supports_worklist ? 1 : 0);
    sqlite3_bind_int(stmt, idx++, static_cast<int>(node.connection_timeout.count()));
    sqlite3_bind_int(stmt, idx++, static_cast<int>(node.dimse_timeout.count()));
    sqlite3_bind_int(stmt, idx++, static_cast<int>(node.max_associations));

    auto status_str = std::string(client::to_string(node.status));
    sqlite3_bind_text(stmt, idx++, status_str.c_str(), -1, SQLITE_TRANSIENT);

    auto last_verified_str = to_timestamp_string(node.last_verified);
    if (last_verified_str.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, last_verified_str.c_str(), -1, SQLITE_TRANSIENT);
    }

    auto last_error_str = to_timestamp_string(node.last_error);
    if (last_error_str.empty()) {
        sqlite3_bind_null(stmt, idx++);
    } else {
        sqlite3_bind_text(stmt, idx++, last_error_str.c_str(), -1, SQLITE_TRANSIENT);
    }

    sqlite3_bind_text(stmt, idx++, node.last_error_message.c_str(), -1, SQLITE_TRANSIENT);

    int64_t pk = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        pk = sqlite3_column_int64(stmt, 0);
    } else {
        auto err = std::string(sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        return kcenon::common::make_error<int64_t>(-1, "Failed to upsert: " + err, "node_repository");
    }

    sqlite3_finalize(stmt);
    return kcenon::common::ok(pk);
}

std::optional<client::remote_node> node_repository::find_by_id(std::string_view node_id) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, node_id, name, ae_title, host, port,
               supports_find, supports_move, supports_get, supports_store, supports_worklist,
               connection_timeout_sec, dimse_timeout_sec, max_associations,
               status, last_verified, last_error, last_error_message,
               created_at, updated_at
        FROM remote_nodes WHERE node_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, node_id.data(), static_cast<int>(node_id.size()), SQLITE_TRANSIENT);

    std::optional<client::remote_node> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::optional<client::remote_node> node_repository::find_by_pk(int64_t pk) const {
    if (!db_) return std::nullopt;

    static constexpr const char* sql = R"(
        SELECT pk, node_id, name, ae_title, host, port,
               supports_find, supports_move, supports_get, supports_store, supports_worklist,
               connection_timeout_sec, dimse_timeout_sec, max_associations,
               status, last_verified, last_error, last_error_message,
               created_at, updated_at
        FROM remote_nodes WHERE pk = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_int64(stmt, 1, pk);

    std::optional<client::remote_node> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = parse_row(stmt);
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::remote_node> node_repository::find_all() const {
    std::vector<client::remote_node> result;
    if (!db_) return result;

    static constexpr const char* sql = R"(
        SELECT pk, node_id, name, ae_title, host, port,
               supports_find, supports_move, supports_get, supports_store, supports_worklist,
               connection_timeout_sec, dimse_timeout_sec, max_associations,
               status, last_verified, last_error, last_error_message,
               created_at, updated_at
        FROM remote_nodes ORDER BY name
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

std::vector<client::remote_node> node_repository::find_by_status(
    client::node_status status) const {
    std::vector<client::remote_node> result;
    if (!db_) return result;

    static constexpr const char* sql = R"(
        SELECT pk, node_id, name, ae_title, host, port,
               supports_find, supports_move, supports_get, supports_store, supports_worklist,
               connection_timeout_sec, dimse_timeout_sec, max_associations,
               status, last_verified, last_error, last_error_message,
               created_at, updated_at
        FROM remote_nodes WHERE status = ? ORDER BY name
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return result;
    }

    auto status_str = std::string(client::to_string(status));
    sqlite3_bind_text(stmt, 1, status_str.c_str(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(parse_row(stmt));
    }

    sqlite3_finalize(stmt);
    return result;
}

VoidResult node_repository::remove(std::string_view node_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{-1, "Database not initialized", "node_repository"});
    }

    static constexpr const char* sql = "DELETE FROM remote_nodes WHERE node_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "node_repository"});
    }

    sqlite3_bind_text(stmt, 1, node_id.data(), static_cast<int>(node_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to delete: " + std::string(sqlite3_errmsg(db_)),
            "node_repository"});
    }

    return kcenon::common::ok();
}

bool node_repository::exists(std::string_view node_id) const {
    if (!db_) return false;

    static constexpr const char* sql = "SELECT 1 FROM remote_nodes WHERE node_id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, node_id.data(), static_cast<int>(node_id.size()), SQLITE_TRANSIENT);

    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

size_t node_repository::count() const {
    if (!db_) return 0;

    static constexpr const char* sql = "SELECT COUNT(*) FROM remote_nodes";

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
// Status Updates
// =============================================================================

VoidResult node_repository::update_status(
    std::string_view node_id,
    client::node_status status,
    std::string_view error_message) {

    if (!db_) {
        return VoidResult(kcenon::common::error_info{-1, "Database not initialized", "node_repository"});
    }

    const char* sql = nullptr;
    if (status == client::node_status::error || status == client::node_status::offline) {
        sql = R"(
            UPDATE remote_nodes SET
                status = ?,
                last_error = CURRENT_TIMESTAMP,
                last_error_message = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE node_id = ?
        )";
    } else {
        sql = R"(
            UPDATE remote_nodes SET
                status = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE node_id = ?
        )";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "node_repository"});
    }

    auto status_str = std::string(client::to_string(status));
    int idx = 1;
    sqlite3_bind_text(stmt, idx++, status_str.c_str(), -1, SQLITE_TRANSIENT);

    if (status == client::node_status::error || status == client::node_status::offline) {
        sqlite3_bind_text(stmt, idx++, error_message.data(),
                          static_cast<int>(error_message.size()), SQLITE_TRANSIENT);
    }

    sqlite3_bind_text(stmt, idx++, node_id.data(), static_cast<int>(node_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update status: " + std::string(sqlite3_errmsg(db_)),
            "node_repository"});
    }

    return kcenon::common::ok();
}

VoidResult node_repository::update_last_verified(std::string_view node_id) {
    if (!db_) {
        return VoidResult(kcenon::common::error_info{-1, "Database not initialized", "node_repository"});
    }

    static constexpr const char* sql = R"(
        UPDATE remote_nodes SET
            last_verified = CURRENT_TIMESTAMP,
            updated_at = CURRENT_TIMESTAMP
        WHERE node_id = ?
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)),
            "node_repository"});
    }

    sqlite3_bind_text(stmt, 1, node_id.data(), static_cast<int>(node_id.size()), SQLITE_TRANSIENT);

    auto rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return VoidResult(kcenon::common::error_info{
            -1, "Failed to update last_verified: " + std::string(sqlite3_errmsg(db_)),
            "node_repository"});
    }

    return kcenon::common::ok();
}

// =============================================================================
// Database Information
// =============================================================================

bool node_repository::is_valid() const noexcept {
    return db_ != nullptr;
}

// =============================================================================
// Private Implementation
// =============================================================================

client::remote_node node_repository::parse_row(void* stmt_ptr) const {
    auto* stmt = static_cast<sqlite3_stmt*>(stmt_ptr);
    client::remote_node node;

    int col = 0;
    node.pk = get_int64_column(stmt, col++);
    node.node_id = get_text_column(stmt, col++);
    node.name = get_text_column(stmt, col++);
    node.ae_title = get_text_column(stmt, col++);
    node.host = get_text_column(stmt, col++);
    node.port = static_cast<uint16_t>(get_int_column(stmt, col++, 104));

    node.supports_find = get_int_column(stmt, col++) != 0;
    node.supports_move = get_int_column(stmt, col++) != 0;
    node.supports_get = get_int_column(stmt, col++) != 0;
    node.supports_store = get_int_column(stmt, col++) != 0;
    node.supports_worklist = get_int_column(stmt, col++) != 0;

    node.connection_timeout = std::chrono::seconds(get_int_column(stmt, col++, 30));
    node.dimse_timeout = std::chrono::seconds(get_int_column(stmt, col++, 60));
    node.max_associations = static_cast<size_t>(get_int_column(stmt, col++, 4));

    auto status_str = get_text_column(stmt, col++);
    node.status = client::node_status_from_string(status_str);

    auto last_verified_str = get_text_column(stmt, col++);
    node.last_verified = from_timestamp_string(last_verified_str.c_str());

    auto last_error_str = get_text_column(stmt, col++);
    node.last_error = from_timestamp_string(last_error_str.c_str());

    node.last_error_message = get_text_column(stmt, col++);

    auto created_at_str = get_text_column(stmt, col++);
    node.created_at = from_timestamp_string(created_at_str.c_str());

    auto updated_at_str = get_text_column(stmt, col++);
    node.updated_at = from_timestamp_string(updated_at_str.c_str());

    return node;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
