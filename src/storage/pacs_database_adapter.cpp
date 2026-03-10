// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file pacs_database_adapter.cpp
 * @brief Implementation of PACS database adapter
 *
 * Implements the pacs_database_adapter class that wraps database_system's
 * unified_database_system for centralized database access.
 *
 * @see Issue #606 - Phase 1: Foundation - PACS Database Adapter
 */

#include <pacs/storage/pacs_database_adapter.hpp>

#ifdef PACS_WITH_DATABASE_SYSTEM

#include <sqlite3.h>

#include <array>
#include <chrono>
#include <pacs/compat/format.hpp>

namespace pacs::storage {

// Use common_system's result helpers
using kcenon::common::make_error;
using kcenon::common::ok;

// ============================================================================
// Implementation Details
// ============================================================================

struct pacs_database_adapter::impl {
    /// Database system instance
    std::unique_ptr<database::integrated::unified_database_system> db;

    /// Native SQLite handle used when unified_database_system lacks SQLite support
    sqlite3* native_sqlite_db{nullptr};

    /// True when native SQLite fallback is active
    bool use_native_sqlite{false};

    /// Database type
    database::database_types db_type{database::database_types::sqlite};

    /// Connection string
    std::string connection_string;

    /// Transaction state
    bool in_transaction{false};

    /// Last error message
    std::string last_error_msg;

    /// Last insert rowid (SQLite compatibility)
    int64_t last_rowid{0};

    impl() = default;

    impl(database::database_types type, std::string conn_str)
        : db_type(type), connection_string(std::move(conn_str)) {}
};

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

/**
 * @brief Convert database::integrated::backend_type from database_types
 */
auto to_backend_type(database::database_types type)
    -> database::integrated::backend_type {
    switch (type) {
        case database::database_types::postgres:
            return database::integrated::backend_type::postgres;
        case database::database_types::sqlite:
            return database::integrated::backend_type::sqlite;
        case database::database_types::mongodb:
            return database::integrated::backend_type::mongodb;
        case database::database_types::redis:
            return database::integrated::backend_type::redis;
        default:
            return database::integrated::backend_type::sqlite;
    }
}

auto is_prefixed_sqlite_connection_string(std::string_view connection_string)
    -> bool {
    constexpr std::array<std::string_view, 3> prefixes = {"dbname=", "db=",
                                                           "database="};

    for (const auto prefix : prefixes) {
        if (connection_string.rfind(prefix, 0) == 0) {
            return true;
        }
    }

    return false;
}

auto normalize_connection_string(database::database_types type,
                                 std::string_view connection_string)
    -> std::string {
    if (type != database::database_types::sqlite) {
        return std::string(connection_string);
    }

    if (connection_string.empty()) {
        return "dbname=:memory:";
    }

    if (is_prefixed_sqlite_connection_string(connection_string)) {
        return std::string(connection_string);
    }

    return pacs::compat::format("dbname={}", connection_string);
}

auto extract_sqlite_connection_target(std::string_view connection_string)
    -> std::string {
    constexpr std::array<std::string_view, 3> prefixes = {"dbname=", "db=",
                                                           "database="};

    for (const auto prefix : prefixes) {
        if (connection_string.rfind(prefix, 0) == 0) {
            return std::string(connection_string.substr(prefix.size()));
        }
    }

    return std::string(connection_string);
}

auto should_use_native_sqlite(std::string_view connection_string) -> bool {
    const auto target = extract_sqlite_connection_target(connection_string);
    return !target.empty() && target != ":memory:";
}

/**
 * @brief Convert unified_database_system query_result to our database_result
 */
auto convert_result(
    const database::integrated::query_result& src) -> database_result {
    database_result result;
    result.affected_rows = src.affected_rows;
    result.execution_time = src.execution_time;

    result.rows.reserve(src.rows.size());
    for (const auto& src_row : src.rows) {
        database_row row;
        for (const auto& [key, value] : src_row) {
            row[key] = value;
        }
        result.rows.push_back(std::move(row));
    }

    return result;
}

/**
 * @brief Create VoidResult error with given parameters
 */
auto make_void_error(int code, const std::string& message,
                     const std::string& module) -> VoidResult {
    return VoidResult(kcenon::common::error_info{code, message, module});
}

auto sqlite_error_message(sqlite3* db, std::string_view fallback)
    -> std::string {
    if (db != nullptr) {
        if (const auto* message = sqlite3_errmsg(db); message != nullptr) {
            return std::string(message);
        }
    }

    return std::string(fallback);
}

auto execute_native_sqlite(sqlite3* db, const std::string& query)
    -> VoidResult {
    char* error_message = nullptr;
    const auto rc =
        sqlite3_exec(db, query.c_str(), nullptr, nullptr, &error_message);
    if (rc != SQLITE_OK) {
        const std::string message =
            error_message != nullptr ? std::string(error_message)
                                     : sqlite_error_message(db, "SQLite execution failed");
        sqlite3_free(error_message);
        return make_void_error(rc, message, "storage");
    }

    return ok();
}

auto select_native_sqlite(sqlite3* db, const std::string& query)
    -> Result<database_result> {
    sqlite3_stmt* statement = nullptr;
    const auto started = std::chrono::steady_clock::now();
    const auto rc =
        sqlite3_prepare_v2(db, query.c_str(), -1, &statement, nullptr);
    if (rc != SQLITE_OK) {
        return make_error<database_result>(
            rc, sqlite_error_message(db, "Failed to prepare SQLite query"),
            "storage");
    }

    database_result result;
    while (true) {
        const auto step_rc = sqlite3_step(statement);
        if (step_rc == SQLITE_ROW) {
            database_row row;
            const auto column_count = sqlite3_column_count(statement);
            for (int i = 0; i < column_count; ++i) {
                const auto* column_name = sqlite3_column_name(statement, i);
                const auto* column_value = sqlite3_column_text(statement, i);
                row[column_name != nullptr ? column_name : ""] =
                    column_value != nullptr
                        ? reinterpret_cast<const char*>(column_value)
                        : "";
            }
            result.rows.push_back(std::move(row));
            continue;
        }

        sqlite3_finalize(statement);
        if (step_rc != SQLITE_DONE) {
            return make_error<database_result>(
                step_rc,
                sqlite_error_message(db, "Failed to execute SQLite query"),
                "storage");
        }

        result.affected_rows = result.rows.size();
        result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started);
        return Result<database_result>::ok(std::move(result));
    }
}

