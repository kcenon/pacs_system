# SDS - AI Service Module

> **Version:** 2.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-05
> **Status:** Framework Complete (HTTP Transport Pending)

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-AI-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#480](https://github.com/kcenon/pacs_system/issues/480), [#468](https://github.com/kcenon/pacs_system/issues/468) |

### Related Standards

| Standard | Description |
|----------|-------------|
| DICOM PS3.3 | Information Object Definitions (SR, SEG, PR) |
| DICOM PS3.6 | Data Dictionary |
| TID 1500 | Measurement Report Template |
| TID 1410 | Planar ROI Measurements and Qualitative Evaluations |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. AI Service Connector](#2-ai-service-connector)
- [3. AI Result Handler](#3-ai-result-handler)
- [4. Class Diagrams](#4-class-diagrams)
- [5. Sequence Diagrams](#5-sequence-diagrams)
- [6. Traceability](#6-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the AI Service module for the PACS System. The module provides:

- **AI Service Connection**: REST client for external AI/ML inference services
- **Job Management**: Tracking and monitoring of inference requests
- **Result Handling**: Processing, validation, and storage of AI-generated DICOM objects
- **DICOM Integration**: Support for Structured Reports (SR), Segmentation (SEG), and Presentation States (PR)

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| AI Service Connector | 1 header, 1 source | DES-AI-001 |
| AI Result Handler | 1 header, 1 source | DES-AI-002 |

### 1.3 Design Identifier Convention

```
DES-AI-<NUMBER>

Where:
- DES: Design Specification prefix
- AI: AI Service module identifier
- NUMBER: 3-digit sequential number (001-002)
```

### 1.4 Supported AI Service Types

| Service Type | Description | Output Format |
|--------------|-------------|---------------|
| Classification | Abnormality detection, triage | Structured Report (SR) |
| Segmentation | Organ/lesion segmentation | Segmentation Object (SEG) |
| Detection | Finding localization | Presentation State (PR) |
| CAD | Computer-Aided Detection | Structured Report with CAD findings |

---

## 2. AI Service Connector

### 2.1 DES-AI-001: AI Service Connector

**Traces to:** SRS-AI-001 (AI Service Integration), FR-9.1 (AI Service Integration)

**Files:**
- `include/pacs/ai/ai_service_connector.hpp`
- `src/ai/ai_service_connector.cpp`

The `ai_service_connector` class provides a unified interface for interacting with external AI inference services, including sending DICOM studies for AI processing, tracking inference job status, and managing active jobs.

#### 2.1.1 Enumerations

```cpp
namespace pacs::ai {

/**
 * @enum inference_status_code
 * @brief Status codes for AI inference jobs
 */
enum class inference_status_code {
    pending,      ///< Job is queued but not started
    running,      ///< Job is currently processing
    completed,    ///< Job completed successfully
    failed,       ///< Job failed with error
    cancelled,    ///< Job was cancelled
    timeout       ///< Job timed out
};

/**
 * @enum authentication_type
 * @brief Types of authentication for AI services
 */
enum class authentication_type {
    none,         ///< No authentication
    api_key,      ///< API key in header
    bearer_token, ///< OAuth2 bearer token
    basic         ///< HTTP basic authentication
};

} // namespace pacs::ai
```

#### 2.1.2 Configuration Structure

```cpp
namespace pacs::ai {

/**
 * @struct ai_service_config
 * @brief Configuration for AI service connection
 */
struct ai_service_config {
    /// Base URL of the AI service (e.g., "https://ai.example.com/v1")
    std::string base_url;

    /// Service name for identification
    std::string service_name{"ai_service"};

    /// Authentication type
    authentication_type auth_type{authentication_type::none};

    /// API key (for api_key auth type)
    std::string api_key;

    /// Username (for basic auth)
    std::string username;

    /// Password (for basic auth)
    std::string password;

    /// Bearer token (for bearer_token auth type)
    std::string bearer_token;

    /// Connection timeout
    std::chrono::milliseconds connection_timeout{30000};

    /// Request timeout for inference operations
    std::chrono::milliseconds request_timeout{300000};  // 5 minutes

    /// Maximum retry attempts on failure
    std::size_t max_retries{3};

    /// Delay between retries (exponential backoff applied)
    std::chrono::milliseconds retry_delay{1000};

    /// Enable TLS certificate verification
    bool verify_ssl{true};

    /// Path to CA certificate bundle (optional)
    std::optional<std::filesystem::path> ca_cert_path;

    /// Status polling interval
    std::chrono::milliseconds polling_interval{5000};
};

} // namespace pacs::ai
```

#### 2.1.3 Request and Status Structures

```cpp
namespace pacs::ai {

/**
 * @struct inference_request
 * @brief Request structure for AI inference
 */
struct inference_request {
    /// Study Instance UID to process
    std::string study_instance_uid;

    /// Series Instance UID (optional, for series-level inference)
    std::optional<std::string> series_instance_uid;

    /// Model ID to use for inference
    std::string model_id;

    /// Custom parameters for the model
    std::map<std::string, std::string> parameters;

    /// Priority level (higher = more urgent)
    int priority{0};

    /// Callback URL for result notification (optional)
    std::optional<std::string> callback_url;

    /// Custom metadata to include with request
    std::map<std::string, std::string> metadata;
};

/**
 * @struct inference_status
 * @brief Status information for an inference job
 */
struct inference_status {
    /// Unique job identifier
    std::string job_id;

    /// Current status code
    inference_status_code status{inference_status_code::pending};

    /// Progress percentage (0-100)
    int progress{0};

    /// Human-readable status message
    std::string message;

    /// Error message (if status is failed)
    std::optional<std::string> error_message;

    /// Time when job was created
    std::chrono::system_clock::time_point created_at;

    /// Time when job started processing
    std::optional<std::chrono::system_clock::time_point> started_at;

    /// Time when job completed
    std::optional<std::chrono::system_clock::time_point> completed_at;

    /// Result UIDs (if completed successfully)
    std::vector<std::string> result_uids;
};

/**
 * @struct model_info
 * @brief Information about an available AI model
 */
struct model_info {
    /// Unique model identifier
    std::string model_id;

    /// Human-readable model name
    std::string name;

    /// Model description
    std::string description;

    /// Model version
    std::string version;

    /// Supported modalities (e.g., "CT", "MR", "CR")
    std::vector<std::string> supported_modalities;

    /// Supported SOP classes
    std::vector<std::string> supported_sop_classes;

    /// Output types (e.g., "SR", "SEG", "PR")
    std::vector<std::string> output_types;

    /// Whether model is currently available
    bool available{true};
};

} // namespace pacs::ai
```

#### 2.1.4 AI Service Connector Class

```cpp
namespace pacs::ai {

/**
 * @class ai_service_connector
 * @brief Connector for external AI inference services
 *
 * Thread Safety: All methods are thread-safe.
 */
class ai_service_connector {
public:
    // Type Aliases
    using status_callback = std::function<void(const inference_status&)>;
    using completion_callback = std::function<void(const std::string& job_id,
                                                   bool success,
                                                   const std::vector<std::string>& result_uids)>;

    // Initialization
    [[nodiscard]] static auto initialize(const ai_service_config& config)
        -> Result<std::monostate>;
    static void shutdown();
    [[nodiscard]] static auto is_initialized() noexcept -> bool;

    // Inference Operations
    [[nodiscard]] static auto request_inference(const inference_request& request)
        -> Result<std::string>;
    [[nodiscard]] static auto check_status(const std::string& job_id)
        -> Result<inference_status>;
    [[nodiscard]] static auto cancel(const std::string& job_id)
        -> Result<std::monostate>;
    [[nodiscard]] static auto wait_for_completion(
        const std::string& job_id,
        std::chrono::milliseconds timeout = std::chrono::minutes{30},
        status_callback callback = nullptr)
        -> Result<inference_status>;
    [[nodiscard]] static auto list_active_jobs()
        -> Result<std::vector<inference_status>>;

    // Model Management
    [[nodiscard]] static auto list_models()
        -> Result<std::vector<model_info>>;
    [[nodiscard]] static auto get_model_info(const std::string& model_id)
        -> Result<model_info>;

    // Health Check
    [[nodiscard]] static auto check_health() -> bool;
    [[nodiscard]] static auto get_latency()
        -> std::optional<std::chrono::milliseconds>;

    // Configuration
    [[nodiscard]] static auto get_config() -> const ai_service_config&;
    [[nodiscard]] static auto update_credentials(
        authentication_type auth_type,
        const std::string& credentials)
        -> Result<std::monostate>;

private:
    ai_service_connector() = delete;
    ~ai_service_connector() = delete;

    class impl;
    static std::unique_ptr<impl> pimpl_;
};

} // namespace pacs::ai
```

#### 2.1.5 Metrics

| Metric Name | Type | Description |
|-------------|------|-------------|
| `pacs_ai_inference_requests_total` | Counter | Total inference requests submitted |
| `pacs_ai_inference_requests_success` | Counter | Successful inference requests |
| `pacs_ai_inference_requests_failed` | Counter | Failed inference requests |
| `pacs_ai_inference_duration_seconds` | Histogram | Inference job duration |
| `pacs_ai_active_jobs` | Gauge | Number of active inference jobs |

---

## 3. AI Result Handler

### 3.1 DES-AI-002: AI Result Handler

**Traces to:** SRS-AI-001 (AI Service Integration), FR-9.1 (AI Service Integration)

**Files:**
- `include/pacs/ai/ai_result_handler.hpp`
- `src/ai/ai_result_handler.cpp`

The `ai_result_handler` class manages the reception, validation, and storage of AI inference outputs, including Structured Reports (SR), Segmentation objects (SEG), and Presentation States (PR).

#### 3.1.1 Enumerations

```cpp
namespace pacs::ai {

/**
 * @brief Types of AI-generated DICOM objects
 */
enum class ai_result_type {
    structured_report,    ///< DICOM SR (Structured Report)
    segmentation,         ///< DICOM SEG (Segmentation)
    presentation_state    ///< DICOM PR (Presentation State)
};

/**
 * @brief Validation status for AI results
 */
enum class validation_status {
    valid,                    ///< All validations passed
    missing_required_tags,    ///< Required DICOM tags are missing
    invalid_reference,        ///< Referenced source images not found
    invalid_template,         ///< SR template conformance failed
    invalid_segment_data,     ///< Segmentation data is malformed
    unknown_error             ///< Unexpected validation error
};

} // namespace pacs::ai
```

#### 3.1.2 Data Structures

```cpp
namespace pacs::ai {

/**
 * @brief Information about an AI result stored in the system
 */
struct ai_result_info {
    /// SOP Instance UID of the AI result
    std::string sop_instance_uid;

    /// Type of AI result
    ai_result_type type;

    /// SOP Class UID
    std::string sop_class_uid;

    /// Series Instance UID
    std::string series_instance_uid;

    /// Study Instance UID of the source study
    std::string source_study_uid;

    /// AI model/algorithm identifier
    std::string algorithm_name;

    /// Algorithm version
    std::string algorithm_version;

    /// Timestamp when the result was received
    std::chrono::system_clock::time_point received_at;

    /// Optional description
    std::optional<std::string> description;
};

/**
 * @brief Source reference linking AI result to original images
 */
struct source_reference {
    /// Study Instance UID
    std::string study_instance_uid;

    /// Series Instance UID (optional, for series-level reference)
    std::optional<std::string> series_instance_uid;

    /// SOP Instance UIDs (optional, for instance-level reference)
    std::vector<std::string> sop_instance_uids;
};

/**
 * @brief CAD finding extracted from Structured Report
 */
struct cad_finding {
    /// Finding type/category
    std::string finding_type;

    /// Location/site description
    std::string location;

    /// Confidence score (0.0 to 1.0)
    std::optional<double> confidence;

    /// Additional measurement or annotation data
    std::optional<std::string> measurement;

    /// Reference to specific image where finding was detected
    std::optional<std::string> referenced_sop_instance_uid;
};

/**
 * @brief Segment information from Segmentation object
 */
struct segment_info {
    /// Segment number (1-based)
    uint16_t segment_number;

    /// Segment label
    std::string segment_label;

    /// Segment description
    std::optional<std::string> description;

    /// Algorithm type that created this segment
    std::string algorithm_type;

    /// RGB color for visualization (optional)
    std::optional<std::tuple<uint8_t, uint8_t, uint8_t>> recommended_display_color;
};

/**
 * @brief Validation result containing status and details
 */
struct validation_result {
    /// Overall validation status
    validation_status status;

    /// Detailed error message if validation failed
    std::optional<std::string> error_message;

    /// List of missing required tags (if applicable)
    std::vector<std::string> missing_tags;

    /// List of invalid references (if applicable)
    std::vector<std::string> invalid_references;
};

/**
 * @brief Configuration for AI result handler
 */
struct ai_handler_config {
    /// Whether to validate source references exist in storage
    bool validate_source_references = true;

    /// Whether to validate SR template conformance
    bool validate_sr_templates = true;

    /// Whether to auto-link results to source studies
    bool auto_link_to_source = true;

    /// Accepted SR template identifiers (empty = accept all)
    std::vector<std::string> accepted_sr_templates;

    /// Maximum segment count for SEG objects (0 = unlimited)
    uint16_t max_segments = 256;
};

} // namespace pacs::ai
```

#### 3.1.3 AI Result Handler Class

```cpp
namespace pacs::ai {

/**
 * @class ai_result_handler
 * @brief Handler for AI-generated DICOM objects
 *
 * Thread Safety: This class is NOT thread-safe. External synchronization
 * is required for concurrent access.
 */
class ai_result_handler {
public:
    [[nodiscard]] static auto create(
        std::shared_ptr<storage::storage_interface> storage,
        std::shared_ptr<storage::index_database> database)
        -> std::unique_ptr<ai_result_handler>;

    virtual ~ai_result_handler();

    // Non-copyable, movable
    ai_result_handler(const ai_result_handler&) = delete;
    auto operator=(const ai_result_handler&) -> ai_result_handler& = delete;
    ai_result_handler(ai_result_handler&&) noexcept;
    auto operator=(ai_result_handler&&) noexcept -> ai_result_handler&;

    // Configuration
    void configure(const ai_handler_config& config);
    [[nodiscard]] auto get_config() const -> ai_handler_config;
    void set_received_callback(ai_result_received_callback callback);
    void set_pre_store_validator(pre_store_validator validator);

    // Structured Report (SR) Operations
    [[nodiscard]] auto receive_structured_report(const core::dicom_dataset& sr)
        -> VoidResult;
    [[nodiscard]] auto validate_sr_template(const core::dicom_dataset& sr)
        -> validation_result;
    [[nodiscard]] auto get_cad_findings(std::string_view sr_sop_instance_uid)
        -> Result<std::vector<cad_finding>>;

    // Segmentation (SEG) Operations
    [[nodiscard]] auto receive_segmentation(const core::dicom_dataset& seg)
        -> VoidResult;
    [[nodiscard]] auto validate_segmentation(const core::dicom_dataset& seg)
        -> validation_result;
    [[nodiscard]] auto get_segment_info(std::string_view seg_sop_instance_uid)
        -> Result<std::vector<segment_info>>;

    // Presentation State (PR) Operations
    [[nodiscard]] auto receive_presentation_state(const core::dicom_dataset& pr)
        -> VoidResult;
    [[nodiscard]] auto validate_presentation_state(const core::dicom_dataset& pr)
        -> validation_result;

    // Source Linking Operations
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        std::string_view source_study_uid) -> VoidResult;
    [[nodiscard]] auto link_to_source(
        std::string_view result_uid,
        const source_reference& references) -> VoidResult;
    [[nodiscard]] auto get_source_reference(std::string_view result_uid)
        -> Result<source_reference>;

    // Query Operations
    [[nodiscard]] auto find_ai_results_for_study(std::string_view study_instance_uid)
        -> Result<std::vector<ai_result_info>>;
    [[nodiscard]] auto find_ai_results_by_type(
        std::string_view study_instance_uid,
        ai_result_type type) -> Result<std::vector<ai_result_info>>;
    [[nodiscard]] auto get_ai_result_info(std::string_view sop_instance_uid)
        -> std::optional<ai_result_info>;
    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool;

    // Removal Operations
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

#### 3.1.4 Supported SOP Classes

| Category | SOP Class UID | Description |
|----------|---------------|-------------|
| Structured Report | 1.2.840.10008.5.1.4.1.1.88.11 | Basic Text SR |
| Structured Report | 1.2.840.10008.5.1.4.1.1.88.22 | Enhanced SR |
| Structured Report | 1.2.840.10008.5.1.4.1.1.88.33 | Comprehensive SR |
| Structured Report | 1.2.840.10008.5.1.4.1.1.88.34 | Comprehensive 3D SR |
| Segmentation | 1.2.840.10008.5.1.4.1.1.66.4 | Segmentation Storage |
| Presentation State | 1.2.840.10008.5.1.4.1.1.11.1 | Grayscale Softcopy PS |
| Presentation State | 1.2.840.10008.5.1.4.1.1.11.2 | Color Softcopy PS |
| Presentation State | 1.2.840.10008.5.1.4.1.1.11.3 | Pseudo-Color Softcopy PS |
| Presentation State | 1.2.840.10008.5.1.4.1.1.11.4 | Blending Softcopy PS |

---

## 4. Class Diagrams

### 4.1 AI Service Connector Class Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                        ai_service_connector                            │
├──────────────────────────────────────────────────────────────────────┤
│ «static»                                                               │
├──────────────────────────────────────────────────────────────────────┤
│ + initialize(config) : Result<monostate>                              │
│ + shutdown() : void                                                    │
│ + is_initialized() : bool                                              │
│ + request_inference(request) : Result<string>                         │
│ + check_status(job_id) : Result<inference_status>                     │
│ + cancel(job_id) : Result<monostate>                                  │
│ + wait_for_completion(job_id, timeout, callback) : Result<status>     │
│ + list_active_jobs() : Result<vector<inference_status>>               │
│ + list_models() : Result<vector<model_info>>                          │
│ + get_model_info(model_id) : Result<model_info>                       │
│ + check_health() : bool                                                │
│ + get_latency() : optional<milliseconds>                              │
│ + get_config() : ai_service_config&                                   │
│ + update_credentials(auth_type, credentials) : Result<monostate>      │
├──────────────────────────────────────────────────────────────────────┤
│ - pimpl_ : unique_ptr<impl>                                           │
└──────────────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────────────┐
│                            impl                                        │
├──────────────────────────────────────────────────────────────────────┤
│ - mutex_ : std::mutex                                                  │
│ - initialized_ : atomic<bool>                                          │
│ - config_ : ai_service_config                                          │
│ - http_client_ : unique_ptr<http_client>                              │
│ - job_tracker_ : unique_ptr<job_tracker>                              │
└──────────────────────────────────────────────────────────────────────┘
```

### 4.2 AI Result Handler Class Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                        ai_result_handler                               │
├──────────────────────────────────────────────────────────────────────┤
│ + create(storage, database) : unique_ptr<ai_result_handler>           │
│ + configure(config) : void                                             │
│ + get_config() : ai_handler_config                                    │
│ + set_received_callback(callback) : void                              │
│ + set_pre_store_validator(validator) : void                           │
│                                                                        │
│ // SR Operations                                                       │
│ + receive_structured_report(sr) : VoidResult                          │
│ + validate_sr_template(sr) : validation_result                        │
│ + get_cad_findings(uid) : Result<vector<cad_finding>>                 │
│                                                                        │
│ // SEG Operations                                                      │
│ + receive_segmentation(seg) : VoidResult                              │
│ + validate_segmentation(seg) : validation_result                      │
│ + get_segment_info(uid) : Result<vector<segment_info>>                │
│                                                                        │
│ // PR Operations                                                       │
│ + receive_presentation_state(pr) : VoidResult                         │
│ + validate_presentation_state(pr) : validation_result                 │
│                                                                        │
│ // Source Linking                                                      │
│ + link_to_source(result_uid, source_uid) : VoidResult                 │
│ + get_source_reference(uid) : Result<source_reference>                │
│                                                                        │
│ // Query                                                               │
│ + find_ai_results_for_study(uid) : Result<vector<ai_result_info>>     │
│ + find_ai_results_by_type(uid, type) : Result<vector<ai_result_info>> │
│ + get_ai_result_info(uid) : optional<ai_result_info>                  │
│ + exists(uid) : bool                                                   │
│                                                                        │
│ // Removal                                                             │
│ + remove(uid) : VoidResult                                             │
│ + remove_ai_results_for_study(uid) : Result<size_t>                   │
├──────────────────────────────────────────────────────────────────────┤
│ - pimpl_ : unique_ptr<impl>                                           │
└──────────────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────────────┐
│                            impl                                        │
├──────────────────────────────────────────────────────────────────────┤
│ - config_ : ai_handler_config                                         │
│ - received_callback_ : ai_result_received_callback                    │
│ - pre_store_validator_ : pre_store_validator                          │
│ - storage_ : shared_ptr<storage_interface>                            │
│ - database_ : shared_ptr<index_database>                              │
│ - ai_results_cache_ : map<string, ai_result_info>                     │
│ - source_links_ : map<string, source_reference>                       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. Sequence Diagrams

### 5.1 AI Inference Request Flow

```
┌─────────┐      ┌───────────────────┐      ┌──────────────┐      ┌───────────────┐
│  Client │      │ ai_service_connector│      │  http_client │      │ AI Service    │
└────┬────┘      └─────────┬─────────┘      └──────┬───────┘      └───────┬───────┘
     │                     │                       │                      │
     │  request_inference()│                       │                      │
     │────────────────────>│                       │                      │
     │                     │                       │                      │
     │                     │ validate request      │                      │
     │                     │──────────┐            │                      │
     │                     │          │            │                      │
     │                     │<─────────┘            │                      │
     │                     │                       │                      │
     │                     │ build_request_json()  │                      │
     │                     │──────────┐            │                      │
     │                     │          │            │                      │
     │                     │<─────────┘            │                      │
     │                     │                       │                      │
     │                     │  POST /inference      │                      │
     │                     │──────────────────────>│                      │
     │                     │                       │                      │
     │                     │                       │  HTTP POST           │
     │                     │                       │─────────────────────>│
     │                     │                       │                      │
     │                     │                       │  {"job_id": "..."}   │
     │                     │                       │<─────────────────────│
     │                     │                       │                      │
     │                     │  Response             │                      │
     │                     │<──────────────────────│                      │
     │                     │                       │                      │
     │                     │ job_tracker.add_job() │                      │
     │                     │──────────┐            │                      │
     │                     │          │            │                      │
     │                     │<─────────┘            │                      │
     │                     │                       │                      │
     │   Result<job_id>    │                       │                      │
     │<────────────────────│                       │                      │
     │                     │                       │                      │
```

### 5.2 AI Result Reception Flow

```
┌────────────┐    ┌───────────────────┐    ┌─────────────┐    ┌────────────────┐
│ AI Service │    │ ai_result_handler │    │   storage   │    │ index_database │
└─────┬──────┘    └─────────┬─────────┘    └──────┬──────┘    └───────┬────────┘
      │                     │                     │                   │
      │ receive_structured_report(sr)             │                   │
      │────────────────────>│                     │                   │
      │                     │                     │                   │
      │                     │ validate_common_tags│                   │
      │                     │──────────┐          │                   │
      │                     │          │          │                   │
      │                     │<─────────┘          │                   │
      │                     │                     │                   │
      │                     │ validate_sr_template│                   │
      │                     │──────────┐          │                   │
      │                     │          │          │                   │
      │                     │<─────────┘          │                   │
      │                     │                     │                   │
      │                     │ pre_store_validator │                   │
      │                     │──────────┐          │                   │
      │                     │          │          │                   │
      │                     │<─────────┘          │                   │
      │                     │                     │                   │
      │                     │  store(sr)          │                   │
      │                     │────────────────────>│                   │
      │                     │                     │                   │
      │                     │    Result<ok>       │                   │
      │                     │<────────────────────│                   │
      │                     │                     │                   │
      │                     │ cache result info   │                   │
      │                     │──────────┐          │                   │
      │                     │          │          │                   │
      │                     │<─────────┘          │                   │
      │                     │                     │                   │
      │                     │ link_to_source()    │                   │
      │                     │──────────┐          │                   │
      │                     │          │          │                   │
      │                     │<─────────┘          │                   │
      │                     │                     │                   │
      │                     │ received_callback() │                   │
      │                     │──────────┐          │                   │
      │                     │          │          │                   │
      │                     │<─────────┘          │                   │
      │                     │                     │                   │
      │   VoidResult        │                     │                   │
      │<────────────────────│                     │                   │
      │                     │                     │                   │
```

### 5.3 Complete AI Analysis Workflow

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                        Complete AI Analysis Workflow                               │
├──────────────────────────────────────────────────────────────────────────────────┤
│                                                                                    │
│  1. Image Received                                                                 │
│     └──▶ C-STORE → storage_scp                                                    │
│                                                                                    │
│  2. Check AI Routing Rules                                                        │
│     └──▶ Modality=CR, BodyPart=CHEST → trigger_ai_analysis()                      │
│                                                                                    │
│  3. Submit Inference Request                                                      │
│     └──▶ ai_service_connector::request_inference(request)                         │
│          ├── Validate study_instance_uid, model_id                                │
│          ├── Build JSON request body                                              │
│          └── POST to AI service endpoint                                          │
│                                                                                    │
│  4. Track Job Status                                                              │
│     └──▶ ai_service_connector::wait_for_completion(job_id)                        │
│          ├── Poll status at polling_interval                                      │
│          └── Invoke status_callback on updates                                    │
│                                                                                    │
│  5. Receive AI Result                                                             │
│     └──▶ ai_result_handler::receive_structured_report(sr)                         │
│          ├── Validate DICOM tags                                                  │
│          ├── Validate SR template                                                 │
│          ├── Validate source references                                           │
│          └── Store to PACS                                                        │
│                                                                                    │
│  6. Link to Source Study                                                          │
│     └──▶ ai_result_handler::link_to_source(result_uid, source_study_uid)          │
│                                                                                    │
│  7. Query Results                                                                 │
│     └──▶ ai_result_handler::find_ai_results_for_study(study_uid)                  │
│                                                                                    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Traceability

### 6.1 Requirements to Design

| SRS ID | Design ID | Implementation |
|--------|-----------|----------------|
| SRS-AI-001 | DES-AI-001 | `ai_service_connector` class with REST client, job tracking |
| SRS-AI-001 | DES-AI-002 | `ai_result_handler` class with SR/SEG/PR support |

### 6.2 Design to Implementation

| Design ID | Class/Component | Header File | Source File |
|-----------|-----------------|-------------|-------------|
| DES-AI-001 | `ai_service_connector` | `include/pacs/ai/ai_service_connector.hpp` | `src/ai/ai_service_connector.cpp` |
| DES-AI-002 | `ai_result_handler` | `include/pacs/ai/ai_result_handler.hpp` | `src/ai/ai_result_handler.cpp` |

### 6.3 Design to Test

| Design ID | Test File | Coverage |
|-----------|-----------|----------|
| DES-AI-001 | `tests/ai/ai_service_connector_test.cpp` | Unit tests for connector |
| DES-AI-002 | `tests/ai/ai_result_handler_test.cpp` | Unit tests for handler |

---

*Document Version: 2.0.0*
*Created: 2026-01-04*
*Updated: 2026-01-05*
*Author: kcenon@naver.com*
