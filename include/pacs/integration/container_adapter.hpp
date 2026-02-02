/**
 * @file container_adapter.hpp
 * @brief Adapter for mapping DICOM VR types to container_system values
 *
 * This file provides the container_adapter class for converting between
 * DICOM data elements and container_system value types. It enables
 * efficient serialization and deserialization of DICOM datasets.
 *
 * @see IR-1 (container_system Integration)
 */

#pragma once

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <kcenon/common/patterns/result.h>

#include <container.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pacs::integration {

/**
 * @brief Adapter for mapping DICOM VR types to container_system values
 *
 * This class provides static methods for converting between DICOM data
 * elements and container_system value types. The mapping follows these
 * rules:
 *
 * | VR Category | DICOM VRs | container value Type |
 * |-------------|-----------|----------------------|
 * | String | AE, AS, CS, DA, DS, DT, IS, LO, LT, PN, SH, ST, TM, UI, UT | string |
 * | Integer | SS, US, SL, UL, SV, UV | int64_t/uint64_t |
 * | Float | FL, FD | float/double |
 * | Binary | OB, OW, OF, OD, OL, UN | bytes |
 * | Sequence | SQ | array of containers |
 * | Special | AT | uint32_t |
 *
 * Thread Safety: All methods are thread-safe as they use only local state.
 *
 * @example
 * @code
 * // Convert element to container value
 * auto element = dicom_element::from_string(tags::patient_name, vr_type::PN, "Doe^John");
 * auto value = container_adapter::to_container_value(element);
 *
 * // Serialize entire dataset
 * dicom_dataset ds;
 * ds.set_string(tags::patient_id, "12345");
 * auto container = container_adapter::serialize_dataset(ds);
 *
 * // Binary serialization
 * auto bytes = container_adapter::to_binary(ds);
 * auto result = container_adapter::from_binary(bytes);
 * @endcode
 */
class container_adapter {
public:
    /// Result type alias for operations returning a value
    template <typename T>
    using Result = kcenon::common::Result<T>;

    // =========================================================================
    // VR to Container Value Mapping
    // =========================================================================

    /**
     * @brief Convert a DICOM element to a container value
     *
     * Maps the element's VR to the appropriate container value type:
     * - String VRs -> string_value
     * - Integer VRs -> llong_value or ullong_value
     * - Float VRs -> float_value or double_value
     * - Binary VRs -> bytes_value
     * - Sequence VR -> array of containers
     * - AT (Attribute Tag) -> uint_value (as 32-bit integer)
     *
     * @param element The DICOM element to convert
     * @return An optimized_value containing the converted data
     */
    [[nodiscard]] static auto to_container_value(const core::dicom_element& element)
        -> container_module::optimized_value;

    /**
     * @brief Convert a container value back to a DICOM element
     *
     * Reconstructs a DICOM element from a container value, using the
     * provided tag and VR information.
     *
     * @param tag The DICOM tag for the element
     * @param vr The value representation
     * @param val The container value to convert
     * @return A new dicom_element containing the data
     */
    [[nodiscard]] static auto from_container_value(
        core::dicom_tag tag,
        encoding::vr_type vr,
        const container_module::optimized_value& val) -> core::dicom_element;

    // =========================================================================
    // Dataset Serialization
    // =========================================================================

    /**
     * @brief Serialize a DICOM dataset to a value_container
     *
     * Converts all elements in the dataset to container values and stores
     * them in a value_container. The container includes metadata for
     * reconstructing the dataset:
     * - "_pacs_version": Protocol version
     * - "_element_count": Number of elements
     * - Each element: "GGGG,EEEE:VR" -> value
     *
     * @param dataset The DICOM dataset to serialize
     * @return A value_container containing all elements
     */
    [[nodiscard]] static auto serialize_dataset(const core::dicom_dataset& dataset)
        -> std::shared_ptr<container_module::value_container>;