auto mutate_native_sqlite(sqlite3* db, const std::string& query)
    -> Result<uint64_t> {
    auto result = execute_native_sqlite(db, query);
    if (result.is_err()) {
        return make_error<uint64_t>(result.error().code, result.error().message,
                                    result.error().module);
    }

    return Result<uint64_t>::ok(
        static_cast<uint64_t>(sqlite3_changes(db)));
}

}  // namespace

// ============================================================================
// pacs_storage_session Implementation
// ============================================================================

pacs_storage_session::pacs_storage_session(
    pacs_database_adapter& adapter) noexcept
    : adapter_(&adapter) {}

auto pacs_storage_session::create_query_builder() const
    -> database::query_builder {
    return adapter_->create_query_builder();
}

auto pacs_storage_session::select(const std::string& query)
    -> Result<database_result> {
    return adapter_->run_select(query);
}

auto pacs_storage_session::insert(const std::string& query)
    -> Result<uint64_t> {
    return adapter_->run_insert(query);
}

auto pacs_storage_session::update(const std::string& query)
    -> Result<uint64_t> {
    return adapter_->run_update(query);
}

auto pacs_storage_session::remove(const std::string& query)
    -> Result<uint64_t> {
    return adapter_->run_remove(query);
}

auto pacs_storage_session::execute(const std::string& query) -> VoidResult {
    return adapter_->run_execute(query);
}

auto pacs_storage_session::last_insert_rowid() const -> int64_t {
    return adapter_->last_insert_rowid();
}

auto pacs_storage_session::begin_unit_of_work() -> Result<pacs_unit_of_work> {
    return adapter_->begin_unit_of_work();
}

// ============================================================================
// pacs_unit_of_work Implementation
// ============================================================================

pacs_unit_of_work::pacs_unit_of_work(
    pacs_database_adapter& adapter, bool active) noexcept
    : adapter_(&adapter), active_(active) {}

pacs_unit_of_work::~pacs_unit_of_work() {
    if (active_ && adapter_ != nullptr) {
        (void)adapter_->rollback_internal();
    }
}

pacs_unit_of_work::pacs_unit_of_work(pacs_unit_of_work&& other) noexcept
    : adapter_(other.adapter_), active_(other.active_) {
    other.adapter_ = nullptr;
    other.active_ = false;
}

