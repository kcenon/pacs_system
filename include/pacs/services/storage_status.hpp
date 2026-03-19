// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file storage_status.hpp
 * @brief Storage SCP status codes for C-STORE operations
 *
 * This file defines status codes specific to storage operations as specified
 * in DICOM PS3.4 Annex B - Storage Service Class.
 *
 * @see DICOM PS3.4 Section B.2.3 - C-STORE SCP Behavior
 * @see DES-SVC-002 - Storage SCP Design Specification
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_STORAGE_STATUS_HPP
#define PACS_SERVICES_STORAGE_STATUS_HPP

#include <cstdint>
#include <string_view>

namespace kcenon::pacs::services {

/**
 * @brief Storage operation status codes
 *
 * These status codes are returned by the storage handler to indicate
 * the result of a C-STORE operation. They map to DICOM status codes
 * defined in PS3.4 Annex B.
 */
enum class storage_status : uint16_t {
    /// Success - image stored successfully (0x0000)
    success = 0x0000,

    /// Warning: Coercion of data elements (0xB000)
    coercion_of_data_elements = 0xB000,

    /// Warning: Data set does not match SOP class (0xB007)
    data_set_does_not_match_sop_class_warning = 0xB007,

    /// Warning: Elements discarded (0xB006)
    elements_discarded = 0xB006,

    /// Failure: Duplicate SOP instance - already exists (0x0111)
    duplicate_sop_instance = 0x0111,

    /// Failure: Out of resources (0xA700)
    out_of_resources = 0xA700,

    /// Failure: Out of resources - Unable to store (0xA701)
    out_of_resources_unable_to_store = 0xA701,

    /// Failure: Data set does not match SOP class (0xA900)
    data_set_does_not_match_sop_class = 0xA900,

    /// Failure: Cannot understand - processing failure (0xC000)
    cannot_understand = 0xC000,

    /// Failure: Unable to process - storage error (0xC001)
    storage_error = 0xC001
};

/**
 * @brief Check if the status indicates success
 * @param status The status to check
 * @return true if the operation was successful
 */
[[nodiscard]] constexpr bool is_success(storage_status status) noexcept {
    return status == storage_status::success;
}

/**
 * @brief Check if the status indicates a warning
 * @param status The status to check
 * @return true if the operation completed with a warning
 */
[[nodiscard]] constexpr bool is_warning(storage_status status) noexcept {
    const auto value = static_cast<uint16_t>(status);
    return (value & 0xF000) == 0xB000;
}

/**
 * @brief Check if the status indicates a failure
 * @param status The status to check
 * @return true if the operation failed
 */
[[nodiscard]] constexpr bool is_failure(storage_status status) noexcept {
    const auto value = static_cast<uint16_t>(status);
    const auto high_nibble = (value & 0xF000) >> 12;
    return high_nibble == 0xA || high_nibble == 0xC ||
           (value >= 0x0100 && value <= 0x01FF);
}

/**
 * @brief Get a human-readable description of the storage status
 * @param status The status to describe
 * @return Description string
 */
[[nodiscard]] constexpr std::string_view to_string(storage_status status) noexcept {
    switch (status) {
        case storage_status::success:
            return "Success";
        case storage_status::coercion_of_data_elements:
            return "Warning: Coercion of data elements";
        case storage_status::data_set_does_not_match_sop_class_warning:
            return "Warning: Data set does not match SOP class";
        case storage_status::elements_discarded:
            return "Warning: Elements discarded";
        case storage_status::duplicate_sop_instance:
            return "Failure: Duplicate SOP instance";
        case storage_status::out_of_resources:
            return "Failure: Out of resources";
        case storage_status::out_of_resources_unable_to_store:
            return "Failure: Out of resources - Unable to store";
        case storage_status::data_set_does_not_match_sop_class:
            return "Failure: Data set does not match SOP class";
        case storage_status::cannot_understand:
            return "Failure: Cannot understand";
        case storage_status::storage_error:
            return "Failure: Storage error";
        default:
            return "Unknown status";
    }
}

/**
 * @brief Convert storage_status to DIMSE status_code
 * @param status The storage status
 * @return Equivalent DIMSE status code
 */
[[nodiscard]] constexpr uint16_t to_status_code(storage_status status) noexcept {
    return static_cast<uint16_t>(status);
}

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_STORAGE_STATUS_HPP
