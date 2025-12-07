/**
 * @file query_cache.hpp
 * @brief DICOM query result cache with monitoring integration
 *
 * This file provides a specialized cache for DICOM C-FIND query results
 * that integrates with the PACS monitoring system for metrics reporting.
 *
 * The query_cache wraps simple_lru_cache and adds:
 * - Integration with pacs_metrics for hit/miss/eviction tracking
 * - Integration with logger_adapter for cache event logging
 * - Helper methods for building cache keys from query parameters
 *
 * @see Issue #209 - [Quick Win] feat(services): Implement simple LRU query cache
 */

#pragma once

#include <pacs/services/cache/simple_lru_cache.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace pacs::services::cache {

// ─────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────

class query_cache;

// ─────────────────────────────────────────────────────
// Query Cache Configuration
// ─────────────────────────────────────────────────────

/**
 * @struct query_cache_config
 * @brief Configuration for the query cache
 */
struct query_cache_config {
    /// Maximum number of cached query results
    std::size_t max_entries{1000};

    /// Time-to-live for cached results (default: 5 minutes)
    std::chrono::seconds ttl{300};

    /// Enable debug logging for cache operations
    bool enable_logging{false};

    /// Enable metrics reporting to pacs_metrics
    bool enable_metrics{true};

    /// Cache identifier for logging and metrics
    std::string cache_name{"cfind_query_cache"};
};

// ─────────────────────────────────────────────────────
// Query Result Type
// ─────────────────────────────────────────────────────

/**
 * @struct cached_query_result
 * @brief Wrapper for cached query results
 *
 * This structure holds the serialized query results along with
 * metadata about when the result was cached.
 */
struct cached_query_result {
    /// Serialized query result data
    std::vector<std::uint8_t> data;

    /// Number of matching records in this result
    std::uint32_t match_count{0};

    /// Timestamp when this result was cached
    std::chrono::steady_clock::time_point cached_at;

    /// Query level (PATIENT, STUDY, SERIES, IMAGE)
    std::string query_level;
};

// ─────────────────────────────────────────────────────
// Query Cache Implementation
// ─────────────────────────────────────────────────────

/**
 * @class query_cache
 * @brief DICOM query result cache with monitoring integration
 *
 * This class provides a specialized cache for C-FIND query results with:
 * - LRU eviction policy
 * - Configurable TTL (Time-To-Live)
 * - Integration with PACS monitoring system
 * - Thread-safe concurrent access
 *
 * Thread Safety: All public methods are thread-safe.
 *
 * @example
 * @code
 * // Create the cache
 * query_cache_config config;
 * config.max_entries = 500;
 * config.ttl = std::chrono::seconds{120};  // 2 minutes
 *
 * query_cache cache(config);
 *
 * // Build a cache key from query parameters
 * std::string key = query_cache::build_key("STUDY", {
 *     {"PatientID", "12345"},
 *     {"StudyDate", "20240101"}
 * });
 *
 * // Check cache
 * auto result = cache.get(key);
 * if (result) {
 *     // Use cached result
 *     return result->data;
 * }
 *
 * // Cache miss - execute query and cache result
 * auto query_result = execute_cfind_query(...);
 * cache.put(key, query_result);
 * @endcode
 */
class query_cache {
public:
    using key_type = std::string;
    using value_type = cached_query_result;
    using size_type = std::size_t;

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a query cache with the given configuration
     * @param config Cache configuration
     */
    explicit query_cache(const query_cache_config& config = query_cache_config{});

    /// Non-copyable
    query_cache(const query_cache&) = delete;
    query_cache& operator=(const query_cache&) = delete;

    /// Movable
    query_cache(query_cache&&) noexcept = default;
    query_cache& operator=(query_cache&&) noexcept = default;

    ~query_cache() = default;

    // =========================================================================
    // Cache Operations
    // =========================================================================

    /**
     * @brief Retrieve a cached query result
     *
     * If found and not expired, the result is returned and the entry is
     * marked as recently used. Cache metrics are updated.
     *
     * @param key The cache key (use build_key to generate)
     * @return The cached result if found, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<cached_query_result> get(const key_type& key);

    /**
     * @brief Store a query result in the cache
     *
     * @param key The cache key
     * @param result The query result to cache
     */
    void put(const key_type& key, const cached_query_result& result);

    /**
     * @brief Store a query result in the cache (move semantics)
     *
     * @param key The cache key
     * @param result The query result to cache (moved)
     */
    void put(const key_type& key, cached_query_result&& result);

    /**
     * @brief Remove a specific entry from the cache
     *
     * @param key The cache key
     * @return true if the entry was found and removed
     */
    bool invalidate(const key_type& key);

    /**
     * @brief Remove all entries from the cache
     */
    void clear();

    /**
     * @brief Remove all expired entries
     * @return Number of entries removed
     */
    size_type purge_expired();

    // =========================================================================
    // Cache Information
    // =========================================================================

    /**
     * @brief Get the current number of cached entries
     * @return Current cache size
     */
    [[nodiscard]] size_type size() const;

    /**
     * @brief Check if the cache is empty
     * @return true if no entries are cached
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Get the maximum cache size
     * @return Maximum number of entries
     */
    [[nodiscard]] size_type max_size() const noexcept;

    /**
     * @brief Get cache statistics
     * @return Reference to cache statistics
     */
    [[nodiscard]] const cache_stats& stats() const noexcept;

    /**
     * @brief Get the cache hit rate
     * @return Hit rate as percentage (0.0 to 100.0)
     */
    [[nodiscard]] double hit_rate() const noexcept;

    /**
     * @brief Reset cache statistics
     */
    void reset_stats() noexcept;

    // =========================================================================
    // Key Generation Helpers
    // =========================================================================

    /**
     * @brief Build a cache key from query parameters
     *
     * Creates a deterministic key from the query level and parameters.
     * The key format is: "level:param1=value1;param2=value2;..."
     * Parameters are sorted by name for consistent key generation.
     *
     * @param query_level The query retrieve level (PATIENT, STUDY, SERIES, IMAGE)
     * @param params List of parameter name-value pairs
     * @return Cache key string
     */
    [[nodiscard]] static std::string build_key(
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);

    /**
     * @brief Build a cache key with AE title prefix
     *
     * Includes the calling AE title in the key to support per-client caching.
     *
     * @param calling_ae The calling AE title
     * @param query_level The query retrieve level
     * @param params List of parameter name-value pairs
     * @return Cache key string
     */
    [[nodiscard]] static std::string build_key_with_ae(
        const std::string& calling_ae,
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);

private:
    query_cache_config config_;
    string_lru_cache<cached_query_result> cache_;
};

// ─────────────────────────────────────────────────────
// Global Query Cache Instance
// ─────────────────────────────────────────────────────

/**
 * @brief Get the global query cache instance
 *
 * Returns a singleton instance of the query cache. The cache is initialized
 * with default settings on first access. Use configure_global_cache() to
 * customize settings before first use.
 *
 * Thread Safety: Thread-safe initialization via Meyer's singleton pattern.
 *
 * @return Reference to the global query cache
 */
[[nodiscard]] query_cache& global_query_cache();

/**
 * @brief Configure the global query cache
 *
 * Must be called before the first access to global_query_cache().
 * Subsequent calls have no effect.
 *
 * @param config Cache configuration
 * @return true if configuration was applied, false if cache was already initialized
 */
bool configure_global_cache(const query_cache_config& config);

}  // namespace pacs::services::cache