auto pacs_unit_of_work::operator=(pacs_unit_of_work&& other) noexcept
    -> pacs_unit_of_work& {
    if (this == &other) {
        return *this;
    }

    if (active_ && adapter_ != nullptr) {
        (void)adapter_->rollback_internal();
    }

    adapter_ = other.adapter_;
    active_ = other.active_;
    other.adapter_ = nullptr;
    other.active_ = false;
    return *this;
}

auto pacs_unit_of_work::create_query_builder() const -> database::query_builder {
    return adapter_->create_query_builder();
}

auto pacs_unit_of_work::select(const std::string& query)
    -> Result<database_result> {
    return adapter_->run_select(query);
}

auto pacs_unit_of_work::insert(const std::string& query) -> Result<uint64_t> {
    return adapter_->run_insert(query);
}

auto pacs_unit_of_work::update(const std::string& query) -> Result<uint64_t> {
    return adapter_->run_update(query);
}

auto pacs_unit_of_work::remove(const std::string& query) -> Result<uint64_t> {
    return adapter_->run_remove(query);
}

auto pacs_unit_of_work::execute(const std::string& query) -> VoidResult {
    return adapter_->run_execute(query);
}

auto pacs_unit_of_work::last_insert_rowid() const -> int64_t {
    return adapter_ != nullptr ? adapter_->last_insert_rowid() : 0;
}

auto pacs_unit_of_work::commit() -> VoidResult {
    if (!active_ || adapter_ == nullptr) {
        return make_void_error(-1, "Unit of work not active", "storage");
    }

    auto result = adapter_->commit_internal();
    if (result.is_ok()) {
        active_ = false;
    }
    return result;
}

auto pacs_unit_of_work::rollback() -> VoidResult {
    if (!active_ || adapter_ == nullptr) {
        return ok();
    }

    auto result = adapter_->rollback_internal();
    if (result.is_ok()) {
        active_ = false;
    }
    return result;
}

auto pacs_unit_of_work::is_active() const noexcept -> bool {
    return active_;
}

// ============================================================================
// Construction / Destruction
// ============================================================================

pacs_database_adapter::pacs_database_adapter(
    const std::filesystem::path& db_path)
    : impl_(std::make_unique<impl>(
          database::database_types::sqlite,
          normalize_connection_string(database::database_types::sqlite,
                                      db_path.string()))) {}

pacs_database_adapter::pacs_database_adapter(
    database::database_types type, const std::string& connection_string)
    : impl_(std::make_unique<impl>(
          type, normalize_connection_string(type, connection_string))) {}

pacs_database_adapter::~pacs_database_adapter() {
    if (!impl_) {
        return;
    }

    if (impl_->use_native_sqlite && impl_->native_sqlite_db != nullptr) {
        if (impl_->in_transaction) {
            (void)execute_native_sqlite(impl_->native_sqlite_db, "ROLLBACK");
        }
        sqlite3_close(impl_->native_sqlite_db);
        impl_->native_sqlite_db = nullptr;
        return;
    }

    if (impl_->db && impl_->db->is_connected()) {
        if (impl_->in_transaction) {
            (void)impl_->db->execute("ROLLBACK");
        }
        (void)impl_->db->disconnect();
    }
}

pacs_database_adapter::pacs_database_adapter(
    pacs_database_adapter&&) noexcept = default;

auto pacs_database_adapter::operator=(pacs_database_adapter&&) noexcept
    -> pacs_database_adapter& = default;

// ============================================================================
// Connection Management
// ============================================================================

