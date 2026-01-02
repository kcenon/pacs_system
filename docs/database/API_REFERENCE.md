# database_system API Reference

> **Version:** 1.0.0
> **Last Updated:** 2025-12-31
> **Context:** pacs_system integration

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Core Classes](#2-core-classes)
- [3. Query Builder](#3-query-builder)
- [4. Result Handling](#4-result-handling)
- [5. Transaction Management](#5-transaction-management)
- [6. DICOM-Specific Extensions](#6-dicom-specific-extensions)
- [7. Error Codes](#7-error-codes)

---

## 1. Overview

The `database_system` library provides a unified database abstraction layer for pacs_system. This document covers the API usage in the context of PACS operations.

### 1.1 Header Files

```cpp
// Core database management
#include <database/database_manager.h>

// Database context for shared state
#include <database/core/database_context.h>

// Query builder for safe SQL construction
#include <database/query_builder.h>
```

### 1.2 Namespaces

```cpp
namespace database {
    // Main classes
    class database_manager;
    class database_context;

    // Query building
    class sql_query_builder;

    // Enumerations
    enum class database_types { sqlite, postgresql, mysql, mongodb, redis };

    namespace core {
        // Value types
        using database_value = std::variant<
            std::monostate,  // NULL
            bool,
            int64_t,
            double,
            std::string,
            std::vector<uint8_t>  // BLOB
        >;
    }
}
```

---

## 2. Core Classes

### 2.1 database_context

Shared context for database operations. Manages connection state and configuration.

```cpp
class database_context {
public:
    database_context();
    ~database_context();

    // Configuration
    void set_connection_timeout(std::chrono::seconds timeout);
    void set_query_timeout(std::chrono::seconds timeout);
    void set_max_connections(size_t count);
};
```

**Usage in pacs_system:**

```cpp
auto db_context = std::make_shared<database::database_context>();
```

### 2.2 database_manager

Main interface for database operations.

#### Constructor

```cpp
explicit database_manager(std::shared_ptr<database_context> context);
```

**Parameters:**
- `context`: Shared database context

**Example:**

```cpp
auto db_context = std::make_shared<database::database_context>();
auto db_manager = std::make_shared<database::database_manager>(db_context);
```

#### set_mode

```cpp
[[nodiscard]] auto set_mode(database_types type) -> bool;
```

Sets the database backend type.

**Parameters:**
- `type`: One of `sqlite`, `postgresql`, `mysql`, `mongodb`, `redis`

**Returns:** `true` on success, `false` on failure

**Example:**

```cpp
if (!db_manager->set_mode(database::database_types::sqlite)) {
    // Handle error
}
```

#### connect_result

```cpp
[[nodiscard]] auto connect_result(const std::string& connection_string)
    -> kcenon::common::VoidResult;
```

Establishes database connection.

**Parameters:**
- `connection_string`: Database path (SQLite) or connection URL (PostgreSQL/MySQL)

**Returns:** `VoidResult` - success or error with details

**Examples:**

```cpp
// SQLite
auto result = db_manager->connect_result("pacs_index.db");

// PostgreSQL (future)
// auto result = db_manager->connect_result(
//     "postgresql://user:pass@localhost:5432/pacs");

if (result.is_err()) {
    log_error("Connection failed: {}", result.error().message);
}
```

#### disconnect_result

```cpp
[[nodiscard]] auto disconnect_result() -> kcenon::common::VoidResult;
```

Closes the database connection.

#### Query Methods

```cpp
// SELECT operations
[[nodiscard]] auto select_query_result(const std::string& sql)
    -> kcenon::common::Result<std::vector<
        std::map<std::string, database::core::database_value>>>;

// INSERT operations
[[nodiscard]] auto insert_query_result(const std::string& sql)
    -> kcenon::common::VoidResult;

// UPDATE operations
[[nodiscard]] auto update_query_result(const std::string& sql)
    -> kcenon::common::VoidResult;

// DELETE operations
[[nodiscard]] auto delete_query_result(const std::string& sql)
    -> kcenon::common::VoidResult;

// Raw SQL execution (for DDL, etc.)
[[nodiscard]] auto execute_query_result(const std::string& sql)
    -> kcenon::common::VoidResult;
```

---

## 3. Query Builder

### 3.1 sql_query_builder

Type-safe SQL query construction to prevent SQL injection.

#### SELECT Queries

```cpp
database::sql_query_builder builder;
auto sql = builder
    .select(std::vector<std::string>{"column1", "column2"})
    .from("table_name")
    .where("column1", "=", value)
    .order_by("column2", "DESC")
    .limit(100)
    .build_for_database(database::database_types::sqlite);
```

**Methods:**

| Method | Description | Example |
|--------|-------------|---------|
| `select(columns)` | Columns to retrieve | `.select({"id", "name"})` |
| `from(table)` | Source table | `.from("patients")` |
| `where(col, op, val)` | Filter condition | `.where("id", "=", "123")` |
| `and_where(col, op, val)` | Additional AND condition | `.and_where("active", "=", true)` |
| `or_where(col, op, val)` | Additional OR condition | `.or_where("status", "=", "pending")` |
| `join(table, condition)` | INNER JOIN | `.join("roles", "users.id = roles.user_id")` |
| `left_join(table, cond)` | LEFT JOIN | `.left_join("profiles", "users.id = profiles.user_id")` |
| `order_by(col, dir)` | Sort results | `.order_by("created_at", "DESC")` |
| `limit(n)` | Limit results | `.limit(50)` |
| `offset(n)` | Skip results | `.offset(100)` |

#### INSERT Queries

```cpp
database::sql_query_builder builder;
auto sql = builder
    .insert_into("table_name")
    .values({
        {"column1", value1},
        {"column2", value2}
    })
    .build_for_database(database::database_types::sqlite);
```

**Methods:**

| Method | Description | Example |
|--------|-------------|---------|
| `insert_into(table)` | Target table | `.insert_into("users")` |
| `values(map)` | Column-value pairs | `.values({% raw %}{{{% endraw %}"name", "John"{% raw %}}}{% endraw %})` |

#### UPDATE Queries

{% raw %}
```cpp
database::sql_query_builder builder;
auto sql = builder
    .update("table_name")
    .set({{"column1", new_value}})
    .where("id", "=", id)
    .build_for_database(database::database_types::sqlite);
```
{% endraw %}

**Methods:**

| Method | Description | Example |
|--------|-------------|---------|
| `update(table)` | Target table | `.update("users")` |
| `set(map)` | Column-value updates | `.set({% raw %}{{{% endraw %}"active", false{% raw %}}}{% endraw %})` |

#### DELETE Queries

```cpp
database::sql_query_builder builder;
auto sql = builder
    .delete_from("table_name")
    .where("id", "=", id)
    .build_for_database(database::database_types::sqlite);
```

**Methods:**

| Method | Description | Example |
|--------|-------------|---------|
| `delete_from(table)` | Target table | `.delete_from("sessions")` |

#### Building the Query

```cpp
auto sql = builder.build_for_database(database::database_types::sqlite);
```

**Supported database types:**
- `database::database_types::sqlite`
- `database::database_types::postgresql`
- `database::database_types::mysql`

---

## 4. Result Handling

### 4.1 database_value Type

Query results use `std::variant` to represent different SQL types:

```cpp
using database_value = std::variant<
    std::monostate,        // SQL NULL
    bool,                  // BOOLEAN
    int64_t,               // INTEGER, BIGINT
    double,                // REAL, DOUBLE
    std::string,           // TEXT, VARCHAR
    std::vector<uint8_t>   // BLOB
>;
```

### 4.2 Extracting Values

```cpp
// SELECT query returns vector of maps
auto result = db_manager->select_query_result(sql);
if (result.is_err()) {
    return make_error<T>(result.error().code,
                         result.error().message, "storage");
}

for (const auto& row : result.value()) {
    // String value
    if (auto it = row.find("patient_name"); it != row.end()) {
        if (std::holds_alternative<std::string>(it->second)) {
            patient.name = std::get<std::string>(it->second);
        }
    }

    // Integer value
    if (auto it = row.find("num_instances"); it != row.end()) {
        if (std::holds_alternative<int64_t>(it->second)) {
            study.num_instances = std::get<int64_t>(it->second);
        }
    }

    // Boolean value
    if (auto it = row.find("active"); it != row.end()) {
        if (std::holds_alternative<int64_t>(it->second)) {
            user.active = std::get<int64_t>(it->second) != 0;
        }
    }
}
```

### 4.3 Helper Functions

pacs_system provides helper functions for common extractions:

```cpp
namespace {

auto get_string_value(
    const std::map<std::string, database::core::database_value>& row,
    const std::string& key) -> std::string {
    auto it = row.find(key);
    if (it == row.end()) return {};
    if (std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    return {};
}

auto get_int64_value(
    const std::map<std::string, database::core::database_value>& row,
    const std::string& key) -> int64_t {
    auto it = row.find(key);
    if (it == row.end()) return 0;
    if (std::holds_alternative<int64_t>(it->second)) {
        return std::get<int64_t>(it->second);
    }
    if (std::holds_alternative<std::string>(it->second)) {
        try {
            return std::stoll(std::get<std::string>(it->second));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

auto get_bool_value(
    const std::map<std::string, database::core::database_value>& row,
    const std::string& key) -> bool {
    return get_int64_value(row, key) != 0;
}

}  // namespace
```

---

## 5. Transaction Management

### 5.1 Transaction Methods

```cpp
// Begin transaction
[[nodiscard]] auto begin_transaction() -> kcenon::common::VoidResult;

// Commit transaction
[[nodiscard]] auto commit_transaction() -> kcenon::common::VoidResult;

// Rollback transaction
[[nodiscard]] auto rollback_transaction() -> kcenon::common::VoidResult;
```

### 5.2 Transaction Pattern

{% raw %}
```cpp
auto update_user_with_roles(const User& user) -> VoidResult {
    // Begin transaction
    auto tx_result = db_manager->begin_transaction();
    if (tx_result.is_err()) {
        return make_error<std::monostate>(
            kSqliteError,
            "Failed to begin transaction: " + tx_result.error().message,
            "storage");
    }

    // Update user
    database::sql_query_builder update_builder;
    auto update_sql = update_builder
        .update("users")
        .set({{"active", user.active ? 1 : 0}})
        .where("id", "=", user.id)
        .build_for_database(database::database_types::sqlite);

    auto update_result = db_manager->update_query_result(update_sql);
    if (update_result.is_err()) {
        (void)db_manager->rollback_transaction();
        return make_error<std::monostate>(
            kSqliteError, "Failed to update user", "storage");
    }

    // Delete existing roles
    database::sql_query_builder delete_builder;
    auto delete_sql = delete_builder
        .delete_from("user_roles")
        .where("user_id", "=", user.id)
        .build_for_database(database::database_types::sqlite);

    auto delete_result = db_manager->delete_query_result(delete_sql);
    if (delete_result.is_err()) {
        (void)db_manager->rollback_transaction();
        return delete_result.propagate();
    }

    // Insert new roles
    for (const auto& role : user.roles) {
        database::sql_query_builder role_builder;
        auto role_sql = role_builder
            .insert_into("user_roles")
            .values({{"user_id", user.id}, {"role", to_string(role)}})
            .build_for_database(database::database_types::sqlite);

        auto role_result = db_manager->insert_query_result(role_sql);
        if (role_result.is_err()) {
            (void)db_manager->rollback_transaction();
            return role_result.propagate();
        }
    }

    // Commit
    auto commit_result = db_manager->commit_transaction();
    if (commit_result.is_err()) {
        (void)db_manager->rollback_transaction();
        return commit_result.propagate();
    }

    return ok();
}
```
{% endraw %}

---

## 6. DICOM-Specific Extensions

### 6.1 Wildcard Pattern Conversion

DICOM C-FIND uses different wildcard characters than SQL:

| DICOM | SQL | Description |
|-------|-----|-------------|
| `*` | `%` | Match any characters |
| `?` | `_` | Match single character |

**Conversion function:**

```cpp
auto to_like_pattern(std::string_view dicom_pattern) -> std::string {
    std::string result;
    result.reserve(dicom_pattern.size() * 2);

    for (char c : dicom_pattern) {
        switch (c) {
            case '*': result += '%'; break;
            case '?': result += '_'; break;
            case '%': result += "\\%"; break;  // Escape SQL wildcard
            case '_': result += "\\_"; break;  // Escape SQL wildcard
            default:  result += c; break;
        }
    }

    return result;
}
```

### 6.2 Patient Query Example

```cpp
auto search_patients(const patient_query& query)
    -> Result<std::vector<patient_record>> {

    database::sql_query_builder builder;
    builder.select(std::vector<std::string>{
        "patient_id", "patient_name", "birth_date", "sex"
    }).from("patients");

    // Apply DICOM wildcard matching for patient name
    if (!query.patient_name.empty()) {
        auto like_pattern = to_like_pattern(query.patient_name);
        builder.where("patient_name", "LIKE", like_pattern);
    }

    // Exact match for patient ID
    if (!query.patient_id.empty()) {
        builder.and_where("patient_id", "=", query.patient_id);
    }

    // Date range for birth date
    if (!query.birth_date_from.empty()) {
        builder.and_where("birth_date", ">=", query.birth_date_from);
    }
    if (!query.birth_date_to.empty()) {
        builder.and_where("birth_date", "<=", query.birth_date_to);
    }

    auto sql = builder.build_for_database(database::database_types::sqlite);
    auto result = db_manager->select_query_result(sql);

    // ... process results ...
}
```

### 6.3 Study Query with Hierarchy

```cpp
auto search_studies(const study_query& query)
    -> Result<std::vector<study_record>> {

    database::sql_query_builder builder;
    builder.select(std::vector<std::string>{
        "p.patient_id", "p.patient_name",
        "s.study_uid", "s.study_date", "s.study_time",
        "s.accession_number", "s.study_description",
        "s.modalities_in_study", "s.num_series", "s.num_instances"
    })
    .from("studies s")
    .join("patients p", "s.patient_pk = p.patient_pk");

    if (!query.patient_name.empty()) {
        auto like_pattern = to_like_pattern(query.patient_name);
        builder.where("p.patient_name", "LIKE", like_pattern);
    }

    if (!query.study_date_from.empty()) {
        builder.and_where("s.study_date", ">=", query.study_date_from);
    }

    if (!query.accession_number.empty()) {
        builder.and_where("s.accession_number", "=", query.accession_number);
    }

    if (!query.modality.empty()) {
        auto like_pattern = "%" + query.modality + "%";
        builder.and_where("s.modalities_in_study", "LIKE", like_pattern);
    }

    builder.order_by("s.study_date", "DESC").limit(query.limit);

    auto sql = builder.build_for_database(database::database_types::sqlite);
    return db_manager->select_query_result(sql);
}
```

---

## 7. Error Codes

### 7.1 Common Error Codes

| Code | Constant | Description |
|------|----------|-------------|
| 1 | `kSqliteError` | General SQLite error |
| 2 | `kUserNotFound` | Requested user not found |
| 3 | `kDuplicateKey` | Unique constraint violation |
| 4 | `kDatabaseNotConnected` | Database not connected |
| 5 | `kTransactionFailed` | Transaction operation failed |

### 7.2 Error Handling Pattern

```cpp
auto result = db_manager->select_query_result(sql);

if (result.is_err()) {
    const auto& error = result.error();

    log_error("Database error [{}]: {} (domain: {})",
              error.code, error.message, error.domain);

    return make_error<T>(error.code, error.message, "storage");
}

// Process result.value()
```

---

## Related Documentation

- [Migration Guide](MIGRATION_GUIDE.md) - Step-by-step migration instructions
- [ADR-001: database_system Integration](../adr/ADR-001-database-system-integration.md) - Architecture decision record
- [Performance Guide](PERFORMANCE_GUIDE.md) - Optimization tips and benchmarks

---

*Document Version: 1.0.0*
*Created: 2025-12-31*
*Author: pacs_system documentation team*
