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
 * @file dicom_dictionary.cpp
 * @brief Implementation of dicom_dictionary class
 */

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/encoding/vr_type.hpp"

#include <algorithm>

namespace kcenon::pacs::core {

// Forward declaration - defined in standard_tags_data.cpp
extern auto get_standard_tags() -> std::span<const tag_info>;

auto dicom_dictionary::instance() -> dicom_dictionary& {
    static dicom_dictionary instance;
    return instance;
}

dicom_dictionary::dicom_dictionary() {
    initialize_standard_tags();
}

void dicom_dictionary::initialize_standard_tags() {
    const auto tags = get_standard_tags();

    tag_map_.reserve(tags.size());
    keyword_map_.reserve(tags.size());

    for (const auto& info : tags) {
        tag_map_.emplace(info.tag, info);
        if (!info.keyword.empty()) {
            keyword_map_.emplace(info.keyword, info.tag);
        }
    }

    standard_count_ = tags.size();
}

auto dicom_dictionary::find(dicom_tag tag) const -> std::optional<tag_info> {
    std::shared_lock lock(mutex_);

    const auto it = tag_map_.find(tag);
    if (it != tag_map_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto dicom_dictionary::find_by_keyword(std::string_view keyword) const
    -> std::optional<tag_info> {
    std::shared_lock lock(mutex_);

    const auto it = keyword_map_.find(keyword);
    if (it != keyword_map_.end()) {
        const auto tag_it = tag_map_.find(it->second);
        if (tag_it != tag_map_.end()) {
            return tag_it->second;
        }
    }
    return std::nullopt;
}

auto dicom_dictionary::contains(dicom_tag tag) const -> bool {
    std::shared_lock lock(mutex_);
    return tag_map_.contains(tag);
}

auto dicom_dictionary::contains_keyword(std::string_view keyword) const -> bool {
    std::shared_lock lock(mutex_);
    return keyword_map_.contains(keyword);
}

auto dicom_dictionary::validate_vm(dicom_tag tag, uint32_t count) const -> bool {
    const auto info = find(tag);
    if (!info) {
        return false;
    }
    return info->vm.is_valid(count);
}

auto dicom_dictionary::get_vr(dicom_tag tag) const -> uint16_t {
    const auto info = find(tag);
    if (!info) {
        return 0;
    }
    return info->vr;
}

auto dicom_dictionary::register_private_tag(const tag_info& info) -> bool {
    // Only allow private tags
    if (!info.tag.is_private()) {
        return false;
    }

    std::unique_lock lock(mutex_);

    // Check if already exists
    if (tag_map_.contains(info.tag)) {
        return false;
    }

    tag_map_.emplace(info.tag, info);
    if (!info.keyword.empty()) {
        keyword_map_.emplace(info.keyword, info.tag);
    }

    return true;
}

auto dicom_dictionary::size() const -> size_t {
    std::shared_lock lock(mutex_);
    return tag_map_.size();
}

auto dicom_dictionary::standard_tag_count() const -> size_t {
    return standard_count_;
}

auto dicom_dictionary::private_tag_count() const -> size_t {
    std::shared_lock lock(mutex_);
    return tag_map_.size() - standard_count_;
}

auto dicom_dictionary::get_tags_in_group(uint16_t group) const
    -> std::vector<tag_info> {
    std::shared_lock lock(mutex_);

    std::vector<tag_info> result;
    for (const auto& [tag, info] : tag_map_) {
        if (tag.group() == group) {
            result.push_back(info);
        }
    }

    std::sort(result.begin(), result.end(),
              [](const auto& a, const auto& b) {
                  return a.tag < b.tag;
              });

    return result;
}

}  // namespace kcenon::pacs::core
