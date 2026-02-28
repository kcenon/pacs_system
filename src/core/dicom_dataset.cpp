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
// Sequence Access
// ============================================================================

auto dicom_dataset::has_sequence(dicom_tag tag) const noexcept -> bool {
    const auto* elem = get(tag);
    return elem != nullptr && elem->is_sequence();
}

auto dicom_dataset::get_sequence(dicom_tag tag) const noexcept
    -> const std::vector<dicom_dataset>* {
    const auto* elem = get(tag);
    if (elem == nullptr || !elem->is_sequence()) {
        return nullptr;
    }
    return &elem->sequence_items();
}

auto dicom_dataset::get_sequence(dicom_tag tag) noexcept
    -> std::vector<dicom_dataset>* {
    auto* elem = get(tag);
    if (elem == nullptr || !elem->is_sequence()) {
        return nullptr;
    }
    return &elem->sequence_items();
}

auto dicom_dataset::get_or_create_sequence(dicom_tag tag)
    -> std::vector<dicom_dataset>& {
    auto* elem = get(tag);
    if (elem != nullptr && elem->is_sequence()) {
        return elem->sequence_items();
    }
    // Create or replace with empty sequence
    insert(dicom_element{tag, encoding::vr_type::SQ});
    return get(tag)->sequence_items();
}

// ============================================================================
// Private Tag Access
// ============================================================================

auto dicom_dataset::get_private_creator(dicom_tag private_data_tag) const
    -> std::optional<std::string> {
    const auto creator_tag = private_data_tag.private_creator_tag();
    if (!creator_tag) {
        return std::nullopt;
    }
    const auto* elem = get(*creator_tag);
    if (elem == nullptr) {
        return std::nullopt;
    }
    auto result = elem->as_string();
    if (result.is_ok()) {
        return result.value();
    }
    return std::nullopt;
}

auto dicom_dataset::get_private_block(std::string_view creator_id,
                                       uint16_t group) const
    -> std::vector<const dicom_element*> {
    std::vector<const dicom_element*> result;

    // Scan Private Creator elements (gggg,0010)-(gggg,00FF) for a match
    for (uint16_t slot = 0x0010; slot <= 0x00FF; ++slot) {
        const dicom_tag creator_tag{group, slot};
        const auto* creator_elem = get(creator_tag);
        if (creator_elem == nullptr) {
            continue;
        }
        auto str_result = creator_elem->as_string();
        if (str_result.is_err() || str_result.value() != creator_id) {
            continue;
        }

        // Found the creator — collect all data elements in its block
        const auto range = creator_tag.private_data_range();
        if (!range) {
            break;
        }
        const auto [first, last] = *range;
        auto it = elements_.lower_bound(first);
        while (it != elements_.end() && it->first <= last) {
            result.push_back(&it->second);
            ++it;
        }
        break;
    }

    return result;
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
