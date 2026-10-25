// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-integration.cppm
 * @brief C++20 module partition for external system adapters.
 *
 * This module partition exports integration-related types:
 * - logger_adapter: Logging framework integration
 * - thread_pool_interface, thread_pool_adapter: Thread pool wrapper
 * - lambda_job, thread_pool_executor_adapter: Executor pattern
 * - network_adapter: Network integration
 * - container_adapter: DI container
 * - dicom_session: DICOM-aware session wrapper
 * - monitoring_adapter: Monitoring system bridge
 *
 * The exports below exist only when pacs_integration is built
 * (PACS_MODULES_WITH_INTEGRATION is defined by cmake/targets.cmake);
 * otherwise this partition is empty.
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef PACS_MODULES_WITH_INTEGRATION
// PACS integration headers
#include <kcenon/pacs/integration/logger_adapter.h>
#include <kcenon/pacs/integration/thread_pool_interface.h>
#include <kcenon/pacs/integration/thread_pool_adapter.h>
#include <kcenon/pacs/integration/executor_adapter.h>
#include <kcenon/pacs/integration/network_adapter.h>
#include <kcenon/pacs/integration/container_adapter.h>
#include <kcenon/pacs/integration/dicom_session.h>
#include <kcenon/pacs/integration/monitoring_adapter.h>
#endif // PACS_MODULES_WITH_INTEGRATION

export module kcenon.pacs:integration;

#ifdef PACS_MODULES_WITH_INTEGRATION

// ============================================================================
// Re-export kcenon::pacs::integration namespace
// ============================================================================

export namespace kcenon::pacs::integration {

// Logger adapter
using kcenon::pacs::integration::logger_adapter;
using kcenon::pacs::integration::logger_config;
using kcenon::pacs::integration::log_level;
using kcenon::pacs::integration::security_event_type;

// Thread pool adapters
using kcenon::pacs::integration::thread_pool_interface;
using kcenon::pacs::integration::thread_pool_config;
using kcenon::pacs::integration::job_priority;
using kcenon::pacs::integration::thread_pool_adapter;
using kcenon::pacs::integration::lambda_job;
using kcenon::pacs::integration::thread_pool_executor_adapter;

// Network adapter
using kcenon::pacs::integration::network_adapter;
using kcenon::pacs::integration::connection_config;
using kcenon::pacs::integration::tls_config;

// Container adapter
using kcenon::pacs::integration::container_adapter;

// Monitoring adapter
using kcenon::pacs::integration::monitoring_adapter;
using kcenon::pacs::integration::monitoring_config;

// DICOM session
using kcenon::pacs::integration::dicom_session;
using kcenon::pacs::integration::pdu_data;

} // namespace kcenon::pacs::integration

#endif // PACS_MODULES_WITH_INTEGRATION
