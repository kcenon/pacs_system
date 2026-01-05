// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-integration.cppm
 * @brief C++20 module partition for external system adapters.
 *
 * This module partition exports integration-related types:
 * - logger_adapter: Logging framework integration
 * - thread_adapter: Thread utilities
 * - thread_pool_adapter: Thread pool wrapper
 * - executor_adapter: Executor pattern
 * - network_adapter: Network integration
 * - container_adapter: DI container
 * - dicom_session: DICOM-aware session wrapper
 * - itk_adapter: ITK integration (optional)
 * - monitoring_adapter: Monitoring system bridge
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

// PACS integration headers
#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/thread_adapter.hpp>
#include <pacs/integration/thread_pool_adapter.hpp>
#include <pacs/integration/executor_adapter.hpp>
#include <pacs/integration/network_adapter.hpp>
#include <pacs/integration/container_adapter.hpp>
#include <pacs/integration/dicom_session.hpp>
#include <pacs/integration/monitoring_adapter.hpp>

export module kcenon.pacs:integration;

// ============================================================================
// Re-export pacs::integration namespace
// ============================================================================

export namespace pacs::integration {

// Adapters
using pacs::integration::logger_adapter;
using pacs::integration::thread_adapter;
using pacs::integration::thread_pool_adapter;
using pacs::integration::thread_pool_interface;
using pacs::integration::executor_adapter;
using pacs::integration::network_adapter;
using pacs::integration::container_adapter;
using pacs::integration::monitoring_adapter;

// DICOM session
using pacs::integration::dicom_session;

} // namespace pacs::integration
