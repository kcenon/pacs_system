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
        case database::database_types::mysql:
            return database::integrated::backend_type::mysql;
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

}  // namespace

// ============================================================================
// Construction / Destruction
// ============================================================================

pacs_database_adapter::pacs_database_adapter(
    const std::filesystem::path& db_path)
    : impl_(std::make_unique<impl>(database::database_types::sqlite,
                                   db_path.string())) {}

pacs_database_adapter::pacs_database_adapter(
    database::database_types type, const std::string& connection_string)
    : impl_(std::make_unique<impl>(type, connection_string)) {}

pacs_database_adapter::~pacs_database_adapter() {
    if (impl_ && impl_->db && impl_->db->is_connected()) {
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

    try {
        // Create database system using builder
        impl_->db =
            database::integrated::unified_database_system::create_builder()
                .set_backend(to_backend_type(impl_->db_type))
                .set_connection_string(impl_->connection_string)
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

        return ok();
    } catch (const std::exception& e) {
        impl_->last_error_msg = e.what();
        return make_void_error(
            -1, pacs::compat::format("Connection failed: {}", e.what()),
            "storage");
    }
}

auto pacs_database_adapter::disconnect() -> VoidResult {
    if (!impl_ || !impl_->db) {
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
    return impl_ && impl_->db && impl_->db->is_connected();
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

auto pacs_database_adapter::select(const std::string& query)
    -> Result<database_result> {
    if (!is_connected()) {
        return make_error<database_result>(
            -1, "Not connected to database", "storage");
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

auto pacs_database_adapter::insert(const std::string& query)
    -> Result<uint64_t> {
    if (!is_connected()) {
        return make_error<uint64_t>(-1, "Not connected to database", "storage");
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

auto pacs_database_adapter::update(const std::string& query)
    -> Result<uint64_t> {
    if (!is_connected()) {
        return make_error<uint64_t>(-1, "Not connected to database", "storage");
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

auto pacs_database_adapter::remove(const std::string& query)
    -> Result<uint64_t> {
    if (!is_connected()) {
        return make_error<uint64_t>(-1, "Not connected to database", "storage");
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

auto pacs_database_adapter::execute(const std::string& query) -> VoidResult {
    if (!is_connected()) {
        return make_void_error(-1, "Not connected to database", "storage");
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

// ============================================================================
// Transaction Support
// ============================================================================

auto pacs_database_adapter::begin_transaction() -> VoidResult {
    if (!is_connected()) {
        return make_void_error(-1, "Not connected to database", "storage");
    }

    if (impl_->in_transaction) {
        return make_void_error(-1, "Transaction already in progress",
                               "storage");
    }

    auto result = impl_->db->execute("BEGIN TRANSACTION");
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

auto pacs_database_adapter::commit() -> VoidResult {
    if (!is_connected()) {
        return make_void_error(-1, "Not connected to database", "storage");
    }

    if (!impl_->in_transaction) {
        return make_void_error(-1, "No transaction in progress", "storage");
    }

    auto result = impl_->db->execute("COMMIT");
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

auto pacs_database_adapter::rollback() -> VoidResult {
    if (!is_connected()) {
        return ok();  // No-op if not connected
    }

    if (!impl_->in_transaction) {
        return ok();  // No-op if not in transaction
    }

    auto result = impl_->db->execute("ROLLBACK");
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
