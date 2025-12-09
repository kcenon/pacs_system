# Security Module Documentation

> **Version:** 1.0.0
> **Last Updated:** 2025-12-09
> **Language:** **English**

---

## Table of Contents

- [Overview](#overview)
- [Access Control Model](#access-control-model)
- [Roles and Permissions](#roles-and-permissions)
- [Authentication](#authentication)
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

## Authentication

Currently, the system supports token-based authentication (integrated via header for MVP).

*   **Header**: `X-User-ID`
*   **Format**: `X-User-ID: <username>`

> **Note**: In a future release, this will be replaced by JWT (JSON Web Tokens) or OIDC integration.

## Configuration

Security is configured via the `AccessControlManager`.

```cpp
auto storage = std::make_shared<sqlite_security_storage>("security.db");
access_control_manager acm;
acm.set_storage(storage);
```
