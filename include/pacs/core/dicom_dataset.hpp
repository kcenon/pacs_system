/**
 * @file dicom_dataset.hpp
 * @brief DICOM Dataset - ordered collection of Data Elements
 *
 * This file provides the dicom_dataset class which represents an ordered
 * collection of DICOM Data Elements as specified in DICOM PS3.5.
 *
 * @see DICOM PS3.5 Section 7.1 - Data Set
 */

#pragma once

#include "dicom_element.hpp"
#include "dicom_tag.hpp"

#include <pacs/encoding/vr_type.hpp>

#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace pacs::core {

/**
 * @brief Represents a DICOM Dataset (ordered collection of Data Elements)
 *
 * A DICOM Dataset is an ordered collection of Data Elements, where each
 * element is uniquely identified by its tag. Elements are stored in
 * ascending tag order as required by the DICOM standard.
 *
 * Thread Safety: This class is NOT thread-safe. External synchronization
 * is required for concurrent access.
 *
 * @example
 * @code
 * dicom_dataset ds;
 *
 * // Set string values
 * ds.set_string(tags::patient_name, encoding::vr_type::PN, "DOE^JOHN");
 * ds.set_string(tags::patient_id, encoding::vr_type::LO, "12345");
 *
 * // Set numeric values
 * ds.set_numeric<uint16_t>(tags::rows, encoding::vr_type::US, 512);
 *
 * // Access values
 * std::string name = ds.get_string(tags::patient_name);
 * uint16_t rows = ds.get_numeric<uint16_t>(tags::rows).value_or(0);
 *
 * // Iterate over elements
 * for (const auto& [tag, element] : ds) {
 *     std::cout << tag.to_string() << ": " << element.as_string() << "\n";
 * }
 * @endcode
 */
class dicom_dataset {
public:
    /// Storage type for elements (ordered by tag)
    using storage_type = std::map<dicom_tag, dicom_element>;

    /// Iterator type
    using iterator = storage_type::iterator;

    /// Const iterator type
    using const_iterator = storage_type::const_iterator;

    // ========================================================================
    // Construction
    // ========================================================================

    /**
     * @brief Default constructor - creates an empty dataset
     */
    dicom_dataset() = default;

    /**
     * @brief Copy constructor
     */
    dicom_dataset(const dicom_dataset&) = default;

    /**
     * @brief Move constructor
     */
    dicom_dataset(dicom_dataset&&) noexcept = default;

    /**
     * @brief Copy assignment
     */
    auto operator=(const dicom_dataset&) -> dicom_dataset& = default;

    /**
     * @brief Move assignment
     */
    auto operator=(dicom_dataset&&) noexcept -> dicom_dataset& = default;

    /**
     * @brief Default destructor
     */
    ~dicom_dataset() = default;

    // ========================================================================
    // Element Access
    // ========================================================================

    /**
     * @brief Check if the dataset contains an element with the given tag
     * @param tag The DICOM tag to check
     * @return true if an element with this tag exists
     */
    [[nodiscard]] auto contains(dicom_tag tag) const noexcept -> bool;

    /**
     * @brief Get a pointer to the element with the given tag
     * @param tag The DICOM tag to look up
     * @return Pointer to the element, or nullptr if not found
     */
    [[nodiscard]] auto get(dicom_tag tag) noexcept -> dicom_element*;

    /**
     * @brief Get a const pointer to the element with the given tag
     * @param tag The DICOM tag to look up
     * @return Const pointer to the element, or nullptr if not found
     */
    [[nodiscard]] auto get(dicom_tag tag) const noexcept -> const dicom_element*;

    // ========================================================================
    // Convenience Accessors
    // ========================================================================

    /**
     * @brief Get the string value of an element
     * @param tag The DICOM tag to look up
     * @param default_value Value to return if element not found
     * @return The element's string value, or default_value if not found
     */
    [[nodiscard]] auto get_string(dicom_tag tag,
                                  std::string_view default_value = "") const
        -> std::string;