auto pacs_database_adapter::connect() -> VoidResult {
    if (!impl_) {
        return make_void_error(-1, "Adapter not initialized", "storage");
    }

    if (impl_->db_type == database::database_types::sqlite &&
        should_use_native_sqlite(impl_->connection_string)) {
        const auto target =
            extract_sqlite_connection_target(impl_->connection_string);
        const auto rc = sqlite3_open_v2(
            target.c_str(), &impl_->native_sqlite_db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
            nullptr);
        if (rc != SQLITE_OK) {
            const auto message = sqlite_error_message(
                impl_->native_sqlite_db, "Failed to open SQLite database");
            if (impl_->native_sqlite_db != nullptr) {
                sqlite3_close(impl_->native_sqlite_db);
                impl_->native_sqlite_db = nullptr;
            }
            impl_->last_error_msg = message;
            return make_void_error(
                rc, pacs::compat::format("Failed to connect: {}", message),
                "storage");
        }

        (void)sqlite3_busy_timeout(impl_->native_sqlite_db, 5000);
        auto pragma_result =
            execute_native_sqlite(impl_->native_sqlite_db, "PRAGMA foreign_keys = ON");
        if (pragma_result.is_err()) {
            impl_->last_error_msg = pragma_result.error().message;
            sqlite3_close(impl_->native_sqlite_db);
            impl_->native_sqlite_db = nullptr;
            return pragma_result;
        }

        impl_->use_native_sqlite = true;
        impl_->last_error_msg.clear();
        return ok();
    }

    try {
        // Create database system using builder
        impl_->db =
            database::integrated::unified_database_system::create_builder()
                .set_backend(to_backend_type(impl_->db_type))
                .enable_logging(database::integrated::db_log_level::warning)
                .build();

        // Connect to database
        auto result =
            impl_->db->connect(to_backend_type(impl_->db_type),
                               impl_->connection_string);

        if (result.is_err()) {
            impl_->last_error_msg = result.error().message;
            return make_void_error(
                result.error().code,
                pacs::compat::format("Failed to connect: {}",
                                     result.error().message),
                "storage");
        }

        impl_->last_error_msg.clear();
        return ok();
    } catch (const std::exception& e) {
        impl_->last_error_msg = e.what();
        return make_void_error(
            -1, pacs::compat::format("Connection failed: {}", e.what()),
            "storage");
    }
}

auto pacs_database_adapter::disconnect() -> VoidResult {
    if (!impl_) {
        return ok();
    }

    if (impl_->use_native_sqlite) {
        if (impl_->in_transaction) {
            auto rollback_result = rollback();
            if (rollback_result.is_err()) {
                // Log warning but continue with disconnect
            }
        }

        if (impl_->native_sqlite_db != nullptr) {
            sqlite3_close(impl_->native_sqlite_db);
            impl_->native_sqlite_db = nullptr;
        }

        impl_->use_native_sqlite = false;
        return ok();
    }

    if (!impl_->db) {
        return ok();
    }

    if (impl_->in_transaction) {
        auto rollback_result = rollback();
        if (rollback_result.is_err()) {
            // Log warning but continue with disconnect
        }
    }

    auto result = impl_->db->disconnect();
    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_void_error(
            result.error().code,
            pacs::compat::format("Failed to disconnect: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto pacs_database_adapter::is_connected() const noexcept -> bool {
    return impl_ &&
           ((impl_->use_native_sqlite && impl_->native_sqlite_db != nullptr) ||
            (impl_->db && impl_->db->is_connected()));
}

auto pacs_database_adapter::open_session() -> pacs_storage_session {
    return pacs_storage_session(*this);
}

auto pacs_database_adapter::open_session() const -> pacs_storage_session {
    return pacs_storage_session(
        const_cast<pacs_database_adapter&>(*this));
}

auto pacs_database_adapter::begin_unit_of_work()
    -> Result<pacs_unit_of_work> {
    auto result = begin_transaction_internal();
    if (result.is_err()) {
        return make_error<pacs_unit_of_work>(
            result.error().code, result.error().message, result.error().module);
    }

    return Result<pacs_unit_of_work>::ok(pacs_unit_of_work(*this, true));
}

// ============================================================================
// Query Builder Factory
// ============================================================================

auto pacs_database_adapter::create_query_builder() -> database::query_builder {
    return database::query_builder(impl_->db_type);
}

// ============================================================================
// CRUD Operations
// ============================================================================

auto pacs_database_adapter::run_select(const std::string& query)
    -> Result<database_result> {
    if (!is_connected()) {
        return make_error<database_result>(
            -1, "Not connected to database", "storage");
    }

    if (impl_->use_native_sqlite) {
        auto result = select_native_sqlite(impl_->native_sqlite_db, query);
        if (result.is_err()) {
            impl_->last_error_msg = result.error().message;
            return make_error<database_result>(
                result.error().code,
                pacs::compat::format("SELECT failed: {}", result.error().message),
                "storage");
        }

        return result;
    }

    auto result = impl_->db->select(query);
    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_error<database_result>(
            result.error().code,
            pacs::compat::format("SELECT failed: {}", result.error().message),
            "storage");
    }

    return kcenon::common::Result<database_result>::ok(
        convert_result(result.value()));
}

auto pacs_database_adapter::run_insert(const std::string& query)
    -> Result<uint64_t> {
    if (!is_connected()) {
        return make_error<uint64_t>(-1, "Not connected to database", "storage");
    }

    if (impl_->use_native_sqlite) {
        auto result = mutate_native_sqlite(impl_->native_sqlite_db, query);
        if (result.is_err()) {
            impl_->last_error_msg = result.error().message;
            return make_error<uint64_t>(
                result.error().code,
                pacs::compat::format("INSERT failed: {}", result.error().message),
                "storage");
        }

        impl_->last_rowid = sqlite3_last_insert_rowid(impl_->native_sqlite_db);
        return result;
    }

    auto result = impl_->db->insert(query);
    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_error<uint64_t>(
            result.error().code,
            pacs::compat::format("INSERT failed: {}", result.error().message),
            "storage");
    }

    // Update last rowid for SQLite compatibility
    if (impl_->db_type == database::database_types::sqlite) {
        // Query for last_insert_rowid
        auto rowid_result =
            impl_->db->select("SELECT last_insert_rowid() as rowid");
        if (rowid_result.is_ok() && !rowid_result.value().empty()) {
            try {
                impl_->last_rowid =
                    std::stoll(rowid_result.value()[0].at("rowid"));
            } catch (...) {
                // Ignore conversion errors
            }
        }
    }

    return kcenon::common::Result<uint64_t>::ok(
        static_cast<uint64_t>(result.value()));
}

