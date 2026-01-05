// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-services.cppm
 * @brief C++20 module partition for DICOM SCP services.
 *
 * This module partition exports service-related types:
 * - scp_service: Abstract SCP base
 * - storage_scp: C-STORE handler
 * - query_scp: C-FIND handler
 * - retrieval_scp: C-MOVE/C-GET handler
 * - verification_scp: C-ECHO handler
 * - mpps_scp: MPPS handler
 * - worklist_scp: Modality Worklist handler
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
#include <vector>

// PACS services headers
#include <pacs/services/scp_service.hpp>
#include <pacs/services/storage_scp.hpp>
#include <pacs/services/query_scp.hpp>
#include <pacs/services/retrieve_scp.hpp>
#include <pacs/services/verification_scp.hpp>
#include <pacs/services/mpps_scp.hpp>
#include <pacs/services/worklist_scp.hpp>
#include <pacs/services/storage_scu.hpp>
#include <pacs/services/sop_class_registry.hpp>
#include <pacs/services/storage_status.hpp>

export module kcenon.pacs:services;

// ============================================================================
// Re-export pacs::services namespace
// ============================================================================

export namespace pacs::services {

// SCP base
using pacs::services::scp_service;

// SCP implementations
using pacs::services::storage_scp;
using pacs::services::storage_scp_config;
using pacs::services::duplicate_policy;
using pacs::services::query_scp;
using pacs::services::retrieval_scp;
using pacs::services::verification_scp;
using pacs::services::mpps_scp;
using pacs::services::worklist_scp;

// SCU implementations
using pacs::services::storage_scu;

// Registry
using pacs::services::sop_class_registry;

// Status
using pacs::services::storage_status;

} // namespace pacs::services
