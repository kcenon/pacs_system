# SDS - Network V2 Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-NET2-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#478](https://github.com/kcenon/pacs_system/issues/478), [#468](https://github.com/kcenon/pacs_system/issues/468) |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. DICOM Server V2](#2-dicom-server-v2)
- [3. Association Handler V2](#3-association-handler-v2)
- [4. Accept Worker](#4-accept-worker)
- [5. PDU Buffer Pool](#5-pdu-buffer-pool)
- [6. Traceability](#6-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the Network V2 implementation. V2 introduces:

- **Enhanced Server**: Improved connection management with `messaging_server`
- **Handler Separation**: PDU handling separated from server logic
- **Buffer Pooling**: Reduced allocation overhead for PDUs
- **Accept Worker**: Dedicated accept thread with graceful shutdown

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| DICOM Server V2 | 1 header, 1 source | DES-NET-006 |
| Association Handler V2 | 1 header, 1 source | DES-NET-007 |
| Accept Worker | 1 header, 1 source | DES-NET-008 |
| PDU Buffer Pool | 1 header, 1 source | DES-NET-009 |

### 1.3 V1 vs V2 Comparison

| Aspect | V1 | V2 |
|--------|----|----|
| Connection Handling | Manual socket management | `messaging_server` integration |
| Thread Model | One thread per connection | Thread pool with worker dispatch |
| Buffer Management | Per-PDU allocation | Buffer pool with reuse |
| Shutdown | Signal-based | Cooperative cancellation token |

---

## 2. DICOM Server V2

### 2.1 DES-NET-006: DICOM Server V2

**Traces to:** SRS-INT-003 (network_system Integration)

**File:** `include/pacs/network/v2/dicom_server_v2.hpp`, `src/network/v2/dicom_server_v2.cpp`

```cpp
namespace pacs::network::v2 {

class dicom_server_v2 {
public:
    explicit dicom_server_v2(const server_config& config);
    ~dicom_server_v2();

    // Lifecycle
    [[nodiscard]] auto start() -> core::result<void>;
    void stop();
    [[nodiscard]] auto is_running() const noexcept -> bool;

    // Service registration
    void register_service(std::shared_ptr<dicom_service_interface> service);
    void unregister_service(const std::string& abstract_syntax);

    // Statistics
    [[nodiscard]] auto active_associations() const -> std::size_t;
    [[nodiscard]] auto total_connections() const -> std::uint64_t;

private:
    void on_connection(std::shared_ptr<messaging_session> session);
    void create_association_handler(std::shared_ptr<messaging_session> session);

    std::unique_ptr<messaging_server> server_;
    std::shared_ptr<thread_pool> worker_pool_;
    std::unordered_map<std::string, std::shared_ptr<dicom_service_interface>> services_;
    std::vector<std::shared_ptr<dicom_association_handler>> active_handlers_;
    mutable std::shared_mutex mutex_;
    server_config config_;
};

} // namespace pacs::network::v2
```

**Architecture:**

```
┌─────────────────────────────────────────────────────────────────┐
│                      DICOM Server V2                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────┐     ┌──────────────────────────────────┐  │
│  │  messaging_server │────▶│  Connection Callback             │  │
│  │  (network_system) │     │  on_connection()                 │  │
│  └──────────────────┘     └────────────┬─────────────────────┘  │
│                                        │                         │
│                                        ▼                         │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                Association Handler Factory                │   │
│  │  Creates dicom_association_handler per connection        │   │
│  └───────────────────────────┬──────────────────────────────┘   │
│                              │                                   │
│              ┌───────────────┼───────────────┐                  │
│              ▼               ▼               ▼                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   Handler 1  │  │   Handler 2  │  │   Handler N  │          │
│  │  (Session 1) │  │  (Session 2) │  │  (Session N) │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Association Handler V2

### 3.1 DES-NET-007: Association Handler V2

**Traces to:** SRS-NET-002 (Association State Machine)

**File:** `include/pacs/network/v2/dicom_association_handler.hpp`, `src/network/v2/dicom_association_handler.cpp`

```cpp
namespace pacs::network::v2 {

class dicom_association_handler : public std::enable_shared_from_this<dicom_association_handler> {
public:
    dicom_association_handler(
        std::shared_ptr<messaging_session> session,
        service_registry& services,
        std::shared_ptr<pdu_buffer_pool> buffer_pool);

    // Start handling
    void start();
    void stop();

    // State
    [[nodiscard]] auto state() const noexcept -> association_state;
    [[nodiscard]] auto calling_ae() const -> const std::string&;
    [[nodiscard]] auto called_ae() const -> const std::string&;

private:
    // PDU handling
    void on_pdu_received(const std::vector<uint8_t>& data);
    void handle_associate_rq(const pdu::a_associate_rq& request);
    void handle_p_data_tf(const pdu::p_data_tf& pdata);
    void handle_release_rq();
    void handle_abort(const pdu::a_abort& abort);

    // DIMSE dispatching
    void dispatch_dimse(presentation_context_id pc_id,
                        const dimse_message& message);

    std::shared_ptr<messaging_session> session_;
    service_registry& services_;
    std::shared_ptr<pdu_buffer_pool> buffer_pool_;
    association_state state_{association_state::idle};
    std::vector<presentation_context> accepted_contexts_;
};

} // namespace pacs::network::v2
```

---

## 4. Accept Worker

### 4.1 DES-NET-008: Accept Worker

**Traces to:** SRS-INT-004 (thread_system Integration)

**File:** `include/pacs/network/detail/accept_worker.hpp`, `src/network/detail/accept_worker.cpp`

```cpp
namespace pacs::network::detail {

class accept_worker : public kcenon::thread::thread_base {
public:
    accept_worker(
        std::string name,
        std::function<void(socket_t)> on_accept,
        socket_t listen_socket);

    ~accept_worker() override;

protected:
    // thread_base implementation
    void run(std::stop_token stop_token) override;

private:
    void accept_loop(std::stop_token stop_token);

    std::function<void(socket_t)> on_accept_;
    socket_t listen_socket_;
};

} // namespace pacs::network::detail
```

**Threading Model:**
- Inherits from `thread_base` for jthread integration
- Uses `std::stop_token` for cooperative cancellation
- Clean shutdown in ~110ms with active connections

---

## 5. PDU Buffer Pool

### 5.1 DES-NET-009: PDU Buffer Pool

**Traces to:** SRS-PERF-005 (Memory Usage)

**File:** `include/pacs/network/pdu_buffer_pool.hpp`, `src/network/pdu_buffer_pool.cpp`

```cpp
namespace pacs::network {

class pdu_buffer_pool {
public:
    explicit pdu_buffer_pool(std::size_t initial_size = 16,
                             std::size_t max_size = 1024);

    // Acquire buffer (may allocate if pool empty)
    [[nodiscard]] auto acquire(std::size_t min_capacity = 16384)
        -> std::shared_ptr<std::vector<uint8_t>>;

    // Release buffer back to pool
    void release(std::shared_ptr<std::vector<uint8_t>> buffer);

    // Statistics
    [[nodiscard]] auto pool_size() const noexcept -> std::size_t;
    [[nodiscard]] auto allocations() const noexcept -> std::uint64_t;
    [[nodiscard]] auto reuses() const noexcept -> std::uint64_t;

private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<std::vector<uint8_t>>> pool_;
    std::size_t max_size_;
    std::atomic<uint64_t> allocations_{0};
    std::atomic<uint64_t> reuses_{0};
};

} // namespace pacs::network
```

**Memory Optimization:**
- Pre-allocated buffer pool reduces GC pressure
- Reuses buffers across PDU operations
- Automatic expansion up to max_size

---

## 6. Traceability

### 6.1 Requirements to Design

| SRS ID | Design ID(s) | Implementation |
|--------|--------------|----------------|
| SRS-INT-003 | DES-NET-006 | DICOM Server V2 with messaging_server |
| SRS-NET-002 | DES-NET-007 | Association state machine handler |
| SRS-INT-004 | DES-NET-008 | Accept worker with thread_base |
| SRS-PERF-005 | DES-NET-009 | PDU buffer pooling |

---

*Document Version: 1.0.0*
*Created: 2026-01-04*
*Author: kcenon@naver.com*
