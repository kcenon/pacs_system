# SDS - Services Cache Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-CACHE-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#477](https://github.com/kcenon/pacs_system/issues/477), [#468](https://github.com/kcenon/pacs_system/issues/468) |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Query Cache](#2-query-cache)
- [3. LRU Cache Implementation](#3-lru-cache-implementation)
- [4. Streaming Query Handler](#4-streaming-query-handler)
- [5. Parallel Query Executor](#5-parallel-query-executor)
- [6. Database Cursor](#6-database-cursor)
- [7. Query Result Stream](#7-query-result-stream)
- [8. Traceability](#8-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Services Cache module for the PACS System. The module provides:

- **Query Cache**: Result caching with TTL and invalidation
- **LRU Cache**: Memory-efficient Least Recently Used eviction
- **Streaming Queries**: Memory-efficient large result handling
- **Parallel Execution**: Multi-threaded query processing

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Query Cache | 1 header, 1 source | DES-CACHE-001 |
| Simple LRU Cache | 1 header | DES-CACHE-002 |
| Streaming Query Handler | 1 header, 1 source | DES-CACHE-003 |
| Parallel Query Executor | 1 header, 1 source | DES-CACHE-004 |
| Database Cursor | 1 header, 1 source | DES-CACHE-005 |
| Query Result Stream | 1 header, 1 source | DES-CACHE-006 |

---

## 2. Query Cache

### 2.1 DES-CACHE-001: Query Cache

**Traces to:** SRS-PERF-003 (Query Response Time)

**File:** `include/pacs/services/cache/query_cache.hpp`, `src/services/cache/query_cache.cpp`

```cpp
namespace pacs::services {

struct cache_config {
    std::size_t max_entries{1000};
    std::chrono::seconds default_ttl{300};  // 5 minutes
    bool enable_statistics{true};
};

class query_cache {
public:
    explicit query_cache(const cache_config& config = {});

    // Cache operations
    [[nodiscard]] auto get(const std::string& key)
        -> std::optional<std::vector<core::dicom_dataset>>;

    void put(const std::string& key,
             std::vector<core::dicom_dataset> results,
             std::chrono::seconds ttl = {});

    void invalidate(const std::string& key);
    void invalidate_by_pattern(const std::string& pattern);
    void clear();

    // Statistics
    [[nodiscard]] auto hit_rate() const noexcept -> double;
    [[nodiscard]] auto size() const noexcept -> std::size_t;

private:
    std::string generate_key(const core::dicom_dataset& query) const;
    void evict_expired();

    simple_lru_cache<std::string, cached_result> cache_;
    cache_config config_;
    mutable std::shared_mutex mutex_;
};

} // namespace pacs::services
```

**Design Rationale:**
- LRU eviction prevents memory exhaustion
- TTL ensures data freshness
- Pattern-based invalidation for study updates

---

## 3. LRU Cache Implementation

### 3.1 DES-CACHE-002: Simple LRU Cache

**Traces to:** SRS-PERF-005 (Memory Usage)

**File:** `include/pacs/services/cache/simple_lru_cache.hpp`

```cpp
namespace pacs::services {

template<typename Key, typename Value>
class simple_lru_cache {
public:
    explicit simple_lru_cache(std::size_t capacity);

    [[nodiscard]] auto get(const Key& key) -> std::optional<Value>;
    void put(const Key& key, Value value);
    void remove(const Key& key);
    void clear();

    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto capacity() const noexcept -> std::size_t;

private:
    struct node {
        Key key;
        Value value;
    };

    std::size_t capacity_;
    std::list<node> items_;
    std::unordered_map<Key, typename std::list<node>::iterator> index_;
};

} // namespace pacs::services
```

**Eviction Policy:**
- O(1) get/put operations using hashmap + doubly linked list
- Evicts least recently used entry when capacity exceeded
- Thread-safe with read-write lock

---

## 4. Streaming Query Handler

### 4.1 DES-CACHE-003: Streaming Query Handler

**Traces to:** SRS-SVC-004 (Query Service)

**File:** `include/pacs/services/cache/streaming_query_handler.hpp`, `src/services/cache/streaming_query_handler.cpp`

```cpp
namespace pacs::services {

class streaming_query_handler {
public:
    explicit streaming_query_handler(storage::index_database& database);

    // Start streaming query
    [[nodiscard]] auto begin_query(const core::dicom_dataset& query)
        -> query_result_stream;

    // Process with callback (memory efficient)
    void process_query(
        const core::dicom_dataset& query,
        std::function<bool(const core::dicom_dataset&)> callback);

    // Configuration
    void set_batch_size(std::size_t size);
    void set_timeout(std::chrono::milliseconds timeout);

private:
    storage::index_database& database_;
    std::size_t batch_size_{100};
    std::chrono::milliseconds timeout_{30000};
};

} // namespace pacs::services
```

**Streaming Benefits:**
- Constant memory usage regardless of result size
- Supports early termination via callback return
- DICOM C-FIND pending response compatible

---

## 5. Parallel Query Executor

### 5.1 DES-CACHE-004: Parallel Query Executor

**Traces to:** SRS-PERF-002 (Concurrent Associations)

**File:** `include/pacs/services/cache/parallel_query_executor.hpp`, `src/services/cache/parallel_query_executor.cpp`

```cpp
namespace pacs::services {

class parallel_query_executor {
public:
    explicit parallel_query_executor(
        std::size_t thread_count = std::thread::hardware_concurrency());

    // Execute query with parallel index searches
    [[nodiscard]] auto execute(
        storage::index_database& database,
        const core::dicom_dataset& query)
        -> std::vector<core::dicom_dataset>;

    // Execute with result merging
    [[nodiscard]] auto execute_and_merge(
        std::vector<storage::index_database*> databases,
        const core::dicom_dataset& query)
        -> std::vector<core::dicom_dataset>;

private:
    std::shared_ptr<kcenon::thread::thread_pool> pool_;
};

} // namespace pacs::services
```

**Parallelization Strategy:**
- Partition search space by date ranges
- Parallel execution on thread pool
- Result merging with deduplication

---

## 6. Database Cursor

### 6.1 DES-CACHE-005: Database Cursor

**Traces to:** SRS-STOR-002 (Index Database)

**File:** `include/pacs/services/cache/database_cursor.hpp`, `src/services/cache/database_cursor.cpp`

```cpp
namespace pacs::services {

class database_cursor {
public:
    database_cursor(sqlite3_stmt* stmt, bool owns = true);
    ~database_cursor();

    // Move-only
    database_cursor(database_cursor&& other) noexcept;
    database_cursor& operator=(database_cursor&& other) noexcept;

    // Iteration
    [[nodiscard]] auto next() -> bool;
    [[nodiscard]] auto current() const -> core::dicom_dataset;

    // Column access
    [[nodiscard]] auto get_text(int col) const -> std::string;
    [[nodiscard]] auto get_int(int col) const -> int64_t;
    [[nodiscard]] auto get_blob(int col) const -> std::vector<uint8_t>;

    // Status
    [[nodiscard]] auto done() const noexcept -> bool;
    [[nodiscard]] auto row_count() const noexcept -> std::size_t;

private:
    sqlite3_stmt* stmt_;
    bool owns_;
    bool done_{false};
    std::size_t row_count_{0};
};

} // namespace pacs::services
```

---

## 7. Query Result Stream

### 7.1 DES-CACHE-006: Query Result Stream

**Traces to:** SRS-SVC-004 (Query Service)

**File:** `include/pacs/services/cache/query_result_stream.hpp`, `src/services/cache/query_result_stream.cpp`

```cpp
namespace pacs::services {

class query_result_stream {
public:
    // Range-based for loop support
    class iterator {
    public:
        using value_type = core::dicom_dataset;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        using iterator_category = std::input_iterator_tag;

        iterator& operator++();
        reference operator*() const;
        bool operator!=(const iterator& other) const;
    };

    iterator begin();
    iterator end();

    // Status
    [[nodiscard]] auto available() const -> std::size_t;
    [[nodiscard]] auto exhausted() const -> bool;

private:
    std::unique_ptr<database_cursor> cursor_;
    std::optional<core::dicom_dataset> current_;
};

} // namespace pacs::services
```

---

## 8. Traceability

### 8.1 Requirements to Design

| SRS ID | Design ID(s) | Implementation |
|--------|--------------|----------------|
| SRS-PERF-003 | DES-CACHE-001, DES-CACHE-002 | Query cache with LRU |
| SRS-SVC-004 | DES-CACHE-003, DES-CACHE-006 | Streaming query support |
| SRS-PERF-002 | DES-CACHE-004 | Parallel query execution |
| SRS-STOR-002 | DES-CACHE-005 | Database cursor abstraction |

---

*Document Version: 1.0.0*
*Created: 2026-01-04*
*Author: kcenon@naver.com*
