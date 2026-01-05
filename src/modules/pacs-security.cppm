// BSD 3-Clause License
// Copyright (c) 2025, kcenon
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
#include <pacs/security/access_control_manager.hpp>
#include <pacs/security/user.hpp>
#include <pacs/security/role.hpp>
#include <pacs/security/permission.hpp>
#include <pacs/security/user_context.hpp>
#include <pacs/security/certificate.hpp>
#include <pacs/security/digital_signature.hpp>
#include <pacs/security/signature_types.hpp>
#include <pacs/security/anonymizer.hpp>
#include <pacs/security/uid_mapping.hpp>
#include <pacs/security/security_storage_interface.hpp>

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
