/**
 * @file dicom_element.cpp
 * @brief Implementation of DICOM Data Element
 */

#include <pacs/core/dicom_element.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/result.hpp>

#include <algorithm>
#include <charconv>
#include <sstream>

namespace pacs::core {

// ============================================================================
// Constructors
// ============================================================================

dicom_element::dicom_element(dicom_tag tag, encoding::vr_type vr) noexcept
    : tag_{tag}, vr_{vr}, data_{}, sequence_items_{} {}

dicom_element::dicom_element(dicom_tag tag, encoding::vr_type vr,
                             std::span<const uint8_t> data)
    : tag_{tag}, vr_{vr}, data_(data.begin(), data.end()), sequence_items_{} {}

dicom_element::dicom_element(const dicom_element&) = default;
dicom_element::dicom_element(dicom_element&&) noexcept = default;
auto dicom_element::operator=(const dicom_element&) -> dicom_element& = default;
auto dicom_element::operator=(dicom_element&&) noexcept -> dicom_element& = default;
dicom_element::~dicom_element() = default;

// ============================================================================
// Factory Methods
// ============================================================================

auto dicom_element::from_string(dicom_tag tag, encoding::vr_type vr,
                                std::string_view value) -> dicom_element {
    dicom_element elem{tag, vr};
    elem.set_string(value);
    return elem;
}

// ============================================================================
// String Value Access
// ============================================================================

auto dicom_element::as_string() const -> pacs::Result<std::string> {
    if (data_.empty()) {
        return pacs::ok(std::string{});
    }

    // For string VRs, convert bytes to string and remove padding
    if (encoding::is_string_vr(vr_)) {
        std::string_view raw{reinterpret_cast<const char*>(data_.data()),
                             data_.size()};
        return pacs::ok(remove_padding(raw, vr_));
    }

    // For numeric VRs, convert to string representation
    if (encoding::is_numeric_vr(vr_)) {
        switch (vr_) {
            case encoding::vr_type::US:
                if (data_.size() >= 2) {
                    if (auto val = as_numeric<uint16_t>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            case encoding::vr_type::SS:
                if (data_.size() >= 2) {
                    if (auto val = as_numeric<int16_t>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            case encoding::vr_type::UL:
                if (data_.size() >= 4) {
                    if (auto val = as_numeric<uint32_t>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            case encoding::vr_type::SL:
                if (data_.size() >= 4) {
                    if (auto val = as_numeric<int32_t>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            case encoding::vr_type::UV:
                if (data_.size() >= 8) {
                    if (auto val = as_numeric<uint64_t>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            case encoding::vr_type::SV:
                if (data_.size() >= 8) {
                    if (auto val = as_numeric<int64_t>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            case encoding::vr_type::FL:
                if (data_.size() >= 4) {
                    if (auto val = as_numeric<float>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            case encoding::vr_type::FD:
                if (data_.size() >= 8) {
                    if (auto val = as_numeric<double>(); val.is_ok())
                        return pacs::ok(std::to_string(val.value()));
                }
                break;
            default:
                break;
        }
    }

    // For binary VRs or unknown, return raw bytes as string
    return pacs::ok(
        std::string{reinterpret_cast<const char*>(data_.data()), data_.size()});
}

auto dicom_element::as_string_list() const
    -> pacs::Result<std::vector<std::string>> {
    auto str_result = as_string();
    if (str_result.is_err()) {
        return pacs::pacs_error<std::vector<std::string>>(
            str_result.error().code, str_result.error().message);
    }
    const std::string str = str_result.value();

    std::vector<std::string> result;
    if (str.empty()) {
        return pacs::ok(result);
    }

    // Split by backslash (DICOM VM delimiter)
    std::string::size_type start = 0;
    std::string::size_type pos = 0;

    while ((pos = str.find('\\', start)) != std::string::npos) {
        result.push_back(str.substr(start, pos - start));
        start = pos + 1;
    }

    // Add the last segment
    result.push_back(str.substr(start));

    return pacs::ok(result);
}

// ============================================================================
// Sequence Access
// ============================================================================

auto dicom_element::sequence_items() -> std::vector<dicom_dataset>& {
    return sequence_items_;
}

auto dicom_element::sequence_items() const -> const std::vector<dicom_dataset>& {
    return sequence_items_;
}

// ============================================================================
// Modification
// ============================================================================

void dicom_element::set_value(std::span<const uint8_t> data) {
    data_.assign(data.begin(), data.end());
}

void dicom_element::set_string(std::string_view value) {
    // Apply padding for string VRs
    std::string padded = apply_padding(value);
    data_.assign(padded.begin(), padded.end());
}

// ============================================================================
// Private Helpers
// ============================================================================

auto dicom_element::apply_padding(std::string_view str) const -> std::string {
    std::string result{str};

    // DICOM requires even length for all values
    if (result.length() % 2 != 0) {
        result += encoding::padding_char(vr_);
    }

    return result;
}

auto dicom_element::remove_padding(std::string_view str,
                                   encoding::vr_type vr) -> std::string {
    if (str.empty()) {
        return {};
    }

    std::string result{str};

    // Determine padding character based on VR
    const char pad = encoding::padding_char(vr);

    // Remove trailing padding characters
    while (!result.empty() && result.back() == pad) {
        result.pop_back();
    }

    // For string VRs (except UI), also trim trailing spaces
    if (encoding::is_string_vr(vr) && vr != encoding::vr_type::UI) {
        while (!result.empty() && result.back() == ' ') {
            result.pop_back();
        }
    }

    return result;
}

}  // namespace pacs::core