auto pacs_database_adapter::run_update(const std::string& query)
    -> Result<uint64_t> {
    if (!is_connected()) {
        return make_error<uint64_t>(-1, "Not connected to database", "storage");
    }

    if (impl_->use_native_sqlite) {
        auto result = mutate_native_sqlite(impl_->native_sqlite_db, query);
        if (result.is_err()) {
            impl_->last_error_msg = result.error().message;
            return make_error<uint64_t>(
                result.error().code,
                pacs::compat::format("UPDATE failed: {}", result.error().message),
                "storage");
        }

        return result;
    }

    auto result = impl_->db->update(query);
    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_error<uint64_t>(
            result.error().code,
            pacs::compat::format("UPDATE failed: {}", result.error().message),
            "storage");
    }

    return kcenon::common::Result<uint64_t>::ok(
        static_cast<uint64_t>(result.value()));
}

auto pacs_database_adapter::run_remove(const std::string& query)
    -> Result<uint64_t> {
    if (!is_connected()) {
        return make_error<uint64_t>(-1, "Not connected to database", "storage");
    }

    if (impl_->use_native_sqlite) {
        auto result = mutate_native_sqlite(impl_->native_sqlite_db, query);
        if (result.is_err()) {
            impl_->last_error_msg = result.error().message;
            return make_error<uint64_t>(
                result.error().code,
                pacs::compat::format("DELETE failed: {}", result.error().message),
                "storage");
        }

        return result;
    }

    auto result = impl_->db->remove(query);
    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_error<uint64_t>(
            result.error().code,
            pacs::compat::format("DELETE failed: {}", result.error().message),
            "storage");
    }

    return kcenon::common::Result<uint64_t>::ok(
        static_cast<uint64_t>(result.value()));
}

auto pacs_database_adapter::run_execute(const std::string& query)
    -> VoidResult {
    if (!is_connected()) {
        return make_void_error(-1, "Not connected to database", "storage");
    }

    if (impl_->use_native_sqlite) {
        auto result = execute_native_sqlite(impl_->native_sqlite_db, query);
        if (result.is_err()) {
            impl_->last_error_msg = result.error().message;
            return make_void_error(
                result.error().code,
                pacs::compat::format("Execute failed: {}", result.error().message),
                "storage");
        }

        return ok();
    }

    auto result = impl_->db->execute(query);
    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_void_error(
            result.error().code,
            pacs::compat::format("Execute failed: {}", result.error().message),
            "storage");
    }

    return ok();
}

auto pacs_database_adapter::select(const std::string& query)
    -> Result<database_result> {
    return run_select(query);
}

auto pacs_database_adapter::insert(const std::string& query)
    -> Result<uint64_t> {
    return run_insert(query);
}

auto pacs_database_adapter::update(const std::string& query)
    -> Result<uint64_t> {
    return run_update(query);
}

auto pacs_database_adapter::remove(const std::string& query)
    -> Result<uint64_t> {
    return run_remove(query);
}

auto pacs_database_adapter::execute(const std::string& query) -> VoidResult {
    return run_execute(query);
}

// ============================================================================
// Transaction Support
// ============================================================================

