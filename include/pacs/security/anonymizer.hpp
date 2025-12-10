/**
 * @file anonymizer.hpp
 * @brief DICOM de-identification/anonymization per PS3.15 Annex E
 *
 * This file provides the anonymizer class which implements DICOM
 * de-identification functionality as specified in DICOM PS3.15
 * (Security and System Management Profiles) Annex E.
 *
 * @see DICOM PS3.15 Annex E - Attribute Confidentiality Profiles
 * @copyright Copyright (c) 2025
 */

#pragma once

#include "anonymization_profile.hpp"
#include "tag_action.hpp"
#include "uid_mapping.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag.hpp>

#include <kcenon/common/patterns/result.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pacs::security {

/**
 * @brief DICOM de-identification/anonymization engine
 *
 * This class provides comprehensive DICOM de-identification capabilities
 * based on DICOM PS3.15 Annex E profiles. It supports multiple profiles
 * for different use cases (research, HIPAA compliance, GDPR, etc.).
 *
 * Thread Safety: This class is NOT thread-safe. Create separate instances
 * for concurrent operations, or use external synchronization.
 *
 * @example
 * @code
 * // Basic anonymization
 * anonymizer anon(anonymization_profile::hipaa_safe_harbor);
 * auto result = anon.anonymize(dataset);
 * if (result.is_ok()) {
 *     auto report = result.value();
 *     std::cout << "Removed " << report.tags_removed << " tags\n";
 * }
 *
 * // Consistent UID mapping across studies
 * uid_mapping mapping;
 * anonymizer anon(anonymization_profile::retain_longitudinal);
 *
 * for (auto& dataset : patient_datasets) {
 *     anon.anonymize_with_mapping(dataset, mapping);
 * }
 *
 * // Custom tag actions
 * anonymizer anon(anonymization_profile::basic);
 * anon.add_tag_action(tags::manufacturer, tag_action_config::make_keep());
 * anon.add_tag_action(tags::patient_id, tag_action_config::make_hash());
 * @endcode
 */
class anonymizer {
public:
    // ========================================================================
    // Construction
    // ========================================================================

    /**
     * @brief Construct with a specific profile
     * @param profile The anonymization profile to use
     */
    explicit anonymizer(anonymization_profile profile = anonymization_profile::basic);

    /**
     * @brief Copy constructor
     */
    anonymizer(const anonymizer& other);

    /**
     * @brief Move constructor
     */
    anonymizer(anonymizer&& other) noexcept;

    /**
     * @brief Copy assignment
     */
    auto operator=(const anonymizer& other) -> anonymizer&;

    /**
     * @brief Move assignment
     */
    auto operator=(anonymizer&& other) noexcept -> anonymizer&;

    /**
     * @brief Default destructor
     */
    ~anonymizer() = default;

    // ========================================================================
    // Anonymization Operations
    // ========================================================================

    /**
     * @brief Anonymize a DICOM dataset
     *
     * Applies the configured profile and any custom tag actions to
     * de-identify the dataset. The dataset is modified in place.
     *
     * @param dataset The dataset to anonymize (modified in place)
     * @return Result containing the anonymization report
     *
     * @note UIDs are regenerated with new values (no mapping preserved).
     *       Use anonymize_with_mapping() for consistent UID handling.
     */
    [[nodiscard]] auto anonymize(core::dicom_dataset& dataset)
        -> kcenon::common::Result<anonymization_report>;

    /**
     * @brief Anonymize with consistent UID mapping
     *
     * Applies de-identification while maintaining consistent UID
     * mappings across multiple datasets. This is essential for
     * longitudinal studies and research linkage.
     *
     * @param dataset The dataset to anonymize (modified in place)
     * @param mapping UID mapping for consistent transformation
     * @return Result containing the anonymization report
     */
    [[nodiscard]] auto anonymize_with_mapping(
        core::dicom_dataset& dataset,
        uid_mapping& mapping
    ) -> kcenon::common::Result<anonymization_report>;

    // ========================================================================
    // Profile Configuration
    // ========================================================================

    /**
     * @brief Get the current profile
     * @return The anonymization profile being used
     */
    [[nodiscard]] auto get_profile() const noexcept -> anonymization_profile;

    /**
     * @brief Set a new profile
     *
     * Changes the anonymization profile. Custom tag actions are preserved.
     *
     * @param profile The new profile to use
     */
    void set_profile(anonymization_profile profile);

    // ========================================================================
    // Custom Tag Actions
    // ========================================================================

    /**
     * @brief Add or override a tag action
     *
     * Sets a custom action for a specific tag, overriding the profile default.
     *
     * @param tag The DICOM tag
     * @param config The action configuration
     */
    void add_tag_action(core::dicom_tag tag, tag_action_config config);

    /**
     * @brief Add multiple tag actions
     * @param actions Map of tags to action configurations
     */
    void add_tag_actions(
        const std::map<core::dicom_tag, tag_action_config>& actions
    );

    /**
     * @brief Remove a custom tag action (reverts to profile default)
     * @param tag The tag to remove custom action for
     * @return true if an action was removed
     */
    auto remove_tag_action(core::dicom_tag tag) -> bool;