    /**
     * @brief Get the numeric value of an element
     * @tparam T The numeric type to return
     * @param tag The DICOM tag to look up
     * @return Optional containing the value, or nullopt if not found/conversion fails
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    [[nodiscard]] auto get_numeric(dicom_tag tag) const -> std::optional<T>;

    // ========================================================================
    // Modification
    // ========================================================================

    /**
     * @brief Insert or replace an element in the dataset
     * @param element The element to insert (copied or moved)
     *
     * If an element with the same tag already exists, it will be replaced.
     * Use std::move() when passing the element to avoid unnecessary copies.
     */
    void insert(dicom_element element);

    /**
     * @brief Set a string value for the given tag
     * @param tag The DICOM tag
     * @param vr The value representation
     * @param value The string value
     *
     * Creates a new element or updates an existing one.
     */
    void set_string(dicom_tag tag, encoding::vr_type vr, std::string_view value);

    /**
     * @brief Set a numeric value for the given tag
     * @tparam T The numeric type
     * @param tag The DICOM tag
     * @param vr The value representation
     * @param value The numeric value
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    void set_numeric(dicom_tag tag, encoding::vr_type vr, T value);

    /**
     * @brief Remove an element from the dataset
     * @param tag The tag of the element to remove
     * @return true if an element was removed, false if not found
     */
    auto remove(dicom_tag tag) -> bool;

    /**
     * @brief Remove all elements from the dataset
     */
    void clear() noexcept;

    // ========================================================================
    // Iteration
    // ========================================================================

    /**
     * @brief Get iterator to the first element
     * @return Iterator to the beginning
     */
    [[nodiscard]] auto begin() noexcept -> iterator;

    /**
     * @brief Get iterator past the last element
     * @return Iterator to the end
     */
    [[nodiscard]] auto end() noexcept -> iterator;

    /**
     * @brief Get const iterator to the first element
     * @return Const iterator to the beginning
     */
    [[nodiscard]] auto begin() const noexcept -> const_iterator;

    /**
     * @brief Get const iterator past the last element
     * @return Const iterator to the end
     */
    [[nodiscard]] auto end() const noexcept -> const_iterator;

    /**
     * @brief Get const iterator to the first element
     * @return Const iterator to the beginning
     */
    [[nodiscard]] auto cbegin() const noexcept -> const_iterator;

    /**
     * @brief Get const iterator past the last element
     * @return Const iterator to the end
     */
    [[nodiscard]] auto cend() const noexcept -> const_iterator;

    // ========================================================================
    // Size Operations
    // ========================================================================

    /**
     * @brief Get the number of elements in the dataset
     * @return The element count
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Check if the dataset is empty
     * @return true if no elements are present
     */
    [[nodiscard]] auto empty() const noexcept -> bool;

    // ========================================================================
    // Utility Operations
    // ========================================================================

    /**
     * @brief Create a copy containing only the specified tags
     * @param tags List of tags to include in the copy
     * @return A new dataset containing only the specified elements
     */
    [[nodiscard]] auto copy_with_tags(std::initializer_list<dicom_tag> tags) const
        -> dicom_dataset;

    /**
     * @brief Create a copy containing only the specified tags
     * @param tags Span of tags to include in the copy
     * @return A new dataset containing only the specified elements
     */
    [[nodiscard]] auto copy_with_tags(std::span<const dicom_tag> tags) const
        -> dicom_dataset;

    /**
     * @brief Merge elements from another dataset
     * @param other The dataset to merge from
     *
     * Elements from 'other' will overwrite existing elements with the same tag.
     */
    void merge(const dicom_dataset& other);

    /**
     * @brief Merge elements from another dataset (move version)
     * @param other The dataset to merge from (moved)
     */
    void merge(dicom_dataset&& other);

private:
    storage_type elements_;
};

// ============================================================================
// Template Implementations
// ============================================================================

template <typename T>
    requires std::is_arithmetic_v<T>
auto dicom_dataset::get_numeric(dicom_tag tag) const -> std::optional<T> {
    const auto* elem = get(tag);
    if (elem == nullptr) {
        return std::nullopt;
    }

    try {
        return elem->as_numeric<T>();
    } catch (const value_conversion_error&) {
        return std::nullopt;
    }
}

template <typename T>
    requires std::is_arithmetic_v<T>
void dicom_dataset::set_numeric(dicom_tag tag, encoding::vr_type vr, T value) {
    insert(dicom_element::from_numeric<T>(tag, vr, value));
}

}  // namespace pacs::core
