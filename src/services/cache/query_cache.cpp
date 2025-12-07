/**
 * @file query_cache.cpp
 * @brief Implementation of DICOM query result cache
 */

#include <pacs/services/cache/query_cache.hpp>

#include <algorithm>
#include <mutex>
#include <sstream>

namespace pacs::services::cache {

// ─────────────────────────────────────────────────────
// query_cache Implementation
// ─────────────────────────────────────────────────────

query_cache::query_cache(const query_cache_config& config)
    : config_(config)
    , cache_(cache_config{
          config.max_entries,
          config.ttl,
          config.enable_metrics,
          config.cache_name}) {
}

std::optional<cached_query_result> query_cache::get(const key_type& key) {
    return cache_.get(key);
}

void query_cache::put(const key_type& key, const cached_query_result& result) {
    cache_.put(key, result);
}

void query_cache::put(const key_type& key, cached_query_result&& result) {
    cache_.put(key, std::move(result));
}

bool query_cache::invalidate(const key_type& key) {
    return cache_.invalidate(key);
}

void query_cache::clear() {
    cache_.clear();
}

query_cache::size_type query_cache::purge_expired() {
    return cache_.purge_expired();
}

query_cache::size_type query_cache::size() const {
    return cache_.size();
}

bool query_cache::empty() const {
    return cache_.empty();
}

query_cache::size_type query_cache::max_size() const noexcept {
    return cache_.max_size();
}

const cache_stats& query_cache::stats() const noexcept {
    return cache_.stats();
}

double query_cache::hit_rate() const noexcept {
    return cache_.hit_rate();
}

void query_cache::reset_stats() noexcept {
    cache_.reset_stats();
}

std::string query_cache::build_key(
    const std::string& query_level,
    const std::vector<std::pair<std::string, std::string>>& params) {

    // Sort parameters for consistent key generation
    auto sorted_params = params;
    std::sort(sorted_params.begin(), sorted_params.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::ostringstream oss;
    oss << query_level << ":";

    bool first = true;
    for (const auto& [name, value] : sorted_params) {
        if (!first) {
            oss << ";";
        }
        first = false;
        oss << name << "=" << value;
    }

    return oss.str();
}

std::string query_cache::build_key_with_ae(
    const std::string& calling_ae,
    const std::string& query_level,
    const std::vector<std::pair<std::string, std::string>>& params) {

    return calling_ae + "/" + build_key(query_level, params);
}

// ─────────────────────────────────────────────────────
// Global Query Cache
// ─────────────────────────────────────────────────────

namespace {

std::once_flag g_cache_init_flag;
std::unique_ptr<query_cache> g_query_cache;
query_cache_config g_pending_config;
bool g_config_pending = false;

void init_global_cache() {
    if (g_config_pending) {
        g_query_cache = std::make_unique<query_cache>(g_pending_config);
    } else {
        g_query_cache = std::make_unique<query_cache>();
    }
}

}  // anonymous namespace

query_cache& global_query_cache() {
    std::call_once(g_cache_init_flag, init_global_cache);
    return *g_query_cache;
}

bool configure_global_cache(const query_cache_config& config) {
    // This is a simple check - if init already happened, return false
    // Not perfectly thread-safe for the pending config, but configure
    // should be called at startup before any access
    if (g_query_cache) {
        return false;
    }
    g_pending_config = config;
    g_config_pending = true;
    return true;
}

}  // namespace pacs::services::cache
