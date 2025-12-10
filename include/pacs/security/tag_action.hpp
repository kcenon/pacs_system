/**
 * @file tag_action.hpp
 * @brief Tag action definitions for DICOM de-identification
 *
 * This file defines actions to be performed on DICOM attributes during
 * de-identification as specified in DICOM PS3.15 Annex E.
 *
 * @see DICOM PS3.15 Annex E - Attribute Confidentiality Profiles
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <pacs/core/dicom_tag.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pacs::security {

/**
 * @brief Actions to perform on DICOM attributes during de-identification
 *
 * These actions correspond to the action codes defined in
 * DICOM PS3.15 Annex E Table E.1-1.
 */
enum class tag_action : std::uint8_t {
    /**
     * @brief D - Remove the attribute entirely
     *
     * The attribute is completely removed from the dataset.
     */
    remove = 0,

    /**
     * @brief Z - Replace with zero-length value
     *
     * The attribute is retained but its value is replaced with
     * a zero-length (empty) value.
     */
    empty = 1,

    /**
     * @brief X - Remove or empty based on presence
     *
     * If the attribute exists, replace with zero-length value.
     * Equivalent to empty for present attributes.
     */
    remove_or_empty = 2,

    /**
     * @brief K - Keep the attribute unchanged
     *
     * The attribute and its value are retained as-is.
     */
    keep = 3,

    /**
     * @brief C - Clean (replace with dummy value)
     *
     * The attribute value is replaced with a configurable
     * dummy value (e.g., "ANONYMOUS" for names).
     */
    replace = 4,

    /**
     * @brief U - Replace UIDs with new values
     *
     * UID values are replaced with newly generated UIDs.
     * Use UID mapping for consistent replacement across datasets.
     */
    replace_uid = 5,

    /**
     * @brief Hash the value for research linkage
     *
     * The value is replaced with a cryptographic hash,
     * allowing de-identified datasets to be linked without
     * revealing the original value.
     */
    hash = 6,

    /**
     * @brief Encrypt the value
     *
     * The value is encrypted and can be decrypted with
     * the appropriate key. Used for pseudonymization.
     */
    encrypt = 7,

    /**
     * @brief Shift dates by a fixed offset
     *
     * Date and time values are shifted by a consistent
     * offset while preserving temporal relationships.
     */
    shift_date = 8
};

/**
 * @brief Convert tag action enum to string representation
 * @param action The tag action
 * @return String name of the action
 */
[[nodiscard]] constexpr auto to_string(tag_action action) noexcept
    -> std::string_view {
    switch (action) {
        case tag_action::remove:
            return "remove";
        case tag_action::empty:
            return "empty";
        case tag_action::remove_or_empty:
            return "remove_or_empty";
        case tag_action::keep:
            return "keep";
        case tag_action::replace:
            return "replace";
        case tag_action::replace_uid:
            return "replace_uid";
        case tag_action::hash:
            return "hash";
        case tag_action::encrypt:
            return "encrypt";
        case tag_action::shift_date:
            return "shift_date";
    }
    return "unknown";
}

/**
 * @brief Configuration for a custom tag action
 *
 * Allows specifying action-specific parameters such as
 * replacement values or hash algorithms.
 */
struct tag_action_config {
    /// The action to perform
    tag_action action{tag_action::remove};

    /// Replacement value (for replace action)
    std::string replacement_value;

    /// Hash algorithm (for hash action): "SHA256", "SHA512"
    std::string hash_algorithm{"SHA256"};

    /// Whether to include salt in hash
    bool use_salt{true};

    /**
     * @brief Create a remove action config
     */
    [[nodiscard]] static auto make_remove() -> tag_action_config {
        return {.action = tag_action::remove};
    }

    /**
     * @brief Create an empty action config
     */
    [[nodiscard]] static auto make_empty() -> tag_action_config {
        return {.action = tag_action::empty};
    }

