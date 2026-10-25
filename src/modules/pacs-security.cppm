// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-security.cppm
 * @brief C++20 module partition for DICOM security types.
 *
 * This module partition exports security-related types:
 * - access_control_manager: RBAC implementation
 * - User, Role, Permission: Identity types
 * - certificate: X.509 certificate management
 * - digital_signature: DICOM digital signatures
 * - anonymizer, tag_action, anonymization_profile: DICOM anonymization
 * - uid_mapping: UID mapping for anonymization
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// PACS security headers
#include <kcenon/pacs/security/access_control_manager.h>
#include <kcenon/pacs/security/user.h>
#include <kcenon/pacs/security/role.h>
#include <kcenon/pacs/security/permission.h>
#include <kcenon/pacs/security/user_context.h>
#include <kcenon/pacs/security/certificate.h>
#include <kcenon/pacs/security/digital_signature.h>
#include <kcenon/pacs/security/signature_types.h>
#include <kcenon/pacs/security/anonymization_profile.h>
#include <kcenon/pacs/security/tag_action.h>
#include <kcenon/pacs/security/anonymizer.h>
#include <kcenon/pacs/security/uid_mapping.h>
#include <kcenon/pacs/security/security_storage_interface.h>

export module kcenon.pacs:security;

// ============================================================================
// Re-export kcenon::pacs::security namespace
// ============================================================================
// The kcenon::pacs::security::Action bit constants are namespace-scope
// constexpr variables with internal linkage, which a module cannot export.
// Include <kcenon/pacs/security/permission.h> to use them.

export namespace kcenon::pacs::security {

// Access control
using kcenon::pacs::security::access_control_manager;
using kcenon::pacs::security::DicomOperation;
using kcenon::pacs::security::AccessCheckResult;
using kcenon::pacs::security::AccessAuditCallback;

// Identity types
using kcenon::pacs::security::User;
using kcenon::pacs::security::Role;
using kcenon::pacs::security::parse_role;
using kcenon::pacs::security::ResourceType;
using kcenon::pacs::security::Permission;
using kcenon::pacs::security::user_context;

// Certificates and signatures
using kcenon::pacs::security::certificate;
using kcenon::pacs::security::private_key;
using kcenon::pacs::security::certificate_chain;
using kcenon::pacs::security::digital_signature;
using kcenon::pacs::security::signature_algorithm;
using kcenon::pacs::security::parse_signature_algorithm;
using kcenon::pacs::security::signature_status;
using kcenon::pacs::security::signature_info;
using kcenon::pacs::security::mac_algorithm;
using kcenon::pacs::security::to_dicom_uid;
using kcenon::pacs::security::certificate_type;
using kcenon::pacs::security::to_dicom_term;

// Anonymization
using kcenon::pacs::security::anonymizer;
using kcenon::pacs::security::private_tag_action;
using kcenon::pacs::security::tag_action;
using kcenon::pacs::security::tag_action_config;
using kcenon::pacs::security::tag_action_record;
using kcenon::pacs::security::anonymization_report;
using kcenon::pacs::security::anonymization_profile;
using kcenon::pacs::security::profile_from_string;
using kcenon::pacs::security::uid_mapping;

// to_string overloads (Role, signature and anonymization enums)
using kcenon::pacs::security::to_string;

// Storage interface
using kcenon::pacs::security::security_storage_interface;

} // namespace kcenon::pacs::security
