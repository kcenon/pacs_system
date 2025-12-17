/**
 * @file study_lock_manager.cpp
 * @brief Implementation of study lock manager
 */

#include "pacs/workflow/study_lock_manager.hpp"

#include <kcenon/common/patterns/result.h>

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

namespace pacs::workflow {

// =============================================================================
// Construction / Destruction
// =============================================================================

study_lock_manager::study_lock_manager()
    : config_{} {}

study_lock_manager::study_lock_manager(const study_lock_manager_config& config)
    : config_{config} {}

study_lock_manager::~study_lock_manager() {
    // Release all locks on destruction
    std::unique_lock lock{mutex_};
    locks_.clear();
    token_to_study_.clear();
}

study_lock_manager::study_lock_manager(study_lock_manager&& other) noexcept
    : config_{std::move(other.config_)},
      locks_{std::move(other.locks_)},
      token_to_study_{std::move(other.token_to_study_)},
      stats_{std::move(other.stats_)},
      next_token_id_{other.next_token_id_.load()},
      on_lock_acquired_{std::move(other.on_lock_acquired_)},
      on_lock_released_{std::move(other.on_lock_released_)},
      on_lock_expired_{std::move(other.on_lock_expired_)} {}

study_lock_manager& study_lock_manager::operator=(
    study_lock_manager&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock{mutex_};
        config_ = std::move(other.config_);
        locks_ = std::move(other.locks_);
        token_to_study_ = std::move(other.token_to_study_);
        stats_ = std::move(other.stats_);
        next_token_id_.store(other.next_token_id_.load());
        on_lock_acquired_ = std::move(other.on_lock_acquired_);
        on_lock_released_ = std::move(other.on_lock_released_);
        on_lock_expired_ = std::move(other.on_lock_expired_);
    }
    return *this;
}

// =============================================================================
// Lock Acquisition
// =============================================================================

auto study_lock_manager::lock(
    const std::string& study_uid,
    const std::string& reason,
    const std::string& holder,
    std::chrono::seconds timeout) -> kcenon::common::Result<lock_token> {
    return lock(study_uid, lock_type::exclusive, reason, holder, timeout);
}