    /**
     * @brief Create a keep action config
     */
    [[nodiscard]] static auto make_keep() -> tag_action_config {
        return {.action = tag_action::keep};
    }

    /**
     * @brief Create a replace action config with a custom value
     * @param value The replacement value
     */
    [[nodiscard]] static auto make_replace(std::string value) -> tag_action_config {
        return {.action = tag_action::replace, .replacement_value = std::move(value)};
    }

    /**
     * @brief Create a hash action config
     * @param algorithm Hash algorithm to use
     * @param salt Whether to use salt
     */
    [[nodiscard]] static auto make_hash(
        std::string algorithm = "SHA256",
        bool salt = true
    ) -> tag_action_config {
        return {
            .action = tag_action::hash,
            .hash_algorithm = std::move(algorithm),
            .use_salt = salt
        };
    }
};

/**
 * @brief Record of an action performed on a tag
 */
struct tag_action_record {
    /// The tag that was processed
    core::dicom_tag tag;

    /// The action that was performed
    tag_action action{tag_action::remove};

    /// Original value (if retained for reporting)
    std::string original_value;

    /// New value (if applicable)
    std::string new_value;

    /// Whether the action was successful
    bool success{true};

    /// Error message if action failed
    std::string error_message;
};

/**
 * @brief Report generated after anonymization
 */
struct anonymization_report {
    /// Total number of tags processed
    std::size_t total_tags_processed{0};

    /// Number of tags removed
    std::size_t tags_removed{0};

    /// Number of tags emptied
    std::size_t tags_emptied{0};

    /// Number of tags replaced
    std::size_t tags_replaced{0};

    /// Number of UIDs replaced
    std::size_t uids_replaced{0};

    /// Number of tags kept unchanged
    std::size_t tags_kept{0};

    /// Number of dates shifted
    std::size_t dates_shifted{0};

    /// Number of values hashed
    std::size_t values_hashed{0};

    /// Detailed action records (optional, for audit)
    std::vector<tag_action_record> action_records;

    /// Profile used for anonymization
    std::string profile_name;

    /// Date offset applied (if any)
    std::optional<std::chrono::days> date_offset;

    /// Timestamp of anonymization
    std::chrono::system_clock::time_point timestamp;

    /// Any warnings generated during anonymization
    std::vector<std::string> warnings;

    /// Any errors encountered (non-fatal)
    std::vector<std::string> errors;

    /**
     * @brief Check if anonymization completed without errors
     */
    [[nodiscard]] auto is_successful() const noexcept -> bool {
        return errors.empty();
    }

    /**
     * @brief Get total number of modifications made
     */
    [[nodiscard]] auto total_modifications() const noexcept -> std::size_t {
        return tags_removed + tags_emptied + tags_replaced +
               uids_replaced + dates_shifted + values_hashed;
    }
};

/**
 * @brief HIPAA Safe Harbor identifiers (18 categories)
 *
 * These correspond to the 18 categories of identifiers specified
 * in 45 CFR 164.514(b)(2) for HIPAA Safe Harbor de-identification.
 */
namespace hipaa_identifiers {
    /// Tags containing names
    [[nodiscard]] auto get_name_tags() -> std::vector<core::dicom_tag>;

    /// Tags containing geographic identifiers
    [[nodiscard]] auto get_geographic_tags() -> std::vector<core::dicom_tag>;

    /// Tags containing dates (except year)
    [[nodiscard]] auto get_date_tags() -> std::vector<core::dicom_tag>;

    /// Tags containing communication identifiers
    [[nodiscard]] auto get_communication_tags() -> std::vector<core::dicom_tag>;

    /// Tags containing unique identifiers
    [[nodiscard]] auto get_unique_id_tags() -> std::vector<core::dicom_tag>;

    /// Get all HIPAA identifier tags
    [[nodiscard]] auto get_all_identifier_tags() -> std::vector<core::dicom_tag>;
} // namespace hipaa_identifiers

} // namespace pacs::security
