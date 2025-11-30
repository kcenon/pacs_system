/**
 * @file dicom_tag.hpp
 * @brief DICOM Tag representation (Group, Element pairs)
 *
 * This file defines the dicom_tag class which represents DICOM tags
 * as specified in DICOM PS3.5. Tags are composed of a Group Number
 * and an Element Number, each being 16-bit unsigned integers.
 *
 * @see DICOM PS3.5 Section 7.1 - Data Elements
 */

#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace pacs::core {

/**
 * @brief Represents a DICOM tag (Group, Element pair)
 *
 * A DICOM tag uniquely identifies a data element within a DICOM dataset.
 * It consists of a 16-bit Group Number and a 16-bit Element Number.
 *
 * Memory layout: Stored as a single uint32_t for optimal memory usage
 * and comparison performance: (group << 16) | element
 *
 * @example
 * @code
 * // Create Patient Name tag
 * dicom_tag tag{0x0010, 0x0010};
 *
 * // Parse from string
 * auto parsed = dicom_tag::from_string("(0010,0020)");
 *
 * // Use in unordered containers
 * std::unordered_map<dicom_tag, std::string> elements;
 * @endcode
 */
class dicom_tag {
public:
    /**
     * @brief Default constructor - creates tag (0000,0000)
     */
    constexpr dicom_tag() noexcept : combined_{0} {}

    /**
     * @brief Construct from group and element numbers
     * @param group The group number (0x0000 - 0xFFFF)
     * @param element The element number (0x0000 - 0xFFFF)
     */
    constexpr dicom_tag(uint16_t group, uint16_t element) noexcept
        : combined_{static_cast<uint32_t>(group) << 16 | element} {}

    /**
     * @brief Construct from combined 32-bit value
     * @param combined The combined value (group << 16 | element)
     */
    explicit constexpr dicom_tag(uint32_t combined) noexcept
        : combined_{combined} {}

    /**
     * @brief Parse tag from string representation
     *
     * Supported formats:
     * - "(GGGG,EEEE)" - Standard DICOM format with parentheses
     * - "GGGGEEEE" - Compact hexadecimal format
     *
     * @param str String representation of the tag
     * @return Optional containing the parsed tag, or nullopt if parsing failed
     */
    [[nodiscard]] static auto from_string(std::string_view str)
        -> std::optional<dicom_tag>;

    /**
     * @brief Get the group number
     * @return The 16-bit group number
     */
    [[nodiscard]] constexpr auto group() const noexcept -> uint16_t {
        return static_cast<uint16_t>(combined_ >> 16);
    }

    /**
     * @brief Get the element number
     * @return The 16-bit element number
     */
    [[nodiscard]] constexpr auto element() const noexcept -> uint16_t {
        return static_cast<uint16_t>(combined_ & 0xFFFF);
    }

    /**
     * @brief Get the combined 32-bit value
     * @return The combined value (group << 16 | element)
     */
    [[nodiscard]] constexpr auto combined() const noexcept -> uint32_t {
        return combined_;
    }

    /**
     * @brief Check if this is a private tag
     *
     * Private tags have odd group numbers (except group 0x0001).
     * Groups with odd numbers in the range (0x0009, 0x00FF) and
     * above 0x0007 are reserved for private use.
     *
     * @return true if this is a private tag, false otherwise
     */
    [[nodiscard]] constexpr auto is_private() const noexcept -> bool {
        const auto grp = group();
        return (grp & 1) != 0 && grp > 0x0008;
    }

    /**
     * @brief Check if this is a group length tag (xxxx,0000)
     * @return true if element number is 0x0000
     */
    [[nodiscard]] constexpr auto is_group_length() const noexcept -> bool {
        return element() == 0x0000;
    }

    /**
     * @brief Check if this is a private creator tag
     *
     * Private creator tags are in the range (gggg,0010)-(gggg,00FF)
     * where gggg is an odd group number.
     *
     * @return true if this is a private creator tag
     */
    [[nodiscard]] constexpr auto is_private_creator() const noexcept -> bool {
        if (!is_private()) {
            return false;
        }
        const auto elem = element();
        return elem >= 0x0010 && elem <= 0x00FF;
    }

    /**
     * @brief Convert to string representation
     * @return String in format "(GGGG,EEEE)"
     */
    [[nodiscard]] auto to_string() const -> std::string;

    /**
     * @brief Three-way comparison operator
     *
     * Tags are compared by their combined 32-bit value, which means
     * they are sorted first by group, then by element.
     */
    [[nodiscard]] constexpr auto operator<=>(const dicom_tag& other) const noexcept
        -> std::strong_ordering = default;

    /**
     * @brief Equality comparison
     */
    [[nodiscard]] constexpr auto operator==(const dicom_tag& other) const noexcept
        -> bool = default;

private:
    uint32_t combined_;
};

}  // namespace pacs::core

// Hash specialization for use in unordered containers
template <>
struct std::hash<pacs::core::dicom_tag> {
    [[nodiscard]] auto operator()(const pacs::core::dicom_tag& tag) const noexcept
        -> size_t {
        return std::hash<uint32_t>{}(tag.combined());
    }
};