auto study_lock_manager::lock(
    const std::string& study_uid,
    lock_type type,
    const std::string& reason,
    const std::string& holder,
    std::chrono::seconds timeout) -> kcenon::common::Result<lock_token> {
    const auto resolved_holder = resolve_holder(holder);
    const auto expiry = calculate_expiry(timeout);
    const auto now = std::chrono::system_clock::now();

    std::unique_lock lock{mutex_};

    // Check if lock can be acquired
    auto it = locks_.find(study_uid);
    if (it != locks_.end()) {
        auto& existing = it->second;

        // Check if existing lock is expired
        if (existing.info.is_expired()) {
            // Clean up expired lock
            auto token_it = token_to_study_.find(existing.info.token_id);
            if (token_it != token_to_study_.end()) {
                token_to_study_.erase(token_it);
            }
            locks_.erase(it);
            it = locks_.end();

            // Notify expiration
            if (on_lock_expired_) {
                lock.unlock();
                on_lock_expired_(study_uid, existing.info);
                lock.lock();
            }
        }
    }

    // Re-check after potential cleanup
    it = locks_.find(study_uid);
    if (it != locks_.end()) {
        auto& existing = it->second;

        // Handle shared lock upgrade/coexistence
        if (type == lock_type::shared &&
            existing.info.type == lock_type::shared) {
            // Check max shared locks
            if (existing.shared_holders.size() >= config_.max_shared_locks) {
                return kcenon::common::Result<lock_token>::err(
                    kcenon::common::error_info{
                        lock_error::max_shared_exceeded,
                        "Maximum shared locks exceeded",
                        "study_lock_manager"});
            }

            // Add holder to shared list
            existing.shared_holders.push_back(resolved_holder);
            existing.info.shared_count = existing.shared_holders.size();

            // Generate new token for this shared holder
            const auto token_id = generate_token_id();
            token_to_study_[token_id] = study_uid;

            lock_token token;
            token.token_id = token_id;
            token.study_uid = study_uid;
            token.type = lock_type::shared;
            token.acquired_at = now;
            token.expires_at = expiry;

            record_acquisition(type);

            if (on_lock_acquired_) {
                lock_info info = existing.info;
                info.token_id = token_id;
                info.holder = resolved_holder;
                lock.unlock();
                on_lock_acquired_(study_uid, info);
            }

            return kcenon::common::Result<lock_token>::ok(token);
        }

        // Cannot acquire lock - already locked
        {
            std::lock_guard stats_lock{stats_mutex_};
            ++stats_.contention_count;
        }

        return kcenon::common::Result<lock_token>::err(
            kcenon::common::error_info{
                lock_error::already_locked,
                "Study is already locked by: " + existing.info.holder,
                "study_lock_manager",
                "Lock type: " + to_string(existing.info.type)});
    }

    // Create new lock
    const auto token_id = generate_token_id();

    lock_entry entry;
    entry.info.study_uid = study_uid;
    entry.info.type = type;
    entry.info.reason = reason;
    entry.info.holder = resolved_holder;
    entry.info.token_id = token_id;
    entry.info.acquired_at = now;
    entry.info.expires_at = expiry;
    entry.info.shared_count = (type == lock_type::shared) ? 1 : 0;

    if (type == lock_type::shared) {
        entry.shared_holders.push_back(resolved_holder);
    }

    locks_[study_uid] = std::move(entry);
    token_to_study_[token_id] = study_uid;

    lock_token token;
    token.token_id = token_id;
    token.study_uid = study_uid;
    token.type = type;
    token.acquired_at = now;
    token.expires_at = expiry;

    record_acquisition(type);

    if (on_lock_acquired_) {
        lock_info info = locks_[study_uid].info;
        lock.unlock();
        on_lock_acquired_(study_uid, info);
    }

    return kcenon::common::Result<lock_token>::ok(token);
}

auto study_lock_manager::try_lock(
    const std::string& study_uid,
    lock_type type,
    const std::string& reason,
    const std::string& holder,
    std::chrono::seconds timeout) -> kcenon::common::Result<lock_token> {
    // try_lock is the same as lock for this implementation
    // since we don't block waiting
    return lock(study_uid, type, reason, holder, timeout);
}

// =============================================================================
// Lock Release
// =============================================================================

auto study_lock_manager::unlock(const lock_token& token)
    -> kcenon::common::Result<std::monostate> {
    std::unique_lock lock{mutex_};

    // Find study by token
    auto token_it = token_to_study_.find(token.token_id);
    if (token_it == token_to_study_.end()) {
        return kcenon::common::Result<std::monostate>::err(
            kcenon::common::error_info{
                lock_error::invalid_token,
                "Invalid or expired token",
                "study_lock_manager"});
    }

    const auto study_uid = token_it->second;
    auto lock_it = locks_.find(study_uid);
    if (lock_it == locks_.end()) {
        // Token exists but lock doesn't - clean up
        token_to_study_.erase(token_it);
        return kcenon::common::Result<std::monostate>::err(
            kcenon::common::error_info{
                lock_error::not_found,
                "Lock not found",
                "study_lock_manager"});
    }

    auto& entry = lock_it->second;
    const auto duration = entry.info.duration();
    const auto type = entry.info.type;
    lock_info released_info = entry.info;

    // Handle shared lock release
    if (type == lock_type::shared && entry.shared_holders.size() > 1) {
        // Just remove this holder from shared list
        auto holder_it = std::find(
            entry.shared_holders.begin(),
            entry.shared_holders.end(),
            token.token_id);  // Using token_id as holder identifier for shared

        if (holder_it != entry.shared_holders.end()) {
            entry.shared_holders.erase(holder_it);
        }
        token_to_study_.erase(token_it);
        entry.info.shared_count = entry.shared_holders.size();

        record_release(type, duration);

        if (on_lock_released_) {
            lock.unlock();
            on_lock_released_(study_uid, released_info);
        }

        return kcenon::common::Result<std::monostate>::ok(std::monostate{});
    }

    // Remove lock entirely
    token_to_study_.erase(token_it);
    locks_.erase(lock_it);

    record_release(type, duration);

    if (on_lock_released_) {
        lock.unlock();
        on_lock_released_(study_uid, released_info);
    }

    return kcenon::common::Result<std::monostate>::ok(std::monostate{});
}

