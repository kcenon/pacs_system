# SDS - Network V2 Module

> **Version:** 2.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-05
> **Status:** Complete

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-NET2-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#478](https://github.com/kcenon/pacs_system/issues/478), [#468](https://github.com/kcenon/pacs_system/issues/468) |

### Related Standards

| Standard | Description |
|----------|-------------|
| DICOM PS3.7 | Message Exchange |
| DICOM PS3.8 | Network Communication Support for Message Exchange |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. PDU Buffer Pool](#2-pdu-buffer-pool)
- [3. Accept Worker](#3-accept-worker)
- [4. DICOM Server V2](#4-dicom-server-v2)
- [5. Association Handler V2](#5-association-handler-v2)
- [6. Class Diagrams](#6-class-diagrams)
- [7. Sequence Diagrams](#7-sequence-diagrams)
- [8. Traceability](#8-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the Network V2 implementation for the PACS System. Network V2 introduces significant architectural improvements over V1:

- **Enhanced Server Architecture**: Uses `messaging_server` from network_system for connection management
- **Separation of Concerns**: PDU handling separated from server logic via `dicom_association_handler`
- **Memory Optimization**: Object pooling for PDU buffers reduces allocation overhead
- **Improved Thread Model**: Dedicated accept worker with `thread_base` integration

### 1.2 Scope

| Component | Files | Design ID |
|-----------|-------|-----------|
| PDU Buffer Pool | 1 header | DES-NET-006 |
| Accept Worker | 1 header, 1 source | DES-NET-007 |
| DICOM Server V2 | 1 header, 1 source | DES-NET-008 |
| Association Handler V2 | 1 header, 1 source | DES-NET-009 |

### 1.3 Design Identifier Convention

```
DES-NET-<NUMBER>

Where:
- DES: Design Specification prefix
- NET: Network module identifier
- NUMBER: 3-digit sequential number
```

### 1.4 V1 vs V2 Architectural Comparison

| Aspect | V1 (dicom_server) | V2 (dicom_server_v2) |
|--------|-------------------|----------------------|
| **Connection Management** | Manual socket management | `messaging_server` integration |
| **Thread Model** | One thread per connection | Thread pool with async I/O |
| **Buffer Management** | Per-PDU allocation | Object pool with buffer reuse |
| **Accept Loop** | Inline in server class | Dedicated `accept_worker` class |
| **Shutdown** | Signal-based interruption | Cooperative cancellation via `stop_token` |
| **State Machine** | Embedded in server | Separated to `dicom_association_handler` |
| **Access Control** | Not supported | RBAC integration via `access_control_manager` |
| **TLS Support** | Not available | Ready (via network_system) |

---

## 2. PDU Buffer Pool

### 2.1 DES-NET-006: PDU Buffer Pool

**Traces to:** SRS-PERF-005 (Memory Usage Optimization)

**File:** `include/pacs/network/pdu_buffer_pool.hpp`

The `pdu_buffer_pool` provides object pooling for PDU-related data structures to reduce allocation overhead during high-throughput network operations.

```cpp
namespace pacs::network {

struct pdu_pool_statistics {
    std::atomic<uint64_t> total_acquisitions{0};
    std::atomic<uint64_t> pool_hits{0};
    std::atomic<uint64_t> pool_misses{0};
    std::atomic<uint64_t> total_releases{0};
    std::atomic<uint64_t> total_bytes_allocated{0};

    [[nodiscard]] auto hit_ratio() const noexcept -> double;
    void reset() noexcept;
};

class pooled_buffer {
public:
    void clear() noexcept;
    void resize(std::size_t new_size);
    void reserve(std::size_t capacity);
    [[nodiscard]] auto size() const noexcept -> std::size_t;
    [[nodiscard]] auto capacity() const noexcept -> std::size_t;
    [[nodiscard]] auto data() noexcept -> uint8_t*;
    [[nodiscard]] auto vector() noexcept -> std::vector<uint8_t>&;
};

template <typename T>
class tracked_pdu_pool {
public:
    explicit tracked_pdu_pool(std::size_t initial_size = 64);

    template <typename... Args>
    auto acquire(Args&&... args) -> unique_ptr_type;

    [[nodiscard]] auto statistics() const noexcept -> const pdu_pool_statistics&;
    [[nodiscard]] auto available() const -> std::size_t;
    void reserve(std::size_t count);
    void clear();
};

class pdu_buffer_pool {
public:
    static constexpr std::size_t DEFAULT_BUFFER_POOL_SIZE = 256;
    static constexpr std::size_t DEFAULT_PDV_POOL_SIZE = 128;
    static constexpr std::size_t DEFAULT_PDATA_POOL_SIZE = 64;

    static auto get() noexcept -> pdu_buffer_pool&;

    auto acquire_buffer() -> tracked_pdu_pool<pooled_buffer>::unique_ptr_type;
    auto acquire_pdv(uint8_t context_id = 0, bool is_command = false,
                     bool is_last = true)
        -> tracked_pdu_pool<presentation_data_value>::unique_ptr_type;
    auto acquire_p_data_tf()
        -> tracked_pdu_pool<p_data_tf_pdu>::unique_ptr_type;

    [[nodiscard]] auto buffer_statistics() const noexcept -> const pdu_pool_statistics&;
    [[nodiscard]] auto pdv_statistics() const noexcept -> const pdu_pool_statistics&;
    [[nodiscard]] auto p_data_statistics() const noexcept -> const pdu_pool_statistics&;

    void reserve_buffers(std::size_t count);
    void reserve_pdvs(std::size_t count);
    void clear_all();
    void reset_statistics();
};

// Convenience factory functions
[[nodiscard]] auto make_pooled_pdu_buffer() -> unique_ptr_type;
[[nodiscard]] auto make_pooled_pdu_buffer(std::size_t size) -> unique_ptr_type;
[[nodiscard]] auto make_pooled_pdv(uint8_t context_id, bool is_command, bool is_last)
    -> unique_ptr_type;
[[nodiscard]] auto make_pooled_p_data_tf() -> unique_ptr_type;

} // namespace pacs::network
```

**Design Rationale:**

- **Singleton Pattern**: Global access via `pdu_buffer_pool::get()` ensures consistent pool usage
- **Statistics Tracking**: `pdu_pool_statistics` enables monitoring of pool efficiency
- **RAII Management**: Acquired objects automatically return to pool on destruction
- **Type Safety**: Template-based pool ensures type-correct object management

**Memory Efficiency:**

| Pool Type | Default Size | Typical Object Size | Purpose |
|-----------|--------------|---------------------|---------|
| Buffer Pool | 256 | 16KB-64KB | Raw PDU data buffers |
| PDV Pool | 128 | ~512B | Presentation Data Values |
| P-DATA Pool | 64 | ~1KB | P-DATA-TF PDU structures |

---

## 3. Accept Worker

### 3.1 DES-NET-007: Accept Worker

**Traces to:** SRS-INT-004 (thread_system Integration)

**File:** `include/pacs/network/detail/accept_worker.hpp`, `src/network/detail/accept_worker.cpp`

The `accept_worker` class provides a dedicated thread for accepting incoming TCP connections, inheriting from `thread_base` for lifecycle management.

```cpp
namespace pacs::network::detail {

class accept_worker : public kcenon::thread::thread_base {
public:
    using connection_callback = std::function<void(uint64_t session_id)>;
    using maintenance_callback = std::function<void()>;

    explicit accept_worker(
        uint16_t port,
        connection_callback on_connection,
        maintenance_callback on_maintenance = nullptr);
    ~accept_worker() override;

    // Configuration
    void set_max_pending_connections(int backlog);
    [[nodiscard]] uint16_t port() const noexcept;
    [[nodiscard]] int max_pending_connections() const noexcept;

    // Status
    [[nodiscard]] bool is_accepting() const noexcept;
    [[nodiscard]] std::string to_string() const override;

protected:
    // thread_base overrides
    result_void before_start() override;
    result_void do_work() override;
    result_void after_stop() override;
    [[nodiscard]] bool should_continue_work() const override;
    void on_stop_requested() override;

private:
    [[nodiscard]] uint64_t next_session_id();

    uint16_t port_;
    connection_callback on_connection_;
    maintenance_callback on_maintenance_;
    int backlog_{128};
    std::atomic<uint64_t> session_id_counter_{0};
    std::atomic<bool> accepting_{false};

#ifdef _WIN32
    SOCKET listen_socket_{INVALID_SOCKET};
    bool wsa_initialized_{false};
#else
    int listen_socket_{-1};
#endif
};

} // namespace pacs::network::detail
```

**Design Rationale:**

- **thread_base Inheritance**: Leverages thread_system's lifecycle management and jthread support
- **Cooperative Cancellation**: Uses `std::stop_token` for graceful shutdown
- **Platform Abstraction**: Handles Windows/POSIX socket differences
- **Maintenance Hook**: Optional callback for periodic tasks (e.g., idle timeout checks)

**Threading Model:**

```
┌─────────────────────────────────────────────────────────────────┐
│                        Accept Worker                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐     ┌──────────────────────────────────────┐  │
│  │ thread_base  │     │          accept_worker               │  │
│  │ ─────────────│     │  ────────────────────────────────    │  │
│  │ - jthread    │     │  - listen_socket_                    │  │
│  │ - stop_token │────►│  - on_connection_ callback           │  │
│  │ - wake_cv_   │     │  - on_maintenance_ callback          │  │
│  └──────────────┘     └──────────────────────────────────────┘  │
│                                                                  │
│  Lifecycle:                                                      │
│  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐     │
│  │ Inactive │──►│ Starting │──►│ Running  │──►│ Stopping │     │
│  └──────────┘   └──────────┘   └──────────┘   └──────────┘     │
│       │              │              │              │             │
│       │         before_start   do_work (loop)  after_stop       │
│       │              │              │              │             │
│       └──────────────┴──────────────┴──────────────┘             │
│                        (thread_base manages)                     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. DICOM Server V2

### 4.1 DES-NET-008: DICOM Server V2

**Traces to:** SRS-INT-003 (network_system Integration)

**File:** `include/pacs/network/v2/dicom_server_v2.hpp`, `src/network/v2/dicom_server_v2.cpp`

The `dicom_server_v2` class provides DICOM network server functionality using `network_system`'s `messaging_server` for connection management.

```cpp
namespace pacs::network::v2 {

class dicom_server_v2 {
public:
    using association_established_callback =
        std::function<void(const std::string& session_id,
                           const std::string& calling_ae,
                           const std::string& called_ae)>;
    using association_closed_callback =
        std::function<void(const std::string& session_id, bool graceful)>;
    using error_callback = std::function<void(const std::string& error)>;

    // Construction
    explicit dicom_server_v2(const server_config& config);
    ~dicom_server_v2();

    // Non-copyable, non-movable
    dicom_server_v2(const dicom_server_v2&) = delete;
    dicom_server_v2& operator=(const dicom_server_v2&) = delete;

    // Service Registration
    void register_service(services::scp_service_ptr service);
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const;

    // Lifecycle Management
    [[nodiscard]] Result<std::monostate> start();
    void stop(duration timeout = std::chrono::seconds{30});
    void wait_for_shutdown();

    // Status Queries
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] size_t active_associations() const noexcept;
    [[nodiscard]] server_statistics get_statistics() const;
    [[nodiscard]] const server_config& config() const noexcept;

    // Callbacks
    void on_association_established(association_established_callback callback);
    void on_association_closed(association_closed_callback callback);
    void on_error(error_callback callback);

    // Security / Access Control
    void set_access_control(std::shared_ptr<security::access_control_manager> acm);
    [[nodiscard]] std::shared_ptr<security::access_control_manager>
        get_access_control() const noexcept;
    void set_access_control_enabled(bool enabled);
    [[nodiscard]] bool is_access_control_enabled() const noexcept;

private:
    // Network callbacks
    void on_connection(std::shared_ptr<messaging_session> session);
    void on_disconnection(const std::string& session_id);
    void on_receive(std::shared_ptr<messaging_session> session,
                    const std::vector<uint8_t>& data);
    void on_network_error(std::shared_ptr<messaging_session> session,
                          std::error_code ec);

    // Handler management
    void create_handler(std::shared_ptr<messaging_session> session);
    void remove_handler(const std::string& session_id);
    [[nodiscard]] std::shared_ptr<dicom_association_handler>
        find_handler(const std::string& session_id) const;
    void check_idle_timeouts();

    // Internal helpers
    [[nodiscard]] dicom_association_handler::service_map build_service_map() const;
    void report_error(const std::string& error);

    // Member variables
    server_config config_;
    std::shared_ptr<messaging_server> server_;
    std::vector<services::scp_service_ptr> services_;
    std::map<std::string, services::scp_service*> sop_class_to_service_;
    std::unordered_map<std::string, std::shared_ptr<dicom_association_handler>> handlers_;
    mutable server_statistics stats_;
    std::atomic<bool> running_{false};
    mutable std::mutex handlers_mutex_;
    mutable std::mutex services_mutex_;
    mutable std::mutex stats_mutex_;
    std::condition_variable shutdown_cv_;
    std::mutex shutdown_mutex_;
    association_established_callback on_established_cb_;
    association_closed_callback on_closed_cb_;
    error_callback on_error_cb_;
    mutable std::mutex callback_mutex_;
    std::shared_ptr<security::access_control_manager> access_control_;
    std::atomic<bool> access_control_enabled_{false};
    mutable std::mutex acl_mutex_;
};

} // namespace pacs::network::v2
```

**Design Rationale:**

- **Delegation Pattern**: Delegates connection management to `messaging_server`
- **Handler per Session**: Each connection gets a dedicated `dicom_association_handler`
- **Service Registry**: SOP Class to service mapping for DIMSE dispatch
- **RBAC Integration**: Optional access control via `access_control_manager`
- **Thread Safety**: All public methods are thread-safe with appropriate mutex protection

**Key Improvements over V1:**

| Feature | V1 | V2 |
|---------|----|----|
| Max Associations | Hard-coded | Configurable with enforcement |
| Idle Timeout | Not supported | Built-in with `check_idle_timeouts()` |
| Statistics | Basic | Comprehensive `server_statistics` |
| Graceful Shutdown | Immediate | Phased with timeout |
| Error Handling | Callbacks | Structured `Result<>` pattern |

---

## 5. Association Handler V2

### 5.1 DES-NET-009: Association Handler V2

**Traces to:** SRS-NET-002 (Association State Machine), SRS-SEC-002 (Access Control)

**File:** `include/pacs/network/v2/dicom_association_handler.hpp`, `src/network/v2/dicom_association_handler.cpp`

The `dicom_association_handler` bridges `network_system` sessions with DICOM protocol handling, managing PDU framing, state machine, and service dispatching.

```cpp
namespace pacs::network::v2 {

enum class handler_state {
    idle,              // Initial state, waiting for A-ASSOCIATE-RQ
    awaiting_response, // Sent response, awaiting next PDU
    established,       // Association established, processing DIMSE
    releasing,         // Graceful release in progress
    closed             // Association closed (released or aborted)
};

[[nodiscard]] constexpr const char* to_string(handler_state state) noexcept;

class dicom_association_handler
    : public std::enable_shared_from_this<dicom_association_handler> {
public:
    using session_ptr = std::shared_ptr<messaging_session>;
    using service_map = std::map<std::string, services::scp_service*>;
    static constexpr size_t pdu_header_size = 6;
    static constexpr size_t max_pdu_size = 64 * 1024 * 1024;  // 64 MB

    // Construction
    dicom_association_handler(
        session_ptr session,
        const server_config& config,
        const service_map& services);
    ~dicom_association_handler();

    // Lifecycle
    void start();
    void stop(bool graceful = false);

    // State Queries
    [[nodiscard]] handler_state state() const noexcept;
    [[nodiscard]] bool is_established() const noexcept;
    [[nodiscard]] bool is_closed() const noexcept;
    [[nodiscard]] std::string session_id() const;
    [[nodiscard]] std::string calling_ae() const;
    [[nodiscard]] std::string called_ae() const;
    [[nodiscard]] association& get_association();
    [[nodiscard]] time_point last_activity() const noexcept;

    // Callbacks
    void set_established_callback(association_established_callback callback);
    void set_closed_callback(association_closed_callback callback);
    void set_error_callback(handler_error_callback callback);

    // Statistics
    [[nodiscard]] uint64_t pdus_received() const noexcept;
    [[nodiscard]] uint64_t pdus_sent() const noexcept;
    [[nodiscard]] uint64_t messages_processed() const noexcept;

    // Security
    void set_access_control(std::shared_ptr<security::access_control_manager> acm);
    void set_access_control_enabled(bool enabled);

private:
    // Network callbacks
    void on_data_received(const std::vector<uint8_t>& data);
    void on_disconnected(const std::string& session_id);
    void on_error(std::error_code ec);

    // PDU processing
    void process_buffer();
    void process_pdu(const integration::pdu_data& pdu);
    void handle_associate_rq(const std::vector<uint8_t>& payload);
    void handle_p_data_tf(const std::vector<uint8_t>& payload);
    void handle_release_rq();
    void handle_abort(const std::vector<uint8_t>& payload);

    // Response sending
    void send_associate_ac();
    void send_associate_rj(reject_result result, uint8_t source, uint8_t reason);
    void send_p_data_tf(const std::vector<uint8_t>& payload);
    void send_release_rp();
    void send_abort(abort_source source, abort_reason reason);
    void send_pdu(pdu_type type, const std::vector<uint8_t>& payload);

    // Service dispatching
    Result<std::monostate> dispatch_to_service(uint8_t context_id,
                                                const dimse::dimse_message& msg);
    [[nodiscard]] services::scp_service* find_service(
        const std::string& sop_class_uid) const;

    // State management
    void transition_to(handler_state new_state);
    void touch();
    void report_error(const std::string& error);
    void close_handler(bool graceful);

    // Member variables
    session_ptr session_;
    server_config config_;
    service_map services_;
    association association_;
    std::atomic<handler_state> state_{handler_state::idle};
    std::vector<uint8_t> receive_buffer_;
    time_point last_activity_;
    std::atomic<uint64_t> pdus_received_{0};
    std::atomic<uint64_t> pdus_sent_{0};
    std::atomic<uint64_t> messages_processed_{0};
    association_established_callback established_callback_;
    association_closed_callback closed_callback_;
    handler_error_callback error_callback_;
    mutable std::mutex mutex_;
    mutable std::mutex callback_mutex_;
    std::shared_ptr<security::access_control_manager> access_control_;
    std::optional<security::user_context> user_context_;
    bool access_control_enabled_{false};
};

} // namespace pacs::network::v2
```

**Design Rationale:**

- **enable_shared_from_this**: Allows safe capture in async callbacks
- **PDU Framing**: Accumulates fragmented TCP data into complete PDUs
- **State Machine**: Explicit state tracking for DICOM association lifecycle
- **Service Dispatch**: Routes DIMSE messages to registered SCP services
- **RBAC Integration**: Optional access control checking per DIMSE operation

**Handler State Machine:**

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Handler State Machine                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌──────────┐                                                        │
│  │   IDLE   │ ◄─────── Initial state (waiting for A-ASSOCIATE-RQ)   │
│  └────┬─────┘                                                        │
│       │                                                              │
│       │ Receive A-ASSOCIATE-RQ                                       │
│       │                                                              │
│       ├─────────────────────────────────────┐                        │
│       │ (valid request)                     │ (invalid request)      │
│       ▼                                     ▼                        │
│  ┌────────────────┐                   ┌──────────┐                  │
│  │  ESTABLISHED   │                   │  CLOSED  │                  │
│  │  (send AC)     │                   │  (RJ/AB) │                  │
│  └───────┬────────┘                   └──────────┘                  │
│          │                                                           │
│          │ Process P-DATA-TF (DIMSE messages)                       │
│          │                                                           │
│          ├─────────────────────────────────────┐                     │
│          │ Receive A-RELEASE-RQ                │ Receive A-ABORT    │
│          ▼                                     │ or Error            │
│     ┌──────────┐                               │                     │
│     │RELEASING │                               │                     │
│     │(send RP) │                               │                     │
│     └────┬─────┘                               │                     │
│          │                                     │                     │
│          ▼                                     ▼                     │
│     ┌──────────────────────────────────────────────┐                │
│     │                    CLOSED                      │                │
│     │  (graceful=true for release, false for abort) │                │
│     └──────────────────────────────────────────────┘                │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

**Access Control Integration:**

| DIMSE Command | Mapped Operation | Required Permission |
|---------------|------------------|---------------------|
| C-ECHO-RQ | CEcho | System/Execute |
| C-STORE-RQ | CStore | Study/Write |
| C-FIND-RQ | CFind | Study/Read, Metadata/Read |
| C-MOVE-RQ | CMove | Study/Export |
| C-GET-RQ | CGet | Study/Export |
| N-CREATE-RQ | NCreate | Study/Write |
| N-SET-RQ | NSet | Study/Write |
| N-GET-RQ | NGet | Study/Read |
| N-DELETE-RQ | NDelete | Study/Delete |
| N-ACTION-RQ | NAction | Study/Execute |
| N-EVENT-REPORT-RQ | NEventReport | Study/Read |

---

## 6. Class Diagrams

### 6.1 Network V2 Component Overview

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           Network V2 Architecture                                │
├─────────────────────────────────────────────────────────────────────────────────┤
│                                                                                  │
│  External Dependencies                                                           │
│  ──────────────────────                                                          │
│                                                                                  │
│  ┌─────────────────────┐     ┌─────────────────────┐                            │
│  │   network_system    │     │   thread_system     │                            │
│  │  (kcenon::network)  │     │  (kcenon::thread)   │                            │
│  ├─────────────────────┤     ├─────────────────────┤                            │
│  │ messaging_server    │     │ thread_base         │                            │
│  │ messaging_session   │     │ (stop_token support)│                            │
│  └──────────┬──────────┘     └──────────┬──────────┘                            │
│             │                           │                                        │
│             │                           │                                        │
│  PACS Network V2 Layer                  │                                        │
│  ─────────────────────                  │                                        │
│             │                           │                                        │
│             ▼                           ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────┐            │
│  │                     dicom_server_v2 (DES-NET-008)                │            │
│  ├─────────────────────────────────────────────────────────────────┤            │
│  │ - config_: server_config                                         │            │
│  │ - server_: shared_ptr<messaging_server>                         │            │
│  │ - services_: vector<scp_service_ptr>                            │            │
│  │ - handlers_: unordered_map<string, shared_ptr<handler>>         │            │
│  │ - access_control_: shared_ptr<access_control_manager>           │            │
│  ├─────────────────────────────────────────────────────────────────┤            │
│  │ + start() / stop() / wait_for_shutdown()                        │            │
│  │ + register_service(scp_service_ptr)                             │            │
│  │ + set_access_control(access_control_manager)                    │            │
│  │ + on_association_established/closed/error(callback)             │            │
│  └──────────────────────────────┬──────────────────────────────────┘            │
│                                 │                                                │
│                                 │ creates per connection                         │
│                                 ▼                                                │
│  ┌─────────────────────────────────────────────────────────────────┐            │
│  │               dicom_association_handler (DES-NET-009)            │            │
│  ├─────────────────────────────────────────────────────────────────┤            │
│  │ - session_: shared_ptr<messaging_session>                       │            │
│  │ - config_: server_config                                         │            │
│  │ - services_: map<string, scp_service*>                          │            │
│  │ - association_: association                                      │            │
│  │ - state_: atomic<handler_state>                                 │            │
│  │ - receive_buffer_: vector<uint8_t>                              │            │
│  │ - user_context_: optional<user_context>                         │            │
│  ├─────────────────────────────────────────────────────────────────┤            │
│  │ + start() / stop(graceful)                                      │            │
│  │ + state() / is_established() / is_closed()                      │            │
│  │ + calling_ae() / called_ae() / session_id()                     │            │
│  │ - handle_associate_rq/p_data_tf/release_rq/abort()              │            │
│  │ - dispatch_to_service(context_id, dimse_message)                │            │
│  └──────────────────────────────┬──────────────────────────────────┘            │
│                                 │                                                │
│                                 │ uses                                           │
│                                 ▼                                                │
│  ┌───────────────────────────────────────────────────────────────────────────┐  │
│  │                       Support Components                                    │  │
│  ├───────────────────────────────────────────────────────────────────────────┤  │
│  │                                                                            │  │
│  │  ┌─────────────────────┐     ┌─────────────────────┐                      │  │
│  │  │ pdu_buffer_pool     │     │    accept_worker    │                      │  │
│  │  │ (DES-NET-006)       │     │   (DES-NET-007)     │                      │  │
│  │  ├─────────────────────┤     ├─────────────────────┤                      │  │
│  │  │ - buffer_pool_      │     │ - port_             │                      │  │
│  │  │ - pdv_pool_         │     │ - listen_socket_    │                      │  │
│  │  │ - p_data_pool_      │     │ - on_connection_    │                      │  │
│  │  ├─────────────────────┤     │ - on_maintenance_   │                      │  │
│  │  │ +get() (singleton)  │     ├─────────────────────┤                      │  │
│  │  │ +acquire_buffer()   │     │ +before_start()     │                      │  │
│  │  │ +acquire_pdv()      │     │ +do_work()          │                      │  │
│  │  │ +buffer_statistics()│     │ +after_stop()       │                      │  │
│  │  └─────────────────────┘     └─────────────────────┘                      │  │
│  │                                                                            │  │
│  └───────────────────────────────────────────────────────────────────────────┘  │
│                                                                                  │
└─────────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Sequence Diagrams

### 7.1 SEQ-NET2-001: Server Startup and Connection Acceptance

```
┌─────────┐     ┌─────────────────┐     ┌──────────────────┐     ┌─────────────┐
│  Main   │     │ dicom_server_v2 │     │ messaging_server │     │   Client    │
└────┬────┘     └────────┬────────┘     └────────┬─────────┘     └──────┬──────┘
     │                   │                       │                      │
     │ 1. new dicom_server_v2(config)            │                      │
     │──────────────────►│                       │                      │
     │                   │                       │                      │
     │ 2. register_service(storage_scp)          │                      │
     │──────────────────►│                       │                      │
     │                   │                       │                      │
     │ 3. start()        │                       │                      │
     │──────────────────►│                       │                      │
     │                   │ 4. Create messaging_server                   │
     │                   │──────────────────────►│                      │
     │                   │                       │                      │
     │                   │ 5. Set connection/receive/error callbacks    │
     │                   │──────────────────────►│                      │
     │                   │                       │                      │
     │                   │ 6. start_server(port) │                      │
     │                   │──────────────────────►│                      │
     │                   │                       │                      │
     │                   │                       │ 7. TCP Connect       │
     │                   │                       │◄─────────────────────│
     │                   │                       │                      │
     │                   │ 8. on_connection(session)                    │
     │                   │◄──────────────────────│                      │
     │                   │                       │                      │
     │                   │ 9. Check max_associations limit              │
     │                   │                       │                      │
     │                   │ 10. create_handler(session)                  │
     │                   │                       │                      │
     │                   │──────────► Create dicom_association_handler  │
     │                   │            with session, config, services    │
     │                   │                       │                      │
     │                   │ 11. handler->start()  │                      │
     │                   │                       │                      │
```

### 7.2 SEQ-NET2-002: Association Establishment with Access Control

```
┌──────────┐     ┌─────────────────────────┐     ┌─────────────────────┐
│  Client  │     │ dicom_association_handler│     │access_control_mgr   │
└────┬─────┘     └───────────┬─────────────┘     └──────────┬──────────┘
     │                       │                              │
     │ 1. A-ASSOCIATE-RQ     │                              │
     │  (CallingAE, CalledAE)│                              │
     │──────────────────────►│                              │
     │                       │                              │
     │                       │ 2. Validate called_ae_title  │
     │                       │    (matches config.ae_title) │
     │                       │                              │
     │                       │ 3. Check ae_whitelist        │
     │                       │                              │
     │                       │ 4. Negotiate presentation    │
     │                       │    contexts with services    │
     │                       │                              │
     │                       │ 5. get_context_for_ae(calling_ae, session_id)
     │                       │─────────────────────────────►│
     │                       │                              │
     │                       │ 6. user_context (with roles) │
     │                       │◄─────────────────────────────│
     │                       │                              │
     │                       │ 7. Store user_context_ for   │
     │                       │    future DIMSE checks       │
     │                       │                              │
     │ 8. A-ASSOCIATE-AC     │                              │
     │  (accepted contexts)  │                              │
     │◄──────────────────────│                              │
     │                       │                              │
     │                       │ 9. transition_to(established)│
     │                       │                              │
     │                       │ 10. established_callback_()  │
     │                       │                              │
```

### 7.3 SEQ-NET2-003: DIMSE Message Processing with Access Control

```
┌──────────┐     ┌─────────────────────────┐     ┌────────────────┐     ┌─────────────┐
│  Client  │     │ dicom_association_handler│     │  scp_service   │     │access_ctrl  │
└────┬─────┘     └───────────┬─────────────┘     └───────┬────────┘     └──────┬──────┘
     │                       │                          │                      │
     │ 1. P-DATA-TF          │                          │                      │
     │  (C-STORE-RQ)         │                          │                      │
     │──────────────────────►│                          │                      │
     │                       │                          │                      │
     │                       │ 2. Append to receive_buffer_                    │
     │                       │                          │                      │
     │                       │ 3. process_buffer()      │                      │
     │                       │    - Check pdu_header_size                      │
     │                       │    - Extract complete PDU │                      │
     │                       │                          │                      │
     │                       │ 4. process_pdu(P-DATA-TF)│                      │
     │                       │                          │                      │
     │                       │ 5. handle_p_data_tf()    │                      │
     │                       │    - Decode P-DATA-TF    │                      │
     │                       │    - Extract PDVs        │                      │
     │                       │    - Decode DIMSE message│                      │
     │                       │                          │                      │
     │                       │ 6. Map C-STORE-RQ to DicomOperation::CStore     │
     │                       │                          │                      │
     │                       │ 7. check_dicom_operation(user_context_, CStore) │
     │                       │─────────────────────────────────────────────────►│
     │                       │                          │                      │
     │                       │ 8. AccessCheckResult     │                      │
     │                       │◄─────────────────────────────────────────────────│
     │                       │                          │                      │
     │                       │ 9. [If allowed] dispatch_to_service()           │
     │                       │─────────────────────────►│                      │
     │                       │                          │                      │
     │                       │ 10. handle_message()     │                      │
     │                       │◄─────────────────────────│                      │
     │                       │                          │                      │
     │ 11. P-DATA-TF         │                          │                      │
     │  (C-STORE-RSP)        │                          │                      │
     │◄──────────────────────│                          │                      │
     │                       │                          │                      │
```

### 7.4 SEQ-NET2-004: Graceful Server Shutdown

```
┌─────────┐     ┌─────────────────┐     ┌───────────────────────────┐     ┌─────────────┐
│  Admin  │     │ dicom_server_v2 │     │ dicom_association_handler │     │msg_server   │
└────┬────┘     └────────┬────────┘     └─────────────┬─────────────┘     └──────┬──────┘
     │                   │                            │                          │
     │ 1. stop(timeout)  │                            │                          │
     │──────────────────►│                            │                          │
     │                   │                            │                          │
     │                   │ 2. running_.exchange(false)│                          │
     │                   │                            │                          │
     │                   │ 3. server_->stop_server()  │                          │
     │                   │───────────────────────────────────────────────────────►│
     │                   │                            │                          │
     │                   │ 4. [Phase 2] Wait for handlers to complete            │
     │                   │    while (!handlers_.empty() && now < deadline)       │
     │                   │       sleep(100ms)         │                          │
     │                   │                            │                          │
     │                   │ 5. [Phase 3] Force stop remaining handlers            │
     │                   │                            │                          │
     │                   │ 6. handler->stop(false)    │                          │
     │                   │───────────────────────────►│                          │
     │                   │                            │                          │
     │                   │                            │ 7. send_abort()          │
     │                   │                            │                          │
     │                   │                            │ 8. session_->stop_session()
     │                   │                            │                          │
     │                   │                            │ 9. closed_callback_()    │
     │                   │◄───────────────────────────│                          │
     │                   │                            │                          │
     │                   │ 10. handlers_.clear()      │                          │
     │                   │                            │                          │
     │                   │ 11. server_.reset()        │                          │
     │                   │                            │                          │
     │                   │ 12. shutdown_cv_.notify_all()                         │
     │                   │                            │                          │
     │ 13. Return        │                            │                          │
     │◄──────────────────│                            │                          │
     │                   │                            │                          │
```

---

## 8. Traceability

### 8.1 SRS to SDS Traceability

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-INT-003** | network_system Integration | DES-NET-008 | `dicom_server_v2` with messaging_server |
| **SRS-NET-002** | Association State Machine | DES-NET-009 | `dicom_association_handler` state machine |
| **SRS-INT-004** | thread_system Integration | DES-NET-007 | `accept_worker` with thread_base |
| **SRS-PERF-005** | Memory Usage Optimization | DES-NET-006 | `pdu_buffer_pool` object pooling |
| **SRS-SEC-002** | Access Control | DES-NET-008, DES-NET-009 | RBAC integration via `access_control_manager` |

### 8.2 SDS to Implementation Traceability

| SDS ID | Design Element | Header File | Source File |
|--------|----------------|-------------|-------------|
| DES-NET-006 | `pdu_buffer_pool` | `include/pacs/network/pdu_buffer_pool.hpp` | (header-only) |
| DES-NET-007 | `accept_worker` | `include/pacs/network/detail/accept_worker.hpp` | `src/network/detail/accept_worker.cpp` |
| DES-NET-008 | `dicom_server_v2` | `include/pacs/network/v2/dicom_server_v2.hpp` | `src/network/v2/dicom_server_v2.cpp` |
| DES-NET-009 | `dicom_association_handler` | `include/pacs/network/v2/dicom_association_handler.hpp` | `src/network/v2/dicom_association_handler.cpp` |

### 8.3 SDS to Test Traceability

| SDS ID | Design Element | Test File | Description |
|--------|----------------|-----------|-------------|
| DES-NET-008 | `dicom_server_v2` | `tests/network/v2/dicom_server_v2_test.cpp` | Server lifecycle, service registration |
| DES-NET-009 | `dicom_association_handler` | `tests/network/v2/dicom_association_handler_test.cpp` | PDU handling, state machine |
| DES-NET-009 | `dicom_association_handler` | `tests/network/v2/state_machine_test.cpp` | Handler state transitions |
| DES-NET-009 | `dicom_association_handler` | `tests/network/v2/service_dispatching_test.cpp` | DIMSE service dispatch |
| DES-NET-009 | `dicom_association_handler` | `tests/network/v2/pdu_framing_test.cpp` | PDU framing and accumulation |
| DES-NET-008, DES-NET-009 | Integration | `tests/network/v2/network_system_integration_test.cpp` | End-to-end integration |
| DES-NET-008 | `dicom_server_v2` | `tests/network/v2/stress_test.cpp` | Load and stress testing |

---

## Appendix A: Configuration Reference

### A.1 server_config Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ae_title` | string | - | Server AE Title (required) |
| `port` | uint16_t | 11112 | DICOM port number |
| `max_associations` | size_t | 0 (unlimited) | Maximum concurrent associations |
| `max_pdu_size` | uint32_t | 16384 | Maximum PDU size |
| `idle_timeout` | duration | 0 (disabled) | Idle connection timeout |
| `ae_whitelist` | vector<string> | {} | Allowed calling AE titles |
| `accept_unknown_calling_ae` | bool | true | Accept unlisted AE titles |
| `implementation_class_uid` | string | (generated) | Implementation Class UID |
| `implementation_version_name` | string | "PACS_V2" | Implementation Version |

---

*Document Version: 2.0.0*
*Created: 2026-01-04*
*Updated: 2026-01-05*
*Author: kcenon@naver.com*
