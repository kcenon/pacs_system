// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
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
#include <kcenon/pacs/services/scp_service.h>
#include <kcenon/pacs/services/storage_scp.h>
#include <kcenon/pacs/services/query_scp.h>
#include <kcenon/pacs/services/retrieve_scp.h>
#include <kcenon/pacs/services/verification_scp.h>
#include <kcenon/pacs/services/mpps_scp.h>
#include <kcenon/pacs/services/worklist_scp.h>
#include <kcenon/pacs/services/storage_scu.h>
#include <kcenon/pacs/services/query_scu.h>
#include <kcenon/pacs/services/sop_class_registry.h>
#include <kcenon/pacs/services/storage_status.h>

export module kcenon.pacs:services;

// ============================================================================
// Re-export kcenon::pacs::services namespace
// ============================================================================

export namespace kcenon::pacs::services {

// SCP base
using kcenon::pacs::services::scp_service;
using kcenon::pacs::services::scp_service_ptr;

// Storage SCP
using kcenon::pacs::services::storage_scp;
using kcenon::pacs::services::storage_scp_config;
using kcenon::pacs::services::duplicate_policy;
using kcenon::pacs::services::storage_handler;
using kcenon::pacs::services::pre_store_handler;
using kcenon::pacs::services::post_store_handler;
using kcenon::pacs::services::get_standard_storage_sop_classes;
using kcenon::pacs::services::ct_image_storage_uid;
using kcenon::pacs::services::enhanced_ct_image_storage_uid;
using kcenon::pacs::services::mr_image_storage_uid;
using kcenon::pacs::services::enhanced_mr_image_storage_uid;
using kcenon::pacs::services::cr_image_storage_uid;
using kcenon::pacs::services::dx_image_storage_presentation_uid;
using kcenon::pacs::services::dx_image_storage_processing_uid;
using kcenon::pacs::services::us_image_storage_uid;
using kcenon::pacs::services::secondary_capture_image_storage_uid;
using kcenon::pacs::services::rt_image_storage_uid;
using kcenon::pacs::services::rt_dose_storage_uid;
using kcenon::pacs::services::rt_structure_set_storage_uid;
using kcenon::pacs::services::rt_plan_storage_uid;

// Query SCP
using kcenon::pacs::services::query_scp;
using kcenon::pacs::services::query_level;
using kcenon::pacs::services::parse_query_level;
using kcenon::pacs::services::query_handler;
using kcenon::pacs::services::cancel_check;
using kcenon::pacs::services::patient_root_find_sop_class_uid;
using kcenon::pacs::services::study_root_find_sop_class_uid;
using kcenon::pacs::services::patient_study_only_find_sop_class_uid;
using kcenon::pacs::services::modality_worklist_find_sop_class_uid;

// Retrieve SCP (C-MOVE / C-GET)
using kcenon::pacs::services::retrieve_scp;
using kcenon::pacs::services::sub_operation_stats;
using kcenon::pacs::services::retrieve_handler;
using kcenon::pacs::services::destination_resolver;
using kcenon::pacs::services::store_sub_operation;
using kcenon::pacs::services::retrieve_cancel_check;
using kcenon::pacs::services::patient_root_move_sop_class_uid;
using kcenon::pacs::services::study_root_move_sop_class_uid;
using kcenon::pacs::services::patient_root_get_sop_class_uid;
using kcenon::pacs::services::study_root_get_sop_class_uid;

// Verification SCP
using kcenon::pacs::services::verification_scp;
using kcenon::pacs::services::verification_sop_class_uid;

// MPPS SCP
using kcenon::pacs::services::mpps_scp;
using kcenon::pacs::services::mpps_status;
using kcenon::pacs::services::parse_mpps_status;
using kcenon::pacs::services::mpps_instance;
using kcenon::pacs::services::mpps_create_handler;
using kcenon::pacs::services::mpps_set_handler;
using kcenon::pacs::services::mpps_sop_class_uid;

// Worklist SCP
using kcenon::pacs::services::worklist_scp;
using kcenon::pacs::services::worklist_handler;
using kcenon::pacs::services::worklist_cancel_check;
using kcenon::pacs::services::worklist_find_sop_class_uid;

// Storage SCU
using kcenon::pacs::services::storage_scu;
using kcenon::pacs::services::storage_scu_config;
using kcenon::pacs::services::store_result;
using kcenon::pacs::services::store_progress_callback;

// Query SCU
using kcenon::pacs::services::query_scu;
using kcenon::pacs::services::query_scu_config;
using kcenon::pacs::services::query_model;
using kcenon::pacs::services::get_find_sop_class_uid;
using kcenon::pacs::services::query_result;
using kcenon::pacs::services::query_streaming_callback;
using kcenon::pacs::services::patient_query_keys;
using kcenon::pacs::services::study_query_keys;
using kcenon::pacs::services::series_query_keys;
using kcenon::pacs::services::instance_query_keys;

// Registry
using kcenon::pacs::services::sop_class_registry;
using kcenon::pacs::services::sop_class_category;
using kcenon::pacs::services::modality_type;
using kcenon::pacs::services::sop_class_info;
using kcenon::pacs::services::is_storage_sop_class;
using kcenon::pacs::services::get_storage_modality;
using kcenon::pacs::services::get_sop_class_name;

// Status
using kcenon::pacs::services::storage_status;
using kcenon::pacs::services::is_success;
using kcenon::pacs::services::is_warning;
using kcenon::pacs::services::is_failure;
using kcenon::pacs::services::to_status_code;

// to_string overloads (query, MPPS, and storage status enums)
using kcenon::pacs::services::to_string;

} // namespace kcenon::pacs::services