auto study_lock_manager::unlock(
    const std::string& study_uid,
    const std::string& holder)
    -> kcenon::common::Result<std::monostate> {
    const auto resolved_holder = resolve_holder(holder);

    std::unique_lock lock{mutex_};

    auto lock_it = locks_.find(study_uid);
    if (lock_it == locks_.end()) {
        return kcenon::common::Result<std::monostate>::err(
            kcenon::common::error_info{
                lock_error::not_found,
                "Lock not found for study",
                "study_lock_manager"});
    }

    auto& entry = lock_it->second;

    // Verify holder matches
    if (entry.info.holder != resolved_holder) {
        // Check shared holders
        bool found_shared = false;
        if (entry.info.type == lock_type::shared) {
            auto holder_it = std::find(
                entry.shared_holders.begin(),
                entry.shared_holders.end(),
                resolved_holder);
            if (holder_it != entry.shared_holders.end()) {
                found_shared = true;
                entry.shared_holders.erase(holder_it);
                entry.info.shared_count = entry.shared_holders.size();
            }
        }

        if (!found_shared) {
            return kcenon::common::Result<std::monostate>::err(
                kcenon::common::error_info{
                    lock_error::permission_denied,
                    "Lock held by different holder: " + entry.info.holder,
                    "study_lock_manager"});
        }

        if (!entry.shared_holders.empty()) {
            // Other shared holders remain
            record_release(entry.info.type, entry.info.duration());
            return kcenon::common::Result<std::monostate>::ok(std::monostate{});
        }
    }

    const auto duration = entry.info.duration();
    const auto type = entry.info.type;
    lock_info released_info = entry.info;

    // Remove token mapping
    auto token_it = token_to_study_.find(entry.info.token_id);
    if (token_it != token_to_study_.end()) {
        token_to_study_.erase(token_it);
    }

    locks_.erase(lock_it);

    record_release(type, duration);

    if (on_lock_released_) {
        lock.unlock();
        on_lock_released_(study_uid, released_info);
    }

    return kcenon::common::Result<std::monostate>::ok(std::monostate{});
}

auto study_lock_manager::force_unlock(
    const std::string& study_uid,
    [[maybe_unused]] const std::string& admin_reason)
    -> kcenon::common::Result<std::monostate> {
    // Note: admin_reason can be used for audit logging in future
    if (!config_.allow_force_unlock) {
        return kcenon::common::Result<std::monostate>::err(
            kcenon::common::error_info{
                lock_error::permission_denied,
                "Force unlock is not allowed",
                "study_lock_manager"});
    }

    std::unique_lock lock{mutex_};

    auto lock_it = locks_.find(study_uid);
    if (lock_it == locks_.end()) {
        return kcenon::common::Result<std::monostate>::err(
            kcenon::common::error_info{
                lock_error::not_found,
                "Lock not found for study",
                "study_lock_manager"});
    }

    const auto duration = lock_it->second.info.duration();
    const auto type = lock_it->second.info.type;
    lock_info released_info = lock_it->second.info;

    // Remove token mapping
    auto token_it = token_to_study_.find(lock_it->second.info.token_id);
    if (token_it != token_to_study_.end()) {
        token_to_study_.erase(token_it);
    }

    // Remove all shared holder tokens
    for (const auto& shared_holder : lock_it->second.shared_holders) {
        // Shared holders may have separate tokens - clean up if needed
        (void)shared_holder;
    }

    locks_.erase(lock_it);

    {
        std::lock_guard stats_lock{stats_mutex_};
        ++stats_.force_unlock_count;
    }
    record_release(type, duration);

    if (on_lock_released_) {
        lock.unlock();
        on_lock_released_(study_uid, released_info);
    }

    return kcenon::common::Result<std::monostate>::ok(std::monostate{});
}

