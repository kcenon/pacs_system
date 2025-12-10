/**
 * @file uid_mapping.cpp
 * @brief Implementation of UID mapping for de-identification
 *
 * @copyright Copyright (c) 2025
 */

#include "pacs/security/uid_mapping.hpp"

#include <chrono>
#include <random>
#include <sstream>

namespace pacs::security {

uid_mapping::uid_mapping(std::string uid_root)
    : uid_root_{std::move(uid_root)} {}

uid_mapping::uid_mapping(const uid_mapping& other) {
    std::shared_lock lock(other.mutex_);
    uid_root_ = other.uid_root_;
    original_to_anon_ = other.original_to_anon_;
    anon_to_original_ = other.anon_to_original_;
    uid_counter_.store(other.uid_counter_.load());
}

uid_mapping::uid_mapping(uid_mapping&& other) noexcept {
    std::unique_lock lock(other.mutex_);
    uid_root_ = std::move(other.uid_root_);
    original_to_anon_ = std::move(other.original_to_anon_);
    anon_to_original_ = std::move(other.anon_to_original_);
    uid_counter_.store(other.uid_counter_.load());
}

auto uid_mapping::operator=(const uid_mapping& other) -> uid_mapping& {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        uid_root_ = other.uid_root_;
        original_to_anon_ = other.original_to_anon_;
        anon_to_original_ = other.anon_to_original_;
        uid_counter_.store(other.uid_counter_.load());
    }
    return *this;
}

auto uid_mapping::operator=(uid_mapping&& other) noexcept -> uid_mapping& {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        uid_root_ = std::move(other.uid_root_);
        original_to_anon_ = std::move(other.original_to_anon_);
        anon_to_original_ = std::move(other.anon_to_original_);
        uid_counter_.store(other.uid_counter_.load());
    }
    return *this;
}

auto uid_mapping::get_or_create(std::string_view original_uid)
    -> kcenon::common::Result<std::string> {
    // First try read-only lookup
    {
        std::shared_lock lock(mutex_);
        auto it = original_to_anon_.find(original_uid);
        if (it != original_to_anon_.end()) {
            return it->second;
        }
    }

    // Need to create new mapping - upgrade to write lock
    std::unique_lock lock(mutex_);

    // Double-check after acquiring write lock
    auto it = original_to_anon_.find(original_uid);
    if (it != original_to_anon_.end()) {
        return it->second;
    }

    // Generate new UID
    auto new_uid = generate_uid();
    std::string original_str{original_uid};

    original_to_anon_[original_str] = new_uid;
    anon_to_original_[new_uid] = original_str;

    return new_uid;
}

auto uid_mapping::get_anonymized(std::string_view original_uid) const
    -> std::optional<std::string> {
    std::shared_lock lock(mutex_);
    auto it = original_to_anon_.find(original_uid);
    if (it != original_to_anon_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto uid_mapping::get_original(std::string_view anonymized_uid) const
    -> std::optional<std::string> {
    std::shared_lock lock(mutex_);
    auto it = anon_to_original_.find(anonymized_uid);
    if (it != anon_to_original_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto uid_mapping::add_mapping(
    std::string_view original_uid,
    std::string_view anonymized_uid
) -> kcenon::common::VoidResult {
    std::unique_lock lock(mutex_);

    auto existing = original_to_anon_.find(original_uid);
    if (existing != original_to_anon_.end()) {
        if (existing->second != anonymized_uid) {
            return kcenon::common::make_error<std::monostate>(
                1,
                "Original UID already mapped to different value",
                "uid_mapping"
            );
        }
        return kcenon::common::ok();
    }

    std::string original_str{original_uid};
    std::string anon_str{anonymized_uid};

    original_to_anon_[original_str] = anon_str;
    anon_to_original_[anon_str] = original_str;

    return kcenon::common::ok();
}

auto uid_mapping::has_mapping(std::string_view original_uid) const -> bool {
    std::shared_lock lock(mutex_);
    return original_to_anon_.contains(original_uid);
}

auto uid_mapping::size() const -> std::size_t {
    std::shared_lock lock(mutex_);
    return original_to_anon_.size();
}

auto uid_mapping::empty() const -> bool {
    std::shared_lock lock(mutex_);
    return original_to_anon_.empty();
}

void uid_mapping::clear() {
    std::unique_lock lock(mutex_);
    original_to_anon_.clear();
    anon_to_original_.clear();
}

auto uid_mapping::remove(std::string_view original_uid) -> bool {
    std::unique_lock lock(mutex_);

    auto it = original_to_anon_.find(original_uid);
    if (it == original_to_anon_.end()) {
        return false;
    }

    auto anon_uid = it->second;
    original_to_anon_.erase(it);
    anon_to_original_.erase(anon_uid);

    return true;
}

auto uid_mapping::to_json() const -> std::string {
    std::shared_lock lock(mutex_);

    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"uid_root\": \"" << uid_root_ << "\",\n";
    oss << "  \"mappings\": [\n";

    bool first = true;
    for (const auto& [original, anon] : original_to_anon_) {
        if (!first) {
            oss << ",\n";
        }
        oss << "    {\"original\": \"" << original
            << "\", \"anonymized\": \"" << anon << "\"}";
        first = false;
    }

    oss << "\n  ]\n";
    oss << "}";

    return oss.str();
}

auto uid_mapping::from_json(std::string_view /*json*/)
    -> kcenon::common::VoidResult {
    // Simple JSON parsing - in production, use a proper JSON library
    // This is a placeholder implementation
    return kcenon::common::make_error<std::monostate>(
        1, "JSON parsing not yet implemented", "uid_mapping"
    );
}

auto uid_mapping::merge(const uid_mapping& other) -> std::size_t {
    std::shared_lock other_lock(other.mutex_);
    std::unique_lock this_lock(mutex_);

    std::size_t added = 0;
    for (const auto& [original, anon] : other.original_to_anon_) {
        if (!original_to_anon_.contains(original)) {
            original_to_anon_[original] = anon;
            anon_to_original_[anon] = original;
            ++added;
        }
    }

    return added;
}

void uid_mapping::set_uid_root(std::string root) {
    std::unique_lock lock(mutex_);
    uid_root_ = std::move(root);
}

auto uid_mapping::get_uid_root() const -> std::string {
    std::shared_lock lock(mutex_);
    return uid_root_;
}

auto uid_mapping::generate_uid() const -> std::string {
    // Generate UID based on timestamp and counter
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();

    auto counter = uid_counter_.fetch_add(1);

    // Random component for additional uniqueness
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<std::uint32_t> dist(0, 999999);
    auto random_part = dist(gen);

    std::ostringstream oss;
    oss << uid_root_ << "." << timestamp << "." << counter << "." << random_part;

    return oss.str();
}

} // namespace pacs::security
