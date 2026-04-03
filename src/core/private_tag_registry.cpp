// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file private_tag_registry.cpp
 * @brief Implementation of vendor-specific private tag registry
 */

#include <kcenon/pacs/core/private_tag_registry.h>

namespace kcenon::pacs::core {

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

}  // namespace kcenon::pacs::core
