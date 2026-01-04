# SDS - Cloud Storage Backends Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-STOR-002 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#473](https://github.com/kcenon/pacs_system/issues/473), [#468](https://github.com/kcenon/pacs_system/issues/468) |

### Related Standards

| Standard | Description |
|----------|-------------|
| DICOM PS3.10 Section 7 | DICOM File Service |
| AWS S3 API | Amazon Simple Storage Service API Reference |
| Azure Blob Storage REST API | Azure Storage Services REST API |
| HIPAA 164.312 | Technical Safeguards for PHI Storage |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Storage Interface](#2-storage-interface)
- [3. AWS S3 Storage Backend](#3-aws-s3-storage-backend)
- [4. Azure Blob Storage Backend](#4-azure-blob-storage-backend)
- [5. Hierarchical Storage Management](#5-hierarchical-storage-management)
- [6. HSM Migration Service](#6-hsm-migration-service)
- [7. SQLite Security Storage](#7-sqlite-security-storage)
- [8. Class Diagrams](#8-class-diagrams)
- [9. Sequence Diagrams](#9-sequence-diagrams)
- [10. Configuration Examples](#10-configuration-examples)
- [11. Traceability](#11-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Cloud Storage Backends module for the PACS System. The module provides:

- **Cloud Storage**: Store DICOM data in AWS S3 and Azure Blob Storage
- **Hierarchical Storage Management (HSM)**: Multi-tier storage with automatic data migration
- **Storage Abstraction**: Unified interface for local and cloud storage backends
- **Background Migration**: Automatic tier migration based on configurable policies

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| S3 Storage Backend | 1 header, 1 source | DES-STOR-005 |
| Azure Blob Storage Backend | 1 header, 1 source | DES-STOR-006 |
| HSM Storage | 1 header, 1 source | DES-STOR-007 |
| HSM Migration Service | 1 header, 1 source | DES-STOR-008 |
| SQLite Security Storage | 1 header, 1 source | DES-STOR-009 |
| HSM Types | 1 header | (header-only) |

### 1.3 Design Identifier Convention

```
DES-STOR-<NUMBER>

Where:
- DES: Design Specification prefix
- STOR: Storage module identifier
- NUMBER: 3-digit sequential number (005-009)
```

### 1.4 External Library Dependencies

| Backend | Library | Purpose |
|---------|---------|---------|
| AWS S3 | AWS SDK for C++ | S3 API operations |
| Azure Blob | Azure SDK for C++ | Blob Storage API operations |
| SQLite | sqlite3 | Local security data storage |
| HSM | Pure C++ | No external dependency (uses storage backends) |

---

## 2. Storage Interface

### 2.1 DES-STOR-001: Storage Interface Base Class (Reference)

**Traces to:** SRS-STOR-001 (Storage Interface)

**File:** `include/pacs/storage/storage_interface.hpp`

Abstract base class for all storage backends using Strategy pattern. All cloud storage implementations inherit from this interface.

```cpp
namespace pacs::storage {

struct storage_statistics {
    std::size_t total_instances{0};
    std::size_t total_bytes{0};
    std::size_t studies_count{0};
    std::size_t series_count{0};
    std::size_t patients_count{0};
};

class storage_interface {
public:
    virtual ~storage_interface() = default;

    // CRUD Operations
    [[nodiscard]] virtual auto store(const core::dicom_dataset& dataset)
        -> VoidResult = 0;
    [[nodiscard]] virtual auto retrieve(std::string_view sop_instance_uid)
        -> Result<core::dicom_dataset> = 0;
    [[nodiscard]] virtual auto remove(std::string_view sop_instance_uid)
        -> VoidResult = 0;
    [[nodiscard]] virtual auto exists(std::string_view sop_instance_uid) const
        -> bool = 0;

    // Query Operations
    [[nodiscard]] virtual auto find(const core::dicom_dataset& query)
        -> Result<std::vector<core::dicom_dataset>> = 0;

    // Batch Operations (default implementation provided)
    [[nodiscard]] virtual auto store_batch(
        const std::vector<core::dicom_dataset>& datasets) -> VoidResult;
    [[nodiscard]] virtual auto retrieve_batch(
        const std::vector<std::string>& sop_instance_uids)
        -> Result<std::vector<core::dicom_dataset>>;

    // Maintenance Operations
    [[nodiscard]] virtual auto get_statistics() const -> storage_statistics = 0;
    [[nodiscard]] virtual auto verify_integrity() -> VoidResult = 0;
};

} // namespace pacs::storage
```

**Design Decisions:**
- Strategy pattern allows runtime backend selection
- Thread safety is required for all implementations
- `Result<T>` type for explicit error handling without exceptions
- Batch operations have default implementations calling single operations

---

## 3. AWS S3 Storage Backend

### 3.1 DES-STOR-005: S3 Storage Class

**Traces to:** SRS-STOR-003 (Cloud Storage Backend), FR-4.2 (Cloud Storage)

**File:** `include/pacs/storage/s3_storage.hpp`, `src/storage/s3_storage.cpp`

AWS S3-compatible storage backend for DICOM files. Supports AWS S3 and S3-compatible services (MinIO, etc.).

#### 3.1.1 Configuration Structure

```cpp
namespace pacs::storage {

struct cloud_storage_config {
    std::string bucket_name;                         // S3 bucket name
    std::string region{"us-east-1"};                 // AWS region
    std::string access_key_id;                       // AWS access key
    std::string secret_access_key;                   // AWS secret key
    std::optional<std::string> endpoint_url;         // Custom endpoint (MinIO)

    // Performance settings
    std::size_t multipart_threshold = 100 * 1024 * 1024;  // 100MB
    std::size_t part_size = 10 * 1024 * 1024;             // 10MB
    std::size_t max_connections = 25;
    std::uint32_t connect_timeout_ms = 3000;
    std::uint32_t request_timeout_ms = 30000;

    // Security settings
    bool enable_encryption = false;                  // SSE-S3
    std::string storage_class{"STANDARD"};           // S3 storage class
};

} // namespace pacs::storage
```

#### 3.1.2 S3 Object Key Structure

Objects are organized hierarchically for efficient prefix-based listing:

```
{bucket}/
  +-- {StudyInstanceUID}/
      +-- {SeriesInstanceUID}/
          +-- {SOPInstanceUID}.dcm
```

**Example:**
```
dicom-archive/
  +-- 1.2.840.113619.2.55.3.604688.123/
      +-- 1.2.840.113619.2.55.3.604688.456/
          +-- 1.2.840.113619.2.55.3.604688.789.dcm
```

#### 3.1.3 Class Interface

```cpp
class s3_storage : public storage_interface {
public:
    explicit s3_storage(const cloud_storage_config& config);
    ~s3_storage() override;

    // storage_interface implementation
    [[nodiscard]] auto store(const core::dicom_dataset& dataset) -> VoidResult override;
    [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
        -> Result<core::dicom_dataset> override;
    [[nodiscard]] auto remove(std::string_view sop_instance_uid) -> VoidResult override;
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool override;
    [[nodiscard]] auto find(const core::dicom_dataset& query)
        -> Result<std::vector<core::dicom_dataset>> override;
    [[nodiscard]] auto get_statistics() const -> storage_statistics override;
    [[nodiscard]] auto verify_integrity() -> VoidResult override;

    // S3-specific operations
    [[nodiscard]] auto store_with_progress(const core::dicom_dataset& dataset,
                                           progress_callback callback) -> VoidResult;
    [[nodiscard]] auto retrieve_with_progress(std::string_view sop_instance_uid,
                                              progress_callback callback)
        -> Result<core::dicom_dataset>;
    [[nodiscard]] auto rebuild_index() -> VoidResult;
    [[nodiscard]] auto is_connected() const -> bool;

private:
    cloud_storage_config config_;
    std::unique_ptr<mock_s3_client> client_;  // Will be AWS SDK S3Client
    std::unordered_map<std::string, s3_object_info> index_;
    mutable std::shared_mutex mutex_;
};
```

#### 3.1.4 Multipart Upload Strategy

Files exceeding `multipart_threshold` use multipart upload for reliability:

| File Size | Upload Strategy | Benefits |
|-----------|-----------------|----------|
| < 100MB | Single PUT | Simple, atomic |
| >= 100MB | Multipart (10MB parts) | Resumable, parallel |

**Multipart Upload Flow:**
1. Initialize multipart upload
2. Upload parts in parallel (up to `max_connections`)
3. Complete multipart upload with ETag verification
4. On failure: Abort and retry

#### 3.1.5 Thread Safety

- Uses `std::shared_mutex` for read-write locking
- Concurrent reads allowed (shared lock)
- Writes require exclusive lock for index updates
- S3 SDK operations are internally thread-safe

---

## 4. Azure Blob Storage Backend

### 4.1 DES-STOR-006: Azure Blob Storage Class

**Traces to:** SRS-STOR-004 (Cloud Storage Backend), FR-4.2 (Cloud Storage)

**File:** `include/pacs/storage/azure_blob_storage.hpp`, `src/storage/azure_blob_storage.cpp`

Azure Blob Storage backend for DICOM files. Supports Azure Storage and Azurite emulator.

#### 4.1.1 Configuration Structure

```cpp
namespace pacs::storage {

struct azure_storage_config {
    std::string container_name;                      // Blob container name
    std::string connection_string;                   // Azure connection string
    std::optional<std::string> endpoint_suffix;      // Sovereign cloud suffix
    std::optional<std::string> endpoint_url;         // Azurite emulator URL

    // Performance settings
    std::size_t block_upload_threshold = 100 * 1024 * 1024;  // 100MB
    std::size_t block_size = 4 * 1024 * 1024;                // 4MB per block
    std::size_t max_concurrency = 8;
    std::uint32_t connect_timeout_ms = 3000;
    std::uint32_t request_timeout_ms = 60000;

    // Security settings
    bool use_https = true;
    std::string access_tier{"Hot"};                  // Hot, Cool, Archive
    std::uint32_t max_retries = 3;
    std::uint32_t retry_delay_ms = 1000;
};

} // namespace pacs::storage
```

#### 4.1.2 Blob Naming Structure

Similar to S3, blobs use hierarchical naming:

```
{container}/
  +-- {StudyInstanceUID}/
      +-- {SeriesInstanceUID}/
          +-- {SOPInstanceUID}.dcm
```

#### 4.1.3 Class Interface

```cpp
class azure_blob_storage : public storage_interface {
public:
    explicit azure_blob_storage(const azure_storage_config& config);
    ~azure_blob_storage() override;

    // storage_interface implementation
    [[nodiscard]] auto store(const core::dicom_dataset& dataset) -> VoidResult override;
    [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
        -> Result<core::dicom_dataset> override;
    [[nodiscard]] auto remove(std::string_view sop_instance_uid) -> VoidResult override;
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool override;
    [[nodiscard]] auto find(const core::dicom_dataset& query)
        -> Result<std::vector<core::dicom_dataset>> override;
    [[nodiscard]] auto get_statistics() const -> storage_statistics override;
    [[nodiscard]] auto verify_integrity() -> VoidResult override;

    // Azure-specific operations
    [[nodiscard]] auto set_access_tier(std::string_view sop_instance_uid,
                                       std::string_view tier) -> VoidResult;
    [[nodiscard]] auto rebuild_index() -> VoidResult;
    [[nodiscard]] auto is_connected() const -> bool;

private:
    azure_storage_config config_;
    std::unique_ptr<mock_azure_client> client_;  // Will be Azure SDK BlobContainerClient
    std::unordered_map<std::string, azure_blob_info> index_;
    mutable std::shared_mutex mutex_;
};
```

#### 4.1.4 Access Tier Management

Azure Blob Storage supports automatic tiering:

| Tier | Access Latency | Storage Cost | Use Case |
|------|----------------|--------------|----------|
| Hot | Milliseconds | Highest | Frequently accessed |
| Cool | Milliseconds | Medium | Infrequently accessed (30+ days) |
| Archive | Hours | Lowest | Rarely accessed (180+ days) |

**Tier Change API:**
```cpp
// Change access tier for an instance
auto result = storage.set_access_tier("1.2.3.4.5.6.7.8.9", "Cool");
```

#### 4.1.5 Block Blob Upload Strategy

Similar to S3 multipart upload:

| File Size | Upload Strategy | Max Blocks |
|-----------|-----------------|------------|
| < 100MB | Single PUT | 1 |
| >= 100MB | Block upload (4MB blocks) | 50,000 |

---

## 5. Hierarchical Storage Management

### 5.1 DES-STOR-007: HSM Storage Class

**Traces to:** SRS-STOR-010 (Hierarchical Storage), FR-4.5 (HSM)

**File:** `include/pacs/storage/hsm_storage.hpp`, `src/storage/hsm_storage.cpp`

Multi-tier storage that combines local and cloud backends with automatic data migration.

#### 5.1.1 Storage Tier Definition

```cpp
enum class storage_tier {
    hot,    // Recent, frequently accessed (SSD/NVMe)
    warm,   // Older, occasionally accessed (HDD)
    cold    // Archive, rarely accessed (S3/Glacier)
};
```

#### 5.1.2 Tier Policy Configuration

```cpp
struct tier_policy {
    std::chrono::days hot_to_warm{30};     // Move to warm after 30 days
    std::chrono::days warm_to_cold{365};   // Move to cold after 1 year
    bool auto_migrate{true};                // Enable automatic migration
    std::size_t min_migration_size{0};      // Minimum size for migration
    std::size_t max_instances_per_cycle{100};
    std::size_t max_bytes_per_cycle{10ULL * 1024 * 1024 * 1024};  // 10GB
};
```

#### 5.1.3 HSM Storage Configuration

```cpp
struct hsm_storage_config {
    tier_policy policy;
    bool track_access_time{true};           // Update timestamps on retrieve
    bool verify_after_migration{true};      // Verify data after migration
    bool delete_after_migration{true};      // Remove source after migration
};
```

#### 5.1.4 Class Interface

```cpp
class hsm_storage : public storage_interface {
public:
    hsm_storage(std::unique_ptr<storage_interface> hot_tier,
                std::unique_ptr<storage_interface> warm_tier,
                std::unique_ptr<storage_interface> cold_tier,
                const hsm_storage_config& config = {});
    ~hsm_storage() override = default;

    // storage_interface implementation
    [[nodiscard]] auto store(const core::dicom_dataset& dataset) -> VoidResult override;
    [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
        -> Result<core::dicom_dataset> override;
    [[nodiscard]] auto remove(std::string_view sop_instance_uid) -> VoidResult override;
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool override;
    [[nodiscard]] auto find(const core::dicom_dataset& query)
        -> Result<std::vector<core::dicom_dataset>> override;
    [[nodiscard]] auto get_statistics() const -> storage_statistics override;
    [[nodiscard]] auto verify_integrity() -> VoidResult override;

    // HSM-specific operations
    [[nodiscard]] auto get_tier(std::string_view sop_instance_uid) const
        -> std::optional<storage_tier>;
    [[nodiscard]] auto migrate(std::string_view sop_instance_uid,
                               storage_tier target_tier) -> VoidResult;
    [[nodiscard]] auto get_migration_candidates(storage_tier from_tier,
                                                storage_tier to_tier) const
        -> std::vector<tier_metadata>;
    [[nodiscard]] auto run_migration_cycle() -> migration_result;
    [[nodiscard]] auto get_hsm_statistics() const -> hsm_statistics;

private:
    std::unique_ptr<storage_interface> hot_tier_;
    std::unique_ptr<storage_interface> warm_tier_;   // May be nullptr
    std::unique_ptr<storage_interface> cold_tier_;   // May be nullptr
    hsm_storage_config config_;
    std::unordered_map<std::string, tier_metadata> metadata_index_;
    mutable std::shared_mutex mutex_;
};
```

#### 5.1.5 Tier Metadata Tracking

```cpp
struct tier_metadata {
    std::string sop_instance_uid;
    storage_tier current_tier{storage_tier::hot};
    std::chrono::system_clock::time_point stored_at;
    std::optional<std::chrono::system_clock::time_point> last_accessed;
    std::size_t size_bytes{0};
    std::string study_instance_uid;
    std::string series_instance_uid;

    // Helper methods
    [[nodiscard]] auto age() const -> std::chrono::system_clock::duration;
    [[nodiscard]] auto time_since_access() const -> std::chrono::system_clock::duration;
    [[nodiscard]] auto should_migrate(const tier_policy& policy,
                                      storage_tier target_tier) const -> bool;
};
```

#### 5.1.6 Data Flow

**Store Operation:**
1. New data always stored in hot tier
2. Metadata created with current timestamp
3. Tier index updated

**Retrieve Operation:**
1. Search tiers in order: hot -> warm -> cold
2. Update `last_accessed` timestamp if `track_access_time` enabled
3. Return data transparently

**Migration Decision:**
```
if (time_since_access >= hot_to_warm) and (current_tier == hot):
    migrate to warm

if (time_since_access >= warm_to_cold) and (current_tier == warm):
    migrate to cold
```

---

## 6. HSM Migration Service

### 6.1 DES-STOR-008: HSM Migration Service Class

**Traces to:** SRS-STOR-011 (HSM Migration), FR-4.5 (HSM)

**File:** `include/pacs/storage/hsm_migration_service.hpp`, `src/storage/hsm_migration_service.cpp`

Background service for automatic tier migration with configurable scheduling.

#### 6.1.1 Service Configuration

```cpp
struct migration_service_config {
    std::chrono::seconds migration_interval{3600};   // 1 hour default
    std::size_t max_concurrent_migrations{4};
    bool auto_start{false};

    using progress_callback = std::function<void(const migration_result&)>;
    progress_callback on_cycle_complete;

    using error_callback = std::function<void(const std::string& uid, const std::string& error)>;
    error_callback on_migration_error;
};
```

#### 6.1.2 Class Interface

```cpp
class hsm_migration_service {
public:
    explicit hsm_migration_service(hsm_storage& storage,
                                   const migration_service_config& config = {});
    hsm_migration_service(hsm_storage& storage,
                          std::shared_ptr<kcenon::thread::thread_pool> thread_pool,
                          const migration_service_config& config = {});
    ~hsm_migration_service();

    // Lifecycle Management
    void start();
    void stop(bool wait_for_completion = true);
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // Manual Operations
    [[nodiscard]] auto run_migration_cycle() -> migration_result;
    void trigger_cycle();

    // Statistics and Monitoring
    [[nodiscard]] auto get_last_result() const -> std::optional<migration_result>;
    [[nodiscard]] auto get_cumulative_stats() const -> migration_result;
    [[nodiscard]] auto time_until_next_cycle() const -> std::optional<std::chrono::seconds>;
    [[nodiscard]] auto cycles_completed() const noexcept -> std::size_t;

    // Configuration
    void set_migration_interval(std::chrono::seconds interval);
    [[nodiscard]] auto get_migration_interval() const noexcept -> std::chrono::seconds;

private:
    hsm_storage& storage_;
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;
    migration_service_config config_;
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<std::size_t> cycles_count_{0};
    // ... other members
};
```

#### 6.1.3 Migration Result Tracking

```cpp
struct migration_result {
    std::size_t instances_migrated{0};
    std::size_t bytes_migrated{0};
    std::chrono::milliseconds duration{0};
    std::vector<std::string> failed_uids;
    std::size_t instances_skipped{0};

    [[nodiscard]] auto is_success() const noexcept -> bool;
    [[nodiscard]] auto total_processed() const noexcept -> std::size_t;
};
```

---

## 7. SQLite Security Storage

### 7.1 DES-STOR-009: SQLite Security Storage Class

**Traces to:** SRS-SEC-002 (Security Storage), FR-5.3 (RBAC)

**File:** `include/pacs/storage/sqlite_security_storage.hpp`, `src/storage/sqlite_security_storage.cpp`

SQLite-based storage for security entities (users, roles, permissions) with SQL injection protection.

#### 7.1.1 Class Interface

```cpp
class sqlite_security_storage : public security::security_storage_interface {
public:
    explicit sqlite_security_storage(std::string db_path);
    ~sqlite_security_storage() override;

    // User Management
    [[nodiscard]] auto create_user(const security::User& user) -> VoidResult override;
    [[nodiscard]] auto get_user(std::string_view id) -> Result<security::User> override;
    [[nodiscard]] auto get_user_by_username(std::string_view username)
        -> Result<security::User> override;
    [[nodiscard]] auto update_user(const security::User& user) -> VoidResult override;
    [[nodiscard]] auto delete_user(std::string_view id) -> VoidResult override;
    [[nodiscard]] auto get_users_by_role(security::Role role)
        -> Result<std::vector<security::User>> override;

private:
    std::string db_path_;
#ifdef PACS_WITH_DATABASE_SYSTEM
    std::shared_ptr<database::database_context> db_context_;
    std::shared_ptr<database::database_manager> db_manager_;
#else
    sqlite3* db_{nullptr};
#endif
};
```

#### 7.1.2 SQL Injection Protection

When `database_system` is available, uses parameterized queries:

```cpp
// Safe: Using database_system query builder
auto query = db_manager_->select("users")
    .where("username", "=", username)
    .build();
```

When unavailable, uses manual escaping:

```cpp
// Fallback: Manual string escaping
auto escaped = escape_string(username);
```

---

## 8. Class Diagrams

### 8.1 Cloud Storage Class Hierarchy

```
                            ┌─────────────────────┐
                            │  storage_interface  │ (Abstract)
                            └─────────┬───────────┘
                                      │
           ┌──────────────────────────┼──────────────────────────┐
           │                          │                          │
           ▼                          ▼                          ▼
┌─────────────────────┐   ┌─────────────────────┐   ┌─────────────────────┐
│     s3_storage      │   │  azure_blob_storage │   │     hsm_storage     │
│   (DES-STOR-005)    │   │   (DES-STOR-006)    │   │   (DES-STOR-007)    │
├─────────────────────┤   ├─────────────────────┤   ├─────────────────────┤
│ - config_           │   │ - config_           │   │ - hot_tier_         │
│ - client_           │   │ - client_           │   │ - warm_tier_        │
│ - index_            │   │ - index_            │   │ - cold_tier_        │
│ - mutex_            │   │ - mutex_            │   │ - metadata_index_   │
├─────────────────────┤   ├─────────────────────┤   ├─────────────────────┤
│ + store()           │   │ + store()           │   │ + store()           │
│ + retrieve()        │   │ + retrieve()        │   │ + retrieve()        │
│ + rebuild_index()   │   │ + set_access_tier() │   │ + migrate()         │
│ + is_connected()    │   │ + is_connected()    │   │ + get_tier()        │
└─────────────────────┘   └─────────────────────┘   └─────────────────────┘
                                                              │
                                                              │ uses
                                                              ▼
                                                    ┌─────────────────────┐
                                                    │hsm_migration_service│
                                                    │   (DES-STOR-008)    │
                                                    ├─────────────────────┤
                                                    │ + start()           │
                                                    │ + stop()            │
                                                    │ + trigger_cycle()   │
                                                    └─────────────────────┘
```

### 8.2 HSM Type Relationships

```
┌─────────────────────┐         ┌─────────────────────┐
│    storage_tier     │         │     tier_policy     │
│    (hsm_types)      │         │    (hsm_types)      │
├─────────────────────┤         ├─────────────────────┤
│ hot                 │         │ hot_to_warm         │
│ warm                │         │ warm_to_cold        │
│ cold                │         │ auto_migrate        │
└─────────────────────┘         │ max_instances       │
         │                      └─────────────────────┘
         │ used by                        │
         ▼                                │ configures
┌─────────────────────┐                   │
│   tier_metadata     │◄──────────────────┘
│    (hsm_types)      │
├─────────────────────┤
│ sop_instance_uid    │
│ current_tier        │
│ stored_at           │
│ last_accessed       │
│ size_bytes          │
├─────────────────────┤
│ + age()             │
│ + should_migrate()  │
└─────────────────────┘
         │
         │ aggregated in
         ▼
┌─────────────────────┐
│  migration_result   │
│    (hsm_types)      │
├─────────────────────┤
│ instances_migrated  │
│ bytes_migrated      │
│ failed_uids         │
│ duration            │
└─────────────────────┘
```

---

## 9. Sequence Diagrams

### 9.1 SEQ-STOR-001: S3 Store Operation

```
┌────────┐    ┌────────────┐    ┌─────────────┐    ┌─────────┐
│ Client │    │ s3_storage │    │ S3 SDK/Mock │    │   S3    │
└───┬────┘    └─────┬──────┘    └──────┬──────┘    └────┬────┘
    │               │                   │                │
    │ store(dataset)│                   │                │
    │──────────────►│                   │                │
    │               │                   │                │
    │               │ serialize_dataset()                │
    │               │◄─────────────────►│                │
    │               │                   │                │
    │               │ build_object_key()│                │
    │               │◄──────────────────┤                │
    │               │                   │                │
    │               │   PutObject/      │                │
    │               │   CreateMultipart │ PUT /{key}     │
    │               │──────────────────►│───────────────►│
    │               │                   │                │
    │               │                   │    200 OK      │
    │               │                   │◄───────────────│
    │               │                   │                │
    │               │ update_index()    │                │
    │               │◄──────────────────┤                │
    │               │                   │                │
    │   VoidResult  │                   │                │
    │◄──────────────│                   │                │
    │               │                   │                │
```

### 9.2 SEQ-STOR-002: HSM Retrieve Operation

```
┌────────┐    ┌─────────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
│ Client │    │ hsm_storage │    │ hot_tier │    │warm_tier │    │cold_tier │
└───┬────┘    └──────┬──────┘    └────┬─────┘    └────┬─────┘    └────┬─────┘
    │                │                │               │               │
    │ retrieve(uid)  │                │               │               │
    │───────────────►│                │               │               │
    │                │                │               │               │
    │                │ exists(uid)?   │               │               │
    │                │───────────────►│               │               │
    │                │     true       │               │               │
    │                │◄───────────────│               │               │
    │                │                │               │               │
    │                │ retrieve(uid)  │               │               │
    │                │───────────────►│               │               │
    │                │   dataset      │               │               │
    │                │◄───────────────│               │               │
    │                │                │               │               │
    │                │ update_access_time()           │               │
    │                │◄───────────────┤               │               │
    │                │                │               │               │
    │   Result<ds>   │                │               │               │
    │◄───────────────│                │               │               │
    │                │                │               │               │
```

### 9.3 SEQ-STOR-003: HSM Migration Cycle

```
┌────────────────────┐    ┌─────────────┐    ┌──────────┐    ┌──────────┐
│hsm_migration_service│    │ hsm_storage │    │ hot_tier │    │warm_tier │
└─────────┬──────────┘    └──────┬──────┘    └────┬─────┘    └────┬─────┘
          │                      │                │               │
          │ (timer expires)      │                │               │
          │                      │                │               │
          │get_migration_candidates()             │               │
          │─────────────────────►│                │               │
          │   [metadata list]    │                │               │
          │◄─────────────────────│                │               │
          │                      │                │               │
          │──┐                   │                │               │
          │  │ for each candidate│                │               │
          │◄─┘                   │                │               │
          │                      │                │               │
          │ migrate(uid, warm)   │                │               │
          │─────────────────────►│                │               │
          │                      │ retrieve(uid)  │               │
          │                      │───────────────►│               │
          │                      │   dataset      │               │
          │                      │◄───────────────│               │
          │                      │                │               │
          │                      │ store(dataset) │               │
          │                      │───────────────────────────────►│
          │                      │    VoidResult  │               │
          │                      │◄───────────────────────────────│
          │                      │                │               │
          │                      │ verify_integrity()             │
          │                      │───────────────────────────────►│
          │                      │                │               │
          │                      │ remove(uid)    │               │
          │                      │───────────────►│               │
          │                      │                │               │
          │                      │update_metadata()               │
          │                      │◄───────────────┤               │
          │                      │                │               │
          │   migration_result   │                │               │
          │◄─────────────────────│                │               │
          │                      │                │               │
          │ on_cycle_complete(result)             │               │
          │◄─────────────────────┤                │               │
          │                      │                │               │
```

---

## 10. Configuration Examples

### 10.1 AWS S3 Configuration

```cpp
#include <pacs/storage/s3_storage.hpp>

// AWS S3 production configuration
pacs::storage::cloud_storage_config s3_config;
s3_config.bucket_name = "my-hospital-dicom-archive";
s3_config.region = "us-east-1";
s3_config.access_key_id = std::getenv("AWS_ACCESS_KEY_ID");
s3_config.secret_access_key = std::getenv("AWS_SECRET_ACCESS_KEY");
s3_config.enable_encryption = true;  // SSE-S3
s3_config.storage_class = "INTELLIGENT_TIERING";

auto s3 = std::make_unique<pacs::storage::s3_storage>(s3_config);
```

```cpp
// MinIO local testing configuration
pacs::storage::cloud_storage_config minio_config;
minio_config.bucket_name = "dicom-test";
minio_config.region = "us-east-1";  // MinIO ignores region
minio_config.access_key_id = "minioadmin";
minio_config.secret_access_key = "minioadmin";
minio_config.endpoint_url = "http://localhost:9000";

auto minio = std::make_unique<pacs::storage::s3_storage>(minio_config);
```

### 10.2 Azure Blob Storage Configuration

```cpp
#include <pacs/storage/azure_blob_storage.hpp>

// Azure production configuration
pacs::storage::azure_storage_config azure_config;
azure_config.container_name = "dicom-container";
azure_config.connection_string =
    "DefaultEndpointsProtocol=https;"
    "AccountName=myhospitalaccount;"
    "AccountKey=<base64-encoded-key>;"
    "EndpointSuffix=core.windows.net";
azure_config.access_tier = "Hot";

auto azure = std::make_unique<pacs::storage::azure_blob_storage>(azure_config);
```

```cpp
// Azurite emulator configuration
pacs::storage::azure_storage_config azurite_config;
azurite_config.container_name = "dicom-test";
azurite_config.connection_string =
    "DefaultEndpointsProtocol=http;"
    "AccountName=devstoreaccount1;"
    "AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/"
    "K1SZFPTOtr/KBHBeksoGMGw==;"
    "BlobEndpoint=http://127.0.0.1:10000/devstoreaccount1";
azurite_config.endpoint_url = "http://127.0.0.1:10000/devstoreaccount1";

auto azurite = std::make_unique<pacs::storage::azure_blob_storage>(azurite_config);
```

### 10.3 HSM Configuration

```cpp
#include <pacs/storage/hsm_storage.hpp>
#include <pacs/storage/file_storage.hpp>
#include <pacs/storage/s3_storage.hpp>

// Create tier backends
auto hot = std::make_unique<pacs::storage::file_storage>(
    pacs::storage::file_storage_config{
        .root_path = "/fast-ssd/dicom",
        .hierarchy = storage_hierarchy::study_series_instance
    });

auto warm = std::make_unique<pacs::storage::file_storage>(
    pacs::storage::file_storage_config{
        .root_path = "/slow-hdd/dicom"
    });

auto cold = std::make_unique<pacs::storage::s3_storage>(s3_config);

// Configure HSM
pacs::storage::hsm_storage_config hsm_config;
hsm_config.policy.hot_to_warm = std::chrono::days{30};    // 30 days
hsm_config.policy.warm_to_cold = std::chrono::days{365};  // 1 year
hsm_config.policy.auto_migrate = true;
hsm_config.policy.max_bytes_per_cycle = 50ULL * 1024 * 1024 * 1024;  // 50GB
hsm_config.verify_after_migration = true;
hsm_config.delete_after_migration = true;

// Create HSM storage
auto hsm = std::make_unique<pacs::storage::hsm_storage>(
    std::move(hot), std::move(warm), std::move(cold), hsm_config);
```

### 10.4 Migration Service Configuration

```cpp
#include <pacs/storage/hsm_migration_service.hpp>

pacs::storage::migration_service_config migration_config;
migration_config.migration_interval = std::chrono::hours{1};
migration_config.max_concurrent_migrations = 4;
migration_config.auto_start = true;

migration_config.on_cycle_complete = [](const pacs::storage::migration_result& result) {
    std::cout << "Migration cycle completed:\n"
              << "  Migrated: " << result.instances_migrated << " instances\n"
              << "  Bytes: " << result.bytes_migrated << "\n"
              << "  Duration: " << result.duration.count() << "ms\n"
              << "  Failed: " << result.failed_uids.size() << "\n";
};

migration_config.on_migration_error = [](const std::string& uid, const std::string& error) {
    std::cerr << "Migration failed for " << uid << ": " << error << "\n";
};

pacs::storage::hsm_migration_service service{*hsm, migration_config};
// Service auto-starts if configured
```

---

## 11. Traceability

### 11.1 Design Element Summary

| Design ID | Element | Header | Source | Test |
|-----------|---------|--------|--------|------|
| DES-STOR-005 | s3_storage | s3_storage.hpp | s3_storage.cpp | s3_storage_test.cpp |
| DES-STOR-006 | azure_blob_storage | azure_blob_storage.hpp | azure_blob_storage.cpp | azure_blob_storage_test.cpp |
| DES-STOR-007 | hsm_storage | hsm_storage.hpp | hsm_storage.cpp | hsm_storage_test.cpp |
| DES-STOR-008 | hsm_migration_service | hsm_migration_service.hpp | hsm_migration_service.cpp | - |
| DES-STOR-009 | sqlite_security_storage | sqlite_security_storage.hpp | sqlite_security_storage.cpp | sqlite_security_storage_test.cpp |
| - | hsm_types | hsm_types.hpp | - | - |

### 11.2 Requirements Traceability

| SRS ID | SRS Description | Design ID(s) |
|--------|-----------------|--------------|
| SRS-STOR-003 | Cloud Storage Backend | DES-STOR-005 |
| SRS-STOR-004 | Azure Storage Backend | DES-STOR-006 |
| SRS-STOR-010 | Hierarchical Storage | DES-STOR-007 |
| SRS-STOR-011 | HSM Migration Service | DES-STOR-008 |
| SRS-SEC-002 | Security Storage | DES-STOR-009 |

### 11.3 PRD Traceability

| PRD ID | PRD Description | SRS ID | Design ID(s) |
|--------|-----------------|--------|--------------|
| FR-4.2 | Cloud Storage | SRS-STOR-003, SRS-STOR-004 | DES-STOR-005, DES-STOR-006 |
| FR-4.5 | Hierarchical Storage Management | SRS-STOR-010, SRS-STOR-011 | DES-STOR-007, DES-STOR-008 |
| FR-5.3 | RBAC Security Storage | SRS-SEC-002 | DES-STOR-009 |

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-01-04 | kcenon@naver.com | Initial version |
