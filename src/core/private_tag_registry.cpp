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
 * @file private_tag_registry.cpp
 * @brief Implementation of vendor-specific private tag registry
 */

#include <pacs/core/private_tag_registry.hpp>

namespace pacs::core {

auto private_tag_registry::instance() -> private_tag_registry& {
    static private_tag_registry registry;
    return registry;
}

auto private_tag_registry::register_tag(std::string_view creator_id,
                                        const private_tag_definition& definition)
    -> bool {
    std::unique_lock lock(mutex_);
    auto key = key_type{std::string{creator_id}, definition.element_offset};
    auto [it, inserted] = definitions_.try_emplace(std::move(key), definition);
    return inserted;
}

auto private_tag_registry::register_vendor(
    std::string_view creator_id,
    std::span<const private_tag_definition> definitions) -> size_t {
    std::unique_lock lock(mutex_);
    size_t count = 0;

    for (const auto& def : definitions) {
        auto key = key_type{std::string{creator_id}, def.element_offset};
        auto [it, inserted] = definitions_.try_emplace(std::move(key), def);
        if (inserted) {
            ++count;
        }
    }

    return count;
}

auto private_tag_registry::find(std::string_view creator_id,
                                uint8_t element_offset) const
    -> std::optional<private_tag_definition> {
    std::shared_lock lock(mutex_);
    auto key = key_type{std::string{creator_id}, element_offset};
    auto it = definitions_.find(key);
    if (it == definitions_.end()) {
        return std::nullopt;
    }
    return it->second;
}

auto private_tag_registry::size() const -> size_t {
    std::shared_lock lock(mutex_);
    return definitions_.size();
}

void private_tag_registry::clear() {
    std::unique_lock lock(mutex_);
    definitions_.clear();
}

}  // namespace pacs::core