auto pacs_database_adapter::begin_transaction_internal() -> VoidResult {
    if (!is_connected()) {
        return make_void_error(-1, "Not connected to database", "storage");
    }

    if (impl_->in_transaction) {
        return make_void_error(-1, "Transaction already in progress",
                               "storage");
    }

    auto result = [&]() -> VoidResult {
        if (impl_->use_native_sqlite) {
            return execute_native_sqlite(impl_->native_sqlite_db,
                                         "BEGIN TRANSACTION");
        }

        auto native_result = impl_->db->execute("BEGIN TRANSACTION");
        if (native_result.is_err()) {
            return make_void_error(native_result.error().code,
                                   native_result.error().message,
                                   native_result.error().module);
        }

        return ok();
    }();
    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_void_error(
            result.error().code,
            pacs::compat::format("BEGIN TRANSACTION failed: {}",
                                 result.error().message),
            "storage");
    }

    impl_->in_transaction = true;
    return ok();
}

auto pacs_database_adapter::commit_internal() -> VoidResult {
    if (!is_connected()) {
        return make_void_error(-1, "Not connected to database", "storage");
    }

    if (!impl_->in_transaction) {
        return make_void_error(-1, "No transaction in progress", "storage");
    }

    auto result = [&]() -> VoidResult {
        if (impl_->use_native_sqlite) {
            return execute_native_sqlite(impl_->native_sqlite_db, "COMMIT");
        }

        auto native_result = impl_->db->execute("COMMIT");
        if (native_result.is_err()) {
            return make_void_error(native_result.error().code,
                                   native_result.error().message,
                                   native_result.error().module);
        }

        return ok();
    }();
    impl_->in_transaction = false;

    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_void_error(
            result.error().code,
            pacs::compat::format("COMMIT failed: {}", result.error().message),
            "storage");
    }

    return ok();
}

auto pacs_database_adapter::rollback_internal() -> VoidResult {
    if (!is_connected()) {
        return ok();  // No-op if not connected
    }

    if (!impl_->in_transaction) {
        return ok();  // No-op if not in transaction
    }

    auto result = [&]() -> VoidResult {
        if (impl_->use_native_sqlite) {
            return execute_native_sqlite(impl_->native_sqlite_db, "ROLLBACK");
        }

        auto native_result = impl_->db->execute("ROLLBACK");
        if (native_result.is_err()) {
            return make_void_error(native_result.error().code,
                                   native_result.error().message,
                                   native_result.error().module);
        }

        return ok();
    }();
    impl_->in_transaction = false;

    if (result.is_err()) {
        impl_->last_error_msg = result.error().message;
        return make_void_error(
            result.error().code,
            pacs::compat::format("ROLLBACK failed: {}",
                                 result.error().message),
            "storage");
    }

    return ok();
}

auto pacs_database_adapter::begin_transaction() -> VoidResult {
    return begin_transaction_internal();
}

auto pacs_database_adapter::commit() -> VoidResult {
    return commit_internal();
}

auto pacs_database_adapter::rollback() -> VoidResult {
    return rollback_internal();
}

auto pacs_database_adapter::in_transaction() const noexcept -> bool {
    return impl_ && impl_->in_transaction;
}

// ============================================================================
// SQLite Compatibility
// ============================================================================

auto pacs_database_adapter::last_insert_rowid() const -> int64_t {
    return impl_ ? impl_->last_rowid : 0;
}

auto pacs_database_adapter::last_error() const -> std::string {
    return impl_ ? impl_->last_error_msg : "";
}

// ============================================================================
// scoped_transaction Implementation
// ============================================================================

scoped_transaction::scoped_transaction(pacs_database_adapter& db) : db_(db) {
    auto result = db_.begin_transaction();
    active_ = result.is_ok();
}

scoped_transaction::~scoped_transaction() {
    if (active_ && !committed_) {
        (void)db_.rollback();
    }
}

auto scoped_transaction::commit() -> VoidResult {
    if (!active_) {
        return make_void_error(-1, "Transaction not active", "storage");
    }

    if (committed_) {
        return make_void_error(-1, "Transaction already committed", "storage");
    }

    auto result = db_.commit();
    if (result.is_ok()) {
        committed_ = true;
        active_ = false;
    }
    return result;
}

void scoped_transaction::rollback() noexcept {
    if (active_ && !committed_) {
        (void)db_.rollback();
        active_ = false;
    }
}

auto scoped_transaction::is_active() const noexcept -> bool {
    return active_ && !committed_;
}

}  // namespace pacs::storage

#endif  // PACS_WITH_DATABASE_SYSTEM
