/**
 * @file thumbnail_service.hpp
 * @brief Thumbnail generation service for DICOM images
 *
 * Provides server-side thumbnail generation and caching for DICOM instances,
 * series, and studies. Supports multiple output formats and configurable sizes.
 *
 * @see Issue #543 - Implement Thumbnail API for DICOM Viewer
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pacs::storage {
class index_database;
}  // namespace pacs::storage

namespace pacs::web {

/**
 * @brief Parameters for thumbnail generation
 */
struct thumbnail_params {
    /// Output size in pixels (64, 128, 256, 512)
    uint16_t size{128};

    /// Output format ("jpeg", "png")
    std::string format{"jpeg"};

    /// Quality for lossy compression (1-100)
    int quality{60};

    /// Frame number for multi-frame images (1-indexed)
    uint32_t frame{1};
};

/**
 * @brief Cached thumbnail entry
 */
struct thumbnail_cache_entry {
    /// Compressed image data
    std::vector<uint8_t> data;

    /// MIME content type
    std::string content_type;

    /// When the entry was created
    std::chrono::system_clock::time_point created_at;

    /// When the entry was last accessed
    std::chrono::system_clock::time_point last_accessed;
};

/**
 * @brief Result type for thumbnail operations
 */
struct thumbnail_result {
    /// Whether the operation succeeded
    bool success{false};

    /// Error message if failed
    std::string error_message;

    /// Thumbnail data if succeeded
    thumbnail_cache_entry entry;

    /// Create a success result
    static thumbnail_result ok(thumbnail_cache_entry entry) {
        thumbnail_result r;
        r.success = true;
        r.entry = std::move(entry);
        return r;
    }

    /// Create an error result
    static thumbnail_result error(std::string message) {
        thumbnail_result r;
        r.success = false;
        r.error_message = std::move(message);
        return r;
    }
};

/**
 * @class thumbnail_service
 * @brief Thumbnail generation and caching service
 *
 * Generates thumbnails from DICOM images with server-side caching.
 * Supports multiple output formats and sizes.
 *
 * @par Example
 * @code
 * auto service = std::make_shared<thumbnail_service>(database);
 *
 * thumbnail_params params;
 * params.size = 256;
 * params.format = "jpeg";
 * params.quality = 75;
 *
 * auto result = service->get_thumbnail("1.2.3.4.5", params);
 * if (result.success) {
 *     // Use result.entry.data and result.entry.content_type
 * }
 * @endcode
 */
class thumbnail_service {
public:
    /**
     * @brief Construct thumbnail service with database
     * @param database Index database for instance lookups
     */
    explicit thumbnail_service(
        std::shared_ptr<storage::index_database> database);

    /// Destructor
    ~thumbnail_service();

    /// Non-copyable
    thumbnail_service(const thumbnail_service&) = delete;
    thumbnail_service& operator=(const thumbnail_service&) = delete;

    /// Movable
    thumbnail_service(thumbnail_service&&) noexcept;
    thumbnail_service& operator=(thumbnail_service&&) noexcept;

    // =========================================================================
    // Thumbnail Generation
    // =========================================================================

    /**
     * @brief Get or generate thumbnail for a specific instance
     * @param sop_instance_uid SOP Instance UID
     * @param params Thumbnail parameters
     * @return Thumbnail result with image data or error
     */
    [[nodiscard]] thumbnail_result get_thumbnail(
        std::string_view sop_instance_uid,
        const thumbnail_params& params);

    /**
     * @brief Get thumbnail for a series (representative image)
     * @param series_uid Series Instance UID
     * @param params Thumbnail parameters
     * @return Thumbnail result with image data or error
     *
     * Selects the middle slice or key image from the series.
     */
    [[nodiscard]] thumbnail_result get_series_thumbnail(
        std::string_view series_uid,
        const thumbnail_params& params);

    /**
     * @brief Get thumbnail for a study (representative image)
     * @param study_uid Study Instance UID
     * @param params Thumbnail parameters
     * @return Thumbnail result with image data or error
     *
     * Selects the representative image from the primary series.
     */
    [[nodiscard]] thumbnail_result get_study_thumbnail(
        std::string_view study_uid,
        const thumbnail_params& params);

    // =========================================================================
    // Cache Management
    // =========================================================================

    /**
     * @brief Clear all cached thumbnails
     */
    void clear_cache();

    /**
     * @brief Clear cached thumbnails for a specific instance
     * @param sop_instance_uid SOP Instance UID to clear
     */
    void clear_cache(std::string_view sop_instance_uid);

    /**
     * @brief Get current cache size in bytes
     * @return Total bytes used by cached thumbnails
     */
    [[nodiscard]] size_t cache_size() const;

    /**
     * @brief Get number of cached entries
     * @return Number of entries in cache
     */
    [[nodiscard]] size_t cache_entry_count() const;

    /**
     * @brief Set maximum cache size
     * @param max_bytes Maximum cache size in bytes
     */
    void set_max_cache_size(size_t max_bytes);

    /**
     * @brief Get maximum cache size
     * @return Maximum cache size in bytes
     */
    [[nodiscard]] size_t max_cache_size() const;

private:
    /// Cache key for lookups
    struct cache_key {
        std::string uid;
        uint16_t size;
        std::string format;
        int quality;
        uint32_t frame;

        bool operator==(const cache_key&) const = default;
    };

    /// Hash function for cache key
    struct cache_key_hash {
        size_t operator()(const cache_key& k) const {
            size_t h = std::hash<std::string>{}(k.uid);
            h ^= std::hash<uint16_t>{}(k.size) << 1;
            h ^= std::hash<std::string>{}(k.format) << 2;
            h ^= std::hash<int>{}(k.quality) << 3;
            h ^= std::hash<uint32_t>{}(k.frame) << 4;
            return h;
        }
    };

    /// Database for instance lookups
    std::shared_ptr<storage::index_database> database_;

    /// Thumbnail cache
    std::unordered_map<cache_key, thumbnail_cache_entry, cache_key_hash> cache_;

    /// Cache mutex
    mutable std::shared_mutex cache_mutex_;

    /// Current cache size in bytes
    size_t current_cache_size_{0};

    /// Maximum cache size (default: 64MB)
    size_t max_cache_size_{64 * 1024 * 1024};

    // =========================================================================
    // Internal Methods
    // =========================================================================

    /**
     * @brief Generate thumbnail from DICOM file
     * @param file_path Path to DICOM file
     * @param params Thumbnail parameters
     * @return Generated thumbnail data or empty on failure
     */
    [[nodiscard]] std::vector<uint8_t> generate_thumbnail(
        std::string_view file_path,
        const thumbnail_params& params);

    /**
     * @brief Select representative instance from series
     * @param series_uid Series Instance UID
     * @return SOP Instance UID of representative instance
     */
    [[nodiscard]] std::optional<std::string> select_representative_instance(
        std::string_view series_uid);

    /**
     * @brief Select representative series from study
     * @param study_uid Study Instance UID
     * @return Series Instance UID of representative series
     */
    [[nodiscard]] std::optional<std::string> select_representative_series(
        std::string_view study_uid);

    /**
     * @brief Evict least recently used entries to make room
     */
    void evict_lru();

    /**
     * @brief Get MIME type for format
     * @param format Format string
     * @return MIME type string
     */
    [[nodiscard]] static std::string get_content_type(std::string_view format);
};

}  // namespace pacs::web