auto study_lock_manager::unlock_all_by_holder(const std::string& holder)
    -> std::size_t {
    const auto resolved_holder = resolve_holder(holder);
    std::size_t count = 0;

    std::unique_lock lock{mutex_};

    std::vector<std::string> to_remove;
    for (auto& [study_uid, entry] : locks_) {
        if (entry.info.holder == resolved_holder) {
            to_remove.push_back(study_uid);
        } else if (entry.info.type == lock_type::shared) {
            // Check shared holders
            auto it = std::find(
                entry.shared_holders.begin(),
                entry.shared_holders.end(),
                resolved_holder);
            if (it != entry.shared_holders.end()) {
                entry.shared_holders.erase(it);
                entry.info.shared_count = entry.shared_holders.size();
                ++count;

                if (entry.shared_holders.empty()) {
                    to_remove.push_back(study_uid);
                }
            }
        }
    }

    for (const auto& study_uid : to_remove) {
        auto it = locks_.find(study_uid);
        if (it != locks_.end()) {
            // Remove token mapping
            auto token_it = token_to_study_.find(it->second.info.token_id);
            if (token_it != token_to_study_.end()) {
                token_to_study_.erase(token_it);
            }

            record_release(it->second.info.type, it->second.info.duration());
            locks_.erase(it);
            ++count;
        }
    }

    return count;
}

// =============================================================================
// Lock Status
// =============================================================================

auto study_lock_manager::is_locked(const std::string& study_uid) const -> bool {
    std::shared_lock lock{mutex_};

    auto it = locks_.find(study_uid);
    if (it == locks_.end()) {
        return false;
    }

    // Check if expired
    if (it->second.info.is_expired()) {
        return false;
    }

    return true;
}

auto study_lock_manager::is_locked(
    const std::string& study_uid,
    lock_type type) const -> bool {
    std::shared_lock lock{mutex_};

    auto it = locks_.find(study_uid);
    if (it == locks_.end()) {
        return false;
    }

    if (it->second.info.is_expired()) {
        return false;
    }

    return it->second.info.type == type;
}

auto study_lock_manager::get_lock_info(const std::string& study_uid) const
    -> std::optional<lock_info> {
    std::shared_lock lock{mutex_};

    auto it = locks_.find(study_uid);
    if (it == locks_.end()) {
        return std::nullopt;
    }

    if (it->second.info.is_expired()) {
        return std::nullopt;
    }

    return it->second.info;
}

auto study_lock_manager::get_lock_info_by_token(
    const std::string& token_id) const
    -> std::optional<lock_info> {
    std::shared_lock lock{mutex_};

    auto token_it = token_to_study_.find(token_id);
    if (token_it == token_to_study_.end()) {
        return std::nullopt;
    }

    auto lock_it = locks_.find(token_it->second);
    if (lock_it == locks_.end()) {
        return std::nullopt;
    }

    if (lock_it->second.info.is_expired()) {
        return std::nullopt;
    }

    return lock_it->second.info;
}

auto study_lock_manager::validate_token(const lock_token& token) const -> bool {
    std::shared_lock lock{mutex_};

    auto token_it = token_to_study_.find(token.token_id);
    if (token_it == token_to_study_.end()) {
        return false;
    }

    auto lock_it = locks_.find(token_it->second);
    if (lock_it == locks_.end()) {
        return false;
    }

    if (lock_it->second.info.is_expired()) {
        return false;
    }

    return token_it->second == token.study_uid;
}

