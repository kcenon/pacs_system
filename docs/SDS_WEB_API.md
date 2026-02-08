# SDS - Web/REST API Module Design Specification

> **Version:** 2.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-02-08

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Module Architecture](#2-module-architecture)
- [3. Component Design](#3-component-design)
  - [DES-WEB-001: rest_server](#des-web-001-rest_server)
  - [DES-WEB-002: rest_config](#des-web-002-rest_config)
  - [DES-WEB-003: rest_types](#des-web-003-rest_types)
  - [DES-WEB-004: patient_endpoints](#des-web-004-patient_endpoints)
  - [DES-WEB-005: study_endpoints](#des-web-005-study_endpoints)
  - [DES-WEB-006: series_endpoints](#des-web-006-series_endpoints)
  - [DES-WEB-007: dicomweb_endpoints](#des-web-007-dicomweb_endpoints)
  - [DES-WEB-008: worklist_endpoints](#des-web-008-worklist_endpoints)
  - [DES-WEB-009: audit_endpoints](#des-web-009-audit_endpoints)
  - [DES-WEB-010: security_endpoints](#des-web-010-security_endpoints)
  - [DES-WEB-011: system_endpoints](#des-web-011-system_endpoints)
  - [DES-WEB-012: association_endpoints](#des-web-012-association_endpoints)
  - [DES-WEB-013: annotation_endpoints](#des-web-013-annotation_endpoints)
  - [DES-WEB-014: jobs_endpoints](#des-web-014-jobs_endpoints)
  - [DES-WEB-015: key_image_endpoints](#des-web-015-key_image_endpoints)
  - [DES-WEB-016: measurement_endpoints](#des-web-016-measurement_endpoints)
  - [DES-WEB-017: metadata_endpoints](#des-web-017-metadata_endpoints)
  - [DES-WEB-018: metrics_endpoints](#des-web-018-metrics_endpoints)
  - [DES-WEB-019: remote_nodes_endpoints](#des-web-019-remote_nodes_endpoints)
  - [DES-WEB-020: routing_endpoints](#des-web-020-routing_endpoints)
  - [DES-WEB-021: thumbnail_endpoints](#des-web-021-thumbnail_endpoints)
  - [DES-WEB-022: viewer_state_endpoints](#des-web-022-viewer_state_endpoints)
- [4. API Endpoint Reference](#4-api-endpoint-reference)
- [5. DICOMweb Standards Compliance](#5-dicomweb-standards-compliance)
- [6. Error Handling](#6-error-handling)
- [7. Security Considerations](#7-security-considerations)
- [8. Dependencies](#8-dependencies)

---

## 1. Overview

### 1.1 Purpose

The Web/REST API module provides HTTP-based interfaces for:

1. **DICOMweb Services**: Standard WADO-RS, STOW-RS, QIDO-RS implementations
2. **Management API**: CRUD operations for patients, studies, series, worklist items
3. **System Monitoring**: Health checks, metrics, active associations
4. **Security Management**: User/role management, audit log access

### 1.2 Standards Compliance

| Standard | Version | Status |
|----------|---------|--------|
| DICOMweb (WADO-RS) | PS3.18 | Implemented |
| DICOMweb (STOW-RS) | PS3.18 | Implemented |
| DICOMweb (QIDO-RS) | PS3.18 | Implemented |
| OpenAPI/Swagger | 3.0 | Planned |

### 1.3 Design Principles

- **RESTful Design**: Resource-oriented URLs, proper HTTP verbs
- **JSON First**: All management APIs use JSON; DICOMweb supports multipart/related
- **CORS Support**: Configurable cross-origin resource sharing
- **Consistent Error Format**: Standardized error responses with codes and messages

---

## 2. Module Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Web/REST API Module                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                        DES-WEB-001: rest_server                         ││
│  │                                                                          ││
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐                  ││
│  │  │  Crow App    │  │  Middleware  │  │   Router     │                  ││
│  │  │  (HTTP/1.1)  │  │  (CORS/Auth) │  │  (Dispatch)  │                  ││
│  │  └──────────────┘  └──────────────┘  └──────────────┘                  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                         Endpoint Handlers                                ││
│  │                                                                          ││
│  │  ┌───────────────────────────────────────────────────────────────────┐  ││
│  │  │                    DICOMweb Endpoints (DES-WEB-007)               │  ││
│  │  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐               │  ││
│  │  │  │  WADO-RS    │  │  STOW-RS    │  │  QIDO-RS    │               │  ││
│  │  │  │  (Retrieve) │  │  (Store)    │  │  (Query)    │               │  ││
│  │  │  └─────────────┘  └─────────────┘  └─────────────┘               │  ││
│  │  └───────────────────────────────────────────────────────────────────┘  ││
│  │                                                                          ││
│  │  ┌───────────────────────────────────────────────────────────────────┐  ││
│  │  │                    Management Endpoints                           │  ││
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐    │  ││
│  │  │  │ Patient │ │  Study  │ │ Series  │ │Worklist │ │  Audit  │    │  ││
│  │  │  │DES-004  │ │DES-005  │ │DES-006  │ │DES-008  │ │DES-009  │    │  ││
│  │  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘    │  ││
│  │  │  ┌─────────┐ ┌─────────┐ ┌─────────┐                            │  ││
│  │  │  │Security │ │ System  │ │  Assoc  │                            │  ││
│  │  │  │DES-010  │ │DES-011  │ │DES-012  │                            │  ││
│  │  │  └─────────┘ └─────────┘ └─────────┘                            │  ││
│  │  └───────────────────────────────────────────────────────────────────┘  ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                      │                                       │
│                                      ▼                                       │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │                        Backend Services                                  ││
│  │                                                                          ││
│  │  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐              ││
│  │  │  index_   │ │  file_    │ │  access_  │ │  dicom_   │              ││
│  │  │  database │ │  storage  │ │  control  │ │  server   │              ││
│  │  └───────────┘ └───────────┘ └───────────┘ └───────────┘              ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Component Design

### DES-WEB-001: rest_server

**Traces to:** SRS-WEB-001 (REST API Server)

#### 3.1.1 Purpose

Core REST server component that initializes the Crow HTTP framework, configures middleware, and registers all endpoint handlers.

#### 3.1.2 Class Design

```cpp
namespace pacs::web {

/**
 * @brief REST API server using Crow framework
 *
 * Provides HTTP/HTTPS endpoints for DICOMweb and management APIs.
 * Thread-safe, supports concurrent connections.
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

private:
    crow::SimpleApp app_;
    std::shared_ptr<rest_server_context> ctx_;
    std::unique_ptr<std::thread> server_thread_;
};

} // namespace pacs::web
```

#### 3.1.3 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/rest_server.hpp` | Public interface declaration |
| `src/web/rest_server.cpp` | Server implementation, endpoint registration |

---

### DES-WEB-002: rest_config

**Traces to:** SRS-WEB-001 (REST API Server Configuration)

#### 3.2.1 Purpose

Configuration structure for REST server settings including port, TLS, CORS, and endpoint options.

#### 3.2.2 Data Structure

```cpp
namespace pacs::web {

/**
 * @brief REST server configuration
 */
struct rest_config {
    // ═══════════════════════════════════════════════════════════════
    // Network Settings
    // ═══════════════════════════════════════════════════════════════

    uint16_t port = 8080;                    ///< HTTP port
    std::string bind_address = "0.0.0.0";    ///< Bind address

    // ═══════════════════════════════════════════════════════════════
    // TLS Settings
    // ═══════════════════════════════════════════════════════════════

    bool enable_tls = false;                 ///< Enable HTTPS
    std::string cert_path;                   ///< TLS certificate path
    std::string key_path;                    ///< TLS private key path

    // ═══════════════════════════════════════════════════════════════
    // CORS Settings
    // ═══════════════════════════════════════════════════════════════

    std::string cors_allowed_origins = "*";  ///< Allowed origins
    std::string cors_allowed_methods =
        "GET, POST, PUT, DELETE, OPTIONS";   ///< Allowed methods
    std::string cors_allowed_headers =
        "Content-Type, Authorization";       ///< Allowed headers

    // ═══════════════════════════════════════════════════════════════
    // Performance Settings
    // ═══════════════════════════════════════════════════════════════

    size_t thread_count = 4;                 ///< Worker thread count
    size_t max_body_size = 512 * 1024 * 1024; ///< Max request body (512MB)
};

} // namespace pacs::web
```

#### 3.2.3 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/rest_config.hpp` | Configuration structure (header-only) |

---

### DES-WEB-003: rest_types

**Traces to:** SRS-WEB-002 (REST API Response Types)

#### 3.3.1 Purpose

Common types, utilities, and JSON helper functions used across all endpoints.

#### 3.3.2 Type Definitions

```cpp
namespace pacs::web::endpoints {

/**
 * @brief Server context shared across all endpoints
 */
struct rest_server_context {
    std::shared_ptr<rest_config> config;
    std::shared_ptr<storage::index_database> database;
    std::shared_ptr<storage::storage_interface> storage;
    std::shared_ptr<security::access_control_manager> security_manager;
};

// ═══════════════════════════════════════════════════════════════
// JSON Utility Functions
// ═══════════════════════════════════════════════════════════════

/**
 * @brief Escape string for JSON output
 */
std::string json_escape(const std::string& s);

/**
 * @brief Create error JSON response
 */
std::string make_error_json(const std::string& code,
                            const std::string& message);

/**
 * @brief Create success JSON response
 */
std::string make_success_json(const std::string& message);

} // namespace pacs::web::endpoints
```

#### 3.3.3 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/rest_types.hpp` | Type definitions and utilities (header-only) |

---

### DES-WEB-004: patient_endpoints

**Traces to:** SRS-WEB-003 (Patient Management API)

#### 3.4.1 Purpose

RESTful endpoints for patient record management (CRUD operations).

#### 3.4.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/patients` | List patients (paginated, filterable) |
| GET | `/api/v1/patients/:id` | Get patient by Patient ID |
| GET | `/api/v1/patients/:id/studies` | List studies for a patient |
| DELETE | `/api/v1/patients/:id` | Delete patient and all studies |

#### 3.4.3 Request/Response Examples

**GET /api/v1/patients**

Query Parameters:
- `limit`: Maximum results (default: 20, max: 100)
- `offset`: Pagination offset
- `name`: Patient name filter (wildcard supported)
- `id`: Patient ID filter

Response:
```json
{
  "data": [
    {
      "pk": 1,
      "patient_id": "PAT001",
      "patient_name": "DOE^JOHN",
      "birth_date": "19800101",
      "sex": "M",
      "num_studies": 5
    }
  ],
  "pagination": {
    "total": 150,
    "count": 20
  }
}
```

#### 3.4.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/patient_endpoints.hpp` | Endpoint registration function |
| `src/web/endpoints/patient_endpoints.cpp` | Endpoint implementation |

---

### DES-WEB-005: study_endpoints

**Traces to:** SRS-WEB-004 (Study Management API)

#### 3.5.1 Purpose

RESTful endpoints for study record management.

#### 3.5.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/studies` | List studies (paginated, filterable) |
| GET | `/api/v1/studies/:uid` | Get study by Study Instance UID |
| GET | `/api/v1/studies/:uid/series` | List series for a study |
| DELETE | `/api/v1/studies/:uid` | Delete study and all series |

#### 3.5.3 Query Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `limit` | integer | Max results (default: 20) |
| `offset` | integer | Pagination offset |
| `patient_id` | string | Filter by Patient ID |
| `study_date` | string | Filter by study date (YYYYMMDD) |
| `modality` | string | Filter by modality |
| `accession_number` | string | Filter by accession number |

#### 3.5.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/study_endpoints.hpp` | Endpoint registration |
| `src/web/endpoints/study_endpoints.cpp` | Implementation |

---

### DES-WEB-006: series_endpoints

**Traces to:** SRS-WEB-005 (Series Management API)

#### 3.6.1 Purpose

RESTful endpoints for series record management.

#### 3.6.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/series/:uid` | Get series by Series Instance UID |
| GET | `/api/v1/series/:uid/instances` | List instances for a series |

#### 3.6.3 Response Format

**GET /api/v1/series/:uid**
```json
{
  "pk": 42,
  "study_pk": 10,
  "series_instance_uid": "1.2.3.4.5.6",
  "modality": "CT",
  "series_number": 1,
  "series_description": "CHEST W/O CONTRAST",
  "body_part_examined": "CHEST",
  "station_name": "CT_SCANNER_01",
  "num_instances": 150
}
```

#### 3.6.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/series_endpoints.hpp` | Endpoint registration |
| `src/web/endpoints/series_endpoints.cpp` | Implementation |

---

### DES-WEB-007: dicomweb_endpoints

**Traces to:** SRS-WEB-006 (DICOMweb Services)

#### 3.7.1 Purpose

Implementation of DICOMweb standard services (WADO-RS, STOW-RS, QIDO-RS) as defined in DICOM PS3.18.

#### 3.7.2 WADO-RS (Web Access to DICOM Objects - RESTful Services)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/dicomweb/studies/:study` | Retrieve all instances in study |
| GET | `/dicomweb/studies/:study/metadata` | Retrieve study metadata (JSON) |
| GET | `/dicomweb/studies/:study/series/:series` | Retrieve series instances |
| GET | `/dicomweb/studies/:study/series/:series/metadata` | Series metadata |
| GET | `/dicomweb/studies/:study/series/:series/instances/:instance` | Retrieve instance |
| GET | `/dicomweb/studies/:study/series/:series/instances/:instance/metadata` | Instance metadata |
| GET | `/dicomweb/studies/:study/series/:series/instances/:instance/frames/:frames` | Retrieve frames |
| GET | `/dicomweb/studies/:study/series/:series/instances/:instance/rendered` | Rendered image |

**Accept Header Negotiation:**

| Accept Type | Response Format |
|-------------|-----------------|
| `application/dicom` | Raw DICOM Part 10 |
| `multipart/related; type="application/dicom"` | Multipart DICOM |
| `application/dicom+json` | DICOM JSON |
| `image/jpeg` | JPEG rendered image |
| `image/png` | PNG rendered image |

#### 3.7.3 STOW-RS (Store Over the Web - RESTful Services)

| Method | Path | Description |
|--------|------|-------------|
| POST | `/dicomweb/studies` | Store instances (any study) |
| POST | `/dicomweb/studies/:study` | Store instances to specific study |

**Request Format:**
- Content-Type: `multipart/related; type="application/dicom"`
- Each part contains a DICOM instance

**Response (Success):**
```json
{
  "00081190": {
    "vr": "UR",
    "Value": ["http://pacs/dicomweb/studies/1.2.3"]
  },
  "00081199": {
    "vr": "SQ",
    "Value": [
      {
        "00081150": { "vr": "UI", "Value": ["1.2.840.10008.5.1.4.1.1.2"] },
        "00081155": { "vr": "UI", "Value": ["1.2.3.4.5.6.7"] },
        "00081190": { "vr": "UR", "Value": ["http://pacs/dicomweb/studies/1.2.3/series/1.2.3.4/instances/1.2.3.4.5.6.7"] }
      }
    ]
  }
}
```

#### 3.7.4 QIDO-RS (Query based on ID for DICOM Objects - RESTful Services)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/dicomweb/studies` | Search studies |
| GET | `/dicomweb/series` | Search series (all studies) |
| GET | `/dicomweb/instances` | Search instances (all studies) |
| GET | `/dicomweb/studies/:study/series` | Search series in study |
| GET | `/dicomweb/studies/:study/instances` | Search instances in study |
| GET | `/dicomweb/studies/:study/series/:series/instances` | Search instances in series |

**Query Parameters (Studies):**

| Parameter | DICOM Tag | Description |
|-----------|-----------|-------------|
| `PatientID` | (0010,0020) | Patient ID |
| `PatientName` | (0010,0010) | Patient Name (wildcard) |
| `StudyDate` | (0008,0020) | Study Date (range supported) |
| `StudyInstanceUID` | (0020,000D) | Study Instance UID |
| `AccessionNumber` | (0008,0050) | Accession Number |
| `ModalitiesInStudy` | (0008,0061) | Modalities |
| `limit` | - | Max results |
| `offset` | - | Pagination offset |

**Response Format:**
```json
[
  {
    "00080005": { "vr": "CS", "Value": ["ISO_IR 100"] },
    "00080020": { "vr": "DA", "Value": ["20260103"] },
    "00080030": { "vr": "TM", "Value": ["120000"] },
    "00080050": { "vr": "SH", "Value": ["ACC001"] },
    "00100010": { "vr": "PN", "Value": [{"Alphabetic": "DOE^JOHN"}] },
    "00100020": { "vr": "LO", "Value": ["PAT001"] },
    "0020000D": { "vr": "UI", "Value": ["1.2.3.4.5.6"] }
  }
]
```

#### 3.7.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/dicomweb_endpoints.hpp` | DICOMweb endpoint registration |
| `src/web/endpoints/dicomweb_endpoints.cpp` | Complete DICOMweb implementation |

---

### DES-WEB-008: worklist_endpoints

**Traces to:** SRS-WEB-007 (Worklist Management API)

#### 3.8.1 Purpose

RESTful endpoints for Modality Worklist (MWL) management.

#### 3.8.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/worklist` | List scheduled procedures (paginated) |
| POST | `/api/v1/worklist` | Create new worklist item |
| GET | `/api/v1/worklist/:id` | Get worklist item by PK |
| PUT | `/api/v1/worklist/:id` | Update worklist item (status) |
| DELETE | `/api/v1/worklist/:id` | Delete worklist item |

#### 3.8.3 Worklist Item Structure

```json
{
  "pk": 1,
  "step_id": "SPS001",
  "step_status": "SCHEDULED",
  "patient_id": "PAT001",
  "patient_name": "DOE^JOHN",
  "birth_date": "19800101",
  "sex": "M",
  "accession_no": "ACC001",
  "requested_proc_id": "RP001",
  "study_uid": "1.2.3.4.5",
  "scheduled_datetime": "20260103T140000",
  "station_ae": "CT_SCANNER",
  "station_name": "CT Room 1",
  "modality": "CT",
  "procedure_desc": "CT CHEST W/O CONTRAST",
  "protocol_code": "CTCHEST001",
  "referring_phys": "SMITH^JANE",
  "referring_phys_id": "PHY001",
  "created_at": "2026-01-02T10:00:00Z",
  "updated_at": "2026-01-03T08:00:00Z"
}
```

#### 3.8.4 Query Parameters

| Parameter | Description |
|-----------|-------------|
| `station_ae` | Filter by scheduled station AE title |
| `modality` | Filter by modality |
| `scheduled_date_from` | Start date range |
| `scheduled_date_to` | End date range |
| `patient_id` | Filter by Patient ID |
| `patient_name` | Filter by Patient Name |
| `accession_no` | Filter by Accession Number |
| `step_id` | Filter by Scheduled Procedure Step ID |
| `include_all_status` | Include completed/cancelled items |

#### 3.8.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/worklist_endpoints.hpp` | Endpoint registration |
| `src/web/endpoints/worklist_endpoints.cpp` | Full CRUD implementation |

---

### DES-WEB-009: audit_endpoints

**Traces to:** SRS-WEB-008 (Audit Log API)

#### 3.9.1 Purpose

RESTful endpoints for audit log access and export.

#### 3.9.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/audit/logs` | List audit entries (paginated) |
| GET | `/api/v1/audit/logs/:id` | Get specific audit entry |
| GET | `/api/v1/audit/export` | Export audit logs (JSON/CSV) |

#### 3.9.3 Audit Record Structure

```json
{
  "pk": 1,
  "event_type": "C-STORE",
  "outcome": "SUCCESS",
  "timestamp": "2026-01-03T10:30:00Z",
  "user_id": "admin",
  "source_ae": "MODALITY1",
  "target_ae": "MY_PACS",
  "source_ip": "192.168.1.100",
  "patient_id": "PAT001",
  "study_uid": "1.2.3.4.5",
  "message": "Study stored successfully",
  "details": "{\"instances\": 150}"
}
```

#### 3.9.4 Query Parameters

| Parameter | Description |
|-----------|-------------|
| `event_type` | Filter by event type (C-STORE, C-FIND, etc.) |
| `outcome` | Filter by outcome (SUCCESS, FAILURE) |
| `user_id` | Filter by user ID |
| `source_ae` | Filter by source AE title |
| `patient_id` | Filter by Patient ID |
| `study_uid` | Filter by Study Instance UID |
| `date_from` | Start date range |
| `date_to` | End date range |
| `format` | Export format (json, csv) |

#### 3.9.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/audit_endpoints.hpp` | Endpoint registration |
| `src/web/endpoints/audit_endpoints.cpp` | Query and export implementation |

---

### DES-WEB-010: security_endpoints

**Traces to:** SRS-WEB-009 (Security Management API)

#### 3.10.1 Purpose

RESTful endpoints for user and role management (RBAC).

#### 3.10.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/v1/security/users` | Create new user |
| POST | `/api/v1/security/users/:id/roles` | Assign role to user |

#### 3.10.3 Request Examples

**POST /api/v1/security/users**
```json
{
  "id": "user001",
  "username": "john.doe"
}
```

**POST /api/v1/security/users/:id/roles**
```json
{
  "role": "RADIOLOGIST"
}
```

Supported roles:
- `ADMINISTRATOR`
- `RADIOLOGIST`
- `TECHNOLOGIST`
- `REFERRING_PHYSICIAN`
- `VIEWER`

#### 3.10.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/security_endpoints.hpp` | Endpoint registration |
| `src/web/endpoints/security_endpoints.cpp` | User/role management |

---

### DES-WEB-011: system_endpoints

**Traces to:** SRS-WEB-010 (System Monitoring API)

#### 3.11.1 Purpose

RESTful endpoints for system health, metrics, and configuration.

#### 3.11.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/system/health` | Health check endpoint |
| GET | `/api/v1/system/info` | System information |
| GET | `/api/v1/system/config` | Current configuration |
| GET | `/api/v1/system/metrics` | Performance metrics |
| GET | `/api/v1/system/storage` | Storage statistics |

#### 3.11.3 Response Examples

**GET /api/v1/system/health**
```json
{
  "status": "healthy",
  "checks": {
    "database": "ok",
    "storage": "ok",
    "dicom_server": "ok"
  },
  "uptime_seconds": 86400
}
```

**GET /api/v1/system/info**
```json
{
  "version": "0.1.2",
  "ae_title": "MY_PACS",
  "hostname": "pacs-server-01",
  "uptime": "1 day, 2:30:15"
}
```

**GET /api/v1/system/storage**
```json
{
  "total_bytes": 1099511627776,
  "used_bytes": 549755813888,
  "free_bytes": 549755813888,
  "usage_percent": 50.0,
  "total_studies": 5000,
  "total_instances": 500000
}
```

#### 3.11.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/system_endpoints.hpp` | Endpoint registration + utilities |
| `src/web/endpoints/system_endpoints.cpp` | Health, info, metrics implementation |

---

### DES-WEB-012: association_endpoints

**Traces to:** SRS-WEB-011 (DICOM Association Monitoring API)

#### 3.12.1 Purpose

RESTful endpoints for monitoring and managing active DICOM associations.

#### 3.12.2 API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/v1/associations/active` | List active associations |
| GET | `/api/v1/associations/:id` | Get association details |
| DELETE | `/api/v1/associations/:id` | Terminate association |

#### 3.12.3 Response Examples

**GET /api/v1/associations/active**
```json
{
  "data": [
    {
      "id": "assoc-001",
      "calling_ae": "MODALITY1",
      "called_ae": "MY_PACS",
      "source_ip": "192.168.1.100",
      "state": "ESTABLISHED",
      "started_at": "2026-01-03T10:30:00Z",
      "service": "C-STORE",
      "transferred_bytes": 104857600
    }
  ],
  "count": 1
}
```

#### 3.12.4 Implementation Notes

> **Note:** Full association management requires integration with the DICOM server component.
> Current implementation returns placeholder data. TODO items marked in code for future integration.

#### 3.12.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/association_endpoints.hpp` | Endpoint registration |
| `src/web/endpoints/association_endpoints.cpp` | Placeholder implementation |

---

### DES-WEB-013: annotation_endpoints

**Traces to:** FR-11.1 (Annotations)

#### 3.13.1 Purpose

REST API for creating, retrieving, updating, and deleting annotations on DICOM images. Supports query filtering by study, series, instance, and user.

#### 3.13.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `POST` | `/api/v1/annotations` | Create annotation |
| `GET` | `/api/v1/annotations` | List annotations (with filters and pagination) |
| `GET` | `/api/v1/annotations/<id>` | Get annotation by ID |
| `PUT` | `/api/v1/annotations/<id>` | Update annotation |
| `DELETE` | `/api/v1/annotations/<id>` | Delete annotation |
| `GET` | `/api/v1/instances/<uid>/annotations` | Get annotations for DICOM instance |

#### 3.13.3 Query Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `study_uid` | string | Filter by Study Instance UID |
| `series_uid` | string | Filter by Series Instance UID |
| `sop_instance_uid` | string | Filter by SOP Instance UID |
| `user_id` | string | Filter by user who created the annotation |
| `limit` | integer | Pagination limit |
| `offset` | integer | Pagination offset |

#### 3.13.4 Dependencies

| Module | Purpose |
|--------|---------|
| `storage::annotation_repository` (DES-STOR-010) | Annotation persistence |
| `storage::index_database` (DES-STOR-003) | Instance validation |

#### 3.13.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/annotation_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/annotation_endpoints.cpp` | Route handlers |

---

### DES-WEB-014: jobs_endpoints

**Traces to:** FR-10.1 (Job Management)

#### 3.14.1 Purpose

Comprehensive job management REST API supporting CRUD operations, job lifecycle control, and real-time WebSocket progress updates. Handles multiple job types (retrieve, store, query, sync, prefetch).

#### 3.14.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/api/v1/jobs` | List jobs (with status/type filters) |
| `POST` | `/api/v1/jobs` | Create new job |
| `GET` | `/api/v1/jobs/<id>` | Get job details |
| `DELETE` | `/api/v1/jobs/<id>` | Delete job |
| `GET` | `/api/v1/jobs/<id>/progress` | Get job progress |
| `POST` | `/api/v1/jobs/<id>/start` | Start job |
| `POST` | `/api/v1/jobs/<id>/pause` | Pause job |
| `POST` | `/api/v1/jobs/<id>/resume` | Resume job |
| `POST` | `/api/v1/jobs/<id>/cancel` | Cancel job |
| `POST` | `/api/v1/jobs/<id>/retry` | Retry failed job |

#### 3.14.3 Job Types

| Type | Description | Parameters |
|------|-------------|------------|
| `retrieve` | C-MOVE/C-GET operation | `study_uid`, `node_id` |
| `store` | C-STORE operation | `study_uid`, `destination_node_id` |
| `query` | C-FIND operation | `query_level`, `filters`, `node_id` |
| `sync` | Synchronization | `sync_config_id` |
| `prefetch` | Prior study prefetch | `patient_id`, `modality` |

#### 3.14.4 Dependencies

| Module | Purpose |
|--------|---------|
| `client::job_manager` (DES-CLI-001) | Job orchestration |
| `client::job_types` | Job type definitions |

#### 3.14.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/jobs_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/jobs_endpoints.cpp` | Route handlers and WebSocket support |

---

### DES-WEB-015: key_image_endpoints

**Traces to:** FR-11.3 (Key Images)

#### 3.15.1 Purpose

Manage key images (clinically significant images marked by radiologists) on DICOM studies, including creation, retrieval, deletion, and export to DICOM SR format.

#### 3.15.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `POST` | `/api/v1/studies/<uid>/key-images` | Create key image for study |
| `GET` | `/api/v1/studies/<uid>/key-images` | List key images for study |
| `DELETE` | `/api/v1/key-images/<id>` | Delete key image |
| `POST` | `/api/v1/studies/<uid>/key-images/export-sr` | Export as DICOM Structured Report |

#### 3.15.3 Dependencies

| Module | Purpose |
|--------|---------|
| `storage::key_image_repository` (DES-STOR-017) | Key image persistence |
| `storage::index_database` (DES-STOR-003) | Study validation |

#### 3.15.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/key_image_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/key_image_endpoints.cpp` | Route handlers |

---

### DES-WEB-016: measurement_endpoints

**Traces to:** FR-11.4 (Measurements)

#### 3.16.1 Purpose

REST API for managing image measurements (distances, areas, volumes) on DICOM instances. Supports different measurement types with geometry and value storage.

#### 3.16.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `POST` | `/api/v1/measurements` | Create measurement |
| `GET` | `/api/v1/measurements` | List measurements (with filters) |
| `GET` | `/api/v1/measurements/<id>` | Get measurement by ID |
| `DELETE` | `/api/v1/measurements/<id>` | Delete measurement |
| `GET` | `/api/v1/instances/<uid>/measurements` | Get measurements for instance |

#### 3.16.3 Dependencies

| Module | Purpose |
|--------|---------|
| `storage::measurement_repository` (DES-STOR-018) | Measurement persistence |
| `storage::index_database` (DES-STOR-003) | Instance validation |

#### 3.16.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/measurement_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/measurement_endpoints.cpp` | Route handlers |

---

### DES-WEB-017: metadata_endpoints

**Traces to:** SRS-WEB-002 (DICOMweb WADO-RS)

#### 3.17.1 Purpose

Selective metadata retrieval endpoints optimized for viewer performance. Provides series navigation, window/level presets, VOI LUT data, and multi-frame information.

#### 3.17.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/api/v1/instances/<uid>/metadata` | Selective metadata for instance |
| `GET` | `/api/v1/series/<uid>/instances/sorted` | Sorted instances in series |
| `GET` | `/api/v1/instances/<uid>/navigation` | Navigation info (prev/next) |
| `GET` | `/api/v1/presets/window-level` | Window/level presets by modality |
| `GET` | `/api/v1/instances/<uid>/voi-lut` | VOI Lookup Table data |
| `GET` | `/api/v1/instances/<uid>/frame-info` | Multi-frame information |

#### 3.17.3 Dependencies

| Module | Purpose |
|--------|---------|
| `web::metadata_service` | Metadata extraction and formatting |
| `storage::index_database` (DES-STOR-003) | Instance lookup |

#### 3.17.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/metadata_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/metadata_endpoints.cpp` | Route handlers |

---

### DES-WEB-018: metrics_endpoints

**Traces to:** FR-12.1 (Database Monitoring)

#### 3.18.1 Purpose

Database monitoring and observability endpoints providing health status, performance metrics, slow query analysis, and Prometheus-compatible metrics export.

#### 3.18.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/api/health/database` | Database health check |
| `GET` | `/api/metrics/database` | Current database metrics (JSON) |
| `GET` | `/api/metrics/database/slow-queries` | Slow query list |
| `GET` | `/metrics` | Prometheus text exposition format |

#### 3.18.3 Query Parameters

| Parameter | Endpoint | Description |
|-----------|----------|-------------|
| `limit` | slow-queries | Maximum results (default: 10) |
| `since_minutes` | slow-queries | Time window (default: 5) |

#### 3.18.4 Prometheus Metrics

| Metric | Type | Description |
|--------|------|-------------|
| `pacs_db_queries_total` | counter | Total query count |
| `pacs_db_query_duration_microseconds` | summary | Query latency distribution |
| `pacs_db_queries_per_second` | gauge | Current throughput |
| `pacs_db_connections` | gauge | Active connections |
| `pacs_db_connection_utilization` | gauge | Connection pool utilization |
| `pacs_db_error_rate` | gauge | Error rate percentage |
| `pacs_db_slow_queries` | counter | Slow query count |

#### 3.18.5 Dependencies

| Module | Purpose |
|--------|---------|
| `services::monitoring::database_metrics_service` (DES-MON-007) | Metrics collection |

#### 3.18.6 Conditional Compilation

Requires `PACS_WITH_DATABASE_SYSTEM` preprocessor flag.

#### 3.18.7 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/metrics_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/metrics_endpoints.cpp` | Route handlers |

---

### DES-WEB-019: remote_nodes_endpoints

**Traces to:** FR-10.4 (Remote Node Manager)

#### 3.19.1 Purpose

Complete remote PACS node management API supporting CRUD operations, connectivity verification, capability detection, and DICOM operations (C-FIND, C-MOVE).

#### 3.19.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/api/v1/remote-nodes` | List remote nodes (with status filter) |
| `POST` | `/api/v1/remote-nodes` | Create remote node |
| `GET` | `/api/v1/remote-nodes/<id>` | Get node details |
| `PUT` | `/api/v1/remote-nodes/<id>` | Update node configuration |
| `DELETE` | `/api/v1/remote-nodes/<id>` | Delete node |
| `POST` | `/api/v1/remote-nodes/<id>/verify` | Verify connectivity (C-ECHO) |
| `GET` | `/api/v1/remote-nodes/<id>/status` | Get node status and capabilities |
| `POST` | `/api/v1/remote-nodes/<id>/query` | Execute C-FIND query |
| `POST` | `/api/v1/remote-nodes/<id>/retrieve` | Retrieve studies (C-MOVE) |

#### 3.19.3 Dependencies

| Module | Purpose |
|--------|---------|
| `client::remote_node_manager` (DES-CLI-005) | Node management |
| `client::remote_node` | Node data model |

#### 3.19.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/remote_nodes_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/remote_nodes_endpoints.cpp` | Route handlers |

---

### DES-WEB-020: routing_endpoints

**Traces to:** FR-10.2 (Routing Manager)

#### 3.20.1 Purpose

Comprehensive routing rule management API supporting CRUD operations, rule reordering, global enable/disable, and dry-run testing for automatic DICOM image routing.

#### 3.20.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/api/v1/routing/rules` | List routing rules (with pagination) |
| `POST` | `/api/v1/routing/rules` | Create routing rule |
| `GET` | `/api/v1/routing/rules/<id>` | Get rule details |
| `PUT` | `/api/v1/routing/rules/<id>` | Update rule |
| `DELETE` | `/api/v1/routing/rules/<id>` | Delete rule |
| `POST` | `/api/v1/routing/rules/reorder` | Reorder rules |
| `POST` | `/api/v1/routing/enable` | Enable routing globally |
| `POST` | `/api/v1/routing/disable` | Disable routing globally |
| `GET` | `/api/v1/routing/status` | Get routing status and statistics |
| `POST` | `/api/v1/routing/test` | Test all rules (dry run) |
| `POST` | `/api/v1/routing/rules/<id>/test` | Test specific rule (dry run) |

#### 3.20.3 Dependencies

| Module | Purpose |
|--------|---------|
| `client::routing_manager` (DES-CLI-002) | Routing logic |
| `client::routing_types` | Rule data models |

#### 3.20.4 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/routing_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/routing_endpoints.cpp` | Route handlers |

---

### DES-WEB-021: thumbnail_endpoints

**Traces to:** SRS-WEB-002 (DICOMweb WADO-RS - Rendered/Thumbnail)

#### 3.21.1 Purpose

Thumbnail generation and management API for DICOM viewer optimization. Supports on-demand generation with configurable size, format, quality, and frame selection. Includes cache management.

#### 3.21.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/api/v1/thumbnails/instances/<uid>` | Get instance thumbnail |
| `GET` | `/api/v1/thumbnails/series/<uid>` | Get series thumbnail |
| `GET` | `/api/v1/thumbnails/studies/<uid>` | Get study thumbnail |
| `DELETE` | `/api/v1/thumbnails/cache` | Clear thumbnail cache |
| `GET` | `/api/v1/thumbnails/cache/stats` | Get cache statistics |

#### 3.21.3 Query Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `size` | integer | 128 | Thumbnail size (64/128/256/512) |
| `format` | string | `jpeg` | Output format (jpeg/png) |
| `quality` | integer | 80 | JPEG quality (1-100) |
| `frame` | integer | 0 | Frame number for multi-frame |

#### 3.21.4 Dependencies

| Module | Purpose |
|--------|---------|
| `web::thumbnail_service` | Image generation and caching |
| `storage::index_database` (DES-STOR-003) | Instance lookup |

#### 3.21.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/thumbnail_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/thumbnail_endpoints.cpp` | Route handlers |

---

### DES-WEB-022: viewer_state_endpoints

**Traces to:** FR-11.2 (Viewer State)

#### 3.22.1 Purpose

Viewer state persistence API enabling save/restore of viewport state, window/level settings, and annotation references. Also tracks per-user study access patterns for recent studies.

#### 3.22.2 Routes

| Method | Route | Description |
|--------|-------|-------------|
| `POST` | `/api/v1/viewer-states` | Save viewer state |
| `GET` | `/api/v1/viewer-states` | List viewer states (with filters) |
| `GET` | `/api/v1/viewer-states/<id>` | Get viewer state by ID |
| `DELETE` | `/api/v1/viewer-states/<id>` | Delete viewer state |
| `GET` | `/api/v1/users/<user_id>/recent-studies` | Get user's recent studies |

#### 3.22.3 Query Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `study_uid` | string | Filter by Study Instance UID |
| `user_id` | string | Filter by user ID |
| `limit` | integer | Pagination limit (also used for recent studies) |

#### 3.22.4 Dependencies

| Module | Purpose |
|--------|---------|
| `storage::viewer_state_repository` (DES-STOR-011) | State persistence |
| `storage::index_database` (DES-STOR-003) | Study validation |

#### 3.22.5 Implementation Files

| File | Description |
|------|-------------|
| `include/pacs/web/endpoints/viewer_state_endpoints.hpp` | Endpoint declarations |
| `src/web/endpoints/viewer_state_endpoints.cpp` | Route handlers |

---

## 4. API Endpoint Reference

### 4.1 Complete Endpoint Summary

#### Management API (`/api/v1/`)

| Endpoint Group | Base Path | Methods | DES ID |
|----------------|-----------|---------|--------|
| Patients | `/api/v1/patients` | GET, DELETE | DES-WEB-004 |
| Studies | `/api/v1/studies` | GET, DELETE | DES-WEB-005 |
| Series | `/api/v1/series` | GET | DES-WEB-006 |
| Worklist | `/api/v1/worklist` | GET, POST, PUT, DELETE | DES-WEB-008 |
| Audit | `/api/v1/audit` | GET | DES-WEB-009 |
| Security | `/api/v1/security` | POST | DES-WEB-010 |
| System | `/api/v1/system` | GET | DES-WEB-011 |
| Associations | `/api/v1/associations` | GET, DELETE | DES-WEB-012 |

#### DICOMweb API (`/dicomweb/`)

| Service | Base Path | Methods | DES ID |
|---------|-----------|---------|--------|
| WADO-RS | `/dicomweb/studies` | GET | DES-WEB-007 |
| STOW-RS | `/dicomweb/studies` | POST | DES-WEB-007 |
| QIDO-RS | `/dicomweb/studies`, `/dicomweb/series`, `/dicomweb/instances` | GET | DES-WEB-007 |

---

## 5. DICOMweb Standards Compliance

### 5.1 WADO-RS Compliance (PS3.18 Section 10)

| Capability | Status | Notes |
|------------|--------|-------|
| Retrieve Study | ✅ Implemented | Full study retrieval |
| Retrieve Series | ✅ Implemented | Full series retrieval |
| Retrieve Instance | ✅ Implemented | Single instance retrieval |
| Retrieve Metadata | ✅ Implemented | JSON metadata |
| Retrieve Frames | ✅ Implemented | Multi-frame support |
| Retrieve Rendered | ✅ Implemented | JPEG/PNG output |
| Bulk Data Retrieval | ✅ Implemented | Separate bulk data |

### 5.2 STOW-RS Compliance (PS3.18 Section 10.5)

| Capability | Status | Notes |
|------------|--------|-------|
| Store Study | ✅ Implemented | Multipart request |
| Store Response | ✅ Implemented | DICOM JSON |
| Duplicate Handling | ✅ Implemented | Configurable |
| Validation | ✅ Implemented | SOP Class validation |

### 5.3 QIDO-RS Compliance (PS3.18 Section 10.6)

| Capability | Status | Notes |
|------------|--------|-------|
| Search Studies | ✅ Implemented | All standard attributes |
| Search Series | ✅ Implemented | Hierarchical and flat |
| Search Instances | ✅ Implemented | Hierarchical and flat |
| Pagination | ✅ Implemented | `limit`/`offset` |
| Wildcards | ✅ Implemented | `*` and `?` |
| Date Ranges | ✅ Implemented | `-` separator |

---

## 6. Error Handling

### 6.1 Error Response Format

All errors return a consistent JSON structure:

```json
{
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable error message"
  }
}
```

### 6.2 HTTP Status Codes

| Status | Error Code | Description |
|--------|------------|-------------|
| 400 | `INVALID_REQUEST` | Malformed request |
| 400 | `INVALID_JSON` | Invalid JSON body |
| 400 | `MISSING_FIELDS` | Required fields missing |
| 401 | `UNAUTHORIZED` | Authentication required |
| 403 | `FORBIDDEN` | Insufficient permissions |
| 404 | `NOT_FOUND` | Resource not found |
| 409 | `CONFLICT` | Resource already exists |
| 500 | `INTERNAL_ERROR` | Server error |
| 501 | `NOT_IMPLEMENTED` | Feature not implemented |
| 503 | `DATABASE_UNAVAILABLE` | Database not configured |
| 503 | `SECURITY_UNAVAILABLE` | Security manager not configured |

### 6.3 Error Code Reference

| Error Code | HTTP Status | Description |
|------------|-------------|-------------|
| `DATABASE_UNAVAILABLE` | 503 | Index database not configured |
| `STORAGE_UNAVAILABLE` | 503 | File storage not configured |
| `SECURITY_UNAVAILABLE` | 503 | Security manager not configured |
| `NOT_FOUND` | 404 | Requested resource not found |
| `QUERY_ERROR` | 500 | Database query failed |
| `CREATE_FAILED` | 500 | Failed to create resource |
| `UPDATE_FAILED` | 500 | Failed to update resource |
| `DELETE_FAILED` | 500 | Failed to delete resource |
| `INVALID_REQUEST` | 400 | Invalid request parameters |
| `INVALID_JSON` | 400 | Malformed JSON body |
| `INVALID_ROLE` | 400 | Invalid role specified |
| `NOT_IMPLEMENTED` | 501 | Feature not yet implemented |

---

## 7. Security Considerations

### 7.1 Authentication

- **Basic Auth**: Supported via middleware (optional)
- **Bearer Token**: JWT support planned
- **AE Title Whitelist**: Applied to DICOMweb requests

### 7.2 Authorization (RBAC)

Role-based access control for management endpoints:

| Role | Patients | Studies | Worklist | Audit | Security |
|------|----------|---------|----------|-------|----------|
| Administrator | Full | Full | Full | Full | Full |
| Radiologist | Read | Read | Read | Read | None |
| Technologist | Read | Read | Full | Read | None |
| Viewer | Read | Read | Read | None | None |

### 7.3 CORS Configuration

Default CORS settings allow cross-origin requests. Production deployments should configure:

```cpp
rest_config config;
config.cors_allowed_origins = "https://viewer.hospital.com";
config.cors_allowed_methods = "GET, POST, PUT, DELETE";
config.cors_allowed_headers = "Content-Type, Authorization";
```

### 7.4 Rate Limiting

- Not implemented in current version
- Planned for future releases
- Recommend using reverse proxy (nginx, HAProxy) for production

---

## 8. Dependencies

### 8.1 External Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| Crow | 1.0+ | HTTP server framework |
| OpenSSL | 1.1+ | TLS support (optional) |

### 8.2 Internal Dependencies

| Module | DES ID | Purpose |
|--------|--------|---------|
| `storage::index_database` | DES-STOR-003 | Query patient/study/series data |
| `storage::storage_interface` | DES-STOR-001 | DICOM file retrieval |
| `storage::file_storage` | DES-STOR-002 | Default file storage |
| `security::access_control_manager` | DES-SEC-005 | User/role management |
| `core::dicom_dataset` | DES-CORE-003 | DICOM data handling |
| `core::dicom_file` | DES-CORE-004 | DICOM file I/O |
| `encoding::compression::jpeg_baseline_codec` | DES-ENC-009 | Image rendering |

---

## Appendix A: Implementation Traceability

| DES ID | Header File | Source File | Test File |
|--------|-------------|-------------|-----------|
| DES-WEB-001 | `include/pacs/web/rest_server.hpp` | `src/web/rest_server.cpp` | `tests/web/rest_server_test.cpp` |
| DES-WEB-002 | `include/pacs/web/rest_config.hpp` | (header-only) | - |
| DES-WEB-003 | `include/pacs/web/rest_types.hpp` | (header-only) | - |
| DES-WEB-004 | `include/pacs/web/endpoints/patient_endpoints.hpp` | `src/web/endpoints/patient_endpoints.cpp` | `tests/web/patient_study_endpoints_test.cpp` |
| DES-WEB-005 | `include/pacs/web/endpoints/study_endpoints.hpp` | `src/web/endpoints/study_endpoints.cpp` | `tests/web/patient_study_endpoints_test.cpp` |
| DES-WEB-006 | `include/pacs/web/endpoints/series_endpoints.hpp` | `src/web/endpoints/series_endpoints.cpp` | - |
| DES-WEB-007 | `include/pacs/web/endpoints/dicomweb_endpoints.hpp` | `src/web/endpoints/dicomweb_endpoints.cpp` | `tests/web/dicomweb_endpoints_test.cpp` |
| DES-WEB-008 | `include/pacs/web/endpoints/worklist_endpoints.hpp` | `src/web/endpoints/worklist_endpoints.cpp` | `tests/web/worklist_audit_endpoints_test.cpp` |
| DES-WEB-009 | `include/pacs/web/endpoints/audit_endpoints.hpp` | `src/web/endpoints/audit_endpoints.cpp` | `tests/web/worklist_audit_endpoints_test.cpp` |
| DES-WEB-010 | `include/pacs/web/endpoints/security_endpoints.hpp` | `src/web/endpoints/security_endpoints.cpp` | - |
| DES-WEB-011 | `include/pacs/web/endpoints/system_endpoints.hpp` | `src/web/endpoints/system_endpoints.cpp` | `tests/web/system_endpoints_test.cpp` |
| DES-WEB-012 | `include/pacs/web/endpoints/association_endpoints.hpp` | `src/web/endpoints/association_endpoints.cpp` | - |
| DES-WEB-013 | `include/pacs/web/endpoints/annotation_endpoints.hpp` | `src/web/endpoints/annotation_endpoints.cpp` | - |
| DES-WEB-014 | `include/pacs/web/endpoints/jobs_endpoints.hpp` | `src/web/endpoints/jobs_endpoints.cpp` | - |
| DES-WEB-015 | `include/pacs/web/endpoints/key_image_endpoints.hpp` | `src/web/endpoints/key_image_endpoints.cpp` | - |
| DES-WEB-016 | `include/pacs/web/endpoints/measurement_endpoints.hpp` | `src/web/endpoints/measurement_endpoints.cpp` | - |
| DES-WEB-017 | `include/pacs/web/endpoints/metadata_endpoints.hpp` | `src/web/endpoints/metadata_endpoints.cpp` | - |
| DES-WEB-018 | `include/pacs/web/endpoints/metrics_endpoints.hpp` | `src/web/endpoints/metrics_endpoints.cpp` | - |
| DES-WEB-019 | `include/pacs/web/endpoints/remote_nodes_endpoints.hpp` | `src/web/endpoints/remote_nodes_endpoints.cpp` | - |
| DES-WEB-020 | `include/pacs/web/endpoints/routing_endpoints.hpp` | `src/web/endpoints/routing_endpoints.cpp` | - |
| DES-WEB-021 | `include/pacs/web/endpoints/thumbnail_endpoints.hpp` | `src/web/endpoints/thumbnail_endpoints.cpp` | - |
| DES-WEB-022 | `include/pacs/web/endpoints/viewer_state_endpoints.hpp` | `src/web/endpoints/viewer_state_endpoints.cpp` | - |

---

## Appendix B: Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-01-03 | kcenon@naver.com | Initial Web/REST API module SDS with DES-WEB-001 to DES-WEB-012 |
| 2.0.0 | 2026-02-08 | kcenon@naver.com | Added DES-WEB-013 to DES-WEB-022: annotation, jobs, key image, measurement, metadata, metrics, remote nodes, routing, thumbnail, viewer state endpoints |

---

*Document Version: 2.0.0*
*Last Updated: 2026-02-08*
*Author: kcenon@naver.com*
