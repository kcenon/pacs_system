// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-security.cppm
 * @brief C++20 module partition for DICOM security types.
 *
 * This module partition exports security-related types:
 * - access_control_manager: RBAC implementation
 * - user, role, permission: Identity types
 * - certificate: X.509 certificate management
 * - digital_signature: DICOM digital signatures
 * - anonymizer: DICOM anonymization
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
#include <kcenon/pacs/security/anonymizer.h>
#include <kcenon/pacs/security/uid_mapping.h>
#include <kcenon/pacs/security/security_storage_interface.h>

export module kcenon.pacs:security;

// ============================================================================
// Re-export pacs::security namespace
// ============================================================================

export namespace pacs::security {

// Access control
using pacs::security::access_control_manager;
using pacs::security::DicomOperation;
using pacs::security::AccessCheckResult;

// Identity types
using pacs::security::user;
using pacs::security::role;
using pacs::security::permission;
using pacs::security::user_context;

// Certificates and signatures
using pacs::security::certificate;
using pacs::security::digital_signature;

// Anonymization
using pacs::security::anonymizer;
using pacs::security::tag_action;
using pacs::security::anonymization_profile;
using pacs::security::uid_mapping;

// Storage interface
using pacs::security::security_storage_interface;

} // namespace pacs::security
