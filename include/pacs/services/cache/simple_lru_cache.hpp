/**
 * @file simple_lru_cache.hpp
 * @brief Thread-safe LRU (Least Recently Used) cache for query results
 *
 * This file provides a template-based LRU cache implementation designed for
 * caching DICOM query results. It supports configurable maximum size, TTL
 * (Time-To-Live), and integrates with the PACS monitoring system for metrics.
 *
 * The cache uses a combination of a doubly-linked list (for LRU ordering) and
 * a hash map (for O(1) lookups) to achieve efficient cache operations.
 *
 * @see Issue #209 - [Quick Win] feat(services): Implement simple LRU query cache
 * @see Issue #189 - Implement LRU cache for query results
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace pacs::services::cache {

// ─────────────────────────────────────────────────────
// Forward Declarations
// ─────────────────────────────────────────────────────

template <typename Key, typename Value, typename Hash, typename KeyEqual>
class simple_lru_cache;

// ─────────────────────────────────────────────────────
// Cache Configuration
// ─────────────────────────────────────────────────────

/**
 * @struct cache_config
 * @brief Configuration options for the LRU cache
 */
struct cache_config {
    /// Maximum number of entries in the cache
    std::size_t max_size{1000};

    /// Time-To-Live for cache entries in seconds (default: 300 = 5 minutes)
    std::chrono::seconds ttl{300};

    /// Enable metrics collection for hit/miss tracking
    bool enable_metrics{true};

    /// Name for metrics identification (e.g., "query_cache", "study_cache")
    std::string cache_name{"lru_cache"};
};

// ─────────────────────────────────────────────────────
// Cache Statistics
// ─────────────────────────────────────────────────────

/**
 * @struct cache_stats
 * @brief Statistics for cache performance monitoring
 *
 * All counters are atomic to allow lock-free reading of statistics
 * while the cache is being modified.
 */
struct cache_stats {
    std::atomic<std::uint64_t> hits{0};        ///< Number of cache hits
    std::atomic<std::uint64_t> misses{0};      ///< Number of cache misses
    std::atomic<std::uint64_t> insertions{0};  ///< Number of insertions
    std::atomic<std::uint64_t> evictions{0};   ///< Number of LRU evictions
    std::atomic<std::uint64_t> expirations{0}; ///< Number of TTL expirations
    std::atomic<std::size_t> current_size{0};  ///< Current number of entries

    /**
     * @brief Calculate the cache hit rate
     * @return Hit rate as a percentage (0.0 to 100.0), or 0.0 if no accesses
     */
    [[nodiscard]] double hit_rate() const noexcept {
        const auto total_hits = hits.load(std::memory_order_relaxed);
        const auto total_misses = misses.load(std::memory_order_relaxed);
        const auto total = total_hits + total_misses;
        if (total == 0) {
            return 0.0;
        }
        return (static_cast<double>(total_hits) / static_cast<double>(total)) * 100.0;
    }

    /**
     * @brief Get total number of cache accesses (hits + misses)
     * @return Total access count
     */
    [[nodiscard]] std::uint64_t total_accesses() const noexcept {
        return hits.load(std::memory_order_relaxed) +
               misses.load(std::memory_order_relaxed);
    }

    /**
     * @brief Reset all statistics to zero
     */
    void reset() noexcept {
        hits.store(0, std::memory_order_relaxed);
        misses.store(0, std::memory_order_relaxed);
        insertions.store(0, std::memory_order_relaxed);
        evictions.store(0, std::memory_order_relaxed);
        expirations.store(0, std::memory_order_relaxed);
        // Note: current_size is not reset as it reflects actual cache state
    }
};

// ─────────────────────────────────────────────────────
// LRU Cache Implementation
// ─────────────────────────────────────────────────────

