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

// PACS workflow headers
#include <kcenon/pacs/workflow/task_scheduler.h>
#include <kcenon/pacs/workflow/auto_prefetch_service.h>
#include <kcenon/pacs/workflow/study_lock_manager.h>

export module kcenon.pacs:workflow;

// ============================================================================
// Re-export pacs::workflow namespace
// ============================================================================

export namespace pacs::workflow {

// Task scheduling
using pacs::workflow::task_scheduler;
using pacs::workflow::task_scheduler_config;

// Prefetching
using pacs::workflow::auto_prefetch_service;
using pacs::workflow::prefetch_config;

// Locking
using pacs::workflow::study_lock_manager;

} // namespace pacs::workflow