auto study_lock_manager::refresh_lock(
    const lock_token& token,
    std::chrono::seconds extension)
    -> kcenon::common::Result<lock_token> {
    std::unique_lock lock{mutex_};

    auto token_it = token_to_study_.find(token.token_id);
    if (token_it == token_to_study_.end()) {
        return kcenon::common::Result<lock_token>::err(
            kcenon::common::error_info{
                lock_error::invalid_token,
                "Invalid or expired token",
                "study_lock_manager"});
    }

    auto lock_it = locks_.find(token_it->second);
    if (lock_it == locks_.end()) {
        return kcenon::common::Result<lock_token>::err(
            kcenon::common::error_info{
                lock_error::not_found,
                "Lock not found",
                "study_lock_manager"});
    }

    if (lock_it->second.info.is_expired()) {
        return kcenon::common::Result<lock_token>::err(
            kcenon::common::error_info{
                lock_error::expired,
                "Lock has expired",
                "study_lock_manager"});
    }

    // Calculate new expiry
    auto new_extension = extension.count() > 0 ? extension : config_.default_timeout;
    if (new_extension.count() > 0) {
        lock_it->second.info.expires_at =
            std::chrono::system_clock::now() + new_extension;
    }

    lock_token updated_token = token;
    updated_token.expires_at = lock_it->second.info.expires_at;

    return kcenon::common::Result<lock_token>::ok(updated_token);
}

// =============================================================================
// Lock Queries
// =============================================================================

auto study_lock_manager::get_all_locks() const -> std::vector<lock_info> {
    std::shared_lock lock{mutex_};

    std::vector<lock_info> result;
    result.reserve(locks_.size());

    for (const auto& [study_uid, entry] : locks_) {
        if (!entry.info.is_expired()) {
            result.push_back(entry.info);
        }
    }

    return result;
}

auto study_lock_manager::get_locks_by_holder(const std::string& holder) const
    -> std::vector<lock_info> {
    const auto resolved_holder = resolve_holder(holder);
    std::shared_lock lock{mutex_};

    std::vector<lock_info> result;

    for (const auto& [study_uid, entry] : locks_) {
        if (entry.info.is_expired()) continue;

        if (entry.info.holder == resolved_holder) {
            result.push_back(entry.info);
        } else if (entry.info.type == lock_type::shared) {
            auto it = std::find(
                entry.shared_holders.begin(),
                entry.shared_holders.end(),
                resolved_holder);
            if (it != entry.shared_holders.end()) {
                result.push_back(entry.info);
            }
        }
    }

    return result;
}

auto study_lock_manager::get_locks_by_type(lock_type type) const
    -> std::vector<lock_info> {
    std::shared_lock lock{mutex_};

    std::vector<lock_info> result;

    for (const auto& [study_uid, entry] : locks_) {
        if (!entry.info.is_expired() && entry.info.type == type) {
            result.push_back(entry.info);
        }
    }

    return result;
}

auto study_lock_manager::get_expired_locks() const -> std::vector<lock_info> {
    std::shared_lock lock{mutex_};

    std::vector<lock_info> result;

    for (const auto& [study_uid, entry] : locks_) {
        if (entry.info.is_expired()) {
            result.push_back(entry.info);
        }
    }

    return result;
}

// =============================================================================
// Maintenance
// =============================================================================

auto study_lock_manager::cleanup_expired_locks() -> std::size_t {
    std::unique_lock lock{mutex_};

    std::vector<std::string> expired_studies;

    for (const auto& [study_uid, entry] : locks_) {
        if (entry.info.is_expired()) {
            expired_studies.push_back(study_uid);
        }
    }

    for (const auto& study_uid : expired_studies) {
        auto it = locks_.find(study_uid);
        if (it != locks_.end()) {
            lock_info expired_info = it->second.info;

            // Remove token mapping
            auto token_it = token_to_study_.find(it->second.info.token_id);
            if (token_it != token_to_study_.end()) {
                token_to_study_.erase(token_it);
            }

            locks_.erase(it);

            if (on_lock_expired_) {
                lock.unlock();
                on_lock_expired_(study_uid, expired_info);
                lock.lock();
            }
        }
    }

    return expired_studies.size();
}

