# Security Module Documentation

> **Version:** 0.1.1.0
> **Last Updated:** 2025-12-11
> **Language:** **English**

---

## Table of Contents

- [Overview](#overview)
- [Access Control Model](#access-control-model)
- [Roles and Permissions](#roles-and-permissions)
- [DICOM Integration](#dicom-integration)
- [AE Title Authentication](#ae-title-authentication)
- [Audit Logging](#audit-logging)
- [REST API Authentication](#rest-api-authentication)
- [Configuration](#configuration)

---

## Overview

The PACS Security Module provides Role-Based Access Control (RBAC) to secure system resources, including DICOM data, audit logs, and system configuration. It integrates with the `pacs_web` module to enforce permissions on REST API endpoints.

## Access Control Model

The system uses a standard RBAC model where:
1.  **Users** are assigned one or more **Roles**.
2.  **Roles** grants a set of **Permissions**.
3.  **Permissions** consist of a **ResourceType** and an **Action**.

### Resource Types

| Resource Type | Description |
|---------------|-------------|
| `Study`       | DICOM Studies, Series, and Images |
| `Metadata`    | Patient and Study metadata (PHI) |
| `System`      | System configuration and service management |
| `Audit`       | Security and operation audit logs |
| `User`        | User and Role management |

### Actions

| Action | Bitmask | Description |
|--------|---------|-------------|
| `Read` | `0x01`  | View or query resources |
| `Write`| `0x02`  | Create or modify resources |
| `Delete`| `0x04` | Remove resources |
| `Export`| `0x08` | Download or transfer resources |
| `Full` | `0xFF`  | All actions |

## Roles and Permissions

The system comes with the following independent pre-defined roles:

### `Viewer`
*   **Purpose**: Clinical viewing only.
*   **Permissions**: 
    *   `Study`: Read
    *   `Metadata`: Read

### `Technologist`
*   **Purpose**: Acquiring images and updating study status.
*   **Permissions**:
    *   `Study`: Read, Write
    *   `Metadata`: Read, Write

### `Radiologist`
*   **Purpose**: Diagnosis and Reporting.
*   **Permissions**:
    *   `Study`: Full (Read, Write, Delete, Export)
    *   `Metadata`: Read, Write

### `Administrator`
*   **Purpose**: User management and system configuration.
*   **Permissions**:
    *   `System`: Full
    *   `User`: Full
    *   `Audit`: Full
    *   `Role`: Full
    *   `Study`: Full (Access to all data)

## DICOM Integration

The security module integrates with the DICOM server to enforce access control on DICOM operations. Each DICOM operation is mapped to a resource type and action.

### DICOM Operation Mapping

| DICOM Operation | Resource Type | Action | Description |
|-----------------|---------------|--------|-------------|
| C-STORE | `Study` | Write | Store images/studies |
| C-FIND | `Metadata` | Read | Query studies/patients |
| C-MOVE | `Study` | Export | Move/retrieve studies |
| C-GET | `Study` | Read + Export | Retrieve studies |
| C-ECHO | `System` | Execute | Verification |
| N-CREATE | `Study` | Write | Create objects |
| N-DELETE | `Study` | Delete | Delete objects |

### Usage with DICOM Server

```cpp
// Create access control manager
auto acm = std::make_shared<access_control_manager>();
acm->set_storage(storage);

// Configure DICOM server with access control
dicom_server_v2 server(config);
server.set_access_control(acm);
server.set_access_control_enabled(true);

// Register AE Titles to users
acm->register_ae_title("CT_SCANNER_AE", "user_id_123");
acm->register_ae_title("PACS_WORKSTATION", "user_id_456");
```

## AE Title Authentication

DICOM clients are authenticated by mapping AE Titles to user accounts. When a DICOM association is established, the calling AE Title is looked up to determine the user context.

### Registering AE Titles

```cpp
// Register an AE Title to a user
acm->register_ae_title("MODALITY_AE", "user_id");

// Unregister an AE Title
acm->unregister_ae_title("MODALITY_AE");

// Get user by AE Title
auto user_opt = acm->get_user_by_ae_title("MODALITY_AE");
```

### Anonymous Access

If an AE Title is not registered, the client receives an anonymous context with no permissions. This effectively blocks unauthorized DICOM clients when access control is enabled.

## Audit Logging

The security module supports audit logging for access control decisions. Register a callback to receive notifications for all permission checks.

```cpp
acm->set_audit_callback(
    [](const user_context& ctx, DicomOperation op, const AccessCheckResult& result) {
        // Log the access attempt
        logger.info("User {} attempted {} - {}",
            ctx.user().username,
            to_string(op),
            result.allowed ? "ALLOWED" : "DENIED: " + result.reason);
    });
```

## REST API Authentication

Currently, the system supports token-based authentication (integrated via header for MVP).

*   **Header**: `X-User-ID`
*   **Format**: `X-User-ID: <username>`

> **Note**: In a future release, this will be replaced by JWT (JSON Web Tokens) or OIDC integration.

## Configuration

Security is configured via the `AccessControlManager`.

### Basic Setup

```cpp
auto storage = std::make_shared<sqlite_security_storage>("security.db");
access_control_manager acm;
acm.set_storage(storage);
```

### Full Configuration Example

```cpp
// 1. Create storage and access control manager
auto storage = std::make_shared<sqlite_security_storage>("security.db");
auto acm = std::make_shared<access_control_manager>();
acm->set_storage(storage);

// 2. Create users
User technologist;
technologist.id = "tech001";
technologist.username = "ct_scanner";
technologist.active = true;
technologist.roles.push_back(Role::Technologist);
acm->create_user(technologist);

// 3. Map AE Titles
acm->register_ae_title("CT_SCANNER", "tech001");

// 4. Set up audit logging
acm->set_audit_callback([](const auto& ctx, auto op, const auto& result) {
    // Custom audit logging
});

// 5. Configure DICOM server
dicom_server_v2 server(config);
server.set_access_control(acm);
server.set_access_control_enabled(true);

// 6. Start server
server.start();
```
