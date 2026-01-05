// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-network-dimse.cppm
 * @brief C++20 module partition for DIMSE message types.
 *
 * This module partition exports comprehensive DIMSE types:
 * - dimse_message: Base message class with factory functions
 * - command_field: DIMSE command field enumeration
 * - status_codes: All DIMSE status codes and categories
 * - N-* service types: N-ACTION, N-CREATE, N-DELETE, N-EVENT-REPORT, N-GET, N-SET
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// PACS DIMSE headers
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/network/dimse/n_action.hpp>
#include <pacs/network/dimse/n_create.hpp>
#include <pacs/network/dimse/n_delete.hpp>
#include <pacs/network/dimse/n_event_report.hpp>
#include <pacs/network/dimse/n_get.hpp>
#include <pacs/network/dimse/n_set.hpp>

export module kcenon.pacs:network_dimse;

// ============================================================================
// Re-export pacs::network::dimse namespace (full)
// ============================================================================

export namespace pacs::network::dimse {

// ==========================================================================
// Core Message Types
// ==========================================================================

// DIMSE message class
using pacs::network::dimse::dimse_message;

// Command field enumeration
using pacs::network::dimse::command_field;

// ==========================================================================
// Status Codes
// ==========================================================================

// Status code type
using pacs::network::dimse::status_code;

// General status codes
using pacs::network::dimse::status_success;
using pacs::network::dimse::status_pending;
using pacs::network::dimse::status_pending_warning;
using pacs::network::dimse::status_cancel;

// Failure status codes
using pacs::network::dimse::status_refused_out_of_resources;
using pacs::network::dimse::status_refused_out_of_resources_matches;
using pacs::network::dimse::status_refused_out_of_resources_subops;
using pacs::network::dimse::status_refused_move_destination_unknown;
using pacs::network::dimse::status_refused_sop_class_not_supported;
using pacs::network::dimse::status_error_dataset_mismatch;
using pacs::network::dimse::status_error_cannot_understand;
using pacs::network::dimse::status_error_unable_to_process;
using pacs::network::dimse::status_error_duplicate_sop_instance;
using pacs::network::dimse::status_error_missing_attribute;
using pacs::network::dimse::status_error_missing_attribute_value;

// Warning status codes
using pacs::network::dimse::status_warning_coercion;
using pacs::network::dimse::status_warning_elements_discarded;
using pacs::network::dimse::status_warning_dataset_mismatch;

// Status category checks
using pacs::network::dimse::is_success;
using pacs::network::dimse::is_pending;
using pacs::network::dimse::is_warning;
using pacs::network::dimse::is_failure;
using pacs::network::dimse::status_to_string;

// ==========================================================================
// C-* Message Factory Functions
// ==========================================================================

// C-ECHO
using pacs::network::dimse::make_c_echo_rq;
using pacs::network::dimse::make_c_echo_rsp;

// C-STORE
using pacs::network::dimse::make_c_store_rq;
using pacs::network::dimse::make_c_store_rsp;

// C-FIND
using pacs::network::dimse::make_c_find_rq;
using pacs::network::dimse::make_c_find_rsp;

// C-MOVE
using pacs::network::dimse::make_c_move_rq;
using pacs::network::dimse::make_c_move_rsp;

// C-GET
using pacs::network::dimse::make_c_get_rq;
using pacs::network::dimse::make_c_get_rsp;

// C-CANCEL
using pacs::network::dimse::make_c_cancel_rq;

// ==========================================================================
// N-* Message Factory Functions
// ==========================================================================

// N-ACTION
using pacs::network::dimse::make_n_action_rq;
using pacs::network::dimse::make_n_action_rsp;

// N-CREATE
using pacs::network::dimse::make_n_create_rq;
using pacs::network::dimse::make_n_create_rsp;

// N-DELETE
using pacs::network::dimse::make_n_delete_rq;
using pacs::network::dimse::make_n_delete_rsp;

// N-EVENT-REPORT
using pacs::network::dimse::make_n_event_report_rq;
using pacs::network::dimse::make_n_event_report_rsp;

// N-GET
using pacs::network::dimse::make_n_get_rq;
using pacs::network::dimse::make_n_get_rsp;

// N-SET
using pacs::network::dimse::make_n_set_rq;
using pacs::network::dimse::make_n_set_rsp;

} // namespace pacs::network::dimse
