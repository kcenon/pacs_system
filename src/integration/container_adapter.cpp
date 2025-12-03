/**
 * @file container_adapter.cpp
 * @brief Implementation of DICOM to container_system value adapter
 */

#include <pacs/integration/container_adapter.hpp>

#include <charconv>
#include <pacs/compat/format.hpp>
#include <sstream>

namespace pacs::integration {

// =============================================================================
// VR to Container Value Mapping
// =============================================================================

auto container_adapter::to_container_value(const core::dicom_element& element)
    -> container_module::optimized_value {
    container_module::optimized_value result;
    const auto vr = element.vr();
    const auto tag = element.tag();

    // Use tag string as the value name
    result.name = make_element_key(tag, vr);

    // Handle empty elements
    // For string VRs, keep as empty string; for others, treat as null
    if (element.is_empty()) {
        if (encoding::is_string_vr(vr)) {
            result.type = container_module::value_types::string_value;
            result.data = std::string{};
        } else {
            result.type = container_module::value_types::null_value;
            result.data = std::monostate{};
        }
        return result;
    }

    // Handle sequence type specially
    if (element.is_sequence()) {
        result.type = container_module::value_types::container_value;
        result.data = sequence_to_container(element);
        return result;
    }

    // Map VR to appropriate container type
    if (encoding::is_string_vr(vr)) {
        result.type = container_module::value_types::string_value;
        result.data = element.as_string();
    } else if (encoding::is_numeric_vr(vr)) {
        // Map numeric VRs to appropriate types
        switch (vr) {
            case encoding::vr_type::SS:  // Signed Short (2 bytes)
                result.type = container_module::value_types::short_value;
                result.data = element.as_numeric<short>();
                break;
            case encoding::vr_type::US:  // Unsigned Short (2 bytes)
                result.type = container_module::value_types::ushort_value;
                result.data = element.as_numeric<unsigned short>();
                break;
            case encoding::vr_type::SL:  // Signed Long (4 bytes)
                result.type = container_module::value_types::int_value;
                result.data = element.as_numeric<int>();
                break;
            case encoding::vr_type::UL:  // Unsigned Long (4 bytes)
                result.type = container_module::value_types::uint_value;
                result.data = element.as_numeric<unsigned int>();
                break;
            case encoding::vr_type::SV:  // Signed 64-bit
                result.type = container_module::value_types::llong_value;
                result.data = element.as_numeric<long long>();
                break;
            case encoding::vr_type::UV:  // Unsigned 64-bit
                result.type = container_module::value_types::ullong_value;
                result.data = element.as_numeric<unsigned long long>();
                break;
            case encoding::vr_type::FL:  // Float
                result.type = container_module::value_types::float_value;
                result.data = element.as_numeric<float>();
                break;
            case encoding::vr_type::FD:  // Double
                result.type = container_module::value_types::double_value;
                result.data = element.as_numeric<double>();
                break;
            default:
                // Fallback to bytes for unhandled numeric types
                result.type = container_module::value_types::bytes_value;
                auto raw = element.raw_data();
                result.data = std::vector<uint8_t>(raw.begin(), raw.end());
                break;
        }
    } else if (encoding::is_binary_vr(vr)) {
        result.type = container_module::value_types::bytes_value;
        auto raw = element.raw_data();
        result.data = std::vector<uint8_t>(raw.begin(), raw.end());
    } else if (vr == encoding::vr_type::AT) {
        // Attribute Tag - store as 32-bit unsigned
        result.type = container_module::value_types::uint_value;
        result.data = element.as_numeric<unsigned int>();
    } else {
        // Default: store as bytes
        result.type = container_module::value_types::bytes_value;
        auto raw = element.raw_data();
        result.data = std::vector<uint8_t>(raw.begin(), raw.end());
    }

    return result;
}

auto container_adapter::from_container_value(
    core::dicom_tag tag,
    encoding::vr_type vr,
    const container_module::optimized_value& val) -> core::dicom_element {
    // Handle null/empty values
    if (val.type == container_module::value_types::null_value) {
        return core::dicom_element{tag, vr};
    }

    // Handle sequence type
    if (vr == encoding::vr_type::SQ) {
        auto elem = core::dicom_element{tag, vr};
        if (std::holds_alternative<std::shared_ptr<container_module::value_container>>(
                val.data)) {
            auto container = std::get<std::shared_ptr<container_module::value_container>>(val.data);
            if (container) {
                auto items = container_to_sequence(*container);
                for (auto& item : items) {
                    elem.sequence_items().push_back(std::move(item));
                }
            }
        }
        return elem;
    }

    // Handle string VRs
    if (encoding::is_string_vr(vr)) {
        std::string str_val;
        if (std::holds_alternative<std::string>(val.data)) {
            str_val = std::get<std::string>(val.data);
        } else {
            str_val = container_module::variant_helpers::to_string(val.data, val.type);
        }
        return core::dicom_element::from_string(tag, vr, str_val);
    }

    // Handle numeric VRs
    if (encoding::is_numeric_vr(vr)) {
        switch (vr) {
            case encoding::vr_type::SS:
                if (std::holds_alternative<short>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<short>(val.data));
                }
                break;
            case encoding::vr_type::US:
                if (std::holds_alternative<unsigned short>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<unsigned short>(val.data));
                }
                break;
            case encoding::vr_type::SL:
                if (std::holds_alternative<int>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<int>(val.data));
                }
                break;
            case encoding::vr_type::UL:
                if (std::holds_alternative<unsigned int>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<unsigned int>(val.data));
                }
                break;
            case encoding::vr_type::SV:
                if (std::holds_alternative<long long>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<long long>(val.data));
                }
                break;
            case encoding::vr_type::UV:
                if (std::holds_alternative<unsigned long long>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<unsigned long long>(val.data));
                }
                break;
            case encoding::vr_type::FL:
                if (std::holds_alternative<float>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<float>(val.data));
                }
                break;
            case encoding::vr_type::FD:
                if (std::holds_alternative<double>(val.data)) {
                    return core::dicom_element::from_numeric(tag, vr,
                        std::get<double>(val.data));
                }
                break;
            default:
                break;
        }
    }

    // Handle binary VRs or fallback
    if (std::holds_alternative<std::vector<uint8_t>>(val.data)) {
        auto& bytes = std::get<std::vector<uint8_t>>(val.data);
        return core::dicom_element{tag, vr, bytes};
    }

    // Return empty element as last resort
    return core::dicom_element{tag, vr};
}

