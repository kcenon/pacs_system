# SDS - Services Cache Module

> **Version:** 1.1.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-05
> **Status:** Complete

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-CACHE-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#477](https://github.com/kcenon/pacs_system/issues/477), [#468](https://github.com/kcenon/pacs_system/issues/468), [#188](https://github.com/kcenon/pacs_system/issues/188), [#189](https://github.com/kcenon/pacs_system/issues/189), [#190](https://github.com/kcenon/pacs_system/issues/190), [#209](https://github.com/kcenon/pacs_system/issues/209) |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Query Cache](#2-query-cache)
- [3. LRU Cache Implementation](#3-lru-cache-implementation)
- [4. Streaming Query Handler](#4-streaming-query-handler)
- [5. Parallel Query Executor](#5-parallel-query-executor)
- [6. Database Cursor](#6-database-cursor)
- [7. Query Result Stream](#7-query-result-stream)
- [8. Class Diagrams](#8-class-diagrams)
- [9. Sequence Diagrams](#9-sequence-diagrams)
- [10. Traceability](#10-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Services Cache module for the PACS System. The module provides performance optimization for DICOM C-FIND query operations through:

- **Query Cache**: Result caching with TTL and pattern-based invalidation
- **LRU Cache**: Thread-safe, memory-efficient Least Recently Used eviction
- **Streaming Queries**: Memory-efficient large result handling with pagination
- **Parallel Execution**: Multi-threaded query processing with timeout support
- **Database Cursor**: Lazy evaluation of database query results

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Query Cache | `query_cache.hpp`, `query_cache.cpp` | DES-CACHE-001 |
| Simple LRU Cache | `simple_lru_cache.hpp` | DES-CACHE-002 |
| Streaming Query Handler | `streaming_query_handler.hpp`, `streaming_query_handler.cpp` | DES-CACHE-003 |
| Parallel Query Executor | `parallel_query_executor.hpp`, `parallel_query_executor.cpp` | DES-CACHE-004 |
| Database Cursor | `database_cursor.hpp`, `database_cursor.cpp` | DES-CACHE-005 |
| Query Result Stream | `query_result_stream.hpp`, `query_result_stream.cpp` | DES-CACHE-006 |

### 1.3 Location

All files are located under:
- Headers: `include/pacs/services/cache/`
- Sources: `src/services/cache/`
- Tests: `tests/services/cache/`

---

## 2. Query Cache

### 2.1 DES-CACHE-001: Query Cache

**Traces to:** SRS-PERF-003 (Query Response Time)

**Files:**
- `include/pacs/services/cache/query_cache.hpp`
- `src/services/cache/query_cache.cpp`

#### 2.1.1 Configuration

```cpp
namespace pacs::services::cache {

struct query_cache_config {
    /// Maximum number of cached query results
    std::size_t max_entries{1000};

    /// Time-to-live for cached results (default: 5 minutes)
    std::chrono::seconds ttl{300};

    /// Enable debug logging for cache operations
    bool enable_logging{false};

    /// Enable metrics reporting to pacs_metrics
    bool enable_metrics{true};

    /// Cache identifier for logging and metrics
    std::string cache_name{"cfind_query_cache"};
};

} // namespace pacs::services::cache
```

#### 2.1.2 Cached Result Type

```cpp
struct cached_query_result {
    /// Serialized query result data
    std::vector<std::uint8_t> data;

    /// Number of matching records in this result
    std::uint32_t match_count{0};

    /// Timestamp when this result was cached
    std::chrono::steady_clock::time_point cached_at;

    /// Query level (PATIENT, STUDY, SERIES, IMAGE)
    std::string query_level;
};
```

#### 2.1.3 Query Cache Class

```cpp
class query_cache {
public:
    using key_type = std::string;
    using value_type = cached_query_result;
    using size_type = std::size_t;

    explicit query_cache(const query_cache_config& config = {});

    // Cache operations
    [[nodiscard]] std::optional<cached_query_result> get(const key_type& key);
    void put(const key_type& key, const cached_query_result& result);
    void put(const key_type& key, cached_query_result&& result);

    // Invalidation
    bool invalidate(const key_type& key);
    size_type invalidate_by_prefix(const std::string& prefix);
    size_type invalidate_by_query_level(const std::string& query_level);
    template <typename Predicate>
    size_type invalidate_if(Predicate pred);
    void clear();
    size_type purge_expired();

    // Information
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_type max_size() const noexcept;
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;
    void reset_stats() noexcept;

    // Key generation helpers
    [[nodiscard]] static std::string build_key(
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);

    [[nodiscard]] static std::string build_key_with_ae(
        const std::string& calling_ae,
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);

private:
    query_cache_config config_;
    string_lru_cache<cached_query_result> cache_;
};
```

#### 2.1.4 Global Cache Instance

```cpp
/// Get the global query cache instance (Meyer's singleton)
[[nodiscard]] query_cache& global_query_cache();

/// Configure the global cache before first use
bool configure_global_cache(const query_cache_config& config);
```

**Design Rationale:**
- LRU eviction prevents memory exhaustion
- TTL ensures data freshness for medical data
- Pattern-based invalidation for efficient cache clearing on data updates
- Global singleton with deferred initialization for application-wide caching

---

## 3. LRU Cache Implementation

### 3.1 DES-CACHE-002: Simple LRU Cache

**Traces to:** SRS-PERF-005 (Memory Usage)

**File:** `include/pacs/services/cache/simple_lru_cache.hpp`

#### 3.1.1 Cache Configuration

```cpp
struct cache_config {
    /// Maximum number of entries in the cache
    std::size_t max_size{1000};

    /// Time-To-Live for cache entries (default: 5 minutes)
    std::chrono::seconds ttl{300};

    /// Enable metrics collection for hit/miss tracking
    bool enable_metrics{true};

    /// Name for metrics identification
    std::string cache_name{"lru_cache"};
};
```

#### 3.1.2 Cache Statistics

```cpp
struct cache_stats {
    std::atomic<std::uint64_t> hits{0};
    std::atomic<std::uint64_t> misses{0};
    std::atomic<std::uint64_t> insertions{0};
    std::atomic<std::uint64_t> evictions{0};
    std::atomic<std::uint64_t> expirations{0};
    std::atomic<std::size_t> current_size{0};

    [[nodiscard]] double hit_rate() const noexcept;
    [[nodiscard]] std::uint64_t total_accesses() const noexcept;
    void reset() noexcept;
};
```

#### 3.1.3 LRU Cache Template

```cpp
template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class simple_lru_cache {
public:
    using key_type = Key;
    using value_type = Value;
    using size_type = std::size_t;
    using clock_type = std::chrono::steady_clock;

    explicit simple_lru_cache(const cache_config& config = {});
    simple_lru_cache(size_type max_size, std::chrono::seconds ttl);

    // Cache operations
    [[nodiscard]] std::optional<Value> get(const Key& key);
    void put(const Key& key, const Value& value);
    void put(const Key& key, Value&& value);
    [[nodiscard]] bool contains(const Key& key) const;
    bool invalidate(const Key& key);

    template <typename Predicate>
    size_type invalidate_if(Predicate pred);

    void clear();
    size_type purge_expired();

    // Information
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_type max_size() const noexcept;
    [[nodiscard]] std::chrono::seconds ttl() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const cache_config& config() const noexcept;

    // Statistics
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;
    void reset_stats() noexcept;

private:
    struct cache_entry {
        Key key;
        Value value;
        time_point expiry_time;
    };

    using list_type = std::list<cache_entry>;
    using map_type = std::unordered_map<Key, list_iterator, Hash, KeyEqual>;

    cache_config config_;
    size_type max_size_;
    mutable std::shared_mutex mutex_;
    list_type lru_list_;
    map_type cache_map_;
    cache_stats stats_;
};

// Type alias for common use case
template <typename Value>
using string_lru_cache = simple_lru_cache<std::string, Value>;
```

**Implementation Details:**
- **O(1) Operations**: Uses hashmap + doubly linked list
- **Thread Safety**: `std::shared_mutex` for reader-writer locking (multiple readers, single writer)
- **TTL Support**: Automatic expiration checking on `get()`
- **LRU Eviction**: Moves accessed entries to front; evicts from back when full

---

## 4. Streaming Query Handler

### 4.1 DES-CACHE-003: Streaming Query Handler

**Traces to:** SRS-SVC-004 (Query Service), FR-5.3

**Files:**
- `include/pacs/services/cache/streaming_query_handler.hpp`
- `src/services/cache/streaming_query_handler.cpp`

```cpp
namespace pacs::services {

class streaming_query_handler {
public:
    using StreamResult = Result<std::unique_ptr<query_result_stream>>;

    explicit streaming_query_handler(storage::index_database* db);

    // Configuration
    void set_page_size(size_t size) noexcept;
    [[nodiscard]] auto page_size() const noexcept -> size_t;
    void set_max_results(size_t max) noexcept;
    [[nodiscard]] auto max_results() const noexcept -> size_t;

    // Stream operations
    [[nodiscard]] auto create_stream(query_level level,
                                     const core::dicom_dataset& query_keys,
                                     const std::string& calling_ae) -> StreamResult;

    [[nodiscard]] auto resume_stream(const std::string& cursor_state,
                                     query_level level,
                                     const core::dicom_dataset& query_keys) -> StreamResult;

    // Compatibility adapter for query_scp
    [[nodiscard]] auto as_query_handler() -> query_handler;

private:
    storage::index_database* db_;
    size_t page_size_{100};
    size_t max_results_{0};  // 0 = unlimited
};

} // namespace pacs::services
```

**Streaming Benefits:**
- Constant memory usage regardless of result size
- Supports cursor-based pagination and resumption
- DICOM C-FIND pending response compatible
- Backward compatible with existing query_scp via adapter

---

## 5. Parallel Query Executor

### 5.1 DES-CACHE-004: Parallel Query Executor

**Traces to:** SRS-PERF-002 (Concurrent Associations)

**Files:**
- `include/pacs/services/cache/parallel_query_executor.hpp`
- `src/services/cache/parallel_query_executor.cpp`

#### 5.1.1 Query Request

```cpp
struct query_request {
    query_level level = query_level::study;
    core::dicom_dataset query_keys;
    std::string calling_ae;
    std::string query_id;
    int priority = 0;  // Lower = higher priority
};
```

#### 5.1.2 Execution Result

```cpp
struct query_execution_result {
    std::string query_id;
    bool success = false;
    std::string error_message;
    std::unique_ptr<query_result_stream> stream;
    std::chrono::milliseconds execution_time{0};
    bool cancelled = false;
    bool timed_out = false;
};
```

#### 5.1.3 Configuration

```cpp
struct parallel_executor_config {
    size_t max_concurrent = 4;
    std::chrono::milliseconds default_timeout{0};  // 0 = no timeout
    size_t page_size = 100;
    bool enable_priority = true;
};
```

#### 5.1.4 Executor Class

```cpp
class parallel_query_executor {
public:
    explicit parallel_query_executor(storage::index_database* db,
                                     const parallel_executor_config& config = {});
    ~parallel_query_executor();

    // Configuration
    void set_max_concurrent(size_t max) noexcept;
    [[nodiscard]] auto max_concurrent() const noexcept -> size_t;
    void set_default_timeout(std::chrono::milliseconds timeout) noexcept;
    [[nodiscard]] auto default_timeout() const noexcept -> std::chrono::milliseconds;

    // Batch execution
    [[nodiscard]] auto execute_all(std::vector<query_request> queries)
        -> std::vector<query_execution_result>;

    // Single query execution
    [[nodiscard]] auto execute_with_timeout(const query_request& query,
                                            std::chrono::milliseconds timeout)
        -> Result<std::unique_ptr<query_result_stream>>;

    [[nodiscard]] auto execute(const query_request& query)
        -> Result<std::unique_ptr<query_result_stream>>;

    // Cancellation
    void cancel_all() noexcept;
    [[nodiscard]] auto is_cancelled() const noexcept -> bool;
    void reset_cancellation() noexcept;

    // Statistics
    [[nodiscard]] auto queries_executed() const noexcept -> size_t;
    [[nodiscard]] auto queries_succeeded() const noexcept -> size_t;
    [[nodiscard]] auto queries_failed() const noexcept -> size_t;
    [[nodiscard]] auto queries_timed_out() const noexcept -> size_t;
    [[nodiscard]] auto queries_in_progress() const noexcept -> size_t;
    void reset_statistics() noexcept;

private:
    storage::index_database* db_;
    parallel_executor_config config_;
    std::atomic<bool> cancelled_{false};
    std::atomic<size_t> queries_executed_{0};
    std::atomic<size_t> queries_succeeded_{0};
    std::atomic<size_t> queries_failed_{0};
    std::atomic<size_t> queries_timed_out_{0};
    std::atomic<size_t> queries_in_progress_{0};
    mutable std::mutex mutex_;
};
```

**Parallelization Strategy:**
- Priority-based scheduling when enabled
- Configurable concurrency limits
- Timeout support with automatic cancellation
- Thread-safe statistics tracking

---

## 6. Database Cursor

### 6.1 DES-CACHE-005: Database Cursor

**Traces to:** SRS-STOR-002 (Index Database)

**Files:**
- `include/pacs/services/cache/database_cursor.hpp`
- `src/services/cache/database_cursor.cpp`

#### 6.1.1 Record Types

```cpp
enum class record_type { patient, study, series, instance };

using query_record = std::variant<storage::patient_record,
                                  storage::study_record,
                                  storage::series_record,
                                  storage::instance_record>;
```

#### 6.1.2 Database Cursor Class

```cpp
class database_cursor {
public:
    ~database_cursor();

    // Non-copyable, movable
    database_cursor(database_cursor&& other) noexcept;
    auto operator=(database_cursor&& other) noexcept -> database_cursor&;

    // Factory methods (SQLite implementation)
    [[nodiscard]] static auto create_patient_cursor(sqlite3* db,
        const storage::patient_query& query) -> Result<std::unique_ptr<database_cursor>>;

    [[nodiscard]] static auto create_study_cursor(sqlite3* db,
        const storage::study_query& query) -> Result<std::unique_ptr<database_cursor>>;

    [[nodiscard]] static auto create_series_cursor(sqlite3* db,
        const storage::series_query& query) -> Result<std::unique_ptr<database_cursor>>;

    [[nodiscard]] static auto create_instance_cursor(sqlite3* db,
        const storage::instance_query& query) -> Result<std::unique_ptr<database_cursor>>;

    // Cursor operations
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto fetch_next() -> std::optional<query_record>;
    [[nodiscard]] auto fetch_batch(size_t batch_size) -> std::vector<query_record>;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto type() const noexcept -> record_type;
    [[nodiscard]] auto reset() -> VoidResult;
    [[nodiscard]] auto serialize() const -> std::string;

private:
    sqlite3_stmt* stmt_{nullptr};
    record_type type_;
    size_t position_{0};
    bool has_more_{true};
    bool stepped_{false};
};
```

**Conditional Compilation:**
- Supports `PACS_WITH_DATABASE_SYSTEM` for `database_system` integration
- Falls back to direct SQLite when not available

**DICOM Wildcard Support:**
- Converts DICOM wildcards (`*`, `?`) to SQL LIKE patterns (`%`, `_`)
- Automatic pattern detection and conversion

---

## 7. Query Result Stream

### 7.1 DES-CACHE-006: Query Result Stream

**Traces to:** SRS-SVC-004 (Query Service), FR-5.3

**Files:**
- `include/pacs/services/cache/query_result_stream.hpp`
- `src/services/cache/query_result_stream.cpp`

#### 7.1.1 Stream Configuration

```cpp
struct stream_config {
    size_t page_size = 100;
    bool include_total_count = false;  // May be expensive for large datasets
};
```

#### 7.1.2 Query Result Stream Class

```cpp
class query_result_stream {
public:
    ~query_result_stream() = default;

    // Non-copyable, movable
    query_result_stream(query_result_stream&&) noexcept = default;
    auto operator=(query_result_stream&&) noexcept -> query_result_stream& = default;

    // Factory methods
    [[nodiscard]] static auto create(storage::index_database* db,
                                     query_level level,
                                     const core::dicom_dataset& query_keys,
                                     const stream_config& config = {})
        -> Result<std::unique_ptr<query_result_stream>>;

    [[nodiscard]] static auto from_cursor(storage::index_database* db,
                                          const std::string& cursor_state,
                                          query_level level,
                                          const core::dicom_dataset& query_keys,
                                          const stream_config& config = {})
        -> Result<std::unique_ptr<query_result_stream>>;

    // Stream operations
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto next_batch() -> std::optional<std::vector<core::dicom_dataset>>;
    [[nodiscard]] auto total_count() const -> std::optional<size_t>;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto level() const noexcept -> query_level;
    [[nodiscard]] auto cursor() const -> std::string;

private:
    query_result_stream(std::unique_ptr<database_cursor> cursor,
                        query_level level,
                        const core::dicom_dataset& query_keys,
                        stream_config config);

    // Record to DICOM dataset conversion
    [[nodiscard]] auto patient_to_dataset(const storage::patient_record& record) const
        -> core::dicom_dataset;
    [[nodiscard]] auto study_to_dataset(const storage::study_record& record) const
        -> core::dicom_dataset;
    [[nodiscard]] auto series_to_dataset(const storage::series_record& record) const
        -> core::dicom_dataset;
    [[nodiscard]] auto instance_to_dataset(const storage::instance_record& record) const
        -> core::dicom_dataset;

    std::unique_ptr<database_cursor> cursor_;
    query_level level_;
    core::dicom_dataset query_keys_;
    stream_config config_;
    mutable std::optional<size_t> total_count_;
};
```

**Features:**
- Lazy evaluation with batch fetching
- Cursor serialization for pagination resumption
- Automatic conversion from database records to DICOM datasets
- Query-level specific field filtering

---

## 8. Class Diagrams

### 8.1 Cache Module Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Services Cache Module                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────────┐         ┌──────────────────────┐              │
│  │  query_cache    │ uses    │  simple_lru_cache    │              │
│  │  (DES-CACHE-001)│────────>│  (DES-CACHE-002)     │              │
│  └────────┬────────┘         └──────────────────────┘              │
│           │                                                         │
│           │ monitors                                                │
│           v                                                         │
│  ┌─────────────────┐                                               │
│  │   cache_stats   │                                               │
│  └─────────────────┘                                               │
│                                                                     │
│  ┌─────────────────────┐      ┌──────────────────────┐            │
│  │streaming_query_     │ uses │ query_result_stream  │            │
│  │handler              │─────>│ (DES-CACHE-006)      │            │
│  │(DES-CACHE-003)      │      └──────────┬───────────┘            │
│  └─────────────────────┘                 │                         │
│                                          │ uses                    │
│  ┌─────────────────────┐                 v                         │
│  │parallel_query_      │      ┌──────────────────────┐            │
│  │executor             │─────>│  database_cursor     │            │
│  │(DES-CACHE-004)      │ uses │  (DES-CACHE-005)     │            │
│  └─────────────────────┘      └──────────────────────┘            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 8.2 LRU Cache Data Structure

```
┌─────────────────────────────────────────────────────────────┐
│                    simple_lru_cache                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  std::unordered_map<Key, list_iterator>                     │
│  ┌─────┬─────┬─────┬─────┐                                 │
│  │ K1  │ K2  │ K3  │ K4  │  O(1) lookup                   │
│  │  │  │  │  │  │  │  │  │                                 │
│  └──┼──┴──┼──┴──┼──┴──┼──┘                                 │
│     │     │     │     │                                     │
│     v     v     v     v                                     │
│  std::list<cache_entry>                                     │
│  ┌─────┐   ┌─────┐   ┌─────┐   ┌─────┐                     │
│  │ K3  │◄──│ K1  │◄──│ K4  │◄──│ K2  │                     │
│  │ V3  │   │ V1  │   │ V4  │   │ V2  │                     │
│  │ exp │   │ exp │   │ exp │   │ exp │                     │
│  └─────┘   └─────┘   └─────┘   └─────┘                     │
│   Most                          Least                       │
│  Recent                        Recent                       │
│                                (evict)                      │
└─────────────────────────────────────────────────────────────┘
```

---

## 9. Sequence Diagrams

### 9.1 Query Cache Hit Flow

```
┌─────────┐      ┌─────────────┐      ┌───────────────┐
│query_scp│      │ query_cache │      │simple_lru_cache│
└────┬────┘      └──────┬──────┘      └───────┬───────┘
     │                  │                     │
     │ get(key)         │                     │
     │─────────────────>│                     │
     │                  │ get(key)            │
     │                  │────────────────────>│
     │                  │                     │
     │                  │   check TTL         │
     │                  │<────────────────────│
     │                  │                     │
     │                  │ splice to front     │
     │                  │ (mark as recent)    │
     │                  │────────────────────>│
     │                  │                     │
     │                  │ optional<value>     │
     │                  │<────────────────────│
     │                  │                     │
     │   cache hit      │ stats.hits++        │
     │   return result  │────────────────────>│
     │<─────────────────│                     │
     │                  │                     │
```

### 9.2 Streaming Query Flow

```
┌─────────┐    ┌─────────────────┐    ┌────────────────┐    ┌──────────────┐
│ Client  │    │streaming_query_ │    │query_result_   │    │database_     │
│         │    │handler          │    │stream          │    │cursor        │
└────┬────┘    └────────┬────────┘    └───────┬────────┘    └──────┬───────┘
     │                  │                     │                    │
     │ create_stream()  │                     │                    │
     │─────────────────>│                     │                    │
     │                  │ create()            │                    │
     │                  │────────────────────>│                    │
     │                  │                     │ create_*_cursor()  │
     │                  │                     │───────────────────>│
     │                  │                     │                    │
     │                  │                     │   cursor           │
     │                  │                     │<───────────────────│
     │                  │   stream            │                    │
     │                  │<────────────────────│                    │
     │   stream         │                     │                    │
     │<─────────────────│                     │                    │
     │                  │                     │                    │
     │ loop             │                     │                    │
     │  next_batch()    │                     │                    │
     │──────────────────────────────────────>│                    │
     │                  │                     │ fetch_batch()      │
     │                  │                     │───────────────────>│
     │                  │                     │                    │
     │                  │                     │   records          │
     │                  │                     │<───────────────────│
     │                  │                     │                    │
     │                  │                     │ convert to DICOM   │
     │                  │                     │ datasets           │
     │   batch results  │                     │                    │
     │<──────────────────────────────────────│                    │
     │                  │                     │                    │
```

---

## 10. Traceability

### 10.1 Requirements to Design

| SRS ID | Requirement | Design ID(s) | Implementation |
|--------|-------------|--------------|----------------|
| SRS-PERF-003 | Query Response Time | DES-CACHE-001, DES-CACHE-002 | Query cache with LRU eviction |
| SRS-PERF-005 | Memory Usage | DES-CACHE-002 | Thread-safe LRU with TTL |
| SRS-SVC-004 | Query Service | DES-CACHE-003, DES-CACHE-006 | Streaming query support |
| SRS-PERF-002 | Concurrent Associations | DES-CACHE-004 | Parallel query execution |
| SRS-STOR-002 | Index Database | DES-CACHE-005 | Database cursor abstraction |

### 10.2 Design to Implementation

| Design ID | Header File | Source File | Test File |
|-----------|-------------|-------------|-----------|
| DES-CACHE-001 | `query_cache.hpp` | `query_cache.cpp` | `query_cache_test.cpp` |
| DES-CACHE-002 | `simple_lru_cache.hpp` | (header-only) | `simple_lru_cache_test.cpp` |
| DES-CACHE-003 | `streaming_query_handler.hpp` | `streaming_query_handler.cpp` | `streaming_query_test.cpp` |
| DES-CACHE-004 | `parallel_query_executor.hpp` | `parallel_query_executor.cpp` | `parallel_query_executor_test.cpp` |
| DES-CACHE-005 | `database_cursor.hpp` | `database_cursor.cpp` | - |
| DES-CACHE-006 | `query_result_stream.hpp` | `query_result_stream.cpp` | - |

### 10.3 Thread Safety Summary

| Component | Thread Safety | Locking Mechanism |
|-----------|---------------|-------------------|
| query_cache | Thread-safe | Uses LRU cache mutex |
| simple_lru_cache | Thread-safe | `std::shared_mutex` (reader-writer) |
| streaming_query_handler | Not thread-safe | Single-thread use |
| parallel_query_executor | Thread-safe | `std::mutex` + atomics |
| database_cursor | Not thread-safe | Single-thread use |
| query_result_stream | Not thread-safe | Single-thread use |

---

*Document Version: 1.1.0*
*Created: 2026-01-04*
*Updated: 2026-01-05*
*Author: kcenon@naver.com*
