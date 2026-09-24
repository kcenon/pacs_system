---
doc_id: "PAC-API-002-WEB"
doc_title: "API Reference - Web, AI & Common Modules"
doc_version: "0.1.5.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "API"
---

# API Reference - Web, AI & Common Modules

> **Part of**: [API Reference Index](API_REFERENCE.md)
> **Covers**: REST API server, DICOMweb (WADO-RS/STOW-RS/QIDO-RS), AI service connector and result handler, common error codes and UID generator

> **Version:** 0.1.5.0
> **Last Updated:** 2025-12-13
> **Language:** **English**

---

## Table of Contents

- [AI Module](#ai-module)
- [Common Module](#common-module)
- [Web Module](#web-module)

---

## AI Module

### `pacs::ai::ai_result_handler`

Handler for AI-generated DICOM objects including Structured Reports (SR), Segmentation objects (SEG), and Presentation States (PR).

```cpp
#include <pacs/ai/ai_result_handler.hpp>

namespace pacs::ai {

// Result types
template <typename T>
using Result = kcenon::common::Result<T>;
using VoidResult = kcenon::common::VoidResult;

// AI result type enumeration
enum class ai_result_type {
    structured_report,    // DICOM SR (Structured Report)
    segmentation,         // DICOM SEG (Segmentation)
    presentation_state    // DICOM PR (Presentation State)
};

// Validation status enumeration
enum class validation_status {
    valid,                    // All validations passed
    missing_required_tags,    // Required DICOM tags are missing
    invalid_reference,        // Referenced source images not found
    invalid_template,         // SR template conformance failed
    invalid_segment_data,     // Segmentation data is malformed
    unknown_error             // Unexpected validation error
};

// AI result information structure
struct ai_result_info {
    std::string sop_instance_uid;           // SOP Instance UID of the AI result
    ai_result_type type;                    // Type of AI result
    std::string sop_class_uid;              // SOP Class UID
    std::string series_instance_uid;        // Series Instance UID
    std::string source_study_uid;           // Study Instance UID of source study
    std::string algorithm_name;             // AI model/algorithm identifier
    std::string algorithm_version;          // Algorithm version
    std::chrono::system_clock::time_point received_at;  // Reception timestamp
    std::optional<std::string> description; // Optional description
};

// Source reference structure
struct source_reference {
    std::string study_instance_uid;
    std::optional<std::string> series_instance_uid;
    std::vector<std::string> sop_instance_uids;
};

// CAD finding from Structured Report
struct cad_finding {
    std::string finding_type;               // Finding type/category
    std::string location;                   // Location/site description
    std::optional<double> confidence;       // Confidence score (0.0 to 1.0)
    std::optional<std::string> measurement; // Additional measurement data
    std::optional<std::string> referenced_sop_instance_uid;
};

// Segment information from Segmentation
struct segment_info {
    uint16_t segment_number;                // Segment number (1-based)
    std::string segment_label;              // Segment label
    std::optional<std::string> description; // Segment description
    std::string algorithm_type;             // Algorithm type
    std::optional<std::tuple<uint8_t, uint8_t, uint8_t>> recommended_display_color;
};

// Validation result
struct validation_result {
    validation_status status;
    std::optional<std::string> error_message;
    std::vector<std::string> missing_tags;
    std::vector<std::string> invalid_references;
};

// Handler configuration
struct ai_handler_config {
    bool validate_source_references = true;
    bool validate_sr_templates = true;
    bool auto_link_to_source = true;
    std::vector<std::string> accepted_sr_templates;
    uint16_t max_segments = 256;
};

// Callback types
using ai_result_received_callback = std::function<void(const ai_result_info& info)>;
using pre_store_validator = std::function<bool(
    const core::dicom_dataset& dataset,
    ai_result_type type)>;

// Main handler class
class ai_result_handler {
public:
    // Factory method
    [[nodiscard]] static auto create(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<storage::index_database> database)
        -> std::unique_ptr<ai_result_handler>;

    virtual ~ai_result_handler();

    // Configuration
    void configure(const ai_handler_config& config);
    [[nodiscard]] auto get_config() const -> ai_handler_config;
    void set_received_callback(ai_result_received_callback callback);
    void set_pre_store_validator(pre_store_validator validator);

    // Structured Report operations
    [[nodiscard]] auto receive_structured_report(const core::dicom_dataset& sr)
        -> VoidResult;
    [[nodiscard]] auto validate_sr_template(const core::dicom_dataset& sr)
        -> validation_result;
    [[nodiscard]] auto get_cad_findings(std::string_view sr_sop_instance_uid)
        -> Result<std::vector<cad_finding>>;

    // Segmentation operations
    [[nodiscard]] auto receive_segmentation(const core::dicom_dataset& seg)
        -> VoidResult;
    [[nodiscard]] auto validate_segmentation(const core::dicom_dataset& seg)
        -> validation_result;
    [[nodiscard]] auto get_segment_info(std::string_view seg_sop_instance_uid)
        -> Result<std::vector<segment_info>>;

    // Presentation State operations
    [[nodiscard]] auto receive_presentation_state(const core::dicom_dataset& pr)
        -> VoidResult;
    [[nodiscard]] auto validate_presentation_state(const core::dicom_dataset& pr)
        -> validation_result;

    // Source linking operations
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        std::string_view source_study_uid) -> VoidResult;
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        const source_reference& references) -> VoidResult;
    [[nodiscard]] auto get_source_reference(std::string_view result_uid)
        -> Result<source_reference>;

    // Query operations
    [[nodiscard]] auto find_ai_results_for_study(std::string_view study_instance_uid)
        -> Result<std::vector<ai_result_info>>;
    [[nodiscard]] auto find_ai_results_by_type(
        std::string_view study_instance_uid,
        ai_result_type type) -> Result<std::vector<ai_result_info>>;
    [[nodiscard]] auto get_ai_result_info(std::string_view sop_instance_uid)
        -> std::optional<ai_result_info>;
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool;

    // Removal operations
    [[nodiscard]] auto remove(std::string_view sop_instance_uid) -> VoidResult;
    [[nodiscard]] auto remove_ai_results_for_study(std::string_view study_instance_uid)
        -> Result<std::size_t>;

protected:
    ai_result_handler(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<storage::index_database> database);

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace pacs::ai
```

#### Usage Example

```cpp
#include <pacs/ai/ai_result_handler.hpp>
#include <pacs/storage/file_storage.hpp>
#include <pacs/storage/index_database.hpp>

// Create handler with storage and database
auto storage = std::make_shared<pacs::storage::file_storage>("/data/pacs");
auto database = std::make_shared<pacs::storage::index_database>("/data/pacs/index.db");
auto handler = pacs::ai::ai_result_handler::create(storage, database);

// Configure validation options
pacs::ai::ai_handler_config config;
config.validate_source_references = true;
config.auto_link_to_source = true;
handler->configure(config);

// Set notification callback
handler->set_received_callback([](const pacs::ai::ai_result_info& info) {
    std::cout << "Received AI result: " << info.sop_instance_uid
              << " (" << info.algorithm_name << ")\n";
});

// Receive AI-generated structured report
pacs::core::dicom_dataset sr_dataset;
// ... populate SR dataset with CAD findings ...
auto result = handler->receive_structured_report(sr_dataset);
if (result.is_ok()) {
    // Successfully stored
    auto findings = handler->get_cad_findings(sr_dataset.get_string(
        pacs::core::tags::sop_instance_uid));
    if (findings.is_ok()) {
        for (const auto& f : findings.value()) {
            std::cout << "Finding: " << f.finding_type
                      << " at " << f.location << "\n";
        }
    }
}

// Query AI results for a study
auto ai_results = handler->find_ai_results_for_study("1.2.3.4.5.6.7.8.9");
if (ai_results.is_ok()) {
    for (const auto& info : ai_results.value()) {
        std::cout << "AI Result: " << info.sop_instance_uid
                  << " Type: " << static_cast<int>(info.type) << "\n";
    }
}
```

---

### `pacs::ai::ai_service_connector`

Connector for external AI inference services with support for async job submission, status tracking, and result retrieval.

```cpp
#include <pacs/ai/ai_service_connector.hpp>

namespace pacs::ai {

// Result type alias
template <typename T>
using Result = kcenon::common::Result<T>;

// Authentication types for AI service connection
enum class authentication_type {
    none,           // No authentication
    api_key,        // API key in header
    bearer_token,   // Bearer token (JWT)
    basic           // HTTP Basic authentication
};

// Inference job status codes
enum class inference_status_code {
    pending,    // Job is queued
    running,    // Job is being processed
    completed,  // Job completed successfully
    failed,     // Job failed
    cancelled,  // Job was cancelled
    timeout     // Job timed out
};

// AI service configuration
struct ai_service_config {
    std::string base_url;                           // AI service endpoint
    std::string service_name{"ai_service"};         // Service identifier for logging
    authentication_type auth_type{authentication_type::none};
    std::string api_key;                            // For api_key auth
    std::string bearer_token;                       // For bearer_token auth
    std::string username;                           // For basic auth
    std::string password;                           // For basic auth
    std::chrono::seconds connection_timeout{30};    // Connection timeout
    std::chrono::seconds request_timeout{300};      // Request timeout
    std::size_t max_retries{3};                     // Max retry attempts
    std::chrono::seconds retry_delay{5};            // Delay between retries
    bool enable_metrics{true};                      // Enable performance metrics
    bool enable_tracing{true};                      // Enable distributed tracing
};

// Inference request structure
struct inference_request {
    std::string study_instance_uid;                 // Required: Study UID
    std::optional<std::string> series_instance_uid; // Optional: Series UID
    std::string model_id;                           // Required: AI model identifier
    std::map<std::string, std::string> parameters;  // Model-specific parameters
    int priority{0};                                // Job priority (higher = more urgent)
    std::optional<std::string> callback_url;        // Webhook for completion
    std::map<std::string, std::string> metadata;    // Custom metadata
};

// Inference status structure
struct inference_status {
    std::string job_id;                             // Unique job identifier
    inference_status_code status;                   // Current status
    int progress{0};                                // Progress 0-100
    std::optional<std::string> message;             // Status message
    std::optional<std::string> result_study_uid;    // Result study UID if completed
    std::optional<std::string> error_code;          // Error code if failed
    std::chrono::system_clock::time_point submitted_at;
    std::optional<std::chrono::system_clock::time_point> started_at;
    std::optional<std::chrono::system_clock::time_point> completed_at;
};

// AI model information
struct model_info {
    std::string model_id;                           // Model identifier
    std::string name;                               // Display name
    std::string version;                            // Model version
    std::string description;                        // Model description
    std::vector<std::string> supported_modalities;  // e.g., ["CT", "MR"]
    std::vector<std::string> output_types;          // e.g., ["SR", "SEG"]
    bool available{true};                           // Model availability
};

// Main connector class (Singleton pattern with static methods)
class ai_service_connector {
public:
    // Initialization and lifecycle
    [[nodiscard]] static auto initialize(const ai_service_config& config)
        -> Result<std::monostate>;
    static void shutdown();
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

    // Inference operations
    [[nodiscard]] static auto request_inference(const inference_request& request)
        -> Result<std::string>;  // Returns job_id
    [[nodiscard]] static auto check_status(const std::string& job_id)
        -> Result<inference_status>;
    [[nodiscard]] static auto cancel(const std::string& job_id)
        -> Result<std::monostate>;
    [[nodiscard]] static auto list_active_jobs()
        -> Result<std::vector<inference_status>>;

    // Model discovery
    [[nodiscard]] static auto list_models()
        -> Result<std::vector<model_info>>;
    [[nodiscard]] static auto get_model_info(const std::string& model_id)
        -> Result<model_info>;

    // Health and diagnostics
    [[nodiscard]] static auto check_health() -> bool;
    [[nodiscard]] static auto get_latency()
        -> std::optional<std::chrono::milliseconds>;

    // Configuration
    [[nodiscard]] static auto update_credentials(
        authentication_type auth_type,
        const std::string& credential) -> Result<std::monostate>;
    [[nodiscard]] static auto get_config() -> const ai_service_config&;

private:
    class impl;
    static std::unique_ptr<impl> pimpl_;
};

// Helper functions
[[nodiscard]] auto to_string(inference_status_code status) -> std::string;
[[nodiscard]] auto to_string(authentication_type auth) -> std::string;

} // namespace pacs::ai
```

**Design Decisions:**

| Decision | Rationale |
|----------|-----------|
| Static singleton pattern | Global access, consistent with other adapters |
| PIMPL pattern | ABI stability, hidden implementation details |
| Result<T> return type | Consistent error handling with common_system |
| Async job model | Long-running inference tasks with status tracking |
| Metrics integration | Performance monitoring via monitoring_adapter |

**Thread Safety:**

All public methods are thread-safe. Internal synchronization uses:
- `std::mutex` for job tracking state
- `std::shared_mutex` for read-heavy operations
- `std::atomic<bool>` for initialization flag

**Usage Example:**

```cpp
#include <pacs/ai/ai_service_connector.hpp>
#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

using namespace pacs::ai;

// Initialize dependencies first
pacs::integration::logger_adapter::initialize({});
pacs::integration::monitoring_adapter::initialize({});

// Configure AI service
ai_service_config config;
config.base_url = "https://ai-service.example.com/api/v1";
config.service_name = "radiology_ai";
config.auth_type = authentication_type::api_key;
config.api_key = "your-api-key-here";
config.request_timeout = std::chrono::minutes(5);

// Initialize connector
auto init_result = ai_service_connector::initialize(config);
if (init_result.is_err()) {
    std::cerr << "Failed to initialize: " << init_result.error().message << "\n";
    return 1;
}

// Submit inference request
inference_request request;
request.study_instance_uid = "1.2.840.10008.5.1.4.1.1.2.1";
request.model_id = "chest-xray-nodule-detector-v2";
request.parameters["threshold"] = "0.7";
request.parameters["output_format"] = "SR";
request.priority = 5;

auto submit_result = ai_service_connector::request_inference(request);
if (submit_result.is_err()) {
    std::cerr << "Submission failed: " << submit_result.error().message << "\n";
    return 1;
}

std::string job_id = submit_result.value();
std::cout << "Job submitted: " << job_id << "\n";

// Poll for completion
while (true) {
    auto status_result = ai_service_connector::check_status(job_id);
    if (status_result.is_err()) {
        std::cerr << "Status check failed\n";
        break;
    }

    auto& status = status_result.value();
    std::cout << "Progress: " << status.progress << "%\n";

    if (status.status == inference_status_code::completed) {
        std::cout << "Result available: " << *status.result_study_uid << "\n";
        break;
    } else if (status.status == inference_status_code::failed) {
        std::cerr << "Job failed: " << *status.error_code << "\n";
        break;
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
}

// Cleanup
ai_service_connector::shutdown();
```

---
## Common Module

### Error Codes

```cpp
#include <pacs/core/result.hpp>

namespace pacs::error_codes {

// ================================================================
// PACS-specific error codes (-700 to -799)
// ================================================================
constexpr int pacs_base = -700;

// DICOM File Errors (-700 to -719)
constexpr int file_not_found = pacs_base - 0;              // -700
constexpr int file_read_error = pacs_base - 1;             // -701
constexpr int file_write_error = pacs_base - 2;            // -702
constexpr int invalid_dicom_file = pacs_base - 3;          // -703
constexpr int missing_dicm_prefix = pacs_base - 4;         // -704

// DICOM Element Errors (-720 to -739)
constexpr int element_not_found = pacs_base - 20;          // -720
constexpr int value_conversion_error = pacs_base - 21;     // -721
constexpr int invalid_vr = pacs_base - 22;                 // -722

// Encoding/Decoding Errors (-740 to -759)
constexpr int decode_error = pacs_base - 40;               // -740
constexpr int encode_error = pacs_base - 41;               // -741
constexpr int compression_error = pacs_base - 42;          // -742

// Network/Association Errors (-760 to -779)
constexpr int association_rejected = pacs_base - 60;       // -760
constexpr int association_aborted = pacs_base - 61;        // -761
constexpr int connection_failed = pacs_base - 64;          // -764

// Storage Errors (-780 to -799)
constexpr int storage_failed = pacs_base - 80;             // -780
constexpr int database_open_error = pacs_base - 83;        // -783
constexpr int bucket_not_found = pacs_base - 90;           // -790

// ================================================================
// Service-specific error codes (-800 to -899)
// ================================================================
constexpr int service_base = -800;

// C-STORE Service Errors (-800 to -819)
constexpr int store_handler_not_set = service_base - 0;    // -800

// See include/pacs/core/result.hpp for the complete error code list.

} // namespace pacs::error_codes
```

---

### UID Generator

```cpp
#include <pacs/common/uid_generator.h>

namespace pacs::common {

class uid_generator {
public:
    // Generate new UID
    static std::string generate();

    // Generate with prefix (organization root)
    static std::string generate(const std::string& root);

    // Validate UID format
    static bool is_valid(const std::string& uid);
};

} // namespace pacs::common
```

---
## Web Module

### `pacs::web::rest_server`

REST API server implementation using Crow framework.

```cpp
#include <pacs/web/rest_server.hpp>

namespace pacs::web {

class rest_server {
public:
    // Construction
    explicit rest_server(const rest_server_config& config);
    ~rest_server();

    // Lifecycle
    void start();        // Blocking
    void start_async();  // Non-blocking
    void stop();         // Graceful shutdown

    // Configuration
    void set_health_checker(std::shared_ptr<monitoring::health_checker> checker);

    // Status
    bool is_running() const;
    uint16_t port() const;
};

} // namespace pacs::web
```

### `pacs::web::rest_server_config`

Configuration for the REST server.

```cpp
#include <pacs/web/rest_server_config.hpp>

namespace pacs::web {

struct rest_server_config {
    uint16_t port = 8080;
    size_t concurrency = 2;
    bool enable_cors = true;
    std::string bind_address = "0.0.0.0";
    
    // TLS options (future)
    bool use_tls = false;
    std::string cert_file;
    std::string key_file;
};

} // namespace pacs::web
```

### DICOMweb (WADO-RS) API

The DICOMweb module implements the WADO-RS (Web Access to DICOM Objects - RESTful) specification
as defined in DICOM PS3.18 for retrieving DICOM objects over HTTP.

#### `pacs::web::dicomweb::multipart_builder`

Builds multipart/related MIME responses for returning multiple DICOM objects.

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

class multipart_builder {
public:
    // Construction (default content type: application/dicom)
    explicit multipart_builder(std::string_view content_type = media_type::dicom);

    // Add parts
    void add_part(std::vector<uint8_t> data,
                  std::optional<std::string_view> content_type = std::nullopt);
    void add_part_with_location(std::vector<uint8_t> data,
                                std::string_view location,
                                std::optional<std::string_view> content_type = std::nullopt);

    // Build response
    [[nodiscard]] auto build() const -> std::string;
    [[nodiscard]] auto content_type_header() const -> std::string;
    [[nodiscard]] auto boundary() const -> std::string_view;

    // Status
    [[nodiscard]] auto empty() const noexcept -> bool;
    [[nodiscard]] auto size() const noexcept -> size_t;
};

} // namespace pacs::web::dicomweb
```

#### Utility Functions

```cpp
namespace pacs::web::dicomweb {

// Parse Accept header into structured format, sorted by quality
[[nodiscard]] auto parse_accept_header(std::string_view accept_header)
    -> std::vector<accept_info>;

// Check if a media type is acceptable based on parsed Accept header
[[nodiscard]] auto is_acceptable(const std::vector<accept_info>& accept_infos,
                                  std::string_view media_type) -> bool;

// Convert DICOM dataset to DicomJSON format
[[nodiscard]] auto dataset_to_dicom_json(
    const core::dicom_dataset& dataset,
    bool include_bulk_data = false,
    std::string_view bulk_data_uri_prefix = "") -> std::string;

// Check if a DICOM tag contains bulk data (Pixel Data, etc.)
[[nodiscard]] auto is_bulk_data_tag(uint32_t tag) -> bool;

} // namespace pacs::web::dicomweb
```

#### Media Type Constants

```cpp
namespace pacs::web::dicomweb {

struct media_type {
    static constexpr std::string_view dicom = "application/dicom";
    static constexpr std::string_view dicom_json = "application/dicom+json";
    static constexpr std::string_view dicom_xml = "application/dicom+xml";
    static constexpr std::string_view octet_stream = "application/octet-stream";
    static constexpr std::string_view jpeg = "image/jpeg";
    static constexpr std::string_view png = "image/png";
    static constexpr std::string_view multipart_related = "multipart/related";
};

} // namespace pacs::web::dicomweb
```

#### REST API Endpoints (WADO-RS)

**Base Path**: `/dicomweb`

##### Retrieve Study
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>`
*   **Description**: Retrieve all DICOM instances of a study.
*   **Accept Headers**:
    *   `application/dicom` (default)
    *   `multipart/related; type="application/dicom"`
    *   `*/*`
*   **Responses**:
    *   `200 OK`: Multipart response with DICOM instances.
    *   `404 Not Found`: Study not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Study Metadata
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/metadata`
*   **Description**: Retrieve metadata for all instances in a study.
*   **Accept Headers**:
    *   `application/dicom+json` (default)
*   **Response Body**:
    ```json
    [
      {
        "00080018": { "vr": "UI", "Value": ["1.2.840..."] },
        "00100010": { "vr": "PN", "Value": [{ "Alphabetic": "DOE^JOHN" }] }
      }
    ]
    ```
*   **Responses**:
    *   `200 OK`: DicomJSON array of instance metadata.
    *   `404 Not Found`: Study not found.

##### Retrieve Series
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>`
*   **Description**: Retrieve all DICOM instances of a series.
*   **Accept Headers**: Same as Retrieve Study.
*   **Responses**:
    *   `200 OK`: Multipart response with DICOM instances.
    *   `404 Not Found`: Series not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Series Metadata
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/metadata`
*   **Description**: Retrieve metadata for all instances in a series.
*   **Accept Headers**: `application/dicom+json` (default)
*   **Responses**:
    *   `200 OK`: DicomJSON array of instance metadata.
    *   `404 Not Found`: Series not found.

##### Retrieve Instance
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>`
*   **Description**: Retrieve a single DICOM instance.
*   **Accept Headers**:
    *   `application/dicom` (default)
    *   `multipart/related; type="application/dicom"`
    *   `*/*`
*   **Responses**:
    *   `200 OK`: Multipart response with single DICOM instance.
    *   `404 Not Found`: Instance not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Instance Metadata
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/metadata`
*   **Description**: Retrieve metadata for a single instance.
*   **Accept Headers**: `application/dicom+json` (default)
*   **Response Body**:
    ```json
    [
      {
        "00080016": { "vr": "UI", "Value": ["1.2.840.10008.5.1.4.1.1.2"] },
        "00080018": { "vr": "UI", "Value": ["1.2.840...instance"] },
        "00100010": { "vr": "PN", "Value": [{ "Alphabetic": "DOE^JOHN" }] },
        "7FE00010": { "vr": "OW", "BulkDataURI": "/dicomweb/studies/.../bulkdata/..." }
      }
    ]
    ```
*   **Responses**:
    *   `200 OK`: DicomJSON array with single instance metadata.
    *   `404 Not Found`: Instance not found.

##### Retrieve Frame
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/frames/<frameList>`
*   **Description**: Retrieve specific frame(s) from a multi-frame DICOM instance.
*   **Path Parameters**:
    *   `frameList`: Comma-separated frame numbers or ranges (1-based). Examples:
        *   `1` - Single frame
        *   `1,3,5` - Multiple frames
        *   `1-5` - Frame range (1, 2, 3, 4, 5)
        *   `1,3-5,7` - Mixed notation
*   **Accept Headers**:
    *   `application/octet-stream` (default) - Raw pixel data
    *   `multipart/related; type="application/octet-stream"` - Multiple frames
*   **Responses**:
    *   `200 OK`: Multipart response with frame pixel data.
    *   `400 Bad Request`: Invalid frame list syntax.
    *   `404 Not Found`: Instance not found or frame number out of range.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Rendered Image
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/rendered`
*   **Description**: Retrieve a rendered (consumer-ready) image from a DICOM instance.
*   **Query Parameters**:
    *   `window-center` (optional): VOI LUT window center value
    *   `window-width` (optional): VOI LUT window width value
    *   `quality` (optional): JPEG quality (1-100, default 75)
    *   `viewport` (optional): Output size as `width,height`
    *   `frame` (optional): Frame number for multi-frame images (1-based, default 1)
*   **Accept Headers**:
    *   `image/jpeg` (default) - JPEG output
    *   `image/png` - PNG output
    *   `*/*` - JPEG (default)
*   **Responses**:
    *   `200 OK`: Rendered image in requested format.
    *   `404 Not Found`: Instance not found.
    *   `406 Not Acceptable`: Requested media type not supported.

##### Retrieve Frame Rendered Image
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances/<sopInstanceUID>/frames/<frameNumber>/rendered`
*   **Description**: Retrieve a rendered image of a specific frame from a multi-frame DICOM instance.
*   **Query Parameters**: Same as Retrieve Rendered Image (except `frame`).
*   **Accept Headers**: Same as Retrieve Rendered Image.
*   **Responses**:
    *   `200 OK`: Rendered image in requested format.
    *   `400 Bad Request`: Invalid frame number.
    *   `404 Not Found`: Instance not found or frame out of range.
    *   `406 Not Acceptable`: Requested media type not supported.

#### Frame Retrieval Utilities

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

/**
 * @brief Parse frame numbers from URL path
 * @param frame_list Comma-separated frame numbers (e.g., "1,3,5" or "1-5")
 * @return Vector of frame numbers (1-based), empty on parse error
 */
[[nodiscard]] auto parse_frame_numbers(std::string_view frame_list)
    -> std::vector<uint32_t>;

/**
 * @brief Extract a single frame from pixel data
 * @param pixel_data Complete pixel data buffer
 * @param frame_number Frame number to extract (1-based)
 * @param frame_size Size of each frame in bytes
 * @return Frame data, or empty vector if frame doesn't exist
 */
[[nodiscard]] auto extract_frame(
    std::span<const uint8_t> pixel_data,
    uint32_t frame_number,
    size_t frame_size) -> std::vector<uint8_t>;

} // namespace pacs::web::dicomweb
```

#### Rendered Image Utilities

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

/**
 * @brief Rendered image output format
 */
enum class rendered_format {
    jpeg,   ///< JPEG format (default)
    png     ///< PNG format
};

/**
 * @brief Parameters for rendered image requests
 */
struct rendered_params {
    rendered_format format{rendered_format::jpeg};  ///< Output format
    int quality{75};                                 ///< JPEG quality (1-100)
    std::optional<double> window_center;             ///< VOI LUT window center
    std::optional<double> window_width;              ///< VOI LUT window width
    uint16_t viewport_width{0};                      ///< Output width (0 = original)
    uint16_t viewport_height{0};                     ///< Output height (0 = original)
    uint32_t frame{1};                               ///< Frame number (1-based)
    std::optional<std::string> presentation_state_uid;  ///< Optional GSPS UID
    bool burn_annotations{false};                    ///< Burn-in annotations
};

/**
 * @brief Result of a rendering operation
 */
struct rendered_result {
    std::vector<uint8_t> data;      ///< Rendered image data
    std::string content_type;        ///< MIME type of result
    bool success{false};             ///< Whether rendering succeeded
    std::string error_message;       ///< Error message if failed

    [[nodiscard]] static rendered_result ok(std::vector<uint8_t> data,
                                            std::string content_type);
    [[nodiscard]] static rendered_result error(std::string msg);
};

/**
 * @brief Parse rendered image parameters from HTTP request
 */
[[nodiscard]] auto parse_rendered_params(
    std::string_view query_string,
    std::string_view accept_header) -> rendered_params;

/**
 * @brief Apply window/level transformation to pixel data
 */
[[nodiscard]] auto apply_window_level(
    std::span<const uint8_t> pixel_data,
    uint16_t width, uint16_t height,
    uint16_t bits_stored, bool is_signed,
    double window_center, double window_width,
    double rescale_slope, double rescale_intercept) -> std::vector<uint8_t>;

/**
 * @brief Render a DICOM image to consumer format
 */
[[nodiscard]] auto render_dicom_image(
    std::string_view file_path,
    const rendered_params& params) -> rendered_result;

} // namespace pacs::web::dicomweb
```

### STOW-RS (Store Over the Web) API

The STOW-RS module implements the Store Over the Web - RESTful specification
as defined in DICOM PS3.18 for storing DICOM objects via HTTP.

#### `pacs::web::dicomweb::multipart_parser`

Parses multipart/related request bodies for STOW-RS uploads.

```cpp
#include <pacs/web/endpoints/dicomweb_endpoints.hpp>

namespace pacs::web::dicomweb {

struct multipart_part {
    std::string content_type;       // Content-Type of this part
    std::string content_location;   // Content-Location header (optional)
    std::string content_id;         // Content-ID header (optional)
    std::vector<uint8_t> data;      // Binary data of this part
};

class multipart_parser {
public:
    struct parse_error {
        std::string code;       // Error code (e.g., "INVALID_BOUNDARY")
        std::string message;    // Human-readable error message
    };

    struct parse_result {
        std::vector<multipart_part> parts;  // Parsed parts (empty on error)
        std::optional<parse_error> error;   // Error if parsing failed

        [[nodiscard]] bool success() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
    };

    // Parse a multipart/related request body
    [[nodiscard]] static auto parse(std::string_view content_type,
                                    std::string_view body) -> parse_result;

    // Extract boundary from Content-Type header
    [[nodiscard]] static auto extract_boundary(std::string_view content_type)
        -> std::optional<std::string>;

    // Extract type parameter from Content-Type header
    [[nodiscard]] static auto extract_type(std::string_view content_type)
        -> std::optional<std::string>;
};

} // namespace pacs::web::dicomweb
```

#### Store Response Types

```cpp
namespace pacs::web::dicomweb {

struct store_instance_result {
    bool success = false;                   // Whether storage succeeded
    std::string sop_class_uid;              // SOP Class UID of the instance
    std::string sop_instance_uid;           // SOP Instance UID of the instance
    std::string retrieve_url;               // URL to retrieve the stored instance
    std::optional<std::string> error_code;  // Error code if failed
    std::optional<std::string> error_message; // Error message if failed
};

struct store_response {
    std::vector<store_instance_result> referenced_instances;  // Successfully stored
    std::vector<store_instance_result> failed_instances;      // Failed to store

    [[nodiscard]] bool all_success() const noexcept;
    [[nodiscard]] bool all_failed() const noexcept;
    [[nodiscard]] bool partial_success() const noexcept;
};

struct validation_result {
    bool valid = true;                      // Whether validation passed
    std::string error_code;                 // Error code if invalid
    std::string error_message;              // Error message if invalid

    [[nodiscard]] explicit operator bool() const noexcept;

    static validation_result ok();
    static validation_result error(std::string code, std::string message);
};

} // namespace pacs::web::dicomweb
```

#### STOW-RS Utility Functions

```cpp
namespace pacs::web::dicomweb {

// Validate a DICOM instance for STOW-RS storage
[[nodiscard]] auto validate_instance(
    const core::dicom_dataset& dataset,
    std::optional<std::string_view> target_study_uid = std::nullopt)
    -> validation_result;

// Build STOW-RS response in DicomJSON format
[[nodiscard]] auto build_store_response_json(
    const store_response& response,
    std::string_view base_url) -> std::string;

} // namespace pacs::web::dicomweb
```

#### REST API Endpoints (STOW-RS)

**Base Path**: `/dicomweb`

##### Store Instances (Study-Independent)
*   **Method**: `POST`
*   **Path**: `/studies`
*   **Description**: Store DICOM instances without specifying a target study.
*   **Content-Type**: `multipart/related; type="application/dicom"; boundary=...`
*   **Request Body**: Multipart/related with DICOM Part 10 files.
    ```
    --boundary
    Content-Type: application/dicom

    <DICOM Part 10 binary data>
    --boundary
    Content-Type: application/dicom

    <DICOM Part 10 binary data>
    --boundary--
    ```
*   **Responses**:
    *   `200 OK`: All instances stored successfully.
        ```json
        {
          "00081190": { "vr": "UR", "Value": ["/dicomweb/studies/1.2.3"] },
          "00081199": {
            "vr": "SQ",
            "Value": [{
              "00081150": { "vr": "UI", "Value": ["1.2.840.10008.5.1.4.1.1.2"] },
              "00081155": { "vr": "UI", "Value": ["1.2.3.4.5.6.7.8.9"] },
              "00081190": { "vr": "UR", "Value": ["/dicomweb/studies/1.2.3/series/.../instances/..."] }
            }]
          }
        }
        ```
    *   `202 Accepted`: Some instances failed (partial success).
    *   `409 Conflict`: All instances failed.
    *   `415 Unsupported Media Type`: Invalid Content-Type.

##### Store Instances (Study-Targeted)
*   **Method**: `POST`
*   **Path**: `/studies/<studyUID>`
*   **Description**: Store DICOM instances to a specific study.
*   **Content-Type**: `multipart/related; type="application/dicom"; boundary=...`
*   **Request Body**: Same as Study-Independent store.
*   **Validation**: All instances must have Study Instance UID matching `<studyUID>`.
*   **Responses**:
    *   `200 OK`: All instances stored successfully.
    *   `202 Accepted`: Some instances failed (partial success).
    *   `409 Conflict`: All instances failed or Study UID mismatch.
    *   `415 Unsupported Media Type`: Invalid Content-Type.

### QIDO-RS (Query based on ID for DICOM Objects) API

The QIDO-RS module implements the Query based on ID for DICOM Objects - RESTful specification
as defined in DICOM PS3.18 for searching DICOM objects via HTTP.

#### QIDO-RS Utility Functions

```cpp
namespace pacs::web::dicomweb {

// Convert database records to DicomJSON format for QIDO-RS responses
[[nodiscard]] auto study_record_to_dicom_json(
    const storage::study_record& record,
    std::string_view patient_id,
    std::string_view patient_name) -> std::string;

[[nodiscard]] auto series_record_to_dicom_json(
    const storage::series_record& record,
    std::string_view study_uid) -> std::string;

[[nodiscard]] auto instance_record_to_dicom_json(
    const storage::instance_record& record,
    std::string_view series_uid,
    std::string_view study_uid) -> std::string;

// Parse QIDO-RS query parameters from HTTP request URL
[[nodiscard]] auto parse_study_query_params(
    const std::string& url_params) -> storage::study_query;

[[nodiscard]] auto parse_series_query_params(
    const std::string& url_params) -> storage::series_query;

[[nodiscard]] auto parse_instance_query_params(
    const std::string& url_params) -> storage::instance_query;

} // namespace pacs::web::dicomweb
```

#### REST API Endpoints (QIDO-RS)

**Base Path**: `/dicomweb`

##### Search Studies
*   **Method**: `GET`
*   **Path**: `/studies`
*   **Description**: Search for studies matching query parameters.
*   **Query Parameters**:
    *   `PatientID` or `00100020`: Patient ID (supports `*` wildcard)
    *   `PatientName` or `00100010`: Patient name (supports `*` wildcard)
    *   `StudyInstanceUID` or `0020000D`: Study Instance UID
    *   `StudyID` or `00200010`: Study ID
    *   `StudyDate` or `00080020`: Study date (YYYYMMDD or YYYYMMDD-YYYYMMDD for range)
    *   `AccessionNumber` or `00080050`: Accession number
    *   `ModalitiesInStudy` or `00080061`: Modalities in study
    *   `ReferringPhysicianName` or `00080090`: Referring physician
    *   `StudyDescription` or `00081030`: Study description
    *   `limit`: Maximum number of results (default: 100)
    *   `offset`: Pagination offset
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching studies.
    *   `503 Service Unavailable`: Database not configured.

##### Search Series
*   **Method**: `GET`
*   **Path**: `/series`
*   **Description**: Search for all series matching query parameters.
*   **Query Parameters**:
    *   `StudyInstanceUID` or `0020000D`: Study Instance UID
    *   `SeriesInstanceUID` or `0020000E`: Series Instance UID
    *   `Modality` or `00080060`: Modality (e.g., CT, MR, US)
    *   `SeriesNumber` or `00200011`: Series number
    *   `SeriesDescription` or `0008103E`: Series description
    *   `BodyPartExamined` or `00180015`: Body part examined
    *   `limit`: Maximum number of results (default: 100)
    *   `offset`: Pagination offset
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching series.
    *   `503 Service Unavailable`: Database not configured.

##### Search Instances
*   **Method**: `GET`
*   **Path**: `/instances`
*   **Description**: Search for all instances matching query parameters.
*   **Query Parameters**:
    *   `SeriesInstanceUID` or `0020000E`: Series Instance UID
    *   `SOPInstanceUID` or `00080018`: SOP Instance UID
    *   `SOPClassUID` or `00080016`: SOP Class UID
    *   `InstanceNumber` or `00200013`: Instance number
    *   `limit`: Maximum number of results (default: 100)
    *   `offset`: Pagination offset
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching instances.
    *   `503 Service Unavailable`: Database not configured.

##### Search Series in Study
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series`
*   **Description**: Search for series within a specific study.
*   **Path Parameters**:
    *   `studyUID`: Study Instance UID
*   **Query Parameters**: Same as Search Series (except StudyInstanceUID is from path)
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching series.
    *   `404 Not Found`: Study not found.
    *   `503 Service Unavailable`: Database not configured.

##### Search Instances in Study
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/instances`
*   **Description**: Search for instances within a specific study.
*   **Path Parameters**:
    *   `studyUID`: Study Instance UID
*   **Query Parameters**: Same as Search Instances
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching instances.
    *   `404 Not Found`: Study not found.
    *   `503 Service Unavailable`: Database not configured.

##### Search Instances in Series
*   **Method**: `GET`
*   **Path**: `/studies/<studyUID>/series/<seriesUID>/instances`
*   **Description**: Search for instances within a specific series.
*   **Path Parameters**:
    *   `studyUID`: Study Instance UID
    *   `seriesUID`: Series Instance UID
*   **Query Parameters**: Same as Search Instances (except SeriesInstanceUID is from path)
*   **Accept**: `application/dicom+json`
*   **Responses**:
    *   `200 OK`: DicomJSON array of matching instances.
    *   `404 Not Found`: Study or series not found.
    *   `503 Service Unavailable`: Database not configured.

---