// =============================================================================
// Dataset Serialization
// =============================================================================

auto container_adapter::serialize_dataset(const core::dicom_dataset& dataset)
    -> std::shared_ptr<container_module::value_container> {
    auto container = std::make_shared<container_module::value_container>();

    // Add metadata
    container->set_value(std::string(kVersionKey), std::string(kProtocolVersion));
    container->set_value(std::string(kElementCountKey),
                         static_cast<unsigned int>(dataset.size()));

    // Serialize each element
    for (const auto& [tag, element] : dataset) {
        auto value = to_container_value(element);
        container->set_unit(value);
    }

    return container;
}

auto container_adapter::deserialize_dataset(
    const container_module::value_container& container)
    -> Result<core::dicom_dataset> {
    core::dicom_dataset dataset;

    // Process each value in the container
    for (const auto& val : container) {
        // Skip metadata keys
        if (val.name.starts_with("_")) {
            continue;
        }

        // Parse element key to get tag and VR
        auto parsed = parse_element_key(val.name);
        if (!parsed) {
            continue;  // Skip invalid keys
        }

        auto [tag, vr] = *parsed;

        // Convert value back to DICOM element
        try {
            auto element = from_container_value(tag, vr, val);
            dataset.insert(std::move(element));
        } catch (const std::exception& e) {
            return Result<core::dicom_dataset>::err(
                kcenon::common::error_info{
                    pacs::compat::format("Failed to convert element {}: {}", val.name, e.what())
                });
        }
    }

    return Result<core::dicom_dataset>::ok(std::move(dataset));
}

// =============================================================================
// Binary Serialization
// =============================================================================

auto container_adapter::to_binary(const core::dicom_dataset& dataset)
    -> std::vector<uint8_t> {
    auto container = serialize_dataset(dataset);
    return container->serialize_array();
}

auto container_adapter::from_binary(std::span<const uint8_t> data)
    -> Result<core::dicom_dataset> {
    container_module::value_container container;

    std::vector<uint8_t> data_vec(data.begin(), data.end());
    if (!container.deserialize(data_vec, false)) {
        return Result<core::dicom_dataset>::err(
            kcenon::common::error_info{
                "Failed to deserialize binary data to container"
            });
    }

    return deserialize_dataset(container);
}

