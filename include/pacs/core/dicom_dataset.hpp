/**
 * @file dicom_dataset.hpp
 * @brief DICOM Dataset - ordered collection of Data Elements
 *
 * This file provides the basic definition of dicom_dataset class.
 * The full implementation will be completed in DES-CORE-003 (Issue #12).
 *
 * @see DICOM PS3.5 Section 7.1 - Data Set
 */

#pragma once

#include "dicom_tag.hpp"

#include <map>
#include <memory>
#include <optional>

namespace pacs::core {

// Forward declaration
class dicom_element;

/**
 * @brief Represents a DICOM Dataset (ordered collection of Data Elements)
 *
 * A DICOM Dataset is an ordered collection of Data Elements, where each
 * element is uniquely identified by its tag. Elements are stored in
 * ascending tag order.
 *
 * @note This is a placeholder implementation. Full functionality will be
 *       added in DES-CORE-003 (Issue #12).
 *
 * @example
 * @code
 * dicom_dataset ds;
 * ds.insert(dicom_element::from_string(tags::patient_name, vr_type::PN, "DOE^JOHN"));
 * @endcode
 */
class dicom_dataset {
public:
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

    /**
     * @brief Check if the dataset is empty
     * @return true if no elements are present
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        return elements_.empty();
    }

    /**
     * @brief Get the number of elements in the dataset
     * @return The element count
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        return elements_.size();
    }

private:
    // Using map to maintain tag ordering (will be enhanced in Issue #12)
    std::map<dicom_tag, std::shared_ptr<dicom_element>> elements_;
};

}  // namespace pacs::core
