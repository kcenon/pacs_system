/**
 * @file dicom_dictionary.hpp
 * @brief DICOM Data Dictionary for tag metadata lookup
 *
 * This file defines the dicom_dictionary class which provides O(1) lookup
 * for DICOM tag metadata as specified in DICOM PS3.6. The dictionary is
 * implemented as a thread-safe singleton with support for private tag
 * registration at runtime.
 *
 * @see DICOM PS3.6 - Data Dictionary
 */

#pragma once

#include "dicom_tag.hpp"
#include "tag_info.hpp"

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pacs::core {

/**
 * @brief DICOM Data Dictionary singleton class
 *
 * Provides O(1) lookup for DICOM tag metadata including VR, VM, keyword,
 * and name. The dictionary is initialized with all standard tags from
 * PS3.6 and supports runtime registration of private tags.
 *
 * Thread Safety:
 * - Read operations (lookup) are thread-safe and can be concurrent
 * - Write operations (register_private_tag) are serialized
 *
 * @example
 * @code
 * auto& dict = dicom_dictionary::instance();
 *
 * // Lookup by tag
 * if (auto info = dict.find(dicom_tag{0x0010, 0x0010})) {
 *     std::cout << info->name;  // "Patient's Name"
 * }
 *
 * // Lookup by keyword
 * if (auto info = dict.find_by_keyword("PatientName")) {
 *     std::cout << info->tag.to_string();  // "(0010,0010)"
 * }
 *
 * // Validate VM
 * if (dict.validate_vm(dicom_tag{0x0008, 0x0060}, 1)) {
 *     // Value count is valid for Modality tag
 * }
 * @endcode
 */
class dicom_dictionary {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global dicom_dictionary instance
     *
     * Thread-safe initialization using C++11 static initialization guarantee.
     */
    [[nodiscard]] static auto instance() -> dicom_dictionary&;

    // Non-copyable and non-movable (singleton)
    dicom_dictionary(const dicom_dictionary&) = delete;
    dicom_dictionary(dicom_dictionary&&) = delete;
    auto operator=(const dicom_dictionary&) -> dicom_dictionary& = delete;
    auto operator=(dicom_dictionary&&) -> dicom_dictionary& = delete;

    /**
     * @brief Find tag metadata by DICOM tag
     * @param tag The DICOM tag to look up
     * @return Optional containing tag_info if found, nullopt otherwise
     *
     * O(1) average time complexity using hash table lookup.
     * Thread-safe for concurrent reads.
     */
    [[nodiscard]] auto find(dicom_tag tag) const -> std::optional<tag_info>;

    /**
     * @brief Find tag metadata by keyword
     * @param keyword The tag keyword (e.g., "PatientName")
     * @return Optional containing tag_info if found, nullopt otherwise
     *
     * O(1) average time complexity using hash table lookup.
     * Thread-safe for concurrent reads.
     */
    [[nodiscard]] auto find_by_keyword(std::string_view keyword) const
        -> std::optional<tag_info>;

    /**
     * @brief Check if a tag exists in the dictionary
     * @param tag The DICOM tag to check
     * @return true if the tag is in the dictionary
     */
    [[nodiscard]] auto contains(dicom_tag tag) const -> bool;

    /**
     * @brief Check if a keyword exists in the dictionary
     * @param keyword The keyword to check
     * @return true if the keyword is in the dictionary
     */
    [[nodiscard]] auto contains_keyword(std::string_view keyword) const -> bool;

    /**
     * @brief Validate that a value count is valid for a tag's VM
     * @param tag The DICOM tag to validate against
     * @param count The number of values to validate
     * @return true if count satisfies the tag's VM, false if tag not found or invalid
     */
    [[nodiscard]] auto validate_vm(dicom_tag tag, uint32_t count) const -> bool;

    /**
     * @brief Get the VR type for a tag
     * @param tag The DICOM tag
     * @return The VR type as uint16_t, or 0 if tag not found
     */
    [[nodiscard]] auto get_vr(dicom_tag tag) const -> uint16_t;

    /**
     * @brief Register a private tag at runtime
     * @param info The tag_info for the private tag
     * @return true if registration succeeded, false if tag already exists
     *
     * Only private tags (odd group numbers > 0x0008) can be registered.
     * Thread-safe, serializes write operations.
     */
    auto register_private_tag(const tag_info& info) -> bool;

    /**
     * @brief Get the total number of tags in the dictionary
     * @return The count of registered tags (standard + private)
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Get the number of standard (non-private) tags
     * @return The count of standard PS3.6 tags
     */
    [[nodiscard]] auto standard_tag_count() const -> size_t;

    /**
     * @brief Get the number of registered private tags
     * @return The count of runtime-registered private tags
     */
    [[nodiscard]] auto private_tag_count() const -> size_t;

    /**
     * @brief Get all tags in a specific group
     * @param group The group number to filter by
     * @return Vector of tag_info for all tags in the group
     */
    [[nodiscard]] auto get_tags_in_group(uint16_t group) const
        -> std::vector<tag_info>;

private:
    /**
     * @brief Private constructor - initializes with standard tags
     */
    dicom_dictionary();

    /**
     * @brief Initialize the dictionary with standard PS3.6 tags
     */
    void initialize_standard_tags();

    /// Hash map for O(1) lookup by tag
    std::unordered_map<dicom_tag, tag_info> tag_map_;

    /// Hash map for O(1) lookup by keyword
    std::unordered_map<std::string_view, dicom_tag> keyword_map_;

    /// Count of standard tags (vs private)
    size_t standard_count_{0};

    /// Mutex for thread-safe private tag registration
    mutable std::shared_mutex mutex_;
};

}  // namespace pacs::core