// =============================================================================
// Utility Functions
// =============================================================================

auto container_adapter::get_container_type(encoding::vr_type vr) noexcept
    -> container_module::value_types {
    if (encoding::is_string_vr(vr)) {
        return container_module::value_types::string_value;
    }

    switch (vr) {
        case encoding::vr_type::SS:
            return container_module::value_types::short_value;
        case encoding::vr_type::US:
            return container_module::value_types::ushort_value;
        case encoding::vr_type::SL:
            return container_module::value_types::int_value;
        case encoding::vr_type::UL:
        case encoding::vr_type::AT:
            return container_module::value_types::uint_value;
        case encoding::vr_type::SV:
            return container_module::value_types::llong_value;
        case encoding::vr_type::UV:
            return container_module::value_types::ullong_value;
        case encoding::vr_type::FL:
            return container_module::value_types::float_value;
        case encoding::vr_type::FD:
            return container_module::value_types::double_value;
        case encoding::vr_type::SQ:
            return container_module::value_types::container_value;
        default:
            return container_module::value_types::bytes_value;
    }
}

// =============================================================================
// Private Helper Functions
// =============================================================================

auto container_adapter::make_element_key(core::dicom_tag tag,
                                          encoding::vr_type vr)
    -> std::string {
    return pacs::compat::format("{:04X},{:04X}:{}", tag.group(), tag.element(),
                       encoding::to_string(vr));
}

auto container_adapter::parse_element_key(std::string_view key)
    -> std::optional<std::pair<core::dicom_tag, encoding::vr_type>> {
    // Expected format: "GGGG,EEEE:VR"
    if (key.size() < 12) {  // Minimum: "0000,0000:XX"
        return std::nullopt;
    }

    // Find the colon separator
    auto colon_pos = key.find(':');
    if (colon_pos == std::string_view::npos || colon_pos < 9) {
        return std::nullopt;
    }

    // Parse group (first 4 hex chars)
    uint16_t group = 0;
    auto group_str = key.substr(0, 4);
    auto group_result = std::from_chars(group_str.data(),
                                         group_str.data() + group_str.size(),
                                         group, 16);
    if (group_result.ec != std::errc{}) {
        return std::nullopt;
    }

    // Parse element (chars 5-8, after comma)
    uint16_t element = 0;
    auto elem_str = key.substr(5, 4);
    auto elem_result = std::from_chars(elem_str.data(),
                                        elem_str.data() + elem_str.size(),
                                        element, 16);
    if (elem_result.ec != std::errc{}) {
        return std::nullopt;
    }

    // Parse VR (2 chars after colon)
    auto vr_str = key.substr(colon_pos + 1, 2);
    auto vr_opt = encoding::from_string(vr_str);
    if (!vr_opt) {
        return std::nullopt;
    }

    return std::make_pair(core::dicom_tag{group, element}, *vr_opt);
}

auto container_adapter::sequence_to_container(const core::dicom_element& element)
    -> std::shared_ptr<container_module::value_container> {
    auto container = std::make_shared<container_module::value_container>();

    const auto& items = element.sequence_items();
    container->set_value("_item_count", static_cast<unsigned int>(items.size()));

    size_t item_index = 0;
    for (const auto& item : items) {
        auto item_container = serialize_dataset(item);
        container_module::optimized_value val;
        val.name = pacs::compat::format("item_{}", item_index++);
        val.type = container_module::value_types::container_value;
        val.data = item_container;
        container->set_unit(val);
    }

    return container;
}

auto container_adapter::container_to_sequence(
    const container_module::value_container& container)
    -> std::vector<core::dicom_dataset> {
    std::vector<core::dicom_dataset> items;

    for (const auto& val : container) {
        if (!val.name.starts_with("item_")) {
            continue;
        }

        if (std::holds_alternative<std::shared_ptr<container_module::value_container>>(
                val.data)) {
            auto item_container =
                std::get<std::shared_ptr<container_module::value_container>>(val.data);
            if (item_container) {
                auto result = deserialize_dataset(*item_container);
                if (result.is_ok()) {
                    items.push_back(std::move(result.value()));
                }
            }
        }
    }

    return items;
}

}  // namespace pacs::integration
