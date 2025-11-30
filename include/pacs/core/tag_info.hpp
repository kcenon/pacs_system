/**
 * @file tag_info.hpp
 * @brief DICOM tag metadata information structure
 *
 * This file defines the tag_info structure which contains metadata about
 * DICOM tags as specified in DICOM PS3.6 Data Dictionary. Each entry
 * includes the tag, VR, value multiplicity, keyword, name, and retirement status.
 *
 * @see DICOM PS3.6 - Data Dictionary
 */

#pragma once

#include "dicom_tag.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pacs::core {

/**
 * @brief Value Multiplicity (VM) specification
 *
 * Represents the allowed number of values for a DICOM data element.
 * VM is specified as a range [min, max] where max can be unbounded.
 *
 * Common examples:
 * - VM "1" -> min=1, max=1
 * - VM "1-2" -> min=1, max=2
 * - VM "1-n" -> min=1, max=nullopt (unbounded)
 * - VM "2-2n" -> min=2, max=nullopt with multiplier=2
 */
struct value_multiplicity {
    uint32_t min{1};                     ///< Minimum number of values
    std::optional<uint32_t> max{1};      ///< Maximum number of values (nullopt = unbounded)
    uint32_t multiplier{1};              ///< For "n", "2n", "3n" patterns

    /**
     * @brief Default constructor - creates VM "1"
     */
    constexpr value_multiplicity() noexcept = default;

    /**
     * @brief Construct with min and max values
     * @param min_val Minimum number of values
     * @param max_val Maximum number of values (nullopt for unbounded)
     * @param mult Multiplier for "n" patterns (default 1)
     */
    constexpr value_multiplicity(uint32_t min_val,
                                  std::optional<uint32_t> max_val,
                                  uint32_t mult = 1) noexcept
        : min{min_val}, max{max_val}, multiplier{mult} {}

    /**
     * @brief Check if a count of values is valid for this VM
     * @param count The number of values to validate
     * @return true if the count satisfies the VM constraint
     */
    [[nodiscard]] constexpr auto is_valid(uint32_t count) const noexcept -> bool {
        if (count < min) {
            return false;
        }
        if (max.has_value()) {
            return count <= max.value();
        }
        // Unbounded case - check multiplier constraint
        if (multiplier > 1) {
            return (count - min) % multiplier == 0;
        }
        return true;
    }

    /**
     * @brief Check if this VM allows multiple values
     * @return true if max > 1 or unbounded
     */
    [[nodiscard]] constexpr auto allows_multiple() const noexcept -> bool {
        return !max.has_value() || max.value() > 1;
    }

    /**
     * @brief Check if this VM is unbounded (ends with "n")
     * @return true if max is not specified
     */
    [[nodiscard]] constexpr auto is_unbounded() const noexcept -> bool {
        return !max.has_value();
    }

    /**
     * @brief Parse VM from string representation
     *
     * Supported formats:
     * - "1" -> min=1, max=1
     * - "1-2" -> min=1, max=2
     * - "1-n" -> min=1, max=unbounded
     * - "2-2n" -> min=2, max=unbounded, multiplier=2
     *
     * @param str String representation of VM
     * @return Parsed value_multiplicity or nullopt if parsing failed
     */
    [[nodiscard]] static auto from_string(std::string_view str)
        -> std::optional<value_multiplicity>;

    /**
     * @brief Convert to string representation
     * @return String in format "min", "min-max", or "min-n"
     */
    [[nodiscard]] auto to_string() const -> std::string;
};

/**
 * @brief DICOM tag metadata information
 *
 * Contains complete metadata about a DICOM tag as defined in PS3.6.
 * This includes the tag itself, its VR, VM, keyword, descriptive name,
 * and whether the tag has been retired from the standard.
 *
 * @example
 * @code
 * tag_info info{
 *     dicom_tag{0x0010, 0x0010},
 *     vr_type::PN,
 *     value_multiplicity{1, 1},
 *     "PatientName",
 *     "Patient's Name",
 *     false
 * };
 * @endcode
 */
struct tag_info {
    dicom_tag tag;                       ///< The DICOM tag (group, element)
    uint16_t vr{0};                      ///< VR type as uint16_t (encoding::vr_type value)
    value_multiplicity vm;               ///< Value Multiplicity specification
    std::string_view keyword;            ///< Tag keyword (e.g., "PatientName")
    std::string_view name;               ///< Human-readable name (e.g., "Patient's Name")
    bool retired{false};                 ///< Whether this tag is retired

    /**
     * @brief Check if this tag_info is valid (has non-empty keyword)
     * @return true if the tag info has been properly initialized
     */
    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {
        return !keyword.empty();
    }

    /**
     * @brief Compare tag_info by tag value
     */
    [[nodiscard]] constexpr auto operator==(const tag_info& other) const noexcept -> bool {
        return tag == other.tag;
    }

    /**
     * @brief Three-way comparison by tag value
     */
    [[nodiscard]] constexpr auto operator<=>(const tag_info& other) const noexcept {
        return tag <=> other.tag;
    }
};

}  // namespace pacs::core
