/**
 * @file dicom_element.hpp
 * @brief DICOM Data Element representation (Tag, VR, Value)
 *
 * This file defines the dicom_element class which represents a DICOM
 * Data Element as specified in DICOM PS3.5. Each element consists of
 * a Tag, Value Representation (VR), and Value.
 *
 * @see DICOM PS3.5 Section 7.1 - Data Elements
 */

#pragma once

#include "dicom_tag.hpp"
#include "result.hpp"

#include <pacs/encoding/vr_type.hpp>

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace pacs::core {

// Forward declaration to break circular dependency
class dicom_dataset;

/**
 * @brief Exception thrown when value conversion fails
 * @deprecated Use Result<T> pattern instead. Error codes are in pacs::error_codes.
 */
class [[deprecated("Use Result<T> pattern instead")]] value_conversion_error
    : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Represents a DICOM Data Element (Tag, VR, Value)
 *
 * A DICOM element is the fundamental unit of data in DICOM. It consists of:
 * - A Tag identifying the attribute (group, element pair)
 * - A Value Representation (VR) describing the data type
 * - A Value containing the actual data
 *
 * This class supports:
 * - String values with automatic padding/trimming per DICOM rules
 * - Numeric values with proper endianness handling
 * - Binary data (OB, OW, etc.)
 * - Sequences (SQ) containing nested datasets
 * - Value Multiplicity (VM > 1) with backslash-separated values
 *
 * @example
 * @code
 * // Create from string
 * auto name = dicom_element::from_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
 *
 * // Create from numeric value
 * auto rows = dicom_element::from_numeric<uint16_t>(tags::rows, vr_type::US, 512);
 *
 * // Access values
 * std::string patient = name.as_string();
 * uint16_t height = rows.as_numeric<uint16_t>();
 * @endcode
 */
class dicom_element {
public:
    /**
     * @brief Construct an empty element with given tag and VR
     * @param tag The DICOM tag
     * @param vr The value representation
     */
    dicom_element(dicom_tag tag, encoding::vr_type vr) noexcept;

    /**
     * @brief Construct an element with raw data
     * @param tag The DICOM tag
     * @param vr The value representation
     * @param data The raw byte data
     */
    dicom_element(dicom_tag tag, encoding::vr_type vr,
                  std::span<const uint8_t> data);

    /**
     * @brief Copy constructor
     */
    dicom_element(const dicom_element&);

    /**
     * @brief Move constructor
     */
    dicom_element(dicom_element&&) noexcept;

    /**
     * @brief Copy assignment
     */
    auto operator=(const dicom_element&) -> dicom_element&;

    /**
     * @brief Move assignment
     */
    auto operator=(dicom_element&&) noexcept -> dicom_element&;

    /**
     * @brief Destructor
     */
    ~dicom_element();

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create an element from a string value
     * @param tag The DICOM tag
     * @param vr The value representation (should be a string VR)
     * @param value The string value
     * @return A new dicom_element containing the string
     */
    [[nodiscard]] static auto from_string(dicom_tag tag, encoding::vr_type vr,
                                          std::string_view value) -> dicom_element;

    /**
     * @brief Create an element from a numeric value
     * @tparam T The numeric type (uint16_t, int32_t, float, double, etc.)
     * @param tag The DICOM tag
     * @param vr The value representation (should be a numeric VR)
     * @param value The numeric value
     * @return A new dicom_element containing the value
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] static auto from_numeric(dicom_tag tag, encoding::vr_type vr,
                                           T value) -> dicom_element;

    /**
     * @brief Create an element from multiple numeric values
     * @tparam T The numeric type
     * @param tag The DICOM tag
     * @param vr The value representation
     * @param values The numeric values
     * @return A new dicom_element containing all values
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] static auto from_numeric_list(dicom_tag tag, encoding::vr_type vr,
                                                std::span<const T> values) -> dicom_element;

    // ========================================================================
    // Accessors
    // ========================================================================

    /**
     * @brief Get the element's tag
     * @return The DICOM tag
     */
    [[nodiscard]] constexpr auto tag() const noexcept -> dicom_tag { return tag_; }

    /**
     * @brief Get the element's VR
     * @return The value representation
     */
    [[nodiscard]] constexpr auto vr() const noexcept -> encoding::vr_type { return vr_; }

    /**
     * @brief Get the value length in bytes
     * @return The number of bytes in the value
     */
    [[nodiscard]] auto length() const noexcept -> uint32_t {
        return static_cast<uint32_t>(data_.size());
    }

    /**
     * @brief Get the raw data bytes
     * @return A span view of the raw data
     */
    [[nodiscard]] auto raw_data() const noexcept -> std::span<const uint8_t> {
        return data_;
    }

    /**
     * @brief Check if the element has no value
     * @return true if the value is empty
     */
    [[nodiscard]] auto is_empty() const noexcept -> bool { return data_.empty(); }

    // ========================================================================
    // String Value Access
    // ========================================================================

    /**
     * @brief Get the value as a string
     *
     * For string VRs, returns the value with trailing padding removed.
     * For numeric VRs, converts the value to a string representation.
     *
     * @return Result containing the string value, or error if conversion fails
     */
    [[nodiscard]] auto as_string() const -> pacs::Result<std::string>;

    /**
     * @brief Get multi-valued string as a list
     *
     * Splits the value by backslash delimiter (DICOM VM > 1).
     *
     * @return Result containing vector of individual string values, or error
     */
    [[nodiscard]] auto as_string_list() const
        -> pacs::Result<std::vector<std::string>>;

    // ========================================================================
    // Numeric Value Access
    // ========================================================================

    /**
     * @brief Get the value as a numeric type
     * @tparam T The target numeric type
     * @return Result containing the numeric value, or error if conversion fails
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] auto as_numeric() const -> pacs::Result<T>;

    /**
     * @brief Get multi-valued numeric data as a list
     * @tparam T The target numeric type
     * @return Result containing vector of numeric values, or error if conversion fails
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] auto as_numeric_list() const -> pacs::Result<std::vector<T>>;

    // ========================================================================
    // Sequence Access
    // ========================================================================

    /**
     * @brief Check if this element is a sequence
     * @return true if VR is SQ
     */
    [[nodiscard]] auto is_sequence() const noexcept -> bool {
        return vr_ == encoding::vr_type::SQ;
    }

    /**
     * @brief Get mutable access to sequence items
     * @return Reference to the sequence items vector
     * @note Only valid if is_sequence() returns true
     */
    [[nodiscard]] auto sequence_items() -> std::vector<dicom_dataset>&;

    /**
     * @brief Get read-only access to sequence items
     * @return Const reference to the sequence items vector
     * @note Only valid if is_sequence() returns true
     */
    [[nodiscard]] auto sequence_items() const -> const std::vector<dicom_dataset>&;

    // ========================================================================
    // Modification
    // ========================================================================

    /**
     * @brief Set the raw value data
     * @param data The new raw data
     */
    void set_value(std::span<const uint8_t> data);

    /**
     * @brief Set the value from a string
     * @param value The string value (will be padded if needed)
     */
    void set_string(std::string_view value);

    /**
     * @brief Set the value from a numeric value
     * @tparam T The numeric type
     * @param value The numeric value
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    void set_numeric(T value);

private:
    dicom_tag tag_;
    encoding::vr_type vr_;
    std::vector<uint8_t> data_;
    std::vector<dicom_dataset> sequence_items_;  // Used only when VR == SQ

    /**
     * @brief Apply DICOM padding to ensure even length
     * @param str The input string
     * @return Padded string with even length
     */
    [[nodiscard]] auto apply_padding(std::string_view str) const -> std::string;

    /**
     * @brief Remove DICOM padding from a string value
     * @param str The padded string
     * @return String with padding removed
     */
    [[nodiscard]] static auto remove_padding(std::string_view str,
                                             encoding::vr_type vr) -> std::string;
};

// ============================================================================
// Template Implementations
// ============================================================================

template <typename T>
    requires std::is_arithmetic_v<T>
auto dicom_element::from_numeric(dicom_tag tag, encoding::vr_type vr,
                                 T value) -> dicom_element {
    dicom_element elem{tag, vr};
    elem.set_numeric(value);
    return elem;
}

template <typename T>
    requires std::is_arithmetic_v<T>
auto dicom_element::from_numeric_list(dicom_tag tag, encoding::vr_type vr,
                                      std::span<const T> values) -> dicom_element {
    dicom_element elem{tag, vr};
    std::vector<uint8_t> data(values.size() * sizeof(T));

    for (size_t i = 0; i < values.size(); ++i) {
        std::memcpy(data.data() + i * sizeof(T), &values[i], sizeof(T));
    }

    elem.set_value(data);
    return elem;
}

template <typename T>
    requires std::is_arithmetic_v<T>
auto dicom_element::as_numeric() const -> pacs::Result<T> {
    if (data_.size() < sizeof(T)) {
        return pacs::pacs_error<T>(
            pacs::error_codes::data_size_mismatch,
            "Insufficient data for numeric conversion: expected " +
                std::to_string(sizeof(T)) + " bytes, got " +
                std::to_string(data_.size()));
    }

    T result{};
    std::memcpy(&result, data_.data(), sizeof(T));
    return pacs::ok(result);
}

template <typename T>
    requires std::is_arithmetic_v<T>
auto dicom_element::as_numeric_list() const -> pacs::Result<std::vector<T>> {
    if (data_.size() % sizeof(T) != 0) {
        return pacs::pacs_error<std::vector<T>>(
            pacs::error_codes::data_size_mismatch,
            "Data size not aligned for numeric type: " +
                std::to_string(data_.size()) + " bytes is not divisible by " +
                std::to_string(sizeof(T)));
    }

    const size_t count = data_.size() / sizeof(T);
    std::vector<T> result(count);

    for (size_t i = 0; i < count; ++i) {
        std::memcpy(&result[i], data_.data() + i * sizeof(T), sizeof(T));
    }

    return pacs::ok(result);
}

template <typename T>
    requires std::is_arithmetic_v<T>
void dicom_element::set_numeric(T value) {
    data_.resize(sizeof(T));
    std::memcpy(data_.data(), &value, sizeof(T));
}

}  // namespace pacs::core
