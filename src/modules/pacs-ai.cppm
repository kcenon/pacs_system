// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-ai.cppm
 * @brief C++20 module partition for AI/ML integration.
 *
 * This module partition exports AI-related types:
 * - ai_service_connector: External AI service connection
 * - ai_result_handler: AI inference result processing
 * - assessment_creator, assessment_manager: AI Result Assessment (AIRA)
 *
 * The exports below exist only when pacs_ai is built
 * (PACS_MODULES_WITH_AI is defined by cmake/targets.cmake);
 * otherwise this partition is empty.
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

#ifdef PACS_MODULES_WITH_AI
// PACS AI headers
#include <kcenon/pacs/ai/ai_service_connector.h>
#include <kcenon/pacs/ai/ai_result_handler.h>
#include <kcenon/pacs/ai/aira_assessment.h>
#include <kcenon/pacs/ai/aira_assessment_manager.h>
#endif // PACS_MODULES_WITH_AI

export module kcenon.pacs:ai;

#ifdef PACS_MODULES_WITH_AI

// ============================================================================
// Re-export kcenon::pacs::ai namespace
// ============================================================================

export namespace kcenon::pacs::ai {

// Configuration structures
using kcenon::pacs::ai::ai_service_config;
using kcenon::pacs::ai::inference_request;
using kcenon::pacs::ai::inference_status;
using kcenon::pacs::ai::model_info;

// Enumerations
using kcenon::pacs::ai::inference_status_code;
using kcenon::pacs::ai::authentication_type;

// AI service connector
using kcenon::pacs::ai::ai_service_connector;

// Result handling
using kcenon::pacs::ai::ai_result_handler;
using kcenon::pacs::ai::ai_handler_config;
using kcenon::pacs::ai::ai_result_type;
using kcenon::pacs::ai::validation_status;
using kcenon::pacs::ai::ai_result_info;
using kcenon::pacs::ai::source_reference;
using kcenon::pacs::ai::cad_finding;
using kcenon::pacs::ai::segment_info;
using kcenon::pacs::ai::validation_result;
using kcenon::pacs::ai::ai_result_received_callback;
using kcenon::pacs::ai::pre_store_validator;

// AI Result Assessment (AIRA)
using kcenon::pacs::ai::assessment_type;
using kcenon::pacs::ai::assessment_status;
using kcenon::pacs::ai::assessed_result_reference;
using kcenon::pacs::ai::assessment_modification;
using kcenon::pacs::ai::assessment_rejection;
using kcenon::pacs::ai::ai_assessment;
using kcenon::pacs::ai::assessment_creation_result;
using kcenon::pacs::ai::assessment_info;
using kcenon::pacs::ai::assessment_creator;
using kcenon::pacs::ai::assessment_manager;

// Helper functions
using kcenon::pacs::ai::to_string;

} // namespace kcenon::pacs::ai

#endif // PACS_MODULES_WITH_AI
