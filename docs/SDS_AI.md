# SDS - AI Service Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-AI-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#480](https://github.com/kcenon/pacs_system/issues/480), [#468](https://github.com/kcenon/pacs_system/issues/468) |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. AI Service Connector](#2-ai-service-connector)
- [3. AI Result Handler](#3-ai-result-handler)
- [4. Integration Workflow](#4-integration-workflow)
- [5. Traceability](#5-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the AI Service module for the PACS System. The module provides:

- **AI Service Connection**: REST client for external AI/ML inference services
- **Result Handling**: Processing and storage of AI analysis results
- **DICOM Integration**: Converting AI results to DICOM SR/SC objects

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| AI Service Connector | 1 header, 1 source | DES-AI-001 |
| AI Result Handler | 1 header, 1 source | DES-AI-002 |

### 1.3 Supported AI Service Types

| Service Type | Description | Output Format |
|--------------|-------------|---------------|
| Classification | Abnormality detection, triage | Probability scores |
| Segmentation | Organ/lesion segmentation | Mask images |
| Detection | Finding localization | Bounding boxes |
| Measurement | Automated measurements | Numeric values |

---

## 2. AI Service Connector

### 2.1 DES-AI-001: AI Service Connector

**Traces to:** SRS-AI-001 (AI Service Integration)

**File:** `include/pacs/ai/ai_service_connector.hpp`, `src/ai/ai_service_connector.cpp`

```cpp
namespace pacs::ai {

struct ai_service_config {
    std::string endpoint_url;
    std::string api_key;
    std::chrono::milliseconds timeout{30000};
    std::size_t max_retries{3};
    bool verify_ssl{true};
};

struct inference_request {
    std::string study_uid;
    std::string series_uid;
    std::vector<std::string> instance_uids;
    std::string model_name;
    std::map<std::string, std::string> parameters;
};

struct inference_result {
    std::string request_id;
    std::string model_name;
    std::string model_version;
    std::chrono::system_clock::time_point timestamp;
    bool success{false};
    std::string error_message;

    // Results (varies by service type)
    std::optional<double> confidence;
    std::optional<std::string> classification;
    std::optional<std::vector<finding>> findings;
    std::optional<std::vector<uint8_t>> segmentation_mask;
};

class ai_service_connector {
public:
    explicit ai_service_connector(const ai_service_config& config);

    // Service registration
    void register_service(const std::string& name,
                          const ai_service_config& config);
    void unregister_service(const std::string& name);

    // Synchronous inference
    [[nodiscard]] auto infer(const inference_request& request)
        -> core::result<inference_result>;

    // Asynchronous inference
    [[nodiscard]] auto infer_async(const inference_request& request)
        -> std::future<core::result<inference_result>>;

    // Batch inference
    [[nodiscard]] auto infer_batch(
        const std::vector<inference_request>& requests)
        -> std::vector<core::result<inference_result>>;

    // Health check
    [[nodiscard]] auto check_service(const std::string& name) -> bool;

private:
    std::string build_request_body(const inference_request& request);
    inference_result parse_response(const std::string& response);

    std::unordered_map<std::string, ai_service_config> services_;
    std::shared_ptr<http_client> client_;
    mutable std::mutex mutex_;
};

} // namespace pacs::ai
```

**Service Integration Flow:**

```
┌─────────────────────────────────────────────────────────────────┐
│                    AI Service Integration                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐                                               │
│  │   DICOM      │                                               │
│  │   Images     │                                               │
│  └───────┬──────┘                                               │
│          │                                                       │
│          ▼                                                       │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │              AI Service Connector                         │   │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐               │   │
│  │  │ Extract  │  │  Build   │  │   Send   │               │   │
│  │  │ Pixels   │──│ Request  │──│   HTTP   │               │   │
│  │  └──────────┘  └──────────┘  └────┬─────┘               │   │
│  └───────────────────────────────────┼──────────────────────┘   │
│                                      │                           │
│                                      ▼                           │
│                            ┌──────────────────┐                 │
│                            │  External AI     │                 │
│                            │  Service (REST)  │                 │
│                            └────────┬─────────┘                 │
│                                     │                            │
│                                     ▼                            │
│                            ┌──────────────────┐                 │
│                            │ Inference Result │                 │
│                            └──────────────────┘                 │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. AI Result Handler

### 3.1 DES-AI-002: AI Result Handler

**Traces to:** SRS-AI-001 (AI Service Integration)

**File:** `include/pacs/ai/ai_result_handler.hpp`, `src/ai/ai_result_handler.cpp`

```cpp
namespace pacs::ai {

enum class result_output_format {
    dicom_sr,           // Structured Report
    dicom_sc,           // Secondary Capture
    dicom_seg,          // Segmentation object
    json                // JSON sidecar file
};

class ai_result_handler {
public:
    explicit ai_result_handler(storage::storage_interface& storage);

    // Process and store AI result
    [[nodiscard]] auto handle_result(
        const inference_result& result,
        const core::dicom_dataset& source_dataset,
        result_output_format format = result_output_format::dicom_sr)
        -> core::result<std::string>;  // Returns stored SOP Instance UID

    // Create DICOM SR from findings
    [[nodiscard]] auto create_sr(
        const inference_result& result,
        const core::dicom_dataset& source)
        -> core::dicom_dataset;

    // Create Secondary Capture from visualization
    [[nodiscard]] auto create_sc(
        const inference_result& result,
        const std::vector<uint8_t>& visualization)
        -> core::dicom_dataset;

    // Create Segmentation from mask
    [[nodiscard]] auto create_seg(
        const inference_result& result,
        const std::vector<uint8_t>& mask,
        const core::dicom_dataset& source)
        -> core::dicom_dataset;

private:
    void add_ai_metadata(core::dicom_dataset& ds,
                         const inference_result& result);
    void link_to_source(core::dicom_dataset& ds,
                        const core::dicom_dataset& source);

    storage::storage_interface& storage_;
};

} // namespace pacs::ai
```

**DICOM SR Template for AI Results:**

```
CONTAINER: "AI Analysis Report" (121060, DCM)
├── TEXT: "Model Name" = result.model_name
├── TEXT: "Model Version" = result.model_version
├── DATE: "Analysis Date" = result.timestamp
├── NUM: "Confidence" = result.confidence
├── CODE: "Finding Category" = result.classification
└── CONTAINER: "Findings"
    ├── SCOORD: "Location" = finding.coordinates
    └── TEXT: "Description" = finding.description
```

---

## 4. Integration Workflow

### 4.1 Automatic AI Analysis Trigger

```
┌─────────────────────────────────────────────────────────────────┐
│                 Automatic AI Analysis Workflow                   │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Image Received                                              │
│     └──▶ C-STORE → storage_scp                                  │
│                                                                  │
│  2. Check AI Routing Rules                                      │
│     └──▶ Modality=CR, BodyPart=CHEST → trigger_ai_analysis()   │
│                                                                  │
│  3. Queue AI Request                                            │
│     └──▶ Add to ai_request_queue (async processing)            │
│                                                                  │
│  4. Execute Inference                                           │
│     └──▶ ai_service_connector.infer_async()                    │
│                                                                  │
│  5. Handle Result                                               │
│     └──▶ ai_result_handler.handle_result()                     │
│          └──▶ Create DICOM SR                                  │
│          └──▶ Store to PACS                                    │
│                                                                  │
│  6. Update Study                                                │
│     └──▶ Link SR to original study                             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. Traceability

### 5.1 Requirements to Design

| SRS ID | Design ID(s) | Implementation |
|--------|--------------|----------------|
| SRS-AI-001 | DES-AI-001 | AI service connector with REST client |
| SRS-AI-001 | DES-AI-002 | Result handler with DICOM SR/SC/SEG output |

---

*Document Version: 1.0.0*
*Created: 2026-01-04*
*Author: kcenon@naver.com*
