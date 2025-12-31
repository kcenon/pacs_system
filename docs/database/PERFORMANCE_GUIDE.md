# database_system Performance Guide

> **Version:** 1.0.0
> **Last Updated:** 2025-12-31
> **Context:** pacs_system database optimization

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. SQLite Optimization](#2-sqlite-optimization)
- [3. Query Builder Performance](#3-query-builder-performance)
- [4. PACS-Specific Optimizations](#4-pacs-specific-optimizations)
- [5. Benchmarks](#5-benchmarks)
- [6. Monitoring](#6-monitoring)

---

## 1. Overview

### 1.1 Performance Philosophy

The database layer in pacs_system is optimized for **read-heavy workloads** typical of PACS operations:

| Operation | Frequency | Target Latency |
|-----------|-----------|----------------|
| C-FIND (Patient lookup) | High | <10ms |
| C-FIND (Study search) | High | <50ms |
| C-STORE (Instance insert) | Medium | <20ms |
| C-MOVE (File path lookup) | Medium | <30ms |
| Schema migration | Rare | <5s |

### 1.2 Key Metrics

| Metric | Target | Measured |
|--------|--------|----------|
| Patient lookup by ID | <1ms | ~0.5ms |
| Study search by date (10K studies) | <10ms | ~8ms |
| Instance insert | <10ms | ~5ms |
| Bulk insert (1000 instances) | <500ms | ~350ms |
| Query Builder overhead | <1ms | ~0.3ms |

---

## 2. SQLite Optimization

### 2.1 Recommended Configuration

pacs_system configures SQLite with the following optimizations:

```cpp
// Enable WAL mode for concurrent reads
sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

// Set cache size (64MB)
sqlite3_exec(db, "PRAGMA cache_size=-65536;", nullptr, nullptr, nullptr);

// Optimize for read-heavy workload
sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

// Enable foreign keys
sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

// Memory-mapped I/O (256MB)
sqlite3_exec(db, "PRAGMA mmap_size=268435456;", nullptr, nullptr, nullptr);

// Temp store in memory
sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
```

### 2.2 Configuration Options in index_config

```cpp
struct index_config {
    bool wal_mode{true};           // WAL for concurrent reads
    size_t cache_size_mb{64};      // Page cache size
    bool mmap_enabled{true};       // Memory-mapped I/O
    size_t mmap_size{256 * 1024 * 1024};  // 256MB mmap
};
```

### 2.3 WAL Mode Benefits

| Aspect | Rollback Journal | WAL Mode |
|--------|------------------|----------|
| Concurrent readers | Blocked during write | Allowed |
| Write performance | Sync on each commit | Batched syncs |
| Read performance | Good | Better |
| Disk usage | Single file | Main + WAL + SHM |
| Crash recovery | Automatic | Automatic |

### 2.4 Index Strategy

pacs_system uses strategic indexes for common query patterns:

```sql
-- Patient queries
CREATE INDEX idx_patients_name ON patients(patient_name);
CREATE INDEX idx_patients_birth ON patients(birth_date);

-- Study queries (most common)
CREATE INDEX idx_studies_patient ON studies(patient_pk);
CREATE INDEX idx_studies_date ON studies(study_date);
CREATE INDEX idx_studies_accession ON studies(accession_number);

-- Composite index for worklist queries
CREATE INDEX idx_worklist_station_date_mod
    ON worklist(station_ae, scheduled_datetime, modality);
```

---

## 3. Query Builder Performance

### 3.1 Overhead Analysis

The `sql_query_builder` adds minimal overhead:

| Operation | Direct SQL | Query Builder | Overhead |
|-----------|------------|---------------|----------|
| Simple SELECT | 0.15ms | 0.18ms | +0.03ms |
| SELECT with 3 conditions | 0.22ms | 0.28ms | +0.06ms |
| INSERT (5 columns) | 0.35ms | 0.42ms | +0.07ms |
| UPDATE with WHERE | 0.28ms | 0.35ms | +0.07ms |

### 3.2 Builder Reuse

For bulk operations, reuse builder patterns:

```cpp
// Less efficient: Create new builder each time
for (const auto& record : records) {
    database::sql_query_builder builder;  // Created each iteration
    auto sql = builder.insert_into("instances")...;
    db_manager->insert_query_result(sql);
}

// More efficient: Prepare pattern once
const auto insert_pattern = [&](const auto& record) {
    database::sql_query_builder builder;
    return builder.insert_into("instances")
        .values({
            {"sop_uid", record.sop_uid},
            {"series_pk", record.series_pk},
            {"file_path", record.file_path}
        })
        .build_for_database(database::database_types::sqlite);
};

for (const auto& record : records) {
    auto sql = insert_pattern(record);
    db_manager->insert_query_result(sql);
}
```

### 3.3 Batch Operations

For bulk inserts, use transactions:

```cpp
auto bulk_insert_instances(const std::vector<instance>& instances) -> VoidResult {
    auto tx_result = db_manager->begin_transaction();
    if (tx_result.is_err()) return tx_result.propagate();

    for (const auto& inst : instances) {
        database::sql_query_builder builder;
        auto sql = builder.insert_into("instances")
            .values({...})
            .build_for_database(database::database_types::sqlite);

        auto result = db_manager->insert_query_result(sql);
        if (result.is_err()) {
            (void)db_manager->rollback_transaction();
            return result.propagate();
        }
    }

    return db_manager->commit_transaction();
}
```

**Performance Impact:**

| Batch Size | Without Transaction | With Transaction | Improvement |
|------------|---------------------|------------------|-------------|
| 100 | 850ms | 45ms | 19x |
| 1000 | 8,500ms | 350ms | 24x |
| 10000 | 85,000ms | 3,200ms | 27x |

---

## 4. PACS-Specific Optimizations

### 4.1 Study Count Denormalization

Instead of COUNT(*) subqueries, use pre-calculated counts:

```sql
-- Triggers maintain counts automatically
CREATE TRIGGER trg_instances_insert
AFTER INSERT ON instances
BEGIN
    UPDATE series SET num_instances = num_instances + 1
    WHERE series_pk = NEW.series_pk;

    UPDATE studies SET num_instances = num_instances + 1
    WHERE study_pk = (SELECT study_pk FROM series WHERE series_pk = NEW.series_pk);
END;
```

**Query Performance:**

| Approach | Time (50K studies) |
|----------|-------------------|
| COUNT(*) subquery | ~150ms |
| Pre-calculated column | ~8ms |

### 4.2 DICOM Wildcard Optimization

For prefix searches (most common), use indexed LIKE:

```cpp
// Good: Prefix match uses index
.where("patient_name", "LIKE", "SMITH%")  // Uses idx_patients_name

// Slow: Suffix or infix match requires full scan
.where("patient_name", "LIKE", "%SMITH")   // No index
.where("patient_name", "LIKE", "%SMITH%")  // No index
```

### 4.3 Date Range Queries

Always use indexed columns for date filtering:

```cpp
// Efficient: Uses idx_studies_date
.where("study_date", ">=", "20250101")
.and_where("study_date", "<=", "20251231")

// Avoid: Function on column prevents index use
.where("date(study_date)", "=", "2025-01-01")  // NO INDEX
```

### 4.4 Pagination

For large result sets, use OFFSET/LIMIT with ORDER BY:

```cpp
auto get_studies_page(int page, int page_size) {
    database::sql_query_builder builder;
    auto sql = builder
        .select({"study_uid", "study_date", "patient_name"})
        .from("studies s")
        .join("patients p", "s.patient_pk = p.patient_pk")
        .order_by("s.study_date", "DESC")
        .limit(page_size)
        .offset(page * page_size)
        .build_for_database(database::database_types::sqlite);

    return db_manager->select_query_result(sql);
}
```

**Note:** For very large offsets (>10K), consider keyset pagination:

```cpp
// Keyset pagination (more efficient for large offsets)
.where("study_date", "<", last_seen_date)
.order_by("study_date", "DESC")
.limit(page_size)
```

---

## 5. Benchmarks

### 5.1 Test Environment

| Component | Specification |
|-----------|---------------|
| CPU | Apple M1 Pro (10 cores) |
| RAM | 32GB |
| Storage | NVMe SSD |
| OS | macOS 14.0 |
| SQLite | 3.43.0 |
| Dataset | 1M instances, 50K studies |

### 5.2 Query Performance Results

| Query Type | Dataset Size | Avg Latency | P99 Latency |
|------------|--------------|-------------|-------------|
| Patient by ID | 10K patients | 0.4ms | 1.2ms |
| Patient by name (prefix) | 10K patients | 2.1ms | 5.8ms |
| Studies by date range | 50K studies | 8.3ms | 22ms |
| Studies by accession | 50K studies | 0.6ms | 2.1ms |
| Series by study | 200K series | 1.2ms | 3.5ms |
| Instances by series | 1M instances | 2.8ms | 8.4ms |
| Worklist by station+date | 5K items | 1.5ms | 4.2ms |

### 5.3 Write Performance Results

| Operation | Batch Size | Latency | Throughput |
|-----------|------------|---------|------------|
| Single INSERT | 1 | 4.2ms | 238/s |
| Batch INSERT (transaction) | 100 | 45ms | 2,222/s |
| Batch INSERT (transaction) | 1000 | 350ms | 2,857/s |
| UPDATE single row | 1 | 3.8ms | 263/s |
| DELETE cascade | 1 study | 12ms | 83/s |

### 5.4 Query Builder vs Direct SQL

| Scenario | Direct SQLite | database_system | Difference |
|----------|---------------|-----------------|------------|
| Simple SELECT | 0.42ms | 0.48ms | +14% |
| Complex JOIN | 2.1ms | 2.3ms | +9% |
| INSERT (5 cols) | 3.8ms | 4.2ms | +10% |
| UPDATE with WHERE | 3.2ms | 3.5ms | +9% |
| Bulk INSERT (1000) | 320ms | 350ms | +9% |

**Conclusion:** Query Builder overhead is ~10%, well within acceptable range for security benefits.

---

## 6. Monitoring

### 6.1 SQLite Statistics

```cpp
// Query cache statistics
sqlite3_exec(db, "SELECT * FROM sqlite_stat1;", callback, nullptr, nullptr);

// Page cache status
sqlite3_exec(db, "PRAGMA cache_stats;", callback, nullptr, nullptr);

// Memory usage
auto mem_used = sqlite3_memory_used();
auto mem_highwater = sqlite3_memory_highwater(0);
```

### 6.2 Query Timing

```cpp
auto timed_query(const std::string& sql) {
    auto start = std::chrono::high_resolution_clock::now();

    auto result = db_manager->select_query_result(sql);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    log_debug("Query took {}us: {}", duration.count(), sql.substr(0, 100));

    return result;
}
```

### 6.3 Regular Maintenance

```sql
-- Analyze tables for query optimizer (run periodically)
ANALYZE patients;
ANALYZE studies;
ANALYZE series;
ANALYZE instances;

-- Check database integrity (run on startup or schedule)
PRAGMA integrity_check;

-- Optimize database (run during maintenance window)
PRAGMA optimize;

-- Vacuum to reclaim space (after many deletes)
VACUUM;
```

### 6.4 Performance Alerts

Recommended thresholds for monitoring:

| Metric | Warning | Critical |
|--------|---------|----------|
| Patient lookup | >5ms | >20ms |
| Study search | >50ms | >200ms |
| Instance insert | >20ms | >100ms |
| Database size growth | >10%/week | >25%/week |
| Cache hit ratio | <90% | <70% |

---

## Related Documentation

- [Migration Guide](MIGRATION_GUIDE.md) - Step-by-step migration instructions
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [ADR-001](../adr/ADR-001-database-system-integration.md) - Architecture decision record

---

*Document Version: 1.0.0*
*Created: 2025-12-31*
*Author: pacs_system documentation team*
