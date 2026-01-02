# ADR-001: Integrate database_system for Database Abstraction

> **Status:** Accepted
> **Date:** 2025-12-31
> **Decision Makers:** pacs_system core team
> **Related Issues:** [#418](https://github.com/kcenon/pacs_system/issues/418), #419, #420, #421, #422

---

## Context

### Current State

The pacs_system project uses **direct SQLite3 C API** calls across 4 files totaling approximately 4,607 lines of code. This approach has served the project well but presents several challenges:

1. **Security Vulnerability**: `sqlite_security_storage.cpp` contained SQL injection vulnerabilities through string interpolation
2. **Code Duplication**: Similar query patterns repeated across multiple files
3. **Limited Flexibility**: Locked to SQLite only, no multi-database support
4. **Inconsistent Error Handling**: Mix of return codes and direct error returns

### Files Affected

| File | Lines | Security Status |
|------|-------|-----------------|
| `src/storage/index_database.cpp` | ~3,200 | Secure (prepared statements) |
| `src/storage/sqlite_security_storage.cpp` | ~200 | **VULNERABLE** (string interpolation) |
| `src/storage/migration_runner.cpp` | ~500 | Secure |
| `src/services/cache/database_cursor.cpp` | ~700 | Secure |

### Security Analysis

The critical vulnerability in `sqlite_security_storage.cpp` allowed SQL injection through user-controlled input:

```cpp
// BEFORE (vulnerable code)
std::string sql = std::format(
    "INSERT INTO users (id, username) VALUES ('{}', '{}');",
    user.id, user.username);  // Direct string interpolation!
sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
```

An attacker could inject:
- `user.username = "admin'); DROP TABLE users; --"`
- Result: Authentication bypass or data destruction

---

## Decision

**We will integrate the `database_system` library to provide a unified database abstraction layer.**

### Selected Approach

The `database_system` library (already maintained by the same team) will be integrated as an optional dependency:

1. **Compile-time conditional**: Use `PACS_WITH_DATABASE_SYSTEM` flag
2. **Query Builder**: All user-facing queries use `sql_query_builder`
3. **Fallback**: Maintain manual escaping for builds without database_system
4. **Unified Result**: Use `Result<T>` pattern consistently

### Why database_system?

| Alternative | Pros | Cons | Decision |
|------------|------|------|----------|
| **Fix vulnerabilities manually** | Simple, no new deps | Doesn't address code duplication | Rejected |
| **Build custom abstraction** | Full control | Reinventing the wheel | Rejected |
| **Use ORM (SQLAlchemy-style)** | High-level API | Too heavy for PACS workload | Rejected |
| **database_system** | SQL injection safe, multi-DB, owned by team | Additional dependency | **Selected** |

---

## Implementation

### CMake Integration

```cmake
# database_system (OPTIONAL - Tier 3, for secure database operations)
if(PACS_BUILD_STORAGE)
    # Search paths
    set(_PACS_DATABASE_PATHS
        "$ENV{DATABASE_SYSTEM_ROOT}"
        "${CMAKE_CURRENT_SOURCE_DIR}/../database_system"
        "${CMAKE_CURRENT_SOURCE_DIR}/database_system"
    )

    # Find and add
    foreach(_path ${_PACS_DATABASE_PATHS})
        if(EXISTS "${_path}/CMakeLists.txt")
            add_subdirectory("${_path}" "${CMAKE_BINARY_DIR}/database_system_build")
            break()
        endif()
    endforeach()

    # Link to targets
    if(TARGET database)
        target_link_libraries(pacs_storage PUBLIC database)
        target_compile_definitions(pacs_storage PUBLIC PACS_WITH_DATABASE_SYSTEM=1)
    endif()
endif()
```

### Code Pattern

{% raw %}
```cpp
#ifdef PACS_WITH_DATABASE_SYSTEM
// Secure: Uses query builder with parameterization
database::sql_query_builder builder;
auto sql = builder
    .insert_into("users")
    .values({{"id", user.id}, {"username", user.username}})
    .build_for_database(database::database_types::sqlite);
auto result = db_manager->insert_query_result(sql);
#else
// Fallback: Manual escaping
std::string escaped_username = escape_string(user.username);
std::string sql = std::format("INSERT INTO users ...", escaped_username);
#endif
```
{% endraw %}

### Migration Phases

| Phase | File | Issue | Priority |
|-------|------|-------|----------|
| 1 | sqlite_security_storage.cpp | #419 | **Critical** (Security) |
| 2 | database_cursor.cpp | #420 | High (DICOM queries) |
| 3 | index_database.cpp | #421 | High (Core storage) |
| 4 | migration_runner.cpp | #422 | Medium |
| 5 | Documentation | #423 | Required for release |

---

## Consequences

### Positive

1. **SQL Injection Prevention**
   - Query Builder ensures all values are properly parameterized
   - No direct string interpolation in SQL queries
   - Static analysis can verify query safety

2. **Multi-Database Support**
   - Future migration to PostgreSQL for production is now possible
   - Same code works with different backends
   - Testing with in-memory SQLite remains fast

3. **Code Reduction**
   - Estimated 60%+ reduction in database-related code
   - Unified patterns across all storage operations
   - Less duplication, easier maintenance

4. **Consistent Error Handling**
   - All operations return `Result<T>` type
   - Error propagation is explicit and type-safe
   - Easier debugging with structured error information

### Negative

1. **Performance Overhead**
   - Query Builder adds ~1ms overhead per query
   - Acceptable for PACS workloads (not high-frequency trading)
   - WAL mode and proper indexing mitigate impact

2. **Additional Dependency**
   - Requires cloning/maintaining database_system
   - Increases build complexity slightly
   - Optional: builds work without it (with fallback)

3. **Migration Effort**
   - ~4 weeks of development time
   - Required testing across all PACS operations
   - Documentation updates needed

4. **Learning Curve**
   - Team must learn Query Builder API
   - Different mental model from raw SQL
   - Mitigated by comprehensive documentation

### Neutral

1. **DDL Remains Raw SQL**
   - CREATE TABLE and migrations use raw SQL strings
   - This is intentional: DDL is not user-input driven
   - Query Builder focuses on DML operations

2. **SQLite Remains Default**
   - No immediate database change
   - Architecture now supports future migration
   - Production PostgreSQL can be considered for v1.0

---

## Validation

### Security Verification

- [ ] Static analysis shows 0 SQL injection vulnerabilities
- [ ] Code review confirms all user inputs pass through Query Builder
- [ ] Penetration test with common SQL injection payloads

### Functional Verification

- [ ] All DICOM operations work (C-FIND, C-STORE, C-MOVE, C-GET)
- [ ] User authentication/authorization functions correctly
- [ ] Schema migrations run successfully
- [ ] Transaction rollback works on errors

### Performance Verification

- [ ] Query latency overhead < 5ms for typical operations
- [ ] No regression in C-STORE throughput benchmarks
- [ ] Database size unchanged for same dataset

---

## Related Documents

- [Migration Guide](../database/MIGRATION_GUIDE.md)
- [API Reference](../database/API_REFERENCE.md)
- [Performance Guide](../database/PERFORMANCE_GUIDE.md)

---

## Decision Log

| Date | Author | Change |
|------|--------|--------|
| 2025-12-31 | pacs_system team | Initial ADR creation, status: Accepted |

---

*Architecture Decision Record following Michael Nygard's template*
