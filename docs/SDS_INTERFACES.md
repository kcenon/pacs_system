# SDS - Interface Specifications

> **Version:** 0.1.2.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2025-12-07

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Public API Interfaces](#2-public-api-interfaces)
- [3. Internal Module Interfaces](#3-internal-module-interfaces)
- [4. External System Interfaces](#4-external-system-interfaces)
- [5. Ecosystem Integration Interfaces](#5-ecosystem-integration-interfaces)
- [6. Error Handling Interface](#6-error-handling-interface)
- [7. Security Interfaces](#7-security-interfaces)
- [8. Web/REST API Interfaces](#8-webrest-api-interfaces)

---

## 1. Overview

### 1.1 Interface Categories

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          Interface Architecture                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                       External Interfaces                                ││
│  │                                                                          ││
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  ││
│  │  │ DICOM SCU    │  │ DICOM SCP    │  │ Management   │                  ││
│  │  │ (Modalities) │  │ (Viewers)    │  │ CLI/API      │                  ││
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                        Public API Layer                                  ││
│  │                                                                          ││
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐              ││
│  │  │storage_scp│ │ query_scp │ │worklist_  │ │ mpps_scp  │              ││
│  │  │           │ │           │ │    scp    │ │           │              ││
│  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘              ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                      Internal Module Interfaces                          ││
│  │                                                                          ││
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐              ││
│  │  │   core    │ │ encoding  │ │  network  │ │  storage  │              ││
│  │  │           │ │           │ │           │ │           │              ││
│  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘              ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                    Ecosystem Integration Layer                           ││
│  │                                                                          ││
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐              ││
│  │  │ container │ │  network  │ │  thread   │ │  logger   │              ││
│  │  │  adapter  │ │  adapter  │ │  adapter  │ │  adapter  │              ││
│  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘              ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Public API Interfaces

### INT-API-001: storage_scp Interface

**Traces to:** DES-SVC-002, SRS-SVC-002

```cpp
namespace pacs::services {

/**
 * @brief Storage SCP public interface
 *
 * Provides C-STORE reception capabilities.
 *
 * @usage
 * @code
 * storage_scp_config config;
 * config.ae_title = "MY_PACS";
 * config.port = 11112;
 *
 * storage_scp server(config);
 * server.set_handler([](const dicom_dataset& ds, const std::string& ae) {
 *     // Process received image
 *     return storage_status::success;
 * });
 *
 * server.start();
 * server.wait_for_shutdown();
 * @endcode
 */
class storage_scp {
public:
    // ═══════════════════════════════════════════════════════════════
    // Construction
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Construct Storage SCP with configuration
     * @param config Server configuration
     * @throws std::invalid_argument if config is invalid
     */
    explicit storage_scp(const storage_scp_config& config);

    // ═══════════════════════════════════════════════════════════════
    // Handler Registration
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Set storage completion handler
     * @param handler Callback for each received image
     *
     * Handler signature:
     *   storage_status(const dicom_dataset& dataset,
     *                  const std::string& calling_ae,
     *                  const std::string& sop_class_uid,
     *                  const std::string& sop_instance_uid)
     */
    void set_handler(storage_handler handler);

    /**
     * @brief Set pre-store validation handler
     * @param handler Returns true to accept, false to reject
     */
    void set_pre_store_handler(
        std::function<bool(const dicom_dataset&)> handler);

    // ═══════════════════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Start accepting connections
     * @return Result indicating success or failure
     */
    [[nodiscard]] common::Result<void> start();

    /**
     * @brief Stop accepting new connections
     * @note Waits for active associations to complete
     */
    void stop();

    /**
     * @brief Block until server shutdown
     */
    void wait_for_shutdown();

    // ═══════════════════════════════════════════════════════════════
    // Status
    // ═══════════════════════════════════════════════════════════════

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] size_t active_associations() const noexcept;
    [[nodiscard]] statistics get_statistics() const;
};

} // namespace pacs::services
```

### INT-API-002: query_scp Interface

**Traces to:** DES-SVC-004, SRS-SVC-004

```cpp
namespace pacs::services {

/**
 * @brief Query SCP public interface
 *
 * Provides C-FIND query capabilities at Patient/Study/Series/Image levels.
 */
class query_scp {
public:
    /**
     * @brief Construct Query SCP with configuration
     */
    explicit query_scp(const query_scp_config& config);

    /**
     * @brief Set custom query handler
     * @param handler Callback returning matching datasets
     *
     * Handler signature:
     *   std::vector<dicom_dataset>(
     *       query_level level,
     *       const dicom_dataset& query_keys,
     *       const std::string& calling_ae)
     */
    void set_handler(query_handler handler);

    /**
     * @brief Start query service
     */
    [[nodiscard]] common::Result<void> start();

    /**
     * @brief Stop query service
     */
    void stop();

    [[nodiscard]] bool is_running() const noexcept;
};

} // namespace pacs::services
```

### INT-API-003: storage_interface

**Traces to:** DES-STOR-001, SRS-STOR-001

```cpp
namespace pacs::storage {

/**
 * @brief Abstract storage backend interface
 *
 * Implementations must be thread-safe.
 */
class storage_interface {
public:
    virtual ~storage_interface() = default;

    // ═══════════════════════════════════════════════════════════════
    // CRUD Operations
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Store a DICOM dataset
     * @param dataset Dataset to store
     * @return Success or error (e.g., duplicate, storage full)
     */
    [[nodiscard]] virtual common::Result<void> store(
        const core::dicom_dataset& dataset) = 0;

    /**
     * @brief Retrieve a DICOM dataset by SOP Instance UID
     * @param sop_instance_uid Unique identifier
     * @return Dataset or error (e.g., not found)
     */
    [[nodiscard]] virtual common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid) = 0;

    /**
     * @brief Delete a stored instance
     * @param sop_instance_uid Unique identifier
     * @return Success or error
     */
    [[nodiscard]] virtual common::Result<void> remove(
        const std::string& sop_instance_uid) = 0;

    /**
     * @brief Check if instance exists
     * @param sop_instance_uid Unique identifier
     * @return true if exists
     */
    [[nodiscard]] virtual bool exists(
        const std::string& sop_instance_uid) = 0;

    // ═══════════════════════════════════════════════════════════════
    // Query Operations
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Find instances matching query
     * @param query Query keys with matching values
     * @return Vector of matching datasets (may be empty)
     */
    [[nodiscard]] virtual common::Result<std::vector<core::dicom_dataset>>
        find(const core::dicom_dataset& query) = 0;
};

} // namespace pacs::storage
```

---

## 3. Internal Module Interfaces

### INT-MOD-001: dicom_dataset Interface

**Traces to:** DES-CORE-003, SRS-CORE-003

```cpp
namespace pacs::core {

/**
 * @brief DICOM Dataset - ordered collection of data elements
 *
 * Thread-safety: NOT thread-safe. External synchronization required.
 */
class dicom_dataset {
public:
    // ═══════════════════════════════════════════════════════════════
    // Element Access (Read)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Check if tag exists
     * @param tag DICOM tag to check
     * @return true if element with tag exists
     */
    [[nodiscard]] bool contains(dicom_tag tag) const noexcept;

    /**
     * @brief Get element by tag (nullable)
     * @param tag DICOM tag
     * @return Pointer to element or nullptr if not found
     */
    [[nodiscard]] const dicom_element* get(dicom_tag tag) const;

    /**
     * @brief Get string value with default
     * @param tag DICOM tag
     * @param default_value Value if not found
     * @return String value or default
     */
    [[nodiscard]] std::string get_string(
        dicom_tag tag,
        std::string_view default_value = "") const;

    /**
     * @brief Get numeric value with default
     * @tparam T Numeric type (int16_t, uint16_t, int32_t, uint32_t, float, double)
     * @param tag DICOM tag
     * @param default_value Value if not found or conversion fails
     * @return Numeric value or default
     */
    template<typename T>
    [[nodiscard]] T get_numeric(dicom_tag tag, T default_value = T{}) const;

    // ═══════════════════════════════════════════════════════════════
    // Element Modification (Write)
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Set string value (auto VR lookup)
     * @param tag DICOM tag
     * @param value String value
     * @throws std::runtime_error if tag not in dictionary
     */
    void set_string(dicom_tag tag, std::string_view value);

    /**
     * @brief Set numeric value (auto VR lookup)
     * @tparam T Numeric type
     * @param tag DICOM tag
     * @param value Numeric value
     */
    template<typename T>
    void set_numeric(dicom_tag tag, T value);

    /**
     * @brief Add or replace element
     * @param element Element to add
     */
    void add(dicom_element element);

    /**
     * @brief Remove element by tag
     * @param tag DICOM tag
     */
    void remove(dicom_tag tag);

    // ═══════════════════════════════════════════════════════════════
    // Iteration
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Iterate elements in tag order
     *
     * @code
     * for (const auto& [tag, element] : dataset) {
     *     std::cout << tag.to_string() << std::endl;
     * }
     * @endcode
     */
    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    // ═══════════════════════════════════════════════════════════════
    // Serialization
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Serialize to bytes
     * @param ts Transfer syntax for encoding
     * @return Binary representation
     */
    [[nodiscard]] std::vector<uint8_t> serialize(
        const transfer_syntax& ts) const;

    /**
     * @brief Deserialize from bytes
     * @param data Binary data
     * @param ts Transfer syntax for decoding
     * @return Dataset or error
     */
    [[nodiscard]] static common::Result<dicom_dataset> deserialize(
        std::span<const uint8_t> data,
        const transfer_syntax& ts);
};

} // namespace pacs::core
```

### INT-MOD-002: association Interface

**Traces to:** DES-NET-004, SRS-NET-003

```cpp
namespace pacs::network {

/**
 * @brief DICOM Association management
 *
 * Represents a single DICOM association (connection).
 * Move-only semantics - owns network resources.
 */
class association {
public:
    // ═══════════════════════════════════════════════════════════════
    // Factory Methods
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Connect as SCU
     * @param host Remote host address
     * @param port Remote port
     * @param config Association configuration
     * @param timeout Connection timeout
     * @return Established association or error
     */
    [[nodiscard]] static common::Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config,
        std::chrono::milliseconds timeout = std::chrono::seconds{30});

    // ═══════════════════════════════════════════════════════════════
    // State
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Get current state
     * @return Association state (idle, established, released, aborted)
     */
    [[nodiscard]] association_state state() const noexcept;

    /**
     * @brief Check if association is established
     * @return true if ready for DIMSE operations
     */
    [[nodiscard]] bool is_established() const noexcept;

    // ═══════════════════════════════════════════════════════════════
    // Presentation Context
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Check if abstract syntax was accepted
     * @param abstract_syntax SOP Class UID
     * @return true if accepted
     */
    [[nodiscard]] bool has_accepted_context(
        std::string_view abstract_syntax) const;

    /**
     * @brief Get accepted presentation context ID
     * @param abstract_syntax SOP Class UID
     * @return Context ID or nullopt if not accepted
     */
    [[nodiscard]] std::optional<uint8_t> accepted_context_id(
        std::string_view abstract_syntax) const;

    // ═══════════════════════════════════════════════════════════════
    // DIMSE Operations
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Send DIMSE message
     * @param context_id Presentation context ID
     * @param message DIMSE message to send
     * @return Success or error (network, timeout)
     */
    [[nodiscard]] common::Result<void> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& message);

    /**
     * @brief Receive DIMSE message
     * @param timeout Receive timeout
     * @return Received message or error
     */
    [[nodiscard]] common::Result<dimse::dimse_message> receive_dimse(
        std::chrono::milliseconds timeout = std::chrono::seconds{30});

    // ═══════════════════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Graceful release
     * @return Success or error
     */
    [[nodiscard]] common::Result<void> release();

    /**
     * @brief Immediate abort
     * @param source Abort source (0=Service User, 2=Service Provider)
     * @param reason Abort reason code
     */
    void abort(uint8_t source = 0, uint8_t reason = 0);
};

} // namespace pacs::network
```

---

## 4. External System Interfaces

### INT-EXT-001: DICOM Network Interface

**Traces to:** FR-2.x, SRS-NET-xxx

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    DICOM Network Protocol Interface                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Protocol: DICOM Upper Layer (PS3.8)                                        │
│  Transport: TCP/IP (TLS optional)                                           │
│  Default Port: 11112                                                         │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Message Types                                                            ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  Association:                                                            ││
│  │    • A-ASSOCIATE-RQ  →  Initiate association                            ││
│  │    • A-ASSOCIATE-AC  ←  Accept association                              ││
│  │    • A-ASSOCIATE-RJ  ←  Reject association                              ││
│  │    • A-RELEASE-RQ    ↔  Request release                                 ││
│  │    • A-RELEASE-RP    ↔  Confirm release                                 ││
│  │    • A-ABORT         ↔  Abort association                               ││
│  │                                                                          ││
│  │  DIMSE (inside P-DATA-TF):                                              ││
│  │    • C-ECHO-RQ/RSP   ↔  Verification                                    ││
│  │    • C-STORE-RQ/RSP  →  Store image                                     ││
│  │    • C-FIND-RQ/RSP   →  Query                                           ││
│  │    • C-MOVE-RQ/RSP   →  Retrieve (sub-operation)                        ││
│  │    • C-GET-RQ/RSP    →  Get (retrieve inline)                           ││
│  │    • N-CREATE-RQ/RSP →  Create MPPS                                     ││
│  │    • N-SET-RQ/RSP    →  Update MPPS                                     ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Supported SOP Classes                                                    ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  Storage SOP Classes:                                                    ││
│  │    • CT Image Storage                    1.2.840.10008.5.1.4.1.1.2      ││
│  │    • MR Image Storage                    1.2.840.10008.5.1.4.1.1.4      ││
│  │    • CR Image Storage                    1.2.840.10008.5.1.4.1.1.1      ││
│  │    • (All standard storage classes)                                      ││
│  │                                                                          ││
│  │  Query/Retrieve SOP Classes:                                             ││
│  │    • Patient Root Q/R Find              1.2.840.10008.5.1.4.1.2.1.1     ││
│  │    • Patient Root Q/R Move              1.2.840.10008.5.1.4.1.2.1.2     ││
│  │    • Study Root Q/R Find                1.2.840.10008.5.1.4.1.2.2.1     ││
│  │    • Study Root Q/R Move                1.2.840.10008.5.1.4.1.2.2.2     ││
│  │                                                                          ││
│  │  Workflow SOP Classes:                                                   ││
│  │    • Modality Worklist                  1.2.840.10008.5.1.4.31          ││
│  │    • MPPS                               1.2.840.10008.3.1.2.3.3         ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Transfer Syntaxes                                                        ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  • Implicit VR Little Endian            1.2.840.10008.1.2              ││
│  │  • Explicit VR Little Endian            1.2.840.10008.1.2.1            ││
│  │  • Explicit VR Big Endian (retired)     1.2.840.10008.1.2.2            ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### INT-EXT-002: Configuration Interface

**Purpose:** External configuration file format

```yaml
# pacs_config.yaml - PACS System Configuration

server:
  ae_title: "MY_PACS"
  port: 11112
  max_associations: 20
  max_pdu_size: 16384

storage:
  path: "/data/dicom"
  naming_scheme: "uid_hierarchical"  # uid_hierarchical | date_hierarchical | flat
  duplicate_policy: "reject"          # reject | replace | ignore

database:
  path: "/data/pacs_index.db"
  cache_size_mb: 64
  wal_mode: true

security:
  tls_enabled: false
  cert_path: "/etc/pacs/cert.pem"
  key_path: "/etc/pacs/key.pem"
  ae_whitelist:
    - "CT_01"
    - "MR_01"
    - "VIEWER_*"

logging:
  level: "info"  # trace | debug | info | warn | error
  file: "/var/log/pacs/pacs.log"
  audit_file: "/var/log/pacs/audit.log"

worklist:
  enabled: true
  source: "database"  # database | external

mpps:
  enabled: true
  forward_to: []  # List of AE titles to forward MPPS
```

---

## 5. Ecosystem Integration Interfaces

### INT-ECO-001: container_adapter Interface

**Traces to:** DES-INT-001, IR-1

```cpp
namespace pacs::integration {

/**
 * @brief Adapter between DICOM VR and container_system values
 */
class container_adapter {
public:
    // ═══════════════════════════════════════════════════════════════
    // VR to Container Mapping
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Convert DICOM element to container value
     * @param element DICOM data element
     * @return Container system value
     *
     * Mapping:
     *   String VRs → std::string
     *   Integer VRs → int64_t
     *   Float VRs → double
     *   Binary VRs → std::vector<uint8_t>
     *   Sequence VR → std::vector<value_container>
     */
    [[nodiscard]] static container::value to_container_value(
        const core::dicom_element& element);

    /**
     * @brief Convert container value to DICOM element
     * @param tag Target DICOM tag
     * @param vr Value Representation
     * @param val Container value
     * @return DICOM data element
     */
    [[nodiscard]] static core::dicom_element from_container_value(
        core::dicom_tag tag,
        encoding::vr_type vr,
        const container::value& val);

    // ═══════════════════════════════════════════════════════════════
    // Bulk Operations
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Serialize entire dataset to container
     */
    [[nodiscard]] static container::value_container serialize_dataset(
        const core::dicom_dataset& dataset);

    /**
     * @brief Deserialize container to dataset
     */
    [[nodiscard]] static common::Result<core::dicom_dataset>
        deserialize_dataset(const container::value_container& container);
};

} // namespace pacs::integration
```

### INT-ECO-002: network_adapter Interface

**Traces to:** DES-INT-002, IR-2

```cpp
namespace pacs::integration {

/**
 * @brief Adapter for network_system TCP/TLS operations
 */
class network_adapter {
public:
    // ═══════════════════════════════════════════════════════════════
    // Server Operations
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Create DICOM server using network_system
     * @param config Server configuration
     * @return Configured server instance
     */
    [[nodiscard]] static std::unique_ptr<network::dicom_server>
        create_server(const network::server_config& config);

    // ═══════════════════════════════════════════════════════════════
    // Client Operations
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Connect to remote DICOM server
     * @param host Remote host
     * @param port Remote port
     * @param timeout Connection timeout
     * @return Connected session or error
     */
    [[nodiscard]] static common::Result<
        network_system::messaging_session_ptr> connect(
            const std::string& host,
            uint16_t port,
            std::chrono::milliseconds timeout);

    // ═══════════════════════════════════════════════════════════════
    // TLS Configuration
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Configure TLS for DICOM connection
     */
    static void configure_tls(
        network_system::tls_config& config,
        const std::filesystem::path& cert_path,
        const std::filesystem::path& key_path,
        const std::filesystem::path& ca_path);
};

} // namespace pacs::integration
```

### INT-ECO-003: thread_adapter Interface

**Traces to:** DES-INT-003, IR-3

```cpp
namespace pacs::integration {

/**
 * @brief Adapter for thread_system job processing
 */
class thread_adapter {
public:
    /**
     * @brief Get shared thread pool for DIMSE processing
     * @return Reference to thread pool
     */
    [[nodiscard]] static thread_system::thread_pool& get_worker_pool();

    /**
     * @brief Submit job to worker pool
     * @param job Job to execute
     * @return Future for result
     */
    template<typename F>
    [[nodiscard]] static auto submit(F&& job)
        -> std::future<std::invoke_result_t<F>>;

    /**
     * @brief Get storage I/O thread pool
     * @return Reference to storage thread pool
     */
    [[nodiscard]] static thread_system::thread_pool& get_storage_pool();
};

} // namespace pacs::integration
```

---

## 6. Error Handling Interface

### INT-ERR-001: Error Codes Interface

**Traces to:** Common error handling requirements

```cpp
namespace pacs::error_codes {

// ═══════════════════════════════════════════════════════════════════════════
// DICOM Parsing Errors (-800 to -819)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int INVALID_DICOM_FILE = -800;
constexpr int INVALID_VR = -801;
constexpr int MISSING_REQUIRED_TAG = -802;
constexpr int INVALID_TRANSFER_SYNTAX = -803;
constexpr int INVALID_DATA_ELEMENT = -804;
constexpr int SEQUENCE_ENCODING_ERROR = -805;
constexpr int INVALID_TAG = -806;
constexpr int VALUE_LENGTH_EXCEEDED = -807;
constexpr int INVALID_UID_FORMAT = -808;

// ═══════════════════════════════════════════════════════════════════════════
// Association Errors (-820 to -839)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int ASSOCIATION_REJECTED = -820;
constexpr int ASSOCIATION_ABORTED = -821;
constexpr int NO_PRESENTATION_CONTEXT = -822;
constexpr int INVALID_PDU = -823;
constexpr int PDU_TOO_LARGE = -824;
constexpr int ASSOCIATION_TIMEOUT = -825;
constexpr int CALLED_AE_NOT_RECOGNIZED = -826;
constexpr int CALLING_AE_NOT_ALLOWED = -827;
constexpr int TRANSPORT_ERROR = -828;

// ═══════════════════════════════════════════════════════════════════════════
// DIMSE Errors (-840 to -859)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int DIMSE_FAILURE = -840;
constexpr int DIMSE_TIMEOUT = -841;
constexpr int DIMSE_INVALID_RESPONSE = -842;
constexpr int DIMSE_CANCELLED = -843;
constexpr int DIMSE_STATUS_ERROR = -844;
constexpr int DIMSE_MISSING_DATASET = -845;
constexpr int DIMSE_UNEXPECTED_MESSAGE = -846;

// ═══════════════════════════════════════════════════════════════════════════
// Storage Errors (-860 to -879)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int STORAGE_FAILED = -860;
constexpr int DUPLICATE_SOP_INSTANCE = -861;
constexpr int INVALID_SOP_CLASS = -862;
constexpr int STORAGE_FULL = -863;
constexpr int FILE_WRITE_ERROR = -864;
constexpr int FILE_READ_ERROR = -865;
constexpr int INDEX_UPDATE_ERROR = -866;
constexpr int INSTANCE_NOT_FOUND = -867;

// ═══════════════════════════════════════════════════════════════════════════
// Query Errors (-880 to -899)
// ═══════════════════════════════════════════════════════════════════════════

constexpr int QUERY_FAILED = -880;
constexpr int NO_MATCHES_FOUND = -881;
constexpr int TOO_MANY_MATCHES = -882;
constexpr int INVALID_QUERY_LEVEL = -883;
constexpr int DATABASE_ERROR = -884;
constexpr int INVALID_QUERY_KEY = -885;

/**
 * @brief Get human-readable error message
 * @param code Error code
 * @return Error description
 */
[[nodiscard]] std::string_view message(int code);

/**
 * @brief Get error category name
 * @param code Error code
 * @return Category (Parsing, Association, DIMSE, Storage, Query)
 */
[[nodiscard]] std::string_view category(int code);

} // namespace pacs::error_codes
```

---

## 7. Security Interfaces

> **Reference:** [SDS_SECURITY.md](SDS_SECURITY.md) - Complete Security Module Design

### INT-SEC-001: Access Control Manager Interface

**Traces to:** DES-SEC-005, SRS-SEC-002

```cpp
namespace pacs::security {

/**
 * @brief Access Control Manager interface for DICOM operations
 *
 * Provides RBAC-based authorization for DICOM network operations.
 *
 * @usage
 * @code
 * access_control_manager acm;
 *
 * // Register AE Title to user mapping
 * acm.register_ae_title("MODALITY1", "tech_user_001");
 *
 * // Get user context for incoming connection
 * auto ctx = acm.get_context_for_ae("MODALITY1", session_id);
 *
 * // Check if operation is allowed
 * auto result = acm.check_dicom_operation(ctx, DicomOperation::CStore);
 * if (result.allowed) {
 *     // Proceed with C-STORE
 * } else {
 *     // Reject: result.reason contains denial reason
 * }
 * @endcode
 */
class access_control_manager {
public:
    // ═══════════════════════════════════════════════════════════════
    // Permission Checks
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Check DICOM operation authorization
     * @param ctx User context for the session
     * @param op DICOM operation to authorize
     * @return AccessCheckResult with allowed status and reason
     */
    [[nodiscard]] AccessCheckResult check_dicom_operation(
        const user_context& ctx, DicomOperation op) const;

    /**
     * @brief Get user context for AE Title
     * @param ae_title Source AE Title
     * @param session_id Session identifier
     * @return User context (anonymous if AE not registered)
     */
    [[nodiscard]] user_context get_context_for_ae(
        std::string_view ae_title,
        const std::string& session_id) const;

    // ═══════════════════════════════════════════════════════════════
    // AE Title Management
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Register AE Title to user mapping
     * @param ae_title The AE Title
     * @param user_id The user ID to associate
     */
    void register_ae_title(std::string_view ae_title, std::string_view user_id);

    /**
     * @brief Unregister AE Title
     * @param ae_title The AE Title to remove
     */
    void unregister_ae_title(std::string_view ae_title);

    // ═══════════════════════════════════════════════════════════════
    // Audit Integration
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Set audit callback for access attempts
     * @param callback Function called on each access check
     */
    void set_audit_callback(AccessAuditCallback callback);
};

} // namespace pacs::security
```

---

### INT-SEC-002: Anonymizer Interface

**Traces to:** DES-SEC-009, SRS-SEC-001

```cpp
namespace pacs::security {

/**
 * @brief DICOM Anonymizer interface for de-identification
 *
 * Implements PS3.15 Annex E anonymization profiles.
 *
 * @usage
 * @code
 * // Basic anonymization
 * anonymizer anon(anonymization_profile::hipaa_safe_harbor);
 * auto result = anon.anonymize(dataset);
 *
 * // Longitudinal study with consistent UIDs
 * uid_mapping mapping;
 * anonymizer anon(anonymization_profile::retain_longitudinal);
 * anon.set_date_offset(std::chrono::days{-100});
 *
 * for (auto& ds : study_datasets) {
 *     anon.anonymize_with_mapping(ds, mapping);
 * }
 *
 * // Save mapping for future use
 * std::string json = mapping.to_json();
 * @endcode
 */
class anonymizer {
public:
    // ═══════════════════════════════════════════════════════════════
    // Construction
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Construct with anonymization profile
     * @param profile The de-identification profile to use
     */
    explicit anonymizer(
        anonymization_profile profile = anonymization_profile::basic);

    // ═══════════════════════════════════════════════════════════════
    // Anonymization Operations
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Anonymize a dataset
     * @param dataset Dataset to anonymize (modified in place)
     * @return Result with anonymization report
     */
    [[nodiscard]] auto anonymize(core::dicom_dataset& dataset)
        -> kcenon::common::Result<anonymization_report>;

    /**
     * @brief Anonymize with consistent UID mapping
     * @param dataset Dataset to anonymize
     * @param mapping UID mapping for consistent transformation
     * @return Result with anonymization report
     */
    [[nodiscard]] auto anonymize_with_mapping(
        core::dicom_dataset& dataset,
        uid_mapping& mapping)
        -> kcenon::common::Result<anonymization_report>;

    // ═══════════════════════════════════════════════════════════════
    // Configuration
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Add custom tag action
     * @param tag DICOM tag
     * @param config Action configuration
     */
    void add_tag_action(core::dicom_tag tag, tag_action_config config);

    /**
     * @brief Set date offset for longitudinal consistency
     * @param offset Number of days to shift dates
     */
    void set_date_offset(std::chrono::days offset);

    /**
     * @brief Set hash salt for hash operations
     * @param salt Salt value
     */
    void set_hash_salt(std::string salt);
};

} // namespace pacs::security
```

---

### INT-SEC-003: Digital Signature Interface

**Traces to:** DES-SEC-012, SRS-SEC-001

```cpp
namespace pacs::security {

/**
 * @brief DICOM Digital Signature interface
 *
 * Static methods for signing and verifying DICOM datasets
 * per PS3.15 Section C.
 *
 * @usage
 * @code
 * // Sign a dataset
 * auto cert = certificate::load_from_pem("signer.pem").value();
 * auto key = private_key::load_from_pem("signer_key.pem").value();
 *
 * auto result = digital_signature::sign(dataset, cert, key);
 * if (result.is_ok()) {
 *     // Dataset now contains Digital Signature Sequence
 * }
 *
 * // Verify signature
 * auto status = digital_signature::verify(dataset);
 * switch (status.value()) {
 *     case signature_status::valid:
 *         // Signature verified
 *         break;
 *     case signature_status::invalid:
 *         // Data tampered
 *         break;
 *     case signature_status::expired:
 *         // Certificate expired
 *         break;
 * }
 * @endcode
 */
class digital_signature {
public:
    // ═══════════════════════════════════════════════════════════════
    // Signature Creation
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Sign a DICOM dataset
     * @param dataset Dataset to sign (modified in place)
     * @param cert Signer's X.509 certificate
     * @param key Signer's private key
     * @param algo Signature algorithm (default: RSA-SHA256)
     * @return Result indicating success or failure
     */
    [[nodiscard]] static auto sign(
        core::dicom_dataset& dataset,
        const certificate& cert,
        const private_key& key,
        signature_algorithm algo = signature_algorithm::rsa_sha256
    ) -> kcenon::common::VoidResult;

    // ═══════════════════════════════════════════════════════════════
    // Signature Verification
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Verify digital signatures in dataset
     * @param dataset Dataset to verify
     * @return Result with signature status
     */
    [[nodiscard]] static auto verify(const core::dicom_dataset& dataset)
        -> kcenon::common::Result<signature_status>;

    /**
     * @brief Verify with trusted certificate store
     * @param dataset Dataset to verify
     * @param trusted_certs Trusted certificates
     * @return Result with signature status
     */
    [[nodiscard]] static auto verify_with_trust(
        const core::dicom_dataset& dataset,
        const std::vector<certificate>& trusted_certs
    ) -> kcenon::common::Result<signature_status>;

    // ═══════════════════════════════════════════════════════════════
    // Signature Information
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Check if dataset has digital signature
     * @param dataset Dataset to check
     * @return true if signature present
     */
    [[nodiscard]] static bool has_signature(
        const core::dicom_dataset& dataset);

    /**
     * @brief Get signature information
     * @param dataset Dataset to inspect
     * @return Optional signature info
     */
    [[nodiscard]] static auto get_signature_info(
        const core::dicom_dataset& dataset
    ) -> std::optional<signature_info>;
};

} // namespace pacs::security
```

---

### INT-SEC-004: Security Storage Interface

**Traces to:** DES-SEC-013, SRS-SEC-002

```cpp
namespace pacs::security {

/**
 * @brief Abstract interface for security data persistence
 *
 * Provides storage operations for RBAC entities (Users, Roles).
 *
 * @usage
 * @code
 * // Create SQLite implementation
 * auto storage = std::make_shared<sqlite_security_storage>("security.db");
 *
 * // Inject into access control manager
 * access_control_manager acm;
 * acm.set_storage(storage);
 *
 * // User operations now persist to database
 * User user{"user1", "technologist", {Role::Technologist}, true};
 * acm.create_user(user);
 * @endcode
 */
class security_storage_interface {
public:
    virtual ~security_storage_interface() = default;

    // ═══════════════════════════════════════════════════════════════
    // User Management
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Create a new user
     * @param user User to create
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual auto create_user(const User& user)
        -> kcenon::common::VoidResult = 0;

    /**
     * @brief Get user by ID
     * @param id User ID
     * @return Result containing user or error
     */
    [[nodiscard]] virtual auto get_user(std::string_view id)
        -> kcenon::common::Result<User> = 0;

    /**
     * @brief Get user by username
     * @param username Username
     * @return Result containing user or error
     */
    [[nodiscard]] virtual auto get_user_by_username(std::string_view username)
        -> kcenon::common::Result<User> = 0;

    /**
     * @brief Update existing user
     * @param user Updated user data
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual auto update_user(const User& user)
        -> kcenon::common::VoidResult = 0;

    /**
     * @brief Delete user
     * @param id User ID to delete
     * @return Result indicating success or error
     */
    [[nodiscard]] virtual auto delete_user(std::string_view id)
        -> kcenon::common::VoidResult = 0;

    // ═══════════════════════════════════════════════════════════════
    // Role Queries
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Get all users with specific role
     * @param role Role to filter by
     * @return Result containing user list
     */
    [[nodiscard]] virtual auto get_users_by_role(Role role)
        -> kcenon::common::Result<std::vector<User>> = 0;

protected:
    security_storage_interface() = default;
    security_storage_interface(const security_storage_interface&) = delete;
    security_storage_interface& operator=(const security_storage_interface&) = delete;
};

} // namespace pacs::security
```

---

## 8. Web/REST API Interfaces

> **Reference:** [SDS_WEB_API.md](SDS_WEB_API.md) - Complete Web/REST API Module Design Specification

### INT-WEB-001: rest_server Interface

**Traces to:** DES-WEB-001, SRS-WEB-001

```cpp
namespace pacs::web {

/**
 * @brief REST API server using Crow framework
 *
 * Provides HTTP/HTTPS endpoints for DICOMweb and management APIs.
 *
 * @usage
 * @code
 * rest_config config;
 * config.port = 8080;
 * config.cors_allowed_origins = "*";
 *
 * rest_server server(config);
 * server.set_database(database);
 * server.set_storage(storage);
 * server.start();
 * server.wait_for_shutdown();
 * @endcode
 */
class rest_server {
public:
    // ═══════════════════════════════════════════════════════════════
    // Construction
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Construct REST server with configuration
     * @param config Server configuration (port, TLS, CORS settings)
     */
    explicit rest_server(const rest_config& config);

    // ═══════════════════════════════════════════════════════════════
    // Dependency Injection
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Set index database for query operations
     */
    void set_database(std::shared_ptr<storage::index_database> db);

    /**
     * @brief Set file storage for DICOM file operations
     */
    void set_storage(std::shared_ptr<storage::storage_interface> storage);

    /**
     * @brief Set security manager for access control
     */
    void set_security_manager(
        std::shared_ptr<security::access_control_manager> manager);

    // ═══════════════════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief Start HTTP server (non-blocking)
     * @return Result indicating success or failure
     */
    [[nodiscard]] core::result<void> start();

    /**
     * @brief Stop server and close all connections
     */
    void stop();

    /**
     * @brief Block until server shutdown
     */
    void wait_for_shutdown();

    // ═══════════════════════════════════════════════════════════════
    // Status
    // ═══════════════════════════════════════════════════════════════

    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] uint16_t port() const noexcept;
};

} // namespace pacs::web
```

### INT-WEB-002: DICOMweb Services Interface

**Traces to:** DES-WEB-007, PS3.18

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        DICOMweb Services Interface                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Base URL: /dicomweb                                                         │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ WADO-RS (Retrieve)                                                       ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  GET  /studies/{study}                       → Retrieve study           ││
│  │  GET  /studies/{study}/metadata              → Study metadata (JSON)    ││
│  │  GET  /studies/{study}/series/{series}       → Retrieve series          ││
│  │  GET  /studies/{study}/series/{series}/instances/{instance}             ││
│  │       → Retrieve instance                                                ││
│  │  GET  /studies/{study}/series/{series}/instances/{instance}/frames/{n}  ││
│  │       → Retrieve specific frames                                         ││
│  │  GET  /studies/{study}/series/{series}/instances/{instance}/rendered    ││
│  │       → Rendered image (JPEG/PNG)                                        ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ STOW-RS (Store)                                                          ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  POST /studies                               → Store (any study)        ││
│  │  POST /studies/{study}                       → Store to specific study  ││
│  │                                                                          ││
│  │  Content-Type: multipart/related; type="application/dicom"              ││
│  │  Response: DICOM JSON with stored instance references                   ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ QIDO-RS (Query)                                                          ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  GET  /studies                               → Search studies           ││
│  │  GET  /series                                → Search all series        ││
│  │  GET  /instances                             → Search all instances     ││
│  │  GET  /studies/{study}/series                → Search series in study   ││
│  │  GET  /studies/{study}/instances             → Search instances in study││
│  │  GET  /studies/{study}/series/{series}/instances                        ││
│  │       → Search instances in series                                       ││
│  │                                                                          ││
│  │  Query Parameters:                                                       ││
│  │    - PatientID, PatientName, StudyDate, ModalitiesInStudy, etc.         ││
│  │    - limit, offset for pagination                                        ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### INT-WEB-003: Management API Interface

**Traces to:** DES-WEB-004 to DES-WEB-012

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        Management API Interface                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  Base URL: /api/v1                                                           │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Patient/Study/Series Management                                          ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  GET    /patients           → List patients (paginated)                 ││
│  │  GET    /patients/{id}      → Get patient by ID                         ││
│  │  DELETE /patients/{id}      → Delete patient and studies                ││
│  │  GET    /patients/{id}/studies → List patient's studies                 ││
│  │                                                                          ││
│  │  GET    /studies            → List studies (paginated)                  ││
│  │  GET    /studies/{uid}      → Get study by UID                          ││
│  │  DELETE /studies/{uid}      → Delete study                              ││
│  │  GET    /studies/{uid}/series → List study's series                     ││
│  │                                                                          ││
│  │  GET    /series/{uid}       → Get series details                        ││
│  │  GET    /series/{uid}/instances → List series' instances                ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Worklist Management                                                      ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  GET    /worklist           → List scheduled procedures                 ││
│  │  POST   /worklist           → Create worklist item                      ││
│  │  GET    /worklist/{id}      → Get item by PK                            ││
│  │  PUT    /worklist/{id}      → Update status                             ││
│  │  DELETE /worklist/{id}      → Delete item                               ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ Audit & Security                                                         ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  GET  /audit/logs           → List audit entries                        ││
│  │  GET  /audit/logs/{id}      → Get audit entry                           ││
│  │  GET  /audit/export         → Export logs (JSON/CSV)                    ││
│  │                                                                          ││
│  │  POST /security/users       → Create user                               ││
│  │  POST /security/users/{id}/roles → Assign role                          ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ System & Associations                                                    ││
│  ├─────────────────────────────────────────────────────────────────────────┤│
│  │                                                                          ││
│  │  GET  /system/health        → Health check                              ││
│  │  GET  /system/info          → System information                        ││
│  │  GET  /system/config        → Current configuration                     ││
│  │  GET  /system/metrics       → Performance metrics                       ││
│  │  GET  /system/storage       → Storage statistics                        ││
│  │                                                                          ││
│  │  GET    /associations/active → List active DICOM associations           ││
│  │  GET    /associations/{id}  → Get association details                   ││
│  │  DELETE /associations/{id}  → Terminate association                     ││
│  │                                                                          ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

*Document Version: 0.1.2.0*
*Created: 2025-11-30*
*Updated: 2026-01-03*
*Author: kcenon@naver.com*
