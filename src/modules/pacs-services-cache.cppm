// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-services-cache.cppm
 * @brief C++20 module partition for query caching.
 *
 * This module partition exports query caching components:
 * - simple_lru_cache: Generic LRU cache implementation
 * - query_cache: DICOM query result cache with monitoring
 * - database_cursor: Database cursor abstraction
 * - query_result_stream, streaming_query_handler: Streaming query results
 * - parallel_query_executor: Multi-threaded query execution
 *
 * The last three groups are declared by their headers only when
 * PACS_WITH_DATABASE_SYSTEM is defined (pacs_services sets it when
 * database_system is available), and are exported under the same condition.
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// PACS cache headers
#include <kcenon/pacs/services/cache/simple_lru_cache.h>
#include <kcenon/pacs/services/cache/query_cache.h>

#ifdef PACS_WITH_DATABASE_SYSTEM
#include <kcenon/pacs/services/cache/database_cursor.h>
#include <kcenon/pacs/services/cache/query_result_stream.h>
#include <kcenon/pacs/services/cache/streaming_query_handler.h>
#include <kcenon/pacs/services/cache/parallel_query_executor.h>
#endif

export module kcenon.pacs:services_cache;

// ============================================================================
// Re-export kcenon::pacs::services::cache namespace
// ============================================================================

export namespace kcenon::pacs::services::cache {

// ==========================================================================
// LRU Cache
// ==========================================================================

// Generic LRU cache template
using kcenon::pacs::services::cache::cache_config;
using kcenon::pacs::services::cache::cache_stats;
using kcenon::pacs::services::cache::simple_lru_cache;
using kcenon::pacs::services::cache::string_lru_cache;

// ==========================================================================
// Query Cache
// ==========================================================================

// Query cache with monitoring integration
using kcenon::pacs::services::cache::query_cache_config;
using kcenon::pacs::services::cache::cached_query_result;
using kcenon::pacs::services::cache::query_cache;
using kcenon::pacs::services::cache::global_query_cache;
using kcenon::pacs::services::cache::configure_global_cache;

} // namespace kcenon::pacs::services::cache

#ifdef PACS_WITH_DATABASE_SYSTEM
export namespace kcenon::pacs::services {

// ==========================================================================
// Database Cursor (requires database_system)
// ==========================================================================

// Cursor for iterating over large result sets
using kcenon::pacs::services::database_cursor;
using kcenon::pacs::services::query_record;

// ==========================================================================
// Query Result Streaming (requires database_system)
// ==========================================================================

// Streaming query results
using kcenon::pacs::services::query_result_stream;
using kcenon::pacs::services::stream_config;
using kcenon::pacs::services::streaming_query_handler;

// ==========================================================================
// Parallel Query Executor (requires database_system)
// ==========================================================================

// Multi-threaded query execution
using kcenon::pacs::services::query_request;
using kcenon::pacs::services::query_execution_result;
using kcenon::pacs::services::parallel_executor_config;
using kcenon::pacs::services::parallel_query_executor;

} // namespace kcenon::pacs::services
#endif // PACS_WITH_DATABASE_SYSTEM
