# database_system Migration Guide

> **Version:** 1.0.1
> **Last Updated:** 2026-01-01
> **Related Epic:** [#418](https://github.com/kcenon/pacs_system/issues/418)

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Prerequisites](#2-prerequisites)
- [3. Step-by-Step Migration](#3-step-by-step-migration)
- [4. Common Patterns](#4-common-patterns)
- [5. Troubleshooting](#5-troubleshooting)
- [6. FAQ](#6-faq)

---

## 1. Overview

### 1.1 Why Migrate?

The pacs_system previously used **direct SQLite3 C API** calls, which presented several challenges:

| Issue | Impact | Solution |
|-------|--------|----------|
| SQL Injection Risk | Security vulnerability in user input handling | Query Builder with parameterized queries |
| Code Duplication | Similar query patterns repeated across files | Unified database abstraction layer |
| Single Database Lock | SQLite only, no multi-database support | database_system supports multiple backends |
| Inconsistent Error Handling | Mix of return codes and exceptions | Unified `Result<T>` pattern |

### 1.2 Benefits

After migration to `database_system`:

- **Security**: SQL injection prevention via `sql_query_builder`
- **Maintainability**: ~60% code reduction in database operations
- **Flexibility**: Support for PostgreSQL, MySQL, SQLite, and more
- **Consistency**: Unified error handling with `Result<T>` pattern
- **Testability**: Mock database injection for unit tests

### 1.3 Migration Scope

The following files were migrated as part of Epic #418:

| File | Issue | Status | Priority |
|------|-------|--------|----------|
| `sqlite_security_storage.cpp` | #419 | Complete | Critical (Security) |
| `database_cursor.cpp` | #420 | Complete | High |
| `index_database.cpp` | #421 | Complete | High |
| `migration_runner.cpp` | #422 | Complete | Medium |

### 1.4 Table-by-Table Migration Status

The following tables in `index_database.cpp` have been migrated to use `sql_query_builder`:

| Table | Issue | Status | Description |
|-------|-------|--------|-------------|
| `patients` | #425 | Complete | Patient demographics |
| `studies` | #426 | Complete | Study metadata |
| `series` | #427 | Complete | Series metadata |
| `instances` | #428 | Complete | DICOM instance metadata |
| `mpps` | #429 | Complete | Modality Performed Procedure Step |
| `worklist` | #430 | Complete | Modality Worklist |
| `audit_logs` | #431 | Complete | Audit trail records |

---

## 2. Prerequisites

### 2.1 System Requirements

- **CMake**: 3.16+
- **C++ Compiler**: C++20 compatible (GCC 10+, Clang 12+, MSVC 2019+)
- **database_system**: v0.1.0+

### 2.2 Installing database_system

#### Option A: Clone as sibling directory (Recommended)

```bash
# From pacs_system parent directory
cd ..
git clone https://github.com/kcenon/database_system.git
```

#### Option B: Clone into pacs_system

```bash
cd pacs_system
git clone https://github.com/kcenon/database_system.git
```

#### Option C: Set environment variable

```bash
export DATABASE_SYSTEM_ROOT=/path/to/database_system
```

### 2.3 Verify Integration

CMake will automatically detect database_system:

```bash
cmake -S . -B build
# Look for: "Found database_system at: /path/to/database_system"
# Look for: "database_system: linked (SQL injection protection enabled)"
```

---

## 3. Step-by-Step Migration

### 3.1 Update Headers

```cpp
// Before
#include <sqlite3.h>

// After
#ifdef PACS_WITH_DATABASE_SYSTEM
#include <database/database_manager.h>
#include <database/query_builder.h>
#include <database/core/database_context.h>
#endif
```

### 3.2 Initialize Database Connection

```cpp
// Before (direct SQLite)
sqlite3* db = nullptr;
sqlite3_open("pacs.db", &db);

// After (database_system)
#ifdef PACS_WITH_DATABASE_SYSTEM
auto db_context = std::make_shared<database::database_context>();
auto db_manager = std::make_shared<database::database_manager>(db_context);

db_manager->set_mode(database::database_types::sqlite);
auto connect_result = db_manager->connect_result("pacs.db");
if (connect_result.is_err()) {
    // Handle error
    return make_error<T>(connect_result.error().code,
                         connect_result.error().message, "storage");
}
#endif
```

### 3.3 Refactor INSERT Operations

```cpp
// Before (VULNERABLE to SQL injection)
std::string sql = std::format(
    "INSERT INTO users (id, username) VALUES ('{}', '{}');",
    user.id, user.username);  // DANGER: user input directly interpolated
sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);

// After (SECURE with query builder)
database::sql_query_builder builder;
auto insert_sql = builder
    .insert_into("users")
    .values({
        {"id", user.id},
        {"username", user.username},
        {"active", user.active ? 1 : 0}
    })
    .build_for_database(database::database_types::sqlite);

auto result = db_manager->insert_query_result(insert_sql);
if (result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to insert user: " + result.error().message,
        "storage");
}
```

### 3.4 Refactor SELECT Operations

```cpp
// Before
const char* sql = "SELECT username, active FROM users WHERE id = ?";
sqlite3_stmt* stmt;
sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
sqlite3_bind_text(stmt, 1, id.data(), id.size(), SQLITE_TRANSIENT);

if (sqlite3_step(stmt) == SQLITE_ROW) {
    user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    user.active = sqlite3_column_int(stmt, 1) != 0;
}
sqlite3_finalize(stmt);

// After
database::sql_query_builder builder;
auto select_sql = builder
    .select(std::vector<std::string>{"username", "active"})
    .from("users")
    .where("id", "=", std::string(id))
    .build_for_database(database::database_types::sqlite);

auto result = db_manager->select_query_result(select_sql);
if (result.is_err()) {
    return make_error<User>(kSqliteError,
                            "DB Error: " + result.error().message, "storage");
}

const auto& rows = result.value();
if (rows.empty()) {
    return make_error<User>(kUserNotFound, "User not found", "storage");
}

// Extract values from result row
const auto& row = rows[0];
if (auto it = row.find("username"); it != row.end()) {
    if (std::holds_alternative<std::string>(it->second)) {
        user.username = std::get<std::string>(it->second);
    }
}
if (auto it = row.find("active"); it != row.end()) {
    if (std::holds_alternative<int64_t>(it->second)) {
        user.active = std::get<int64_t>(it->second) != 0;
    }
}
```

### 3.5 Refactor UPDATE Operations

{% raw %}
```cpp
// Before
std::string sql = std::format(
    "UPDATE users SET active = {} WHERE id = '{}';",
    user.active ? 1 : 0, escape_string(user.id));
sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);

// After
database::sql_query_builder builder;
auto update_sql = builder
    .update("users")
    .set({{"active", user.active ? 1 : 0}})
    .where("id", "=", user.id)
    .build_for_database(database::database_types::sqlite);

auto result = db_manager->update_query_result(update_sql);
if (result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to update user: " + result.error().message,
        "storage");
}
```
{% endraw %}

### 3.6 Refactor DELETE Operations

```cpp
// Before
std::string sql = std::format(
    "DELETE FROM users WHERE id = '{}';", escape_string(id));
sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);

// After
database::sql_query_builder builder;
auto delete_sql = builder
    .delete_from("users")
    .where("id", "=", std::string(id))
    .build_for_database(database::database_types::sqlite);

auto result = db_manager->delete_query_result(delete_sql);
if (result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to delete user: " + result.error().message,
        "storage");
}
```

### 3.7 Transaction Support

```cpp
// Before
sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
// ... operations ...
sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
// or on error:
sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);

// After
auto tx_result = db_manager->begin_transaction();
if (tx_result.is_err()) {
    return make_error<std::monostate>(
        kSqliteError, "Failed to begin transaction", "storage");
}

// ... operations ...

if (all_operations_succeeded) {
    auto commit_result = db_manager->commit_transaction();
    if (commit_result.is_err()) {
        (void)db_manager->rollback_transaction();
        return commit_result.propagate();
    }
} else {
    (void)db_manager->rollback_transaction();
}
```

---

## 4. Common Patterns

### 4.1 Conditional Compilation

When maintaining backward compatibility:

```cpp
#ifdef PACS_WITH_DATABASE_SYSTEM
// Use database_system with sql_query_builder
std::shared_ptr<database::database_manager> db_manager_;
#else
// Fallback to direct SQLite
sqlite3* db_{nullptr};
#endif
```

### 4.1.1 Test Code Conditional Compilation

When writing tests that use `database_cursor`, use a helper macro to select the appropriate handle type based on build configuration:

```cpp
// In test files
#ifdef PACS_WITH_DATABASE_SYSTEM
#define GET_CURSOR_HANDLE(db) (db)->db_manager()
#else
#define GET_CURSOR_HANDLE(db) (db)->native_handle()
#endif

// Usage in tests
TEST_CASE("cursor test") {
    auto db_result = index_database::open(":memory:");
    REQUIRE(db_result.is_ok());
    auto db = std::move(db_result.value());

    patient_query query;
    auto result = database_cursor::create_patient_cursor(
        GET_CURSOR_HANDLE(db.get()), query);
    REQUIRE(result.is_ok());
}
```

This pattern ensures tests compile correctly in both:
- **With database_system**: Uses `db_manager()` returning `std::shared_ptr<database::database_manager>`
- **Without database_system**: Uses `native_handle()` returning `sqlite3*`

### 4.2 Value Extraction Helpers

```cpp
namespace {

auto get_string_value(
    const std::map<std::string, database::core::database_value>& row,
    const std::string& key) -> std::string {
    auto it = row.find(key);
    if (it == row.end()) {
        return {};
    }
    if (std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    return {};
}

auto get_int64_value(
    const std::map<std::string, database::core::database_value>& row,
    const std::string& key) -> int64_t {
    auto it = row.find(key);
    if (it == row.end()) {
        return 0;
    }
    if (std::holds_alternative<int64_t>(it->second)) {
        return std::get<int64_t>(it->second);
    }
    return 0;
}

}  // namespace
```

### 4.3 JOIN Queries

```cpp
database::sql_query_builder builder;
auto select_sql = builder
    .select(std::vector<std::string>{"u.id", "u.username", "u.active"})
    .from("users u")
    .join("user_roles ur", "u.id = ur.user_id")
    .where("ur.role", "=", std::string(to_string(role)))
    .build_for_database(database::database_types::sqlite);
```

### 4.4 DICOM Wildcard Support

For C-FIND operations with DICOM wildcards:

```cpp
// Convert DICOM wildcard to SQL LIKE pattern
// "*" -> "%" (any characters)
// "?" -> "_" (single character)
auto to_like_pattern(std::string_view dicom_pattern) -> std::string {
    std::string result;
    result.reserve(dicom_pattern.size() * 2);
    for (char c : dicom_pattern) {
        switch (c) {
            case '*': result += '%'; break;
            case '?': result += '_'; break;
            case '%': result += "\\%"; break;  // escape SQL special chars
            case '_': result += "\\_"; break;
            default: result += c; break;
        }
    }
    return result;
}
```

---

## 5. Troubleshooting

### 5.1 database_system Not Found

**Symptom:**
```
[--] database_system not found - SQL injection protection via query builder disabled
```

**Solution:**
1. Clone database_system to `../database_system`
2. Or set `DATABASE_SYSTEM_ROOT` environment variable
3. Re-run CMake configuration

### 5.2 Deprecated Warnings

**Symptom:**
```
warning: 'database_base' is deprecated
```

**Solution:** These warnings are suppressed automatically in CMakeLists.txt. If you see them:
```cmake
target_compile_options(your_target PRIVATE -Wno-deprecated-declarations)
```

### 5.3 Query Builder Build Errors

**Symptom:**
```
error: no member named 'select' in 'database::sql_query_builder'
```

**Solution:** Ensure proper include order:
```cpp
#include <database/query_builder.h>  // Contains sql_query_builder
```

### 5.4 Result Value Access

**Symptom:** Incorrect value extraction from query results.

**Solution:** Always check variant type before accessing:
```cpp
if (std::holds_alternative<std::string>(it->second)) {
    value = std::get<std::string>(it->second);
} else if (std::holds_alternative<int64_t>(it->second)) {
    // Handle numeric type stored as string in some cases
    value = std::to_string(std::get<int64_t>(it->second));
}
```

---

## 6. FAQ

### Q: Is database_system required for pacs_system?

**A:** No. database_system is optional. Without it, pacs_system falls back to direct SQLite with manual string escaping for SQL injection prevention. However, using database_system is **strongly recommended** for better security and maintainability.

### Q: What databases are supported?

**A:** database_system supports:
- SQLite (current default for pacs_system)
- PostgreSQL
- MySQL/MariaDB
- MongoDB (NoSQL)
- Redis (Key-Value)

### Q: Can I switch databases without code changes?

**A:** For most operations using Query Builder, yes. Simply change the database type:
```cpp
db_manager->set_mode(database::database_types::postgresql);
```

Note: DDL (CREATE TABLE, etc.) may require database-specific syntax.

### Q: How do I run DDL statements?

**A:** Use `execute_query_result` for raw SQL:
```cpp
const char* create_sql = R"(
    CREATE TABLE IF NOT EXISTS users (
        id TEXT PRIMARY KEY,
        username TEXT UNIQUE NOT NULL
    );
)";
auto result = db_manager->execute_query_result(create_sql);
```

### Q: Are there performance implications?

**A:** The Query Builder adds minimal overhead (<1ms per query). For high-throughput scenarios, consider:
- Reusing builder objects where possible
- Using batch operations for bulk inserts
- Enabling WAL mode for SQLite (already configured in pacs_system)

---

## Related Documentation

- [API Reference](API_REFERENCE.md) - Complete API documentation
- [ADR-001: database_system Integration](../adr/ADR-001-database-system-integration.md) - Architecture decision record
- [Performance Guide](PERFORMANCE_GUIDE.md) - Optimization tips and benchmarks

---

*Document Version: 1.0.1*
*Created: 2025-12-31*
*Author: pacs_system documentation team*