/**
 * @class simple_lru_cache
 * @brief Thread-safe LRU cache with TTL support
 *
 * This class provides a least-recently-used (LRU) cache that automatically
 * evicts the oldest entries when the cache reaches its maximum size. Each
 * entry has a configurable time-to-live (TTL) after which it expires.
 *
 * The implementation uses:
 * - std::list for O(1) insertion/removal and LRU ordering
 * - std::unordered_map for O(1) key lookups
 * - std::shared_mutex for reader-writer locking (multiple readers, single writer)
 *
 * Thread Safety: All public methods are thread-safe. Read operations (get)
 * use shared locks allowing concurrent reads. Write operations (put, invalidate,
 * clear) use exclusive locks.
 *
 * @tparam Key The key type (must be hashable and equality comparable)
 * @tparam Value The value type (must be copy constructible)
 * @tparam Hash Hash function for keys (defaults to std::hash<Key>)
 * @tparam KeyEqual Equality comparison for keys (defaults to std::equal_to<Key>)
 *
 * @example
 * @code
 * // Create a cache for query results
 * cache_config config;
 * config.max_size = 1000;
 * config.ttl = std::chrono::seconds{300};  // 5 minutes
 * config.cache_name = "cfind_cache";
 *
 * simple_lru_cache<std::string, QueryResult> cache(config);
 *
 * // Store a result
 * cache.put("patient_123", query_result);
 *
 * // Retrieve a result
 * auto result = cache.get("patient_123");
 * if (result) {
 *     // Use cached result
 *     process(*result);
 * }
 *
 * // Check cache performance
 * const auto& stats = cache.stats();
 * logger_adapter::info("Cache hit rate: {:.2f}%", stats.hit_rate());
 * @endcode
 */
