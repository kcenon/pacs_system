---
doc_id: "PAC-API-002-NETWORK"
doc_title: "API Reference - Network Module"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "API"
---

# API Reference - Network Module

> **Part of**: [API Reference Index](API_REFERENCE.md)
> **Covers**: DIMSE protocol, association state machine, PDU, network_system v2 integration

> **Version:** 0.1.5.0
> **Last Updated:** 2025-12-13
> **Language:** **English**

---

## Table of Contents

- [Network Module](#network-module)
- [Network V2 Module (Optional)](#network-v2-module-optional)

---

## Network Module

### `pacs::network::association`

DICOM Association management.

```cpp
#include <pacs/network/association.h>

namespace pacs::network {

// Association configuration
struct association_config {
    std::string calling_ae_title;    // SCU AE Title
    std::string called_ae_title;     // SCP AE Title
    uint32_t max_pdu_size = 16384;   // Maximum PDU size
    std::chrono::seconds timeout{30}; // Association timeout

    // Add presentation context
    void add_context(
        const std::string& abstract_syntax,
        const std::vector<transfer_syntax>& transfer_syntaxes
    );
};

// Association state
enum class association_state {
    idle,
    awaiting_transport,
    awaiting_associate_response,
    awaiting_release_response,
    established,
    released,
    aborted
};

class association {
public:
    // Factory (SCU)
    static common::Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config
    );

    // Factory (SCP) - created by server
    // association is move-only
    association(association&&) noexcept;
    association& operator=(association&&) noexcept;
    ~association();

    // State
    association_state state() const noexcept;
    bool is_established() const noexcept;

    // Properties
    const std::string& calling_ae_title() const noexcept;
    const std::string& called_ae_title() const noexcept;
    uint32_t max_pdu_size() const noexcept;

    // Presentation context
    bool has_accepted_context(const std::string& abstract_syntax) const;
    std::optional<uint8_t> accepted_context_id(
        const std::string& abstract_syntax
    ) const;
    transfer_syntax accepted_transfer_syntax(uint8_t context_id) const;

    // DIMSE operations
    common::Result<void> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& message
    );

    common::Result<dimse::dimse_message> receive_dimse();

    // Lifecycle
    common::Result<void> release();
    void abort();
};

} // namespace pacs::network
```

---

### `pacs::network::dimse::dimse_message`

DIMSE message handling for both DIMSE-C and DIMSE-N services.

```cpp
#include <pacs/network/dimse/dimse_message.hpp>

namespace pacs::network::dimse {

class dimse_message {
public:
    // DIMSE-C Factory Methods
    static dimse_message make_c_echo_rq(uint16_t msg_id, std::string_view sop_class_uid);
    static dimse_message make_c_store_rq(uint16_t msg_id, ...);
    static dimse_message make_c_find_rq(uint16_t msg_id, ...);
    static dimse_message make_c_move_rq(uint16_t msg_id, ...);
    static dimse_message make_c_get_rq(uint16_t msg_id, ...);

    // DIMSE-N Factory Methods
    static dimse_message make_n_create_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid = ""
    );
    static dimse_message make_n_create_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    static dimse_message make_n_set_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid
    );
    static dimse_message make_n_set_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    static dimse_message make_n_get_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        const std::vector<core::dicom_tag>& attribute_list = {}
    );
    static dimse_message make_n_get_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    static dimse_message make_n_event_report_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t event_type_id
    );
    static dimse_message make_n_event_report_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t event_type_id,
        status_code status
    );

    static dimse_message make_n_action_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t action_type_id
    );
    static dimse_message make_n_action_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        uint16_t action_type_id,
        status_code status
    );

    static dimse_message make_n_delete_rq(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid
    );
    static dimse_message make_n_delete_rsp(
        uint16_t msg_id,
        std::string_view sop_class_uid,
        std::string_view sop_instance_uid,
        status_code status
    );

    // Data Set Accessors (Issue #488 - Result<T> pattern)
    [[nodiscard]] bool has_dataset() const noexcept;
    [[nodiscard]] pacs::Result<std::reference_wrapper<core::dicom_dataset>> dataset();
    [[nodiscard]] pacs::Result<std::reference_wrapper<const core::dicom_dataset>> dataset() const;
    void set_dataset(core::dicom_dataset ds);
    void clear_dataset() noexcept;

    // DIMSE-N Specific Accessors
    std::string requested_sop_class_uid() const;
    void set_requested_sop_class_uid(std::string_view uid);
    std::string requested_sop_instance_uid() const;
    void set_requested_sop_instance_uid(std::string_view uid);
    std::optional<uint16_t> event_type_id() const;
    void set_event_type_id(uint16_t type_id);
    std::optional<uint16_t> action_type_id() const;
    void set_action_type_id(uint16_t type_id);
    std::vector<core::dicom_tag> attribute_identifier_list() const;
    void set_attribute_identifier_list(const std::vector<core::dicom_tag>& tags);
};

} // namespace pacs::network::dimse
```

**DIMSE-N Example:**
```cpp
using namespace pacs::network::dimse;

// N-CREATE: Create MPPS instance
auto create_rq = make_n_create_rq(1, mpps_sop_class_uid, generated_instance_uid);
create_rq.set_dataset(mpps_dataset);

// N-SET: Update MPPS status
auto set_rq = make_n_set_rq(2, mpps_sop_class_uid, mpps_instance_uid);
set_rq.set_dataset(status_update_dataset);

// N-EVENT-REPORT: Storage commitment notification
auto event_rq = make_n_event_report_rq(3, storage_commitment_sop_class, instance_uid, 1);
event_rq.set_dataset(commitment_result);

// N-ACTION: Request storage commitment
auto action_rq = make_n_action_rq(4, storage_commitment_sop_class, instance_uid, 1);
action_rq.set_dataset(commitment_request);

// N-GET: Retrieve specific attributes
std::vector<core::dicom_tag> attrs = {tags::PatientName, tags::PatientID};
auto get_rq = make_n_get_rq(5, printer_sop_class, printer_instance_uid, attrs);

// N-DELETE: Delete print job
auto delete_rq = make_n_delete_rq(6, print_job_sop_class, job_instance_uid);
```

**Example:**
```cpp
using namespace pacs::network;

// Configure association
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.add_context(
    sop_class::verification,
    {transfer_syntax::implicit_vr_little_endian()}
);

// Connect
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_err()) {
    std::cerr << "Connect failed: " << result.error().message << std::endl;
    return;
}

auto assoc = std::move(result.value());

// Perform C-ECHO
// ...

// Release
assoc.release();
```

---

## Network V2 Module (Optional)

### `pacs::network::v2::dicom_association_handler`

Bridge between network_system's messaging_session and DICOM protocol handling.
This class is part of the Phase 4 network_system integration effort.

```cpp
#include <pacs/network/v2/dicom_association_handler.hpp>

namespace pacs::network::v2 {

// Handler state machine
enum class handler_state {
    idle,              // Initial state, waiting for A-ASSOCIATE-RQ
    awaiting_response, // Sent response, awaiting next PDU
    established,       // Association established, processing DIMSE
    releasing,         // Graceful release in progress
    closed             // Association closed (released or aborted)
};

// Callback types
using association_established_callback = std::function<void(
    const std::string& session_id,
    const std::string& calling_ae,
    const std::string& called_ae)>;

using association_closed_callback = std::function<void(
    const std::string& session_id,
    bool graceful)>;

using handler_error_callback = std::function<void(
    const std::string& session_id,
    const std::string& error)>;

class dicom_association_handler
    : public std::enable_shared_from_this<dicom_association_handler> {
public:
    using session_ptr = std::shared_ptr<network_system::session::messaging_session>;
    using service_map = std::map<std::string, services::scp_service*>;

    // Constants
    static constexpr size_t pdu_header_size = 6;
    static constexpr size_t max_pdu_size = 64 * 1024 * 1024;

    // Construction
    dicom_association_handler(
        session_ptr session,
        const server_config& config,
        const service_map& services);
    ~dicom_association_handler();

    // Non-copyable, non-movable
    dicom_association_handler(const dicom_association_handler&) = delete;
    dicom_association_handler& operator=(const dicom_association_handler&) = delete;

    // Lifecycle
    void start();                      // Begin processing session
    void stop(bool graceful = false);  // Stop handler

    // State queries
    handler_state state() const noexcept;
    bool is_established() const noexcept;
    bool is_closed() const noexcept;
    std::string session_id() const;
    std::string calling_ae() const;
    std::string called_ae() const;
    association& get_association();

    // Callbacks
    void set_established_callback(association_established_callback callback);
    void set_closed_callback(association_closed_callback callback);
    void set_error_callback(handler_error_callback callback);

    // Statistics
    uint64_t pdus_received() const noexcept;
    uint64_t pdus_sent() const noexcept;
    uint64_t messages_processed() const noexcept;
};

} // namespace pacs::network::v2
```

**Example Usage:**

```cpp
#include <pacs/network/v2/dicom_association_handler.hpp>

using namespace pacs::network;
using namespace pacs::network::v2;

// Create handler for an incoming session
auto handler = std::make_shared<dicom_association_handler>(
    session,           // messaging_session from network_system
    server_config,     // Server configuration
    service_map        // SOP Class -> Service mapping
);

// Set up callbacks
handler->set_established_callback(
    [](const auto& session_id, const auto& calling, const auto& called) {
        std::cout << "Association: " << calling << " -> " << called << '\n';
    });

handler->set_closed_callback(
    [](const auto& session_id, bool graceful) {
        std::cout << "Closed: " << (graceful ? "graceful" : "aborted") << '\n';
    });

handler->set_error_callback(
    [](const auto& session_id, const auto& error) {
        std::cerr << "Error: " << error << '\n';
    });

// Start processing
handler->start();

// Handler will automatically:
// 1. Receive and validate A-ASSOCIATE-RQ
// 2. Send A-ASSOCIATE-AC or A-ASSOCIATE-RJ
// 3. Dispatch DIMSE messages to registered services
// 4. Handle A-RELEASE-RQ with A-RELEASE-RP
// 5. Handle A-ABORT
```

---

### `pacs::network::v2::dicom_server_v2`

DICOM server using network_system's messaging_server for connection management.
This is the recommended server implementation for new applications.

```cpp
#include <pacs/network/v2/dicom_server_v2.hpp>

namespace pacs::network::v2 {

class dicom_server_v2 {
public:
    // Type aliases
    using clock = std::chrono::steady_clock;
    using duration = std::chrono::milliseconds;
    using time_point = clock::time_point;
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

    // Service registration (call before start)
    void register_service(services::scp_service_ptr service);
    std::vector<std::string> supported_sop_classes() const;

    // Lifecycle
    Result<std::monostate> start();
    void stop(duration timeout = std::chrono::seconds{30});
    void wait_for_shutdown();

    // Status
    bool is_running() const noexcept;
    size_t active_associations() const noexcept;
    server_statistics get_statistics() const;
    const server_config& config() const noexcept;

    // Callbacks
    void on_association_established(association_established_callback callback);
    void on_association_closed(association_closed_callback callback);
    void on_error(error_callback callback);
};

} // namespace pacs::network::v2
```

**Usage Example:**

```cpp
#include <pacs/network/v2/dicom_server_v2.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/services/storage_scp.hpp>

using namespace pacs::network;
using namespace pacs::network::v2;
using namespace pacs::services;

// Configure server
server_config config;
config.ae_title = "MY_PACS";
config.port = 11112;
config.max_associations = 20;
config.idle_timeout = std::chrono::minutes{5};

// Create server
dicom_server_v2 server{config};

// Register services
server.register_service(std::make_unique<verification_scp>());
server.register_service(std::make_unique<storage_scp>("/path/to/storage"));

// Set callbacks
server.on_association_established(
    [](const std::string& session_id,
       const std::string& calling_ae,
       const std::string& called_ae) {
        std::cout << "Association: " << calling_ae << " -> " << called_ae << '\n';
    });

server.on_association_closed(
    [](const std::string& session_id, bool graceful) {
        std::cout << "Session " << session_id << " closed "
                  << (graceful ? "gracefully" : "abruptly") << '\n';
    });

server.on_error([](const std::string& error) {
    std::cerr << "Server error: " << error << '\n';
});

// Start server
auto result = server.start();
if (result.is_err()) {
    std::cerr << "Failed to start: " << result.error().message << '\n';
    return 1;
}

std::cout << "Server running on port " << config.port << '\n';

// Wait for shutdown signal
server.wait_for_shutdown();
```

**Key Features:**

- **API-compatible** with existing `dicom_server` for easy migration
- **Automatic session management** via network_system callbacks
- **Thread-safe** handler map with proper synchronization
- **Configurable limits** for max associations with automatic rejection
- **Idle timeout support** for cleaning up inactive connections

**Migration from dicom_server:**

1. Replace `dicom_server` with `dicom_server_v2`
2. No changes needed to service registration or callbacks
3. Requires `PACS_WITH_NETWORK_SYSTEM` compile definition

---
