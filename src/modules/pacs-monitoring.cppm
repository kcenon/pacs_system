// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-monitoring.cppm
 * @brief C++20 module partition for system monitoring.
 *
 * This module partition exports monitoring-related types:
 * - pacs_monitor: Unified monitoring system
 * - health_checker: Health status verification
 * - health_status: Health status data
 * - pacs_metrics: DICOM-specific metrics
 * - Metric collectors for associations, services, storage
 *
 * The exports below exist only when pacs_monitoring is built
 * (PACS_MODULES_WITH_MONITORING is defined by cmake/targets.cmake);
 * otherwise this partition is empty.
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef PACS_MODULES_WITH_MONITORING
// PACS monitoring headers
#include <kcenon/pacs/monitoring/pacs_monitor.h>
#include <kcenon/pacs/monitoring/health_checker.h>
#include <kcenon/pacs/monitoring/health_status.h>
#include <kcenon/pacs/monitoring/health_json.h>
#include <kcenon/pacs/monitoring/pacs_metrics.h>

// PACS metric collectors (CRTP-based)
#include <kcenon/pacs/monitoring/collectors/dicom_collector_base.h>
#include <kcenon/pacs/monitoring/collectors/dicom_metrics_collector.h>
#include <kcenon/pacs/monitoring/collectors/dicom_association_collector.h>
#include <kcenon/pacs/monitoring/collectors/dicom_service_collector.h>
#include <kcenon/pacs/monitoring/collectors/dicom_storage_collector.h>
#endif // PACS_MODULES_WITH_MONITORING

export module kcenon.pacs:monitoring;

#ifdef PACS_MODULES_WITH_MONITORING

// ============================================================================
// Re-export kcenon::pacs::monitoring namespace
// ============================================================================

export namespace kcenon::pacs::monitoring {

// Monitor
using kcenon::pacs::monitoring::pacs_monitor;
using kcenon::pacs::monitoring::pacs_monitor_config;
using kcenon::pacs::monitoring::metric_type;
using kcenon::pacs::monitoring::metric_value;
using kcenon::pacs::monitoring::metrics_snapshot;
using kcenon::pacs::monitoring::monitor_health_status;
using kcenon::pacs::monitoring::health_check_result;

// Health
using kcenon::pacs::monitoring::health_checker;
using kcenon::pacs::monitoring::health_checker_config;
using kcenon::pacs::monitoring::health_level;
using kcenon::pacs::monitoring::database_status;
using kcenon::pacs::monitoring::storage_status;
using kcenon::pacs::monitoring::association_metrics;
using kcenon::pacs::monitoring::storage_metrics;
using kcenon::pacs::monitoring::version_info;
using kcenon::pacs::monitoring::health_status;
using kcenon::pacs::monitoring::to_json;
using kcenon::pacs::monitoring::to_json_pretty;

// Metrics core
using kcenon::pacs::monitoring::pacs_metrics;
using kcenon::pacs::monitoring::association_counters;
using kcenon::pacs::monitoring::data_transfer_metrics;
using kcenon::pacs::monitoring::pool_counters;
using kcenon::pacs::monitoring::operation_counter;
using kcenon::pacs::monitoring::dimse_operation;

// to_string overloads (metric, health, and DIMSE operation enums)
using kcenon::pacs::monitoring::to_string;

// CRTP collector base
using kcenon::pacs::monitoring::dicom_collector_base;
using kcenon::pacs::monitoring::dicom_metric;
using kcenon::pacs::monitoring::config_map;
using kcenon::pacs::monitoring::stats_map;

// Unified DICOM metrics collector
using kcenon::pacs::monitoring::dicom_metrics_collector;
using kcenon::pacs::monitoring::dicom_metrics_snapshot;

// Specialized collectors
using kcenon::pacs::monitoring::dicom_association_collector;
using kcenon::pacs::monitoring::association_metric;

using kcenon::pacs::monitoring::dicom_service_collector;
using kcenon::pacs::monitoring::service_metric;

using kcenon::pacs::monitoring::dicom_storage_collector;
using kcenon::pacs::monitoring::storage_metric;

} // namespace kcenon::pacs::monitoring

#endif // PACS_MODULES_WITH_MONITORING