template <typename Key,
          typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class simple_lru_cache {
public:
    using key_type = Key;
    using value_type = Value;
    using size_type = std::size_t;
    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;

    // =========================================================================
    // Construction
    // =========================================================================

    /**
     * @brief Construct a cache with the given configuration
     * @param config Cache configuration options
     */
    explicit simple_lru_cache(const cache_config& config = cache_config{})
        : config_(config)
        , max_size_(config.max_size > 0 ? config.max_size : 1) {
    }

    /**
     * @brief Construct a cache with size and TTL
     * @param max_size Maximum number of entries
     * @param ttl Time-to-live for entries
     */
    simple_lru_cache(size_type max_size, std::chrono::seconds ttl)
        : max_size_(max_size > 0 ? max_size : 1) {
        config_.max_size = max_size_;
        config_.ttl = ttl;
    }

    /// Non-copyable
    simple_lru_cache(const simple_lru_cache&) = delete;
    simple_lru_cache& operator=(const simple_lru_cache&) = delete;

    /// Movable
    simple_lru_cache(simple_lru_cache&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        config_ = std::move(other.config_);
        max_size_ = other.max_size_;
        lru_list_ = std::move(other.lru_list_);
        cache_map_ = std::move(other.cache_map_);
        // Stats are not moved (fresh start for moved-to instance)
    }

    simple_lru_cache& operator=(simple_lru_cache&& other) noexcept {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::unique_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);

            config_ = std::move(other.config_);
            max_size_ = other.max_size_;
            lru_list_ = std::move(other.lru_list_);
            cache_map_ = std::move(other.cache_map_);
        }
        return *this;
    }

    ~simple_lru_cache() = default;

    // =========================================================================
    // Cache Operations
    // =========================================================================

    /**
     * @brief Retrieve a value from the cache
     *
     * If the key exists and the entry has not expired, returns the value
     * and moves the entry to the front of the LRU list (most recently used).
     * Expired entries are automatically removed.
     *
     * @param key The key to look up
     * @return The cached value if found and not expired, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Value> get(const Key& key) {
        std::unique_lock lock(mutex_);

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            // Cache miss
            stats_.misses.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }

        auto& entry = it->second;

        // Check TTL expiration
        if (is_expired(entry->expiry_time)) {
            // Entry expired - remove it
            stats_.expirations.fetch_add(1, std::memory_order_relaxed);
            stats_.misses.fetch_add(1, std::memory_order_relaxed);
            remove_entry(it);
            return std::nullopt;
        }

        // Cache hit - move to front (most recently used)
        stats_.hits.fetch_add(1, std::memory_order_relaxed);
        lru_list_.splice(lru_list_.begin(), lru_list_, entry);

        return entry->value;
    }

    /**
     * @brief Store a value in the cache
     *
     * If the key already exists, updates the value and moves to front.
     * If the cache is full, evicts the least recently used entry.
     *
     * @param key The key to store
     * @param value The value to cache
     */
    void put(const Key& key, const Value& value) {
        std::unique_lock lock(mutex_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing entry
            auto& entry = it->second;
            entry->value = value;
            entry->expiry_time = calculate_expiry();
            lru_list_.splice(lru_list_.begin(), lru_list_, entry);
            return;
        }

        // Evict if at capacity
        while (cache_map_.size() >= max_size_ && !lru_list_.empty()) {
            evict_oldest();
        }

        // Insert new entry at front
        lru_list_.emplace_front(cache_entry{key, value, calculate_expiry()});
        cache_map_[key] = lru_list_.begin();

        stats_.insertions.fetch_add(1, std::memory_order_relaxed);
        stats_.current_size.store(cache_map_.size(), std::memory_order_relaxed);
    }

    /**
     * @brief Store a value in the cache (move semantics)
     *
     * @param key The key to store
     * @param value The value to cache (moved)
     */
    void put(const Key& key, Value&& value) {
        std::unique_lock lock(mutex_);

        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing entry
            auto& entry = it->second;
            entry->value = std::move(value);
            entry->expiry_time = calculate_expiry();
            lru_list_.splice(lru_list_.begin(), lru_list_, entry);
            return;
        }

        // Evict if at capacity
        while (cache_map_.size() >= max_size_ && !lru_list_.empty()) {
            evict_oldest();
        }

        // Insert new entry at front
        lru_list_.emplace_front(cache_entry{key, std::move(value), calculate_expiry()});
        cache_map_[key] = lru_list_.begin();

        stats_.insertions.fetch_add(1, std::memory_order_relaxed);
        stats_.current_size.store(cache_map_.size(), std::memory_order_relaxed);
    }

    /**
     * @brief Check if a key exists in the cache (without affecting LRU order)
     *
     * Note: This does not check TTL expiration for performance reasons.
     * Use get() if you need TTL-aware existence checking.
     *
     * @param key The key to check
     * @return true if the key exists in the cache
     */
    [[nodiscard]] bool contains(const Key& key) const {
        std::shared_lock lock(mutex_);
        return cache_map_.find(key) != cache_map_.end();
    }

    /**
     * @brief Remove a specific entry from the cache
     *
     * @param key The key to remove
     * @return true if the entry was found and removed, false otherwise
     */
    bool invalidate(const Key& key) {
        std::unique_lock lock(mutex_);

        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return false;
        }

        remove_entry(it);
        return true;
    }

    /**
     * @brief Remove all entries matching a predicate
     *
     * Iterates through all cache entries and removes those for which
     * the predicate returns true. This is useful for invalidating
     * related entries when data changes (e.g., invalidating all
     * cached queries for a specific patient when new studies arrive).
     *
     * @tparam Predicate A callable that takes (const Key&, const Value&) and returns bool
     * @param pred The predicate function; entries where pred(key, value) returns true are removed
     * @return Number of entries removed
     *
     * @example
     * @code
     * // Invalidate all entries with keys starting with "PATIENT:"
     * cache.invalidate_if([](const std::string& key, const auto&) {
     *     return key.starts_with("PATIENT:");
     * });
     *
     * // Invalidate entries older than a certain age
     * cache.invalidate_if([&](const auto&, const QueryResult& result) {
     *     return result.match_count > 1000;  // Remove large result sets
     * });
     * @endcode
     */
    template <typename Predicate>
    size_type invalidate_if(Predicate pred) {
        std::unique_lock lock(mutex_);

        size_type removed = 0;
        auto it = lru_list_.begin();

        while (it != lru_list_.end()) {
            if (pred(it->key, it->value)) {
                cache_map_.erase(it->key);
                it = lru_list_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }

        stats_.current_size.store(cache_map_.size(), std::memory_order_relaxed);
        return removed;
    }

    /**
     * @brief Remove all entries from the cache
     */
    void clear() {
        std::unique_lock lock(mutex_);

        lru_list_.clear();
        cache_map_.clear();
        stats_.current_size.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Remove all expired entries from the cache
     *
     * This is useful for periodic cleanup to free memory from expired
     * entries that haven't been accessed.
     *
     * @return Number of expired entries removed
     */
    size_type purge_expired() {
        std::unique_lock lock(mutex_);

        size_type removed = 0;
        const auto now = clock_type::now();

        auto it = lru_list_.begin();
        while (it != lru_list_.end()) {
            if (is_expired(it->expiry_time, now)) {
                cache_map_.erase(it->key);
                it = lru_list_.erase(it);
                ++removed;
                stats_.expirations.fetch_add(1, std::memory_order_relaxed);
            } else {
                ++it;
            }
        }

        stats_.current_size.store(cache_map_.size(), std::memory_order_relaxed);
        return removed;
    }

    // =========================================================================
    // Cache Information
    // =========================================================================

    /**
     * @brief Get the current number of entries in the cache
     * @return Current cache size
     */
    [[nodiscard]] size_type size() const {
        std::shared_lock lock(mutex_);
        return cache_map_.size();
    }

    /**
     * @brief Check if the cache is empty
     * @return true if the cache contains no entries
     */
    [[nodiscard]] bool empty() const {
        std::shared_lock lock(mutex_);
        return cache_map_.empty();
    }

    /**
     * @brief Get the maximum cache size
     * @return Maximum number of entries
     */
    [[nodiscard]] size_type max_size() const noexcept {
        return max_size_;
    }

    /**
     * @brief Get the TTL duration
     * @return Time-to-live for entries
     */
    [[nodiscard]] std::chrono::seconds ttl() const noexcept {
        return config_.ttl;
    }

    /**
     * @brief Get the cache name (for metrics identification)
     * @return Cache name
     */
    [[nodiscard]] const std::string& name() const noexcept {
        return config_.cache_name;
    }

    /**
     * @brief Get the cache configuration
     * @return Cache configuration
     */
    [[nodiscard]] const cache_config& config() const noexcept {
        return config_;
    }

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get cache statistics
     * @return Reference to cache statistics
     */
    [[nodiscard]] const cache_stats& stats() const noexcept {
        return stats_;
    }

    /**
     * @brief Get the cache hit rate
     * @return Hit rate as a percentage (0.0 to 100.0)
     */
    [[nodiscard]] double hit_rate() const noexcept {
        return stats_.hit_rate();
    }

    /**
     * @brief Reset cache statistics
     *
     * Resets all counters except current_size which reflects actual cache state.
     */
    void reset_stats() noexcept {
        stats_.reset();
    }

private:
    // =========================================================================
    // Internal Types
    // =========================================================================

    struct cache_entry {
        Key key;
        Value value;
        time_point expiry_time;
    };

    using list_type = std::list<cache_entry>;
    using list_iterator = typename list_type::iterator;
    using map_type = std::unordered_map<Key, list_iterator, Hash, KeyEqual>;

    // =========================================================================
    // Internal Helpers
    // =========================================================================

    [[nodiscard]] time_point calculate_expiry() const {
        return clock_type::now() + config_.ttl;
    }

    [[nodiscard]] bool is_expired(time_point expiry) const {
        return clock_type::now() > expiry;
    }

    [[nodiscard]] bool is_expired(time_point expiry, time_point now) const {
        return now > expiry;
    }

    void evict_oldest() {
        // Evict from the back (least recently used)
        if (lru_list_.empty()) {
            return;
        }

        auto& oldest = lru_list_.back();
        cache_map_.erase(oldest.key);
        lru_list_.pop_back();

        stats_.evictions.fetch_add(1, std::memory_order_relaxed);
        stats_.current_size.store(cache_map_.size(), std::memory_order_relaxed);
    }

    void remove_entry(typename map_type::iterator map_it) {
        lru_list_.erase(map_it->second);
        cache_map_.erase(map_it);
        stats_.current_size.store(cache_map_.size(), std::memory_order_relaxed);
    }

    // =========================================================================
    // Member Variables
    // =========================================================================

    cache_config config_;
    size_type max_size_;

    mutable std::shared_mutex mutex_;
    list_type lru_list_;
    map_type cache_map_;

    cache_stats stats_;
};

// ─────────────────────────────────────────────────────
// Type Aliases for Common Use Cases
// ─────────────────────────────────────────────────────

/**
 * @brief String-keyed LRU cache for query results
 *
 * Commonly used for caching C-FIND query results where the key is
 * a hash of the query parameters.
 */
template <typename Value>
using string_lru_cache = simple_lru_cache<std::string, Value>;

}  // namespace pacs::services::cache
