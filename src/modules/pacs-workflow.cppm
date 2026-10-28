// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-workflow.cppm
 * @brief C++20 module partition for DICOM workflow types.
 *
 * This module partition exports workflow-related types:
 * - task_scheduler: Background task scheduling
 * - auto_prefetch_service: Intelligent prefetching
 * - study_lock_manager: Study-level locking
 *
 * The exports below exist only when pacs_workflow is built
 * (PACS_MODULES_WITH_WORKFLOW is defined by cmake/targets.cmake);
 * otherwise this partition is empty.
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_MODULES_WITH_WORKFLOW
// PACS workflow headers
#include <kcenon/pacs/workflow/task_scheduler_config.h>
#include <kcenon/pacs/workflow/task_scheduler.h>
#include <kcenon/pacs/workflow/prefetch_config.h>
#include <kcenon/pacs/workflow/auto_prefetch_service.h>
#include <kcenon/pacs/workflow/study_lock_manager.h>
#endif // PACS_MODULES_WITH_WORKFLOW

export module kcenon.pacs:workflow;

#ifdef PACS_MODULES_WITH_WORKFLOW

// ============================================================================
// Re-export kcenon::pacs::workflow namespace
// ============================================================================

export namespace kcenon::pacs::workflow {

// Task scheduling
using kcenon::pacs::workflow::task_scheduler;
using kcenon::pacs::workflow::task_scheduler_config;
using kcenon::pacs::workflow::interval_schedule;
using kcenon::pacs::workflow::cron_schedule;
using kcenon::pacs::workflow::one_time_schedule;
using kcenon::pacs::workflow::schedule;
using kcenon::pacs::workflow::task_type;
using kcenon::pacs::workflow::task_state;
using kcenon::pacs::workflow::task_execution_record;
using kcenon::pacs::workflow::task_id;
using kcenon::pacs::workflow::task_callback;
using kcenon::pacs::workflow::task_callback_with_result;
using kcenon::pacs::workflow::scheduled_task;
using kcenon::pacs::workflow::cleanup_config;
using kcenon::pacs::workflow::archive_destination_type;
using kcenon::pacs::workflow::archive_config;
using kcenon::pacs::workflow::verification_config;
using kcenon::pacs::workflow::scheduler_stats;

// Prefetching
using kcenon::pacs::workflow::auto_prefetch_service;
using kcenon::pacs::workflow::prefetch_request;
using kcenon::pacs::workflow::prefetch_service_config;
using kcenon::pacs::workflow::remote_pacs_config;
using kcenon::pacs::workflow::prefetch_criteria;
using kcenon::pacs::workflow::prefetch_result;
using kcenon::pacs::workflow::prior_study_info;

// Locking
using kcenon::pacs::workflow::study_lock_manager;
using kcenon::pacs::workflow::study_lock_manager_config;
using kcenon::pacs::workflow::lock_type;
using kcenon::pacs::workflow::parse_lock_type;
using kcenon::pacs::workflow::lock_token;
using kcenon::pacs::workflow::lock_info;
using kcenon::pacs::workflow::lock_manager_stats;

// to_string overloads (task and lock enums)
using kcenon::pacs::workflow::to_string;

} // namespace kcenon::pacs::workflow

#endif // PACS_MODULES_WITH_WORKFLOW