    /**
     * @brief Deserialize a value_container back to a DICOM dataset
     *
     * Reconstructs a DICOM dataset from a previously serialized
     * value_container.
     *
     * @param container The container to deserialize
     * @return Result containing the dataset or error
     */
    [[nodiscard]] static auto deserialize_dataset(
        const container_module::value_container& container)
        -> Result<core::dicom_dataset>;

    // =========================================================================
    // Binary Serialization
    // =========================================================================

    /**
     * @brief Serialize a DICOM dataset to binary format
     *
     * Uses container_system's serialization for efficient binary encoding.
     * The resulting format is suitable for network transmission or storage.
     *
     * @param dataset The DICOM dataset to serialize
     * @return A vector of bytes containing the serialized data
     */
    [[nodiscard]] static auto to_binary(const core::dicom_dataset& dataset)
        -> std::vector<uint8_t>;

    /**
     * @brief Deserialize binary data back to a DICOM dataset
     *
     * Reconstructs a DICOM dataset from binary data previously created
     * by to_binary().
     *
     * @param data The binary data to deserialize
     * @return Result containing the dataset or error
     */
    [[nodiscard]] static auto from_binary(std::span<const uint8_t> data)
        -> Result<core::dicom_dataset>;

    // =========================================================================
    // Utility Functions
    // =========================================================================

    /**
     * @brief Get the container value type for a DICOM VR
     *
     * @param vr The DICOM value representation
     * @return The corresponding container value_types enum
     */
    [[nodiscard]] static auto get_container_type(encoding::vr_type vr) noexcept
        -> container_module::value_types;

    /**
     * @brief Check if a VR maps to a string value
     *
     * @param vr The DICOM value representation
     * @return true if the VR should be stored as a string
     */
    [[nodiscard]] static constexpr auto maps_to_string(encoding::vr_type vr) noexcept
        -> bool {
        return encoding::is_string_vr(vr);
    }

    /**
     * @brief Check if a VR maps to a numeric value
     *
     * @param vr The DICOM value representation
     * @return true if the VR should be stored as a number
     */
    [[nodiscard]] static constexpr auto maps_to_numeric(encoding::vr_type vr) noexcept
        -> bool {
        return encoding::is_numeric_vr(vr);
    }

    /**
     * @brief Check if a VR maps to binary data
     *
     * @param vr The DICOM value representation
     * @return true if the VR should be stored as bytes
     */
    [[nodiscard]] static constexpr auto maps_to_binary(encoding::vr_type vr) noexcept
        -> bool {
        return encoding::is_binary_vr(vr);
    }

private:
    /// Protocol version for serialization format
    static constexpr std::string_view kProtocolVersion = "1.0.0";

    /// Key for protocol version in container
    static constexpr std::string_view kVersionKey = "_pacs_version";

    /// Key for element count in container
    static constexpr std::string_view kElementCountKey = "_element_count";

    /**
     * @brief Create a key string for an element in the container
     *
     * Format: "GGGG,EEEE:VR" (e.g., "0010,0020:LO")
     *
     * @param tag The DICOM tag
     * @param vr The value representation
     * @return Key string for container storage
     */
    [[nodiscard]] static auto make_element_key(core::dicom_tag tag,
                                                encoding::vr_type vr)
        -> std::string;

    /**
     * @brief Parse an element key back to tag and VR
     *
     * @param key The key string to parse
     * @return Pair of (tag, vr) or nullopt if parsing fails
     */
    [[nodiscard]] static auto parse_element_key(std::string_view key)
        -> std::optional<std::pair<core::dicom_tag, encoding::vr_type>>;

    /**
     * @brief Convert sequence items to container array
     *
     * @param element The sequence element
     * @return Shared pointer to value_container containing items
     */
    [[nodiscard]] static auto sequence_to_container(
        const core::dicom_element& element)
        -> std::shared_ptr<container_module::value_container>;

    /**
     * @brief Convert container array back to sequence items
     *
     * @param container The container containing sequence items
     * @return Vector of datasets representing sequence items
     */
    [[nodiscard]] static auto container_to_sequence(
        const container_module::value_container& container)
        -> std::vector<core::dicom_dataset>;
};

}  // namespace pacs::integration