    /**
     * @brief Clear all custom tag actions
     */
    void clear_custom_actions();

    /**
     * @brief Get the effective action for a tag
     *
     * Returns the custom action if set, otherwise the profile default.
     *
     * @param tag The DICOM tag
     * @return The tag action configuration
     */
    [[nodiscard]] auto get_tag_action(core::dicom_tag tag) const
        -> tag_action_config;

    // ========================================================================
    // Date Shifting
    // ========================================================================

    /**
     * @brief Set date offset for longitudinal consistency
     *
     * All date/time values will be shifted by this offset, preserving
     * temporal relationships while removing actual dates.
     *
     * @param offset The number of days to shift (can be negative)
     */
    void set_date_offset(std::chrono::days offset);

    /**
     * @brief Get the current date offset
     * @return Optional containing the offset, or nullopt if not set
     */
    [[nodiscard]] auto get_date_offset() const noexcept
        -> std::optional<std::chrono::days>;

    /**
     * @brief Clear the date offset (dates will be zeroed instead)
     */
    void clear_date_offset();

    /**
     * @brief Generate a random date offset
     *
     * Generates a random offset within the specified range.
     *
     * @param min_days Minimum offset (days)
     * @param max_days Maximum offset (days)
     * @return The generated offset
     */
    [[nodiscard]] static auto generate_random_date_offset(
        std::chrono::days min_days = std::chrono::days{-365},
        std::chrono::days max_days = std::chrono::days{365}
    ) -> std::chrono::days;

    // ========================================================================
    // Encryption Configuration
    // ========================================================================

    /**
     * @brief Set encryption key for encrypt actions
     * @param key The encryption key (must be 32 bytes for AES-256)
     * @return Result indicating success or error
     */
    [[nodiscard]] auto set_encryption_key(std::span<const std::uint8_t> key)
        -> kcenon::common::VoidResult;

    /**
     * @brief Check if encryption is configured
     * @return true if an encryption key is set
     */
    [[nodiscard]] auto has_encryption_key() const noexcept -> bool;

    // ========================================================================
    // Hash Configuration
    // ========================================================================

    /**
     * @brief Set salt for hash operations
     *
     * The salt is combined with values before hashing to prevent
     * rainbow table attacks.
     *
     * @param salt The salt value
     */
    void set_hash_salt(std::string salt);

    /**
     * @brief Get the current hash salt
     * @return Optional containing the salt, or nullopt if not set
     */
    [[nodiscard]] auto get_hash_salt() const
        -> std::optional<std::string>;

    // ========================================================================
    // Audit and Reporting
    // ========================================================================

    /**
     * @brief Enable detailed action recording
     *
     * When enabled, the anonymization report will include detailed
     * records of each action performed.
     *
     * @param enable true to enable detailed recording
     */
    void set_detailed_reporting(bool enable);

    /**
     * @brief Check if detailed reporting is enabled
     * @return true if detailed reporting is enabled
     */
    [[nodiscard]] auto is_detailed_reporting() const noexcept -> bool;

    // ========================================================================
    // Static Helpers
    // ========================================================================

    /**
     * @brief Get tags to process for a given profile
     * @param profile The anonymization profile
     * @return Map of tags to their default actions
     */
    [[nodiscard]] static auto get_profile_actions(anonymization_profile profile)
        -> std::map<core::dicom_tag, tag_action_config>;

    /**
     * @brief Get a list of HIPAA Safe Harbor identifier tags
     * @return Vector of tags that are HIPAA identifiers
     */
    [[nodiscard]] static auto get_hipaa_identifier_tags()
        -> std::vector<core::dicom_tag>;

    /**
     * @brief Get a list of GDPR personal data tags
     * @return Vector of tags containing personal data per GDPR
     */
    [[nodiscard]] static auto get_gdpr_personal_data_tags()
        -> std::vector<core::dicom_tag>;

private:
    /// Current anonymization profile
    anonymization_profile profile_;

    /// Custom tag actions (override profile defaults)
    std::map<core::dicom_tag, tag_action_config> custom_actions_;

    /// Date offset for shifting
    std::optional<std::chrono::days> date_offset_;

    /// Encryption key (if set)
    std::vector<std::uint8_t> encryption_key_;

    /// Hash salt
    std::optional<std::string> hash_salt_;

    /// Whether to include detailed action records in report
    bool detailed_reporting_{false};

    // Internal helpers
    [[nodiscard]] auto apply_action(
        core::dicom_dataset& dataset,
        core::dicom_tag tag,
        const tag_action_config& config,
        uid_mapping* mapping
    ) -> tag_action_record;

    [[nodiscard]] auto shift_date(std::string_view date_string) const
        -> std::string;

    [[nodiscard]] auto hash_value(std::string_view value) const
        -> std::string;

    [[nodiscard]] auto encrypt_value(std::string_view value) const
        -> kcenon::common::Result<std::string>;

    void initialize_profile_actions();
};

} // namespace pacs::security