auto study_lock_manager::get_stats() const -> lock_manager_stats {
    std::lock_guard stats_lock{stats_mutex_};
    std::shared_lock lock{mutex_};

    lock_manager_stats current_stats = stats_;
    current_stats.active_locks = 0;
    current_stats.exclusive_locks = 0;
    current_stats.shared_locks = 0;
    current_stats.migration_locks = 0;

    for (const auto& [study_uid, entry] : locks_) {
        if (!entry.info.is_expired()) {
            ++current_stats.active_locks;
            switch (entry.info.type) {
                case lock_type::exclusive:
                    ++current_stats.exclusive_locks;
                    break;
                case lock_type::shared:
                    ++current_stats.shared_locks;
                    break;
                case lock_type::migration:
                    ++current_stats.migration_locks;
                    break;
            }
        }
    }

    return current_stats;
}

void study_lock_manager::reset_stats() {
    std::lock_guard stats_lock{stats_mutex_};
    stats_ = lock_manager_stats{};
}

auto study_lock_manager::get_config() const
    -> const study_lock_manager_config& {
    return config_;
}

void study_lock_manager::set_config(const study_lock_manager_config& config) {
    std::unique_lock lock{mutex_};
    config_ = config;
}

// =============================================================================
// Event Callbacks
// =============================================================================

void study_lock_manager::set_on_lock_acquired(lock_event_callback callback) {
    on_lock_acquired_ = std::move(callback);
}

void study_lock_manager::set_on_lock_released(lock_event_callback callback) {
    on_lock_released_ = std::move(callback);
}

void study_lock_manager::set_on_lock_expired(lock_event_callback callback) {
    on_lock_expired_ = std::move(callback);
}

// =============================================================================
// Internal Methods
// =============================================================================

auto study_lock_manager::generate_token_id() const -> std::string {
    const auto id = next_token_id_.fetch_add(1);
    const auto now = std::chrono::system_clock::now();
    const auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::ostringstream oss;
    oss << "lock_" << std::hex << std::setfill('0')
        << std::setw(12) << time_ms
        << "_" << std::setw(8) << id;

    return oss.str();
}

auto study_lock_manager::resolve_holder(const std::string& holder) const
    -> std::string {
    if (!holder.empty()) {
        return holder;
    }

    // Use thread ID as default holder
    std::ostringstream oss;
    oss << "thread_" << std::this_thread::get_id();
    return oss.str();
}

auto study_lock_manager::calculate_expiry(std::chrono::seconds timeout) const
    -> std::optional<std::chrono::system_clock::time_point> {
    auto effective_timeout = timeout.count() > 0 ? timeout : config_.default_timeout;
    if (effective_timeout.count() <= 0) {
        return std::nullopt;  // No expiration
    }

    return std::chrono::system_clock::now() + effective_timeout;
}

auto study_lock_manager::can_acquire_lock(
    const std::string& study_uid,
    lock_type type) const -> bool {
    auto it = locks_.find(study_uid);
    if (it == locks_.end()) {
        return true;
    }

    if (it->second.info.is_expired()) {
        return true;
    }

    // Shared locks can coexist with other shared locks
    if (type == lock_type::shared &&
        it->second.info.type == lock_type::shared) {
        return it->second.shared_holders.size() < config_.max_shared_locks;
    }

    return false;
}

void study_lock_manager::record_acquisition(lock_type type) {
    std::lock_guard stats_lock{stats_mutex_};
    ++stats_.total_acquisitions;
    (void)type;  // Type-specific stats are calculated in get_stats()
}

void study_lock_manager::record_release(
    lock_type type,
    std::chrono::milliseconds duration) {
    std::lock_guard stats_lock{stats_mutex_};
    ++stats_.total_releases;

    // Update average duration
    if (stats_.total_releases > 0) {
        auto total_duration =
            stats_.avg_lock_duration.count() * (stats_.total_releases - 1) +
            duration.count();
        stats_.avg_lock_duration =
            std::chrono::milliseconds{total_duration / stats_.total_releases};
    }

    // Update max duration
    if (duration > stats_.max_lock_duration) {
        stats_.max_lock_duration = duration;
    }

    (void)type;  // Type-specific stats are calculated in get_stats()
}

}  // namespace pacs::workflow
