/**
 * @file uid_mapping.hpp
 * @brief UID mapping for consistent de-identification across studies
 *
 * This file provides the uid_mapping class for maintaining consistent
 * UID transformations during DICOM de-identification.
 *
 * @see DICOM PS3.15 Annex E - Attribute Confidentiality Profiles
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>

namespace pacs::security {

/**
 * @brief Manages UID mappings for consistent de-identification
 *
 * When de-identifying multiple DICOM instances from the same study or
 * patient, UIDs must be consistently mapped to new values. This class
 * maintains a bidirectional mapping between original and anonymized UIDs.
 *
 * Thread Safety: This class is thread-safe for concurrent access.
 *
 * @example
 * @code
 * uid_mapping mapping;
 *
 * // First encounter - creates new mapping
 * auto new_uid = mapping.get_or_create("1.2.3.original.uid");
 * // new_uid = "1.2.3.4.5.6.anonymized.uid" (newly generated)
 *
 * // Second encounter - returns same mapping
 * auto same_uid = mapping.get_or_create("1.2.3.original.uid");
 * // same_uid == new_uid (consistent)
 *
 * // Reverse lookup
 * auto original = mapping.get_original(new_uid.value());
 * // original = "1.2.3.original.uid"
 * @endcode
 */
class uid_mapping {
public:
    /**
     * @brief Default constructor - creates empty mapping
     */
    uid_mapping() = default;

    /**
     * @brief Constructor with custom UID root
     * @param uid_root Custom root for generated UIDs (e.g., "1.2.3.4")
     */
    explicit uid_mapping(std::string uid_root);

    /**
     * @brief Copy constructor (creates independent copy of mappings)
     */
    uid_mapping(const uid_mapping& other);

    /**
     * @brief Move constructor
     */
    uid_mapping(uid_mapping&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    auto operator=(const uid_mapping& other) -> uid_mapping&;

    /**
     * @brief Move assignment
     */
    auto operator=(uid_mapping&& other) noexcept -> uid_mapping&;

    /**
     * @brief Default destructor
     */
    ~uid_mapping() = default;

    // ========================================================================
    // Mapping Operations
    // ========================================================================

    /**
     * @brief Get existing mapping or create new one
     *
     * If the original UID has been mapped before, returns the existing
     * anonymized UID. Otherwise, generates a new UID and stores the mapping.
     *
     * @param original_uid The original UID to map
     * @return Result containing the anonymized UID
     */
    [[nodiscard]] auto get_or_create(std::string_view original_uid)
        -> kcenon::common::Result<std::string>;

    /**
     * @brief Get existing mapping without creating new one
     *
     * @param original_uid The original UID to look up
     * @return Optional containing the anonymized UID, or nullopt if not found
     */
    [[nodiscard]] auto get_anonymized(std::string_view original_uid) const
        -> std::optional<std::string>;

    /**
     * @brief Get original UID from anonymized UID (reverse lookup)
     *
     * @param anonymized_uid The anonymized UID to look up
     * @return Optional containing the original UID, or nullopt if not found
     */
    [[nodiscard]] auto get_original(std::string_view anonymized_uid) const
        -> std::optional<std::string>;

    /**
     * @brief Add a specific mapping
     *
     * Adds a mapping between original and anonymized UIDs.
     * Fails if the original UID is already mapped to a different value.
     *
     * @param original_uid The original UID
     * @param anonymized_uid The anonymized UID
     * @return Result indicating success or error
     */
    [[nodiscard]] auto add_mapping(
        std::string_view original_uid,
        std::string_view anonymized_uid
    ) -> kcenon::common::VoidResult;

    // ========================================================================
    // Query Operations
    // ========================================================================

    /**
     * @brief Check if an original UID has been mapped
     * @param original_uid The original UID to check
     * @return true if a mapping exists
     */
    [[nodiscard]] auto has_mapping(std::string_view original_uid) const -> bool;

    /**
     * @brief Get the number of mappings
     * @return The count of UID mappings
     */
    [[nodiscard]] auto size() const -> std::size_t;

    /**
     * @brief Check if the mapping is empty
     * @return true if no mappings exist
     */
    [[nodiscard]] auto empty() const -> bool;

    // ========================================================================
    // Management Operations
    // ========================================================================

    /**
     * @brief Clear all mappings
     */
    void clear();

    /**
     * @brief Remove a specific mapping
     * @param original_uid The original UID to remove
     * @return true if a mapping was removed
     */
    auto remove(std::string_view original_uid) -> bool;

    // ========================================================================
    // Persistence Operations
    // ========================================================================

    /**
     * @brief Export mappings to JSON format
     * @return JSON string containing all mappings
     */
    [[nodiscard]] auto to_json() const -> std::string;

    /**
     * @brief Import mappings from JSON format
     * @param json JSON string containing mappings
     * @return Result indicating success or error
     */
    [[nodiscard]] auto from_json(std::string_view json)
        -> kcenon::common::VoidResult;

    /**
     * @brief Merge mappings from another uid_mapping
     *
     * Adds all mappings from 'other' that don't conflict with existing mappings.
     *
     * @param other The uid_mapping to merge from
     * @return Number of mappings added
     */
    auto merge(const uid_mapping& other) -> std::size_t;

    // ========================================================================
    // UID Generation
    // ========================================================================

    /**
     * @brief Set the UID root for generated UIDs
     * @param root The UID root (e.g., "1.2.3.4")
     */
    void set_uid_root(std::string root);

    /**
     * @brief Get the current UID root
     * @return The UID root string
     */
    [[nodiscard]] auto get_uid_root() const -> std::string;

    /**
     * @brief Generate a new unique UID
     * @return A new UID string
     */
    [[nodiscard]] auto generate_uid() const -> std::string;

private:
    /// UID root for generated UIDs (default: pacs_system root)
    std::string uid_root_{"1.2.826.0.1.3680043.8.498.1"};

    /// Forward mapping: original -> anonymized
    std::map<std::string, std::string, std::less<>> original_to_anon_;

    /// Reverse mapping: anonymized -> original
    std::map<std::string, std::string, std::less<>> anon_to_original_;

    /// Counter for UID generation
    mutable std::atomic<std::uint64_t> uid_counter_{0};

    /// Mutex for thread-safe access
    mutable std::shared_mutex mutex_;
};

} // namespace pacs::security
