# SDS - Security Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-03
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-SEC-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#469](https://github.com/kcenon/pacs_system/issues/469), [#468](https://github.com/kcenon/pacs_system/issues/468) |

### Related Standards

| Standard | Description |
|----------|-------------|
| DICOM PS3.15 | Security and System Management Profiles |
| DICOM PS3.15 Annex E | Attribute Confidentiality Profiles |
| DICOM PS3.15 Section C | Digital Signatures |
| HIPAA | 45 CFR 164.514(b)(2) - Safe Harbor De-identification |
| GDPR | Article 4(5) - Pseudonymization |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. RBAC Access Control](#2-rbac-access-control)
- [3. DICOM Anonymization](#3-dicom-anonymization)
- [4. Digital Signatures](#4-digital-signatures)
- [5. Security Storage](#5-security-storage)
- [6. Class Diagrams](#6-class-diagrams)
- [7. Sequence Diagrams](#7-sequence-diagrams)
- [8. Traceability](#8-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Security module for the PACS System. The Security module provides:

- **Role-Based Access Control (RBAC)**: Authorization for DICOM operations
- **DICOM Anonymization**: De-identification per PS3.15 Annex E profiles
- **Digital Signatures**: Dataset signing and verification per PS3.15 Section C
- **Security Storage**: Persistence interface for security-related data

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| RBAC Access Control | 5 headers, 1 source | DES-SEC-001 to DES-SEC-005 |
| DICOM Anonymization | 4 headers, 3 sources | DES-SEC-006 to DES-SEC-009 |
| Digital Signatures | 3 headers, 2 sources | DES-SEC-010 to DES-SEC-012 |
| Security Storage | 1 header | DES-SEC-013 |

### 1.3 Design Identifier Convention

```
DES-SEC-<NUMBER>

Where:
- DES: Design Specification prefix
- SEC: Security module identifier
- NUMBER: 3-digit sequential number (001-013)
```

---

## 2. RBAC Access Control

### 2.1 DES-SEC-001: User Entity

**Traces to:** SRS-SEC-002 (Access Control), NFR-3 (Security)

**File:** `include/pacs/security/user.hpp`

The `User` struct represents an authenticated user in the PACS system.

```cpp
namespace pacs::security {

struct User {
    std::string id;              // Unique user identifier
    std::string username;        // Human-readable username
    std::vector<Role> roles;     // Assigned roles
    bool active{true};           // Account status

    bool has_role(Role role) const;
};

} // namespace pacs::security
```

**Design Rationale:**
- Simple value struct for easy serialization
- Multiple roles support for flexible permission assignment
- Active flag for soft-delete and account suspension

---

### 2.2 DES-SEC-002: Role Entity

**Traces to:** SRS-SEC-002 (Access Control)

**File:** `include/pacs/security/role.hpp`

Predefined roles in the PACS system following the principle of least privilege.

```cpp
namespace pacs::security {

enum class Role {
    Viewer,         // Read-only access to studies
    Technologist,   // Upload/modify studies, no delete
    Radiologist,    // Full clinical access including verification
    Administrator,  // User management, system configuration
    System          // Internal system operations (highest privilege)
};

constexpr std::string_view to_string(Role role);
std::optional<Role> parse_role(std::string_view str);

} // namespace pacs::security
```

**Role Hierarchy:**

```
                    ┌──────────┐
                    │  System  │  (All permissions)
                    └────┬─────┘
                         │
              ┌──────────┴───────────┐
              │                      │
      ┌───────┴───────┐      ┌───────┴───────┐
      │ Administrator │      │  Radiologist  │
      │ (User Mgmt)   │      │ (Clinical)    │
      └───────────────┘      └───────┬───────┘
                                     │
                             ┌───────┴───────┐
                             │ Technologist  │
                             └───────┬───────┘
                                     │
                             ┌───────┴───────┐
                             │    Viewer     │
                             └───────────────┘
```

---

### 2.3 DES-SEC-003: Permission Entity

**Traces to:** SRS-SEC-002 (Access Control)

**File:** `include/pacs/security/permission.hpp`

Fine-grained permission model using resource types and action bitmasks.

```cpp
namespace pacs::security {

enum class ResourceType {
    Study,      // DICOM studies/series/instances
    Metadata,   // Patient/Study metadata
    System,     // System configuration
    Audit,      // Audit logs
    User,       // User management
    Role,       // Role management
    Series,     // DICOM Series
    Image       // DICOM Image
};

namespace Action {
    constexpr std::uint32_t None    = 0;
    constexpr std::uint32_t Read    = 1 << 0;
    constexpr std::uint32_t Write   = 1 << 1;  // Create/Update
    constexpr std::uint32_t Delete  = 1 << 2;
    constexpr std::uint32_t Export  = 1 << 3;  // Download/Move
    constexpr std::uint32_t Execute = 1 << 4;  // Run commands
    constexpr std::uint32_t All     = 0xFFFFFFFF;
}

struct Permission {
    ResourceType resource;
    std::uint32_t actions;  // Bitmask of Action flags

    constexpr bool has_action(std::uint32_t action_mask) const;
};

} // namespace pacs::security
```

**Permission Matrix (Default):**

| Role | Study | Metadata | System | Audit | User | Role |
|------|-------|----------|--------|-------|------|------|
| Viewer | R | R | - | - | - | - |
| Technologist | RW | RW | - | - | - | - |
| Radiologist | RWE | RWE | R | R | - | - |
| Administrator | RWDE | RWDE | RWE | RW | RWDE | RW |
| System | All | All | All | All | All | All |

*R=Read, W=Write, D=Delete, E=Export*

---

### 2.4 DES-SEC-004: User Context

**Traces to:** SRS-SEC-002 (Access Control), SRS-SEC-003 (Audit Trail)

**File:** `include/pacs/security/user_context.hpp`

Session-based security context for access control decisions and audit logging.

```cpp
namespace pacs::security {

class user_context {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    // Construction
    user_context(User user, std::string session_id);

    // Factory methods for special contexts
    static user_context system_context();
    static user_context anonymous_context(const std::string& session_id);

    // Accessors
    [[nodiscard]] const User& user() const noexcept;
    [[nodiscard]] const std::string& session_id() const noexcept;
    [[nodiscard]] time_point created_at() const noexcept;
    [[nodiscard]] time_point last_activity() const noexcept;

    // Validation
    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] bool has_role(Role role) const;

    // Activity tracking
    void touch();

    // Audit source information
    void set_source_ae_title(std::string ae);
    void set_source_ip(std::string ip);
    [[nodiscard]] const std::optional<std::string>& source_ae_title() const noexcept;
    [[nodiscard]] const std::optional<std::string>& source_ip() const noexcept;

private:
    User user_;
    std::string session_id_;
    time_point created_at_;
    time_point last_activity_;
    std::optional<std::string> source_ae_title_;
    std::optional<std::string> source_ip_;
};

} // namespace pacs::security
```

**Context Types:**

| Context Type | Purpose | Permissions |
|--------------|---------|-------------|
| `system_context()` | Internal operations | All (System role) |
| `anonymous_context()` | Unknown/unauthenticated | None |
| User-specific | Normal operation | Based on assigned roles |

---

### 2.5 DES-SEC-005: Access Control Manager

**Traces to:** SRS-SEC-002 (Access Control), SRS-SEC-003 (Audit Trail)

**File:** `include/pacs/security/access_control_manager.hpp`, `src/security/access_control_manager.cpp`

Central component for permission checks and DICOM operation authorization.

```cpp
namespace pacs::security {

enum class DicomOperation {
    CStore, CFind, CMove, CGet, CEcho,
    NCreate, NSet, NGet, NDelete, NAction, NEventReport
};

struct AccessCheckResult {
    bool allowed{false};
    std::string reason;

    static AccessCheckResult allow();
    static AccessCheckResult deny(std::string reason);
    explicit operator bool() const;
};

using AccessAuditCallback = std::function<void(
    const user_context& ctx,
    DicomOperation op,
    const AccessCheckResult& result
)>;

class access_control_manager {
public:
    access_control_manager();

    // Permission Checks
    [[nodiscard]] bool check_permission(const User& user, ResourceType resource,
                                        std::uint32_t action_mask) const;
    [[nodiscard]] bool has_role(const User& user, Role role) const;

    // Access Validation
    [[nodiscard]] auto validate_access(const user_context& ctx,
                                       ResourceType resource,
                                       std::uint32_t action_mask)
        -> kcenon::common::VoidResult;

    // DICOM Operation Authorization
    [[nodiscard]] AccessCheckResult check_dicom_operation(
        const user_context& ctx, DicomOperation op) const;

    // AE Title to User mapping
    [[nodiscard]] user_context get_context_for_ae(
        std::string_view ae_title, const std::string& session_id) const;
    void register_ae_title(std::string_view ae_title, std::string_view user_id);
    void unregister_ae_title(std::string_view ae_title);

    // Configuration
    void set_role_permissions(Role role, std::vector<Permission> permissions);
    [[nodiscard]] const std::vector<Permission>& get_role_permissions(Role role) const;

    // Storage Integration
    void set_storage(std::shared_ptr<security_storage_interface> storage);

    // Audit
    void set_audit_callback(AccessAuditCallback callback);

    // User Management Facade
    [[nodiscard]] auto create_user(const User& user) -> kcenon::common::VoidResult;
    [[nodiscard]] auto assign_role(std::string_view user_id, Role role)
        -> kcenon::common::VoidResult;
    [[nodiscard]] auto get_user(std::string_view id)
        -> kcenon::common::Result<User>;
    [[nodiscard]] auto get_user_by_ae_title(std::string_view ae_title)
        -> std::optional<User>;

private:
    std::map<Role, std::vector<Permission>> role_permissions_;
    std::shared_ptr<security_storage_interface> storage_;
    std::map<std::string, std::string> ae_to_user_id_;
    AccessAuditCallback audit_callback_;
    mutable std::mutex mutex_;

    void initialize_default_permissions();
    static std::pair<ResourceType, std::uint32_t>
        map_dicom_operation(DicomOperation op);
};

} // namespace pacs::security
```

**DICOM Operation to Permission Mapping:**

| DICOM Operation | ResourceType | Action |
|-----------------|--------------|--------|
| C-ECHO | System | Execute |
| C-STORE | Study | Write |
| C-FIND | Study, Metadata | Read |
| C-MOVE | Study | Export |
| C-GET | Study | Export |
| N-CREATE | Study | Write |
| N-SET | Study | Write |
| N-GET | Study | Read |
| N-DELETE | Study | Delete |
| N-ACTION | Study | Execute |
| N-EVENT-REPORT | Study | Read |

---

## 3. DICOM Anonymization

### 3.1 DES-SEC-006: Anonymization Profile

**Traces to:** SRS-SEC-001 (Data Protection), NFR-3 (Security)

**File:** `include/pacs/security/anonymization_profile.hpp`

Predefined de-identification profiles based on DICOM PS3.15 Annex E.

```cpp
namespace pacs::security {

enum class anonymization_profile : std::uint8_t {
    basic = 0,                          // Remove direct identifiers
    clean_pixel = 1,                    // Remove burned-in annotations
    clean_descriptions = 2,             // Sanitize text fields
    retain_longitudinal = 3,            // Preserve temporal relationships
    retain_patient_characteristics = 4, // Preserve demographics
    hipaa_safe_harbor = 5,              // HIPAA 18-identifier removal
    gdpr_compliant = 6                  // GDPR pseudonymization
};

constexpr std::string_view to_string(anonymization_profile profile) noexcept;
std::optional<anonymization_profile> profile_from_string(std::string_view name);

} // namespace pacs::security
```

**Profile Comparison:**

| Profile | Identifiers | Dates | UIDs | Demographics | Use Case |
|---------|-------------|-------|------|--------------|----------|
| basic | Remove | Zero | Replace | Remove | Basic de-id |
| clean_pixel | Remove | Zero | Replace | Remove | Burned-in text |
| clean_descriptions | Remove | Zero | Replace | Remove | Free-text fields |
| retain_longitudinal | Remove | Shift | Consistent | Remove | Temporal studies |
| retain_patient_characteristics | Remove | Zero | Replace | Keep | Research |
| hipaa_safe_harbor | Remove (18) | Year only | Replace | Remove | US compliance |
| gdpr_compliant | Pseudonymize | Shift | Consistent | Pseudonymize | EU compliance |

---

### 3.2 DES-SEC-007: Tag Action

**Traces to:** SRS-SEC-001 (Data Protection)

**File:** `include/pacs/security/tag_action.hpp`, `src/security/tag_action.cpp`

Tag-level de-identification actions per PS3.15 Annex E Table E.1-1.

```cpp
namespace pacs::security {

enum class tag_action : std::uint8_t {
    remove = 0,         // D - Delete attribute entirely
    empty = 1,          // Z - Replace with zero-length value
    remove_or_empty = 2,// X - Remove or empty based on presence
    keep = 3,           // K - Keep unchanged
    replace = 4,        // C - Replace with dummy value
    replace_uid = 5,    // U - Replace UIDs with new values
    hash = 6,           // Hash for research linkage
    encrypt = 7,        // Encrypt for pseudonymization
    shift_date = 8      // Shift dates by fixed offset
};

struct tag_action_config {
    tag_action action{tag_action::remove};
    std::string replacement_value;
    std::string hash_algorithm{"SHA256"};
    bool use_salt{true};

    // Factory methods
    static tag_action_config make_remove();
    static tag_action_config make_empty();
    static tag_action_config make_keep();
    static tag_action_config make_replace(std::string value);
    static tag_action_config make_hash(std::string algorithm = "SHA256",
                                       bool salt = true);
};

struct tag_action_record {
    core::dicom_tag tag;
    tag_action action;
    std::string original_value;
    std::string new_value;
    bool success{true};
    std::string error_message;
};

struct anonymization_report {
    std::size_t total_tags_processed{0};
    std::size_t tags_removed{0};
    std::size_t tags_emptied{0};
    std::size_t tags_replaced{0};
    std::size_t uids_replaced{0};
    std::size_t tags_kept{0};
    std::size_t dates_shifted{0};
    std::size_t values_hashed{0};
    std::vector<tag_action_record> action_records;
    std::string profile_name;
    std::optional<std::chrono::days> date_offset;
    std::chrono::system_clock::time_point timestamp;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    [[nodiscard]] bool is_successful() const noexcept;
    [[nodiscard]] std::size_t total_modifications() const noexcept;
};

namespace hipaa_identifiers {
    std::vector<core::dicom_tag> get_name_tags();
    std::vector<core::dicom_tag> get_geographic_tags();
    std::vector<core::dicom_tag> get_date_tags();
    std::vector<core::dicom_tag> get_communication_tags();
    std::vector<core::dicom_tag> get_unique_id_tags();
    std::vector<core::dicom_tag> get_all_identifier_tags();
}

} // namespace pacs::security
```

---

### 3.3 DES-SEC-008: UID Mapping

**Traces to:** SRS-SEC-001 (Data Protection)

**File:** `include/pacs/security/uid_mapping.hpp`, `src/security/uid_mapping.cpp`

Thread-safe UID mapping for consistent de-identification across datasets.

```cpp
namespace pacs::security {

class uid_mapping {
public:
    uid_mapping() = default;
    explicit uid_mapping(std::string uid_root);

    // Mapping Operations
    [[nodiscard]] auto get_or_create(std::string_view original_uid)
        -> kcenon::common::Result<std::string>;
    [[nodiscard]] auto get_anonymized(std::string_view original_uid) const
        -> std::optional<std::string>;
    [[nodiscard]] auto get_original(std::string_view anonymized_uid) const
        -> std::optional<std::string>;
    [[nodiscard]] auto add_mapping(std::string_view original_uid,
                                   std::string_view anonymized_uid)
        -> kcenon::common::VoidResult;

    // Query Operations
    [[nodiscard]] bool has_mapping(std::string_view original_uid) const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;

    // Management
    void clear();
    bool remove(std::string_view original_uid);

    // Persistence
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] auto from_json(std::string_view json)
        -> kcenon::common::VoidResult;
    std::size_t merge(const uid_mapping& other);

    // UID Generation
    void set_uid_root(std::string root);
    [[nodiscard]] std::string get_uid_root() const;
    [[nodiscard]] std::string generate_uid() const;

private:
    std::string uid_root_{"1.2.826.0.1.3680043.8.498.1"};
    std::map<std::string, std::string, std::less<>> original_to_anon_;
    std::map<std::string, std::string, std::less<>> anon_to_original_;
    mutable std::atomic<std::uint64_t> uid_counter_{0};
    mutable std::shared_mutex mutex_;
};

} // namespace pacs::security
```

**Thread Safety:** Uses `std::shared_mutex` for concurrent read access.

---

### 3.4 DES-SEC-009: Anonymizer

**Traces to:** SRS-SEC-001 (Data Protection), NFR-3 (Security)

**File:** `include/pacs/security/anonymizer.hpp`, `src/security/anonymizer.cpp`

Main de-identification engine implementing PS3.15 Annex E profiles.

```cpp
namespace pacs::security {

class anonymizer {
public:
    explicit anonymizer(anonymization_profile profile = anonymization_profile::basic);

    // Copy/Move semantics
    anonymizer(const anonymizer& other);
    anonymizer(anonymizer&& other) noexcept;
    anonymizer& operator=(const anonymizer& other);
    anonymizer& operator=(anonymizer&& other) noexcept;
    ~anonymizer() = default;

    // Anonymization Operations
    [[nodiscard]] auto anonymize(core::dicom_dataset& dataset)
        -> kcenon::common::Result<anonymization_report>;
    [[nodiscard]] auto anonymize_with_mapping(core::dicom_dataset& dataset,
                                              uid_mapping& mapping)
        -> kcenon::common::Result<anonymization_report>;

    // Profile Configuration
    [[nodiscard]] anonymization_profile get_profile() const noexcept;
    void set_profile(anonymization_profile profile);

    // Custom Tag Actions
    void add_tag_action(core::dicom_tag tag, tag_action_config config);
    void add_tag_actions(const std::map<core::dicom_tag, tag_action_config>& actions);
    bool remove_tag_action(core::dicom_tag tag);
    void clear_custom_actions();
    [[nodiscard]] tag_action_config get_tag_action(core::dicom_tag tag) const;

    // Date Shifting
    void set_date_offset(std::chrono::days offset);
    [[nodiscard]] std::optional<std::chrono::days> get_date_offset() const noexcept;
    void clear_date_offset();
    [[nodiscard]] static std::chrono::days generate_random_date_offset(
        std::chrono::days min_days = std::chrono::days{-365},
        std::chrono::days max_days = std::chrono::days{365});

    // Encryption Configuration
    [[nodiscard]] auto set_encryption_key(std::span<const std::uint8_t> key)
        -> kcenon::common::VoidResult;
    [[nodiscard]] bool has_encryption_key() const noexcept;

    // Hash Configuration
    void set_hash_salt(std::string salt);
    [[nodiscard]] std::optional<std::string> get_hash_salt() const;

    // Reporting
    void set_detailed_reporting(bool enable);
    [[nodiscard]] bool is_detailed_reporting() const noexcept;

    // Static Helpers
    [[nodiscard]] static auto get_profile_actions(anonymization_profile profile)
        -> std::map<core::dicom_tag, tag_action_config>;
    [[nodiscard]] static auto get_hipaa_identifier_tags()
        -> std::vector<core::dicom_tag>;
    [[nodiscard]] static auto get_gdpr_personal_data_tags()
        -> std::vector<core::dicom_tag>;

private:
    anonymization_profile profile_;
    std::map<core::dicom_tag, tag_action_config> custom_actions_;
    std::optional<std::chrono::days> date_offset_;
    std::vector<std::uint8_t> encryption_key_;
    std::optional<std::string> hash_salt_;
    bool detailed_reporting_{false};

    // Internal helpers
    [[nodiscard]] tag_action_record apply_action(core::dicom_dataset& dataset,
                                                  core::dicom_tag tag,
                                                  const tag_action_config& config,
                                                  uid_mapping* mapping);
    [[nodiscard]] std::string shift_date(std::string_view date_string) const;
    [[nodiscard]] std::string hash_value(std::string_view value) const;
    [[nodiscard]] auto encrypt_value(std::string_view value) const
        -> kcenon::common::Result<std::string>;
    void initialize_profile_actions();
};

} // namespace pacs::security
```

**Thread Safety:** NOT thread-safe. Create separate instances for concurrent operations.

---

## 4. Digital Signatures

### 4.1 DES-SEC-010: Certificate

**Traces to:** SRS-SEC-001 (Data Protection), NFR-3 (Security)

**File:** `include/pacs/security/certificate.hpp`, `src/security/certificate.cpp`

X.509 certificate handling for DICOM digital signatures.

```cpp
namespace pacs::security {

class certificate {
public:
    certificate();
    certificate(const certificate& other);
    certificate(certificate&& other) noexcept;
    certificate& operator=(const certificate& other);
    certificate& operator=(certificate&& other) noexcept;
    ~certificate();

    // Factory Methods
    [[nodiscard]] static auto load_from_pem(std::string_view path)
        -> kcenon::common::Result<certificate>;
    [[nodiscard]] static auto load_from_pem_string(std::string_view pem_data)
        -> kcenon::common::Result<certificate>;
    [[nodiscard]] static auto load_from_der(std::span<const std::uint8_t> der_data)
        -> kcenon::common::Result<certificate>;

    // Certificate Information
    [[nodiscard]] std::string subject_name() const;
    [[nodiscard]] std::string subject_common_name() const;
    [[nodiscard]] std::string subject_organization() const;
    [[nodiscard]] std::string issuer_name() const;
    [[nodiscard]] std::string serial_number() const;
    [[nodiscard]] std::string thumbprint() const;  // SHA-256

    // Validity
    [[nodiscard]] std::chrono::system_clock::time_point not_before() const;
    [[nodiscard]] std::chrono::system_clock::time_point not_after() const;
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] bool is_expired() const;

    // Export
    [[nodiscard]] std::string to_pem() const;
    [[nodiscard]] std::vector<std::uint8_t> to_der() const;

    // Internal
    [[nodiscard]] bool is_loaded() const noexcept;

private:
    std::unique_ptr<certificate_impl> impl_;  // PIMPL pattern
};

class private_key {
public:
    private_key();
    private_key(const private_key&) = delete;  // Non-copyable
    private_key(private_key&& other) noexcept;
    private_key& operator=(const private_key&) = delete;
    private_key& operator=(private_key&& other) noexcept;
    ~private_key();  // Securely erases key material

    // Factory Methods
    [[nodiscard]] static auto load_from_pem(std::string_view path,
                                            std::string_view password = "")
        -> kcenon::common::Result<private_key>;
    [[nodiscard]] static auto load_from_pem_string(std::string_view pem_data,
                                                   std::string_view password = "")
        -> kcenon::common::Result<private_key>;

    // Key Information
    [[nodiscard]] std::string algorithm_name() const;  // "RSA", "EC"
    [[nodiscard]] int key_size() const;  // bits
    [[nodiscard]] bool is_loaded() const noexcept;

private:
    std::unique_ptr<private_key_impl> impl_;
};

class certificate_chain {
public:
    certificate_chain() = default;

    void add(certificate cert);
    [[nodiscard]] const certificate* end_entity() const;
    [[nodiscard]] const std::vector<certificate>& certificates() const;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] static auto load_from_pem(std::string_view path)
        -> kcenon::common::Result<certificate_chain>;

private:
    std::vector<certificate> certs_;
};

} // namespace pacs::security
```

**Design Decisions:**
- PIMPL pattern hides OpenSSL implementation details
- Private keys are non-copyable for security
- Destructor securely erases key material

---

### 4.2 DES-SEC-011: Signature Types

**Traces to:** SRS-SEC-001 (Data Protection)

**File:** `include/pacs/security/signature_types.hpp`

Type definitions for digital signature operations per PS3.15.

```cpp
namespace pacs::security {

enum class signature_algorithm {
    rsa_sha256,    // Recommended
    rsa_sha384,
    rsa_sha512,    // Highest security
    ecdsa_sha256,  // Compact signatures
    ecdsa_sha384
};

enum class signature_status {
    valid,              // Signature verified successfully
    invalid,            // Verification failed (data tampered)
    expired,            // Certificate expired
    untrusted_signer,   // Certificate not trusted
    revoked,            // Certificate revoked
    no_signature        // No signature present
};

struct signature_info {
    std::string signature_uid;                        // (0400,0100)
    std::string signer_name;
    std::string signer_organization;
    std::chrono::system_clock::time_point timestamp;  // (0400,0105)
    signature_algorithm algorithm;
    std::vector<std::uint32_t> signed_tags;
    std::string certificate_thumbprint;

    bool operator==(const signature_info&) const = default;
};

enum class mac_algorithm {
    ripemd160,  // Legacy
    sha1,       // Deprecated
    md5,        // Deprecated
    sha256,     // Recommended
    sha384,
    sha512
};

enum class certificate_type {
    x509_certificate,        // Single certificate
    x509_certificate_chain   // Full chain
};

constexpr std::string_view to_string(signature_algorithm algo);
constexpr std::string_view to_string(signature_status status);
constexpr std::string_view to_dicom_uid(mac_algorithm algo);
constexpr std::string_view to_dicom_term(certificate_type type);

} // namespace pacs::security
```

---

### 4.3 DES-SEC-012: Digital Signature

**Traces to:** SRS-SEC-001 (Data Protection), NFR-3 (Security)

**File:** `include/pacs/security/digital_signature.hpp`, `src/security/digital_signature.cpp`

DICOM Digital Signature creation and verification per PS3.15 Section C.

```cpp
namespace pacs::security {

class digital_signature {
public:
    // Signature Creation
    [[nodiscard]] static auto sign(
        core::dicom_dataset& dataset,
        const certificate& cert,
        const private_key& key,
        signature_algorithm algo = signature_algorithm::rsa_sha256
    ) -> kcenon::common::VoidResult;

    [[nodiscard]] static auto sign_tags(
        core::dicom_dataset& dataset,
        const certificate& cert,
        const private_key& key,
        std::span<const core::dicom_tag> tags_to_sign,
        signature_algorithm algo = signature_algorithm::rsa_sha256
    ) -> kcenon::common::VoidResult;

    // Signature Verification
    [[nodiscard]] static auto verify(const core::dicom_dataset& dataset)
        -> kcenon::common::Result<signature_status>;

    [[nodiscard]] static auto verify_with_trust(
        const core::dicom_dataset& dataset,
        const std::vector<certificate>& trusted_certs
    ) -> kcenon::common::Result<signature_status>;

    // Signature Information
    [[nodiscard]] static auto get_signature_info(const core::dicom_dataset& dataset)
        -> std::optional<signature_info>;
    [[nodiscard]] static auto get_all_signatures(const core::dicom_dataset& dataset)
        -> std::vector<signature_info>;
    [[nodiscard]] static bool has_signature(const core::dicom_dataset& dataset);

    // Utility
    static bool remove_signatures(core::dicom_dataset& dataset);
    [[nodiscard]] static std::string generate_signature_uid();

private:
    static auto compute_mac(const core::dicom_dataset& dataset,
                           std::span<const core::dicom_tag> tags,
                           mac_algorithm algo) -> std::vector<std::uint8_t>;
    static auto sign_mac(std::span<const std::uint8_t> mac_data,
                        const private_key& key,
                        signature_algorithm algo)
        -> kcenon::common::Result<std::vector<std::uint8_t>>;
    static bool verify_mac_signature(std::span<const std::uint8_t> mac_data,
                                     std::span<const std::uint8_t> signature,
                                     const certificate& cert,
                                     signature_algorithm algo);
};

} // namespace pacs::security
```

**DICOM Signature Attributes:**

| Tag | Name | Description |
|-----|------|-------------|
| (0400,0100) | Digital Signature UID | Unique identifier |
| (0400,0105) | Digital Signature DateTime | Signing timestamp |
| (0400,0110) | Certificate Type | X509_1993_SIG |
| (0400,0115) | Certificate of Signer | X.509 certificate |
| (0400,0120) | Signature | Signature bytes |
| (0400,0305) | Certified Timestamp Type | Timestamp source |
| (0400,0310) | Certified Timestamp | Timestamp value |

---

## 5. Security Storage

### 5.1 DES-SEC-013: Security Storage Interface

**Traces to:** SRS-SEC-002 (Access Control), NFR-2 (Reliability)

**File:** `include/pacs/security/security_storage_interface.hpp`

Abstract interface for persisting RBAC data (Users, Roles).

```cpp
namespace pacs::security {

class security_storage_interface {
public:
    template <typename T> using Result = kcenon::common::Result<T>;
    using VoidResult = kcenon::common::VoidResult;

    virtual ~security_storage_interface() = default;

    // User Management
    [[nodiscard]] virtual auto create_user(const User& user) -> VoidResult = 0;
    [[nodiscard]] virtual auto get_user(std::string_view id) -> Result<User> = 0;
    [[nodiscard]] virtual auto get_user_by_username(std::string_view username)
        -> Result<User> = 0;
    [[nodiscard]] virtual auto update_user(const User& user) -> VoidResult = 0;
    [[nodiscard]] virtual auto delete_user(std::string_view id) -> VoidResult = 0;

    // Role Queries
    [[nodiscard]] virtual auto get_users_by_role(Role role)
        -> Result<std::vector<User>> = 0;

protected:
    security_storage_interface() = default;
    security_storage_interface(const security_storage_interface&) = delete;
    security_storage_interface& operator=(const security_storage_interface&) = delete;
    security_storage_interface(security_storage_interface&&) = default;
    security_storage_interface& operator=(security_storage_interface&&) = default;
};

} // namespace pacs::security
```

**Implementation:** SQLite-based implementation exists in `sqlite_security_storage` (tested in `tests/security/sqlite_security_storage_test.cpp`).

---

## 6. Class Diagrams

### 6.1 RBAC Class Diagram

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                              RBAC Access Control                               │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ┌─────────────────┐        ┌─────────────────┐        ┌──────────────────┐  │
│  │      User       │        │      Role       │        │    Permission    │  │
│  │ (DES-SEC-001)   │        │ (DES-SEC-002)   │        │  (DES-SEC-003)   │  │
│  ├─────────────────┤        ├─────────────────┤        ├──────────────────┤  │
│  │ - id: string    │        │ <<enumeration>> │        │ - resource: Type │  │
│  │ - username: str │◄───────│ Viewer          │        │ - actions: uint32│  │
│  │ - roles: vec    │ *    1 │ Technologist    │        ├──────────────────┤  │
│  │ - active: bool  │        │ Radiologist     │        │ +has_action()    │  │
│  ├─────────────────┤        │ Administrator   │        └────────┬─────────┘  │
│  │ +has_role()     │        │ System          │                 │            │
│  └────────┬────────┘        └────────┬────────┘                 │            │
│           │                          │                          │ *          │
│           │                          │            ┌─────────────┴──────┐     │
│           │                          └────────────►                    │     │
│           ▼                                       │access_control_mgr  │     │
│  ┌─────────────────┐                             │  (DES-SEC-005)     │     │
│  │   user_context  │◄─────────────────────────────│                    │     │
│  │ (DES-SEC-004)   │                             ├────────────────────┤     │
│  ├─────────────────┤                             │ -role_permissions_  │     │
│  │ - user_: User   │                             │ -storage_           │     │
│  │ - session_id_   │                             │ -ae_to_user_id_     │     │
│  │ - created_at_   │                             │ -audit_callback_    │     │
│  │ - source_ae_    │                             ├────────────────────┤     │
│  │ - source_ip_    │                             │ +check_permission() │     │
│  ├─────────────────┤                             │ +validate_access()  │     │
│  │ +system_context │                             │ +check_dicom_op()   │     │
│  │ +anonymous_ctx  │                             │ +register_ae_title()│     │
│  │ +is_valid()     │                             │ +create_user()      │     │
│  │ +has_role()     │                             └──────────┬─────────┘     │
│  │ +touch()        │                                        │               │
│  └─────────────────┘                                        │               │
│                                                             ▼               │
│                                       ┌─────────────────────────────────┐   │
│                                       │ security_storage_interface      │   │
│                                       │     (DES-SEC-013)               │   │
│                                       │  <<abstract>>                   │   │
│                                       ├─────────────────────────────────┤   │
│                                       │ +create_user()                  │   │
│                                       │ +get_user()                     │   │
│                                       │ +update_user()                  │   │
│                                       │ +delete_user()                  │   │
│                                       │ +get_users_by_role()            │   │
│                                       └─────────────────────────────────┘   │
│                                                                              │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Anonymization Class Diagram

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                              DICOM Anonymization                               │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ┌──────────────────────┐         ┌───────────────────────┐                   │
│  │ anonymization_profile│         │     tag_action        │                   │
│  │   (DES-SEC-006)      │         │   (DES-SEC-007)       │                   │
│  ├──────────────────────┤         ├───────────────────────┤                   │
│  │ <<enumeration>>      │         │ <<enumeration>>       │                   │
│  │ basic                │         │ remove / empty        │                   │
│  │ clean_pixel          │         │ remove_or_empty       │                   │
│  │ clean_descriptions   │         │ keep / replace        │                   │
│  │ retain_longitudinal  │         │ replace_uid / hash    │                   │
│  │ retain_patient_char. │         │ encrypt / shift_date  │                   │
│  │ hipaa_safe_harbor    │         └───────────┬───────────┘                   │
│  │ gdpr_compliant       │                     │                               │
│  └───────────┬──────────┘                     │                               │
│              │                                ▼                               │
│              │              ┌──────────────────────────────┐                  │
│              │              │     tag_action_config        │                  │
│              │              ├──────────────────────────────┤                  │
│              │              │ - action: tag_action         │                  │
│              │              │ - replacement_value: string  │                  │
│              │              │ - hash_algorithm: string     │                  │
│              │              │ - use_salt: bool             │                  │
│              │              ├──────────────────────────────┤                  │
│              │              │ +make_remove() / +make_empty()│                 │
│              │              │ +make_keep() / +make_replace()│                 │
│              │              │ +make_hash()                 │                  │
│              │              └──────────────────────────────┘                  │
│              │                                                                │
│              ▼                                                                │
│  ┌────────────────────────────────────────────────────────────────────┐      │
│  │                        anonymizer (DES-SEC-009)                     │      │
│  ├────────────────────────────────────────────────────────────────────┤      │
│  │ - profile_: anonymization_profile                                   │      │
│  │ - custom_actions_: map<dicom_tag, tag_action_config>               │      │
│  │ - date_offset_: optional<days>                                      │      │
│  │ - encryption_key_: vector<uint8>                                    │      │
│  │ - hash_salt_: optional<string>                                      │      │
│  ├────────────────────────────────────────────────────────────────────┤      │
│  │ +anonymize(dataset) -> Result<anonymization_report>                 │      │
│  │ +anonymize_with_mapping(dataset, mapping) -> Result<report>         │      │
│  │ +add_tag_action(tag, config) / +clear_custom_actions()             │      │
│  │ +set_date_offset(days) / +set_encryption_key(key)                  │      │
│  │ +set_hash_salt(salt) / +set_detailed_reporting(bool)               │      │
│  └────────────────────────────────────────────────────────────────────┘      │
│                                    │                                          │
│                                    │ uses                                     │
│                                    ▼                                          │
│  ┌────────────────────────────────────────────────────────────────────┐      │
│  │                      uid_mapping (DES-SEC-008)                      │      │
│  ├────────────────────────────────────────────────────────────────────┤      │
│  │ - uid_root_: string                                                 │      │
│  │ - original_to_anon_: map<string, string>                           │      │
│  │ - anon_to_original_: map<string, string>                           │      │
│  │ - uid_counter_: atomic<uint64>                                      │      │
│  │ - mutex_: shared_mutex                                              │      │
│  ├────────────────────────────────────────────────────────────────────┤      │
│  │ +get_or_create(original) -> Result<string>                          │      │
│  │ +get_anonymized(original) / +get_original(anonymized)              │      │
│  │ +add_mapping(original, anonymized) -> VoidResult                    │      │
│  │ +to_json() / +from_json(json) / +merge(other)                      │      │
│  │ +generate_uid() -> string                                           │      │
│  └────────────────────────────────────────────────────────────────────┘      │
│                                                                               │
└───────────────────────────────────────────────────────────────────────────────┘
```

### 6.3 Digital Signature Class Diagram

```
┌───────────────────────────────────────────────────────────────────────────────┐
│                              Digital Signatures                                │
├───────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ┌────────────────────────┐     ┌────────────────────────┐                    │
│  │ signature_algorithm    │     │    signature_status    │                    │
│  │   (DES-SEC-011)        │     │    (DES-SEC-011)       │                    │
│  ├────────────────────────┤     ├────────────────────────┤                    │
│  │ <<enumeration>>        │     │ <<enumeration>>        │                    │
│  │ rsa_sha256 (default)   │     │ valid / invalid        │                    │
│  │ rsa_sha384 / rsa_sha512│     │ expired / untrusted    │                    │
│  │ ecdsa_sha256 / ecdsa384│     │ revoked / no_signature │                    │
│  └────────────┬───────────┘     └────────────┬───────────┘                    │
│               │                              │                                 │
│               │              ┌───────────────┘                                 │
│               │              │                                                 │
│               ▼              ▼                                                 │
│  ┌────────────────────────────────────────────────────────────────────┐       │
│  │                   digital_signature (DES-SEC-012)                   │       │
│  ├────────────────────────────────────────────────────────────────────┤       │
│  │ <<static class>>                                                    │       │
│  ├────────────────────────────────────────────────────────────────────┤       │
│  │ +sign(dataset, cert, key, algo) -> VoidResult                       │       │
│  │ +sign_tags(dataset, cert, key, tags, algo) -> VoidResult           │       │
│  │ +verify(dataset) -> Result<signature_status>                        │       │
│  │ +verify_with_trust(dataset, trusted_certs) -> Result<status>       │       │
│  │ +get_signature_info(dataset) -> optional<signature_info>           │       │
│  │ +get_all_signatures(dataset) -> vector<signature_info>             │       │
│  │ +has_signature(dataset) -> bool                                     │       │
│  │ +remove_signatures(dataset) -> bool                                 │       │
│  │ +generate_signature_uid() -> string                                 │       │
│  └────────────────────────────────────────────────────────────────────┘       │
│                          │                    │                                │
│                          │ uses               │ uses                           │
│                          ▼                    ▼                                │
│  ┌─────────────────────────────┐  ┌─────────────────────────────┐             │
│  │   certificate (DES-SEC-010) │  │   private_key (DES-SEC-010) │             │
│  ├─────────────────────────────┤  ├─────────────────────────────┤             │
│  │ - impl_: unique_ptr<impl>   │  │ - impl_: unique_ptr<impl>   │             │
│  ├─────────────────────────────┤  ├─────────────────────────────┤             │
│  │ +load_from_pem(path)        │  │ +load_from_pem(path, pwd)   │             │
│  │ +load_from_pem_string(data) │  │ +load_from_pem_string()     │             │
│  │ +load_from_der(data)        │  │ +algorithm_name() -> string │             │
│  │ +subject_name() -> string   │  │ +key_size() -> int          │             │
│  │ +issuer_name() -> string    │  │ +is_loaded() -> bool        │             │
│  │ +serial_number() -> string  │  └─────────────────────────────┘             │
│  │ +thumbprint() -> string     │                 ▲                             │
│  │ +not_before() / +not_after()│                 │                             │
│  │ +is_valid() / +is_expired() │                 │ Non-copyable                │
│  │ +to_pem() / +to_der()       │                 │ (Security)                  │
│  └─────────────────────────────┘                 │                             │
│               │                                  │                             │
│               │ contains                         │                             │
│               ▼                                                                │
│  ┌─────────────────────────────┐                                              │
│  │   certificate_chain         │                                              │
│  │   (DES-SEC-010)             │                                              │
│  ├─────────────────────────────┤                                              │
│  │ - certs_: vector<cert>      │                                              │
│  ├─────────────────────────────┤                                              │
│  │ +add(cert)                  │                                              │
│  │ +end_entity() -> cert*      │                                              │
│  │ +certificates() -> vec&     │                                              │
│  │ +load_from_pem(path)        │                                              │
│  └─────────────────────────────┘                                              │
│                                                                                │
└───────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Sequence Diagrams

### 7.1 SEQ-SEC-001: DICOM Operation Authorization

```
┌─────────┐     ┌─────────────┐     ┌───────────────────┐     ┌─────────────┐
│ Network │     │ user_context│     │access_control_mgr │     │   Storage   │
└────┬────┘     └──────┬──────┘     └─────────┬─────────┘     └──────┬──────┘
     │                 │                      │                      │
     │ 1. Request received (AE Title)         │                      │
     │─────────────────────────────────────────►                     │
     │                 │                      │                      │
     │                 │ 2. get_context_for_ae(ae_title, session_id) │
     │                 │◄─────────────────────│                      │
     │                 │                      │                      │
     │                 │                      │ 3. get_user(user_id) │
     │                 │                      │─────────────────────►│
     │                 │                      │                      │
     │                 │                      │    4. User data      │
     │                 │                      │◄─────────────────────│
     │                 │                      │                      │
     │                 │  5. user_context     │                      │
     │                 │◄─────────────────────│                      │
     │                 │                      │                      │
     │ 6. check_dicom_operation(ctx, C-STORE) │                      │
     │─────────────────────────────────────────►                     │
     │                 │                      │                      │
     │                 │  7. map_dicom_operation(C-STORE)            │
     │                 │                      │ -> (Study, Write)    │
     │                 │                      │                      │
     │                 │  8. check_permission(user, Study, Write)    │
     │                 │                      │                      │
     │                 │  9. AccessCheckResult (allowed=true)        │
     │◄─────────────────────────────────────────                     │
     │                 │                      │                      │
     │ 10. audit_callback(ctx, op, result)    │                      │
     │                 │                      │                      │
```

### 7.2 SEQ-SEC-002: DICOM Anonymization Flow

```
┌─────────┐     ┌───────────┐     ┌─────────────┐     ┌─────────────┐
│  Client │     │ anonymizer│     │ uid_mapping │     │   dataset   │
└────┬────┘     └─────┬─────┘     └──────┬──────┘     └──────┬──────┘
     │                │                  │                   │
     │ 1. anonymize_with_mapping(dataset, mapping)          │
     │────────────────►                  │                   │
     │                │                  │                   │
     │                │ 2. get_profile_actions(profile)      │
     │                │──────────────────────────────────────►
     │                │                  │                   │
     │                │ 3. For each tag in dataset:          │
     │                │──────────────────────────────────────►
     │                │                  │                   │
     │                │ 4. get_tag_action(tag)               │
     │                │                  │                   │
     │                │ 5. If UID tag:   │                   │
     │                │  get_or_create(original_uid)         │
     │                │─────────────────►│                   │
     │                │                  │                   │
     │                │  6. new_uid      │                   │
     │                │◄─────────────────│                   │
     │                │                  │                   │
     │                │ 7. apply_action(dataset, tag, config)│
     │                │──────────────────────────────────────►
     │                │                  │                   │
     │                │ 8. If date tag & date_offset set:    │
     │                │    shift_date(date_string)           │
     │                │                  │                   │
     │                │ 9. If hash action:                   │
     │                │    hash_value(value)                 │
     │                │                  │                   │
     │                │ 10. Record action in report          │
     │                │                  │                   │
     │ 11. anonymization_report          │                   │
     │◄────────────────                  │                   │
     │                │                  │                   │
```

### 7.3 SEQ-SEC-003: Digital Signature Creation

```
┌─────────┐     ┌──────────────────┐     ┌─────────────┐     ┌─────────────┐
│  Client │     │ digital_signature│     │ certificate │     │ private_key │
└────┬────┘     └────────┬─────────┘     └──────┬──────┘     └──────┬──────┘
     │                   │                      │                   │
     │ 1. sign(dataset, cert, key, algo)        │                   │
     │───────────────────►                      │                   │
     │                   │                      │                   │
     │                   │ 2. Collect all tags to sign              │
     │                   │──────────────────────────────────────────►
     │                   │                      │                   │
     │                   │ 3. compute_mac(dataset, tags, sha256)    │
     │                   │                      │                   │
     │                   │ 4. sign_mac(mac_data, key, algo)         │
     │                   │──────────────────────────────────────────►
     │                   │                      │                   │
     │                   │ 5. signature bytes   │                   │
     │                   │◄─────────────────────────────────────────│
     │                   │                      │                   │
     │                   │ 6. Get certificate DER                   │
     │                   │─────────────────────►│                   │
     │                   │                      │                   │
     │                   │ 7. DER bytes         │                   │
     │                   │◄─────────────────────│                   │
     │                   │                      │                   │
     │                   │ 8. Create Digital Signature Sequence     │
     │                   │    (0400,0561) with:                     │
     │                   │    - Signature UID (0400,0100)           │
     │                   │    - DateTime (0400,0105)                │
     │                   │    - Certificate (0400,0115)             │
     │                   │    - MAC (0400,0120)                     │
     │                   │    - Signed tags list                    │
     │                   │                      │                   │
     │                   │ 9. Add sequence to dataset               │
     │                   │                      │                   │
     │ 10. VoidResult (success)                 │                   │
     │◄──────────────────                       │                   │
     │                   │                      │                   │
```

### 7.4 SEQ-SEC-004: Digital Signature Verification

```
┌─────────┐     ┌──────────────────┐     ┌─────────────┐     ┌─────────────┐
│  Client │     │ digital_signature│     │ certificate │     │   dataset   │
└────┬────┘     └────────┬─────────┘     └──────┬──────┘     └──────┬──────┘
     │                   │                      │                   │
     │ 1. verify(dataset)                       │                   │
     │───────────────────►                      │                   │
     │                   │                      │                   │
     │                   │ 2. Get Digital Signature Sequence        │
     │                   │─────────────────────────────────────────►│
     │                   │                      │                   │
     │                   │ 3. If no sequence:   │                   │
     │                   │    return no_signature                   │
     │                   │                      │                   │
     │                   │ 4. Extract certificate from (0400,0115)  │
     │                   │                      │                   │
     │                   │ 5. Load certificate  │                   │
     │                   │─────────────────────►│                   │
     │                   │                      │                   │
     │                   │ 6. Check is_valid()  │                   │
     │                   │─────────────────────►│                   │
     │                   │                      │                   │
     │                   │ 7. If expired: return expired            │
     │                   │                      │                   │
     │                   │ 8. Get signed tags list                  │
     │                   │                      │                   │
     │                   │ 9. compute_mac(dataset, tags, algo)      │
     │                   │                      │                   │
     │                   │ 10. verify_mac_signature(mac, sig, cert) │
     │                   │                      │                   │
     │                   │ 11. If verification fails: return invalid│
     │                   │                      │                   │
     │ 12. Result<signature_status::valid>      │                   │
     │◄──────────────────                       │                   │
     │                   │                      │                   │
```

---

## 8. Traceability

### 8.1 SRS to SDS Traceability

| SRS ID | SRS Description | SDS ID(s) | Design Element |
|--------|-----------------|-----------|----------------|
| **SRS-SEC-001** | Data Protection (Anonymization) | DES-SEC-006, DES-SEC-007, DES-SEC-008, DES-SEC-009 | anonymization_profile, tag_action, uid_mapping, anonymizer |
| **SRS-SEC-002** | Access Control (RBAC) | DES-SEC-001, DES-SEC-002, DES-SEC-003, DES-SEC-004, DES-SEC-005, DES-SEC-013 | User, Role, Permission, user_context, access_control_manager, security_storage_interface |
| **SRS-SEC-003** | Audit Trail | DES-SEC-004, DES-SEC-005 | user_context (source info), access_control_manager (audit callback) |

### 8.2 SDS to Implementation Traceability

| SDS ID | Design Element | Header File | Source File |
|--------|----------------|-------------|-------------|
| DES-SEC-001 | User | `include/pacs/security/user.hpp` | - |
| DES-SEC-002 | Role | `include/pacs/security/role.hpp` | - |
| DES-SEC-003 | Permission | `include/pacs/security/permission.hpp` | - |
| DES-SEC-004 | user_context | `include/pacs/security/user_context.hpp` | - |
| DES-SEC-005 | access_control_manager | `include/pacs/security/access_control_manager.hpp` | `src/security/access_control_manager.cpp` |
| DES-SEC-006 | anonymization_profile | `include/pacs/security/anonymization_profile.hpp` | - |
| DES-SEC-007 | tag_action, tag_action_config | `include/pacs/security/tag_action.hpp` | `src/security/tag_action.cpp` |
| DES-SEC-008 | uid_mapping | `include/pacs/security/uid_mapping.hpp` | `src/security/uid_mapping.cpp` |
| DES-SEC-009 | anonymizer | `include/pacs/security/anonymizer.hpp` | `src/security/anonymizer.cpp` |
| DES-SEC-010 | certificate, private_key, certificate_chain | `include/pacs/security/certificate.hpp` | `src/security/certificate.cpp` |
| DES-SEC-011 | signature_algorithm, signature_status, signature_info | `include/pacs/security/signature_types.hpp` | - |
| DES-SEC-012 | digital_signature | `include/pacs/security/digital_signature.hpp` | `src/security/digital_signature.cpp` |
| DES-SEC-013 | security_storage_interface | `include/pacs/security/security_storage_interface.hpp` | - |

### 8.3 SDS to Test Traceability

| SDS ID | Design Element | Test File |
|--------|----------------|-----------|
| DES-SEC-005 | access_control_manager | `tests/security/access_control_manager_test.cpp` |
| DES-SEC-008 | uid_mapping | `tests/security/uid_mapping_test.cpp` |
| DES-SEC-009 | anonymizer | `tests/security/anonymizer_test.cpp` |
| DES-SEC-012 | digital_signature | `tests/security/digital_signature_test.cpp` |
| DES-SEC-013 | security_storage_interface (SQLite impl) | `tests/security/sqlite_security_storage_test.cpp` |

---

## Appendix A: HIPAA Safe Harbor Identifiers

The following 18 categories of identifiers must be removed for HIPAA Safe Harbor compliance (45 CFR 164.514(b)(2)):

1. Names
2. Geographic data smaller than state
3. Dates (except year) related to individual
4. Phone numbers
5. Fax numbers
6. Email addresses
7. Social Security numbers
8. Medical record numbers
9. Health plan beneficiary numbers
10. Account numbers
11. Certificate/license numbers
12. Vehicle identifiers and serial numbers
13. Device identifiers and serial numbers
14. Web URLs
15. IP addresses
16. Biometric identifiers
17. Full-face photographs
18. Any other unique identifying number

## Appendix B: DICOM Tags for Anonymization

### Direct Identifiers (Always Remove)

| Tag | Name | Action |
|-----|------|--------|
| (0010,0010) | Patient's Name | Remove/Replace |
| (0010,0020) | Patient ID | Remove/Replace |
| (0010,0030) | Patient's Birth Date | Remove/Shift |
| (0010,0040) | Patient's Sex | Keep (research) |
| (0008,0050) | Accession Number | Remove/Replace |
| (0008,0080) | Institution Name | Remove |
| (0008,0081) | Institution Address | Remove |
| (0008,0090) | Referring Physician's Name | Remove |
| (0008,1050) | Performing Physician's Name | Remove |
| (0010,1000) | Other Patient IDs | Remove |
| (0010,1001) | Other Patient Names | Remove |

### UIDs (Replace Consistently)

| Tag | Name |
|-----|------|
| (0008,0018) | SOP Instance UID |
| (0020,000D) | Study Instance UID |
| (0020,000E) | Series Instance UID |
| (0008,0016) | SOP Class UID |
| (0002,0003) | Media Storage SOP Instance UID |

---

*Document generated for Issue #469 - Security Module SDS*
