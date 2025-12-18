/**
 * @file dicom_dataset.cpp
 * @brief Implementation of DICOM Dataset
 */

#include <pacs/core/dicom_dataset.hpp>

namespace pacs::core {

// ============================================================================
// Element Access
// ============================================================================

auto dicom_dataset::contains(dicom_tag tag) const noexcept -> bool {
    return elements_.contains(tag);
}

auto dicom_dataset::get(dicom_tag tag) noexcept -> dicom_element* {
    auto it = elements_.find(tag);
    if (it == elements_.end()) {
        return nullptr;
    }
    return &it->second;
}

auto dicom_dataset::get(dicom_tag tag) const noexcept -> const dicom_element* {
    auto it = elements_.find(tag);
    if (it == elements_.end()) {
        return nullptr;
    }
    return &it->second;
}

// ============================================================================
// Convenience Accessors
// ============================================================================

auto dicom_dataset::get_string(dicom_tag tag,
                               std::string_view default_value) const
    -> std::string {
    const auto* elem = get(tag);
    if (elem == nullptr) {
        return std::string{default_value};
    }
    return elem->as_string().unwrap_or(std::string{default_value});
}

// ============================================================================
// Modification
// ============================================================================

void dicom_dataset::insert(dicom_element element) {
    elements_.insert_or_assign(element.tag(), std::move(element));
}

void dicom_dataset::set_string(dicom_tag tag, encoding::vr_type vr,
                               std::string_view value) {
    insert(dicom_element::from_string(tag, vr, value));
}

auto dicom_dataset::remove(dicom_tag tag) -> bool {
    return elements_.erase(tag) > 0;
}

void dicom_dataset::clear() noexcept {
    elements_.clear();
}

// ============================================================================
// Iteration
// ============================================================================

auto dicom_dataset::begin() noexcept -> iterator {
    return elements_.begin();
}

auto dicom_dataset::end() noexcept -> iterator {
    return elements_.end();
}

auto dicom_dataset::begin() const noexcept -> const_iterator {
    return elements_.begin();
}

auto dicom_dataset::end() const noexcept -> const_iterator {
    return elements_.end();
}

auto dicom_dataset::cbegin() const noexcept -> const_iterator {
    return elements_.cbegin();
}

auto dicom_dataset::cend() const noexcept -> const_iterator {
    return elements_.cend();
}

// ============================================================================
// Size Operations
// ============================================================================

auto dicom_dataset::size() const noexcept -> size_t {
    return elements_.size();
}

auto dicom_dataset::empty() const noexcept -> bool {
    return elements_.empty();
}

// ============================================================================
// Utility Operations
// ============================================================================

auto dicom_dataset::copy_with_tags(std::initializer_list<dicom_tag> tags) const
    -> dicom_dataset {
    return copy_with_tags(std::span{tags.begin(), tags.size()});
}

auto dicom_dataset::copy_with_tags(std::span<const dicom_tag> tags) const
    -> dicom_dataset {
    dicom_dataset result;

    for (const auto& tag : tags) {
        const auto* elem = get(tag);
        if (elem != nullptr) {
            result.insert(*elem);
        }
    }

    return result;
}

void dicom_dataset::merge(const dicom_dataset& other) {
    for (const auto& [tag, element] : other) {
        insert(element);
    }
}

void dicom_dataset::merge(dicom_dataset&& other) {
    for (auto& [tag, element] : other.elements_) {
        insert(std::move(element));
    }
    other.clear();
}

}  // namespace pacs::core
