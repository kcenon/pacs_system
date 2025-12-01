/**
 * @file monitoring_adapter.cpp
 * @brief Implementation of PACS monitoring adapter
 */

#include <pacs/integration/monitoring_adapter.hpp>

#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace pacs::integration {

// =============================================================================
// Metric Names
// =============================================================================

namespace metrics {
// Counter metrics
constexpr const char* c_store_total = "pacs_c_store_total";
constexpr const char* c_store_success = "pacs_c_store_success";
constexpr const char* c_store_failure = "pacs_c_store_failure";
constexpr const char* c_store_bytes_total = "pacs_c_store_bytes_total";
constexpr const char* c_find_total = "pacs_c_find_total";
constexpr const char* associations_total = "pacs_associations_total";

// Histogram metrics (using timing)
constexpr const char* c_store_duration = "pacs_c_store_duration_seconds";
constexpr const char* c_find_duration = "pacs_c_find_duration_seconds";
constexpr const char* c_find_matches = "pacs_c_find_matches";

// Gauge metrics
constexpr const char* associations_active = "pacs_associations_active";
constexpr const char* storage_instances = "pacs_storage_instances";
constexpr const char* storage_bytes = "pacs_storage_bytes";
}  // namespace metrics

// =============================================================================
// Span Implementation
// =============================================================================

class monitoring_adapter::span::impl {
public:
    explicit impl(std::string_view operation_name) {
        auto& tracer = kcenon::monitoring::global_tracer();
        auto result = tracer.start_span(std::string(operation_name));
        if (result.is_ok()) {
            span_ = result.value();
            valid_ = true;
        }
    }

    ~impl() {
        if (valid_ && span_) {
            auto& tracer = kcenon::monitoring::global_tracer();
            tracer.finish_span(span_);
        }
    }

    void set_tag(std::string_view key, std::string_view value) {
        if (valid_ && span_) {
            span_->tags[std::string(key)] = std::string(value);
        }
    }

    void add_event(std::string_view name) {
        if (valid_ && span_) {
            // Add event as a tag with timestamp
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 now.time_since_epoch())
                                 .count();
            span_->tags["event." + std::string(name)] = std::to_string(timestamp);
        }
    }

    void set_error(const std::exception& e) {
        if (valid_ && span_) {
            span_->status = kcenon::monitoring::trace_span::status_code::error;
            span_->status_message = e.what();
            span_->tags["error"] = "true";
            span_->tags["error.message"] = e.what();
        }
    }

    [[nodiscard]] auto trace_id() const -> std::string {
        if (valid_ && span_) {
            return span_->trace_id;
        }
        return "";
    }

    [[nodiscard]] auto span_id() const -> std::string {
        if (valid_ && span_) {
            return span_->span_id;
        }
        return "";
    }

    [[nodiscard]] auto is_valid() const noexcept -> bool { return valid_; }

private:
    std::shared_ptr<kcenon::monitoring::trace_span> span_;
    bool valid_{false};
};

// Span class implementation
monitoring_adapter::span::span(std::string_view operation_name)
    : impl_(std::make_unique<impl>(operation_name)) {}

monitoring_adapter::span::~span() = default;

monitoring_adapter::span::span(span&& other) noexcept = default;

monitoring_adapter::span& monitoring_adapter::span::operator=(span&& other) noexcept = default;

void monitoring_adapter::span::set_tag(std::string_view key, std::string_view value) {
    if (impl_) {
        impl_->set_tag(key, value);
    }
}

void monitoring_adapter::span::add_event(std::string_view name) {
    if (impl_) {
        impl_->add_event(name);
    }
}

void monitoring_adapter::span::set_error(const std::exception& e) {
    if (impl_) {
        impl_->set_error(e);
    }
}

auto monitoring_adapter::span::trace_id() const -> std::string {
    if (impl_) {
        return impl_->trace_id();
    }
    return "";
}

auto monitoring_adapter::span::span_id() const -> std::string {
    if (impl_) {
        return impl_->span_id();
    }
    return "";
}

auto monitoring_adapter::span::is_valid() const noexcept -> bool {
    return impl_ && impl_->is_valid();
}

// =============================================================================
// Monitoring Adapter Implementation
// =============================================================================

class monitoring_adapter::impl {
public:
    impl() = default;
    ~impl() { shutdown(); }

    void initialize(const monitoring_config& config) {
        std::lock_guard lock(mutex_);

        if (initialized_) {
            return;
        }

        config_ = config;

        // Initialize performance monitor
        if (config.enable_metrics) {
            auto& monitor = kcenon::monitoring::global_performance_monitor();
            monitor.set_enabled(true);
            monitor.get_profiler().set_max_samples(config.max_samples_per_operation);

            auto init_result = monitor.initialize();
            if (init_result.is_err()) {
                // Log warning but continue - metrics will still work locally
            }
        }

        // Initialize counters and gauges to zero
        counters_[metrics::c_store_total] = 0;
        counters_[metrics::c_store_success] = 0;
        counters_[metrics::c_store_failure] = 0;
        counters_[metrics::c_store_bytes_total] = 0;
        counters_[metrics::c_find_total] = 0;
        counters_[metrics::associations_total] = 0;

        gauges_[metrics::associations_active] = 0.0;
        gauges_[metrics::storage_instances] = 0.0;
        gauges_[metrics::storage_bytes] = 0.0;

        initialized_ = true;
    }

    void shutdown() {
        std::lock_guard lock(mutex_);

        if (!initialized_) {
            return;
        }

        // Cleanup performance monitor
        auto& monitor = kcenon::monitoring::global_performance_monitor();
        auto cleanup_result = monitor.cleanup();
        (void)cleanup_result;  // Ignore cleanup errors during shutdown

        initialized_ = false;
    }

    [[nodiscard]] auto is_initialized() const noexcept -> bool {
        return initialized_.load();
    }

    void increment_counter(std::string_view name, std::int64_t value) {
        if (!initialized_) return;

        std::lock_guard lock(counters_mutex_);
        counters_[std::string(name)] += value;
    }

    void set_gauge(std::string_view name, double value) {
        if (!initialized_) return;

        std::lock_guard lock(gauges_mutex_);
        gauges_[std::string(name)] = value;
    }

    void record_histogram(std::string_view name, double value) {
        if (!initialized_ || !config_.enable_metrics) return;

        auto& profiler = kcenon::monitoring::global_performance_monitor().get_profiler();
        // Convert double to nanoseconds for histogram recording
        auto duration = std::chrono::nanoseconds(static_cast<std::int64_t>(value * 1e9));
        profiler.record_sample(std::string(name), duration, true);
    }

    void record_timing(std::string_view name, std::chrono::nanoseconds duration) {
        if (!initialized_ || !config_.enable_metrics) return;

        auto& profiler = kcenon::monitoring::global_performance_monitor().get_profiler();
        profiler.record_sample(std::string(name), duration, true);
    }

    void record_c_store(std::chrono::nanoseconds duration,
                        std::size_t bytes,
                        bool success) {
        if (!initialized_) return;

        // Update counters
        {
            std::lock_guard lock(counters_mutex_);
            counters_[metrics::c_store_total]++;
            if (success) {
                counters_[metrics::c_store_success]++;
            } else {
                counters_[metrics::c_store_failure]++;
            }
            counters_[metrics::c_store_bytes_total] += static_cast<std::int64_t>(bytes);
        }

        // Record timing histogram
        if (config_.enable_metrics) {
            auto& profiler =
                kcenon::monitoring::global_performance_monitor().get_profiler();
            profiler.record_sample(metrics::c_store_duration, duration, success);
        }
    }

    void record_c_find(std::chrono::nanoseconds duration,
                       std::size_t matches,
                       query_level level) {
        if (!initialized_) return;

        // Update counters
        {
            std::lock_guard lock(counters_mutex_);
            counters_[metrics::c_find_total]++;
        }

        // Record timing and matches histogram
        if (config_.enable_metrics) {
            auto& profiler =
                kcenon::monitoring::global_performance_monitor().get_profiler();
            profiler.record_sample(metrics::c_find_duration, duration, true);

            // Record matches as a timing metric (converting to nanoseconds)
            auto matches_as_duration =
                std::chrono::nanoseconds(static_cast<std::int64_t>(matches));
            profiler.record_sample(metrics::c_find_matches, matches_as_duration, true);
        }

        (void)level;  // Level used for tagging in tracing
    }

    void record_association(const std::string& calling_ae, bool established) {
        if (!initialized_) return;

        // Update counters and gauges
        {
            std::lock_guard lock(counters_mutex_);
            counters_[metrics::associations_total]++;
        }

        {
            std::lock_guard lock(gauges_mutex_);
            if (established) {
                gauges_[metrics::associations_active]++;
            } else {
                gauges_[metrics::associations_active] =
                    std::max(0.0, gauges_[metrics::associations_active] - 1);
            }
        }

        (void)calling_ae;  // Used for logging/tagging
    }

    void update_storage_stats(std::size_t total_instances, std::size_t total_bytes) {
        if (!initialized_) return;

        std::lock_guard lock(gauges_mutex_);
        gauges_[metrics::storage_instances] = static_cast<double>(total_instances);
        gauges_[metrics::storage_bytes] = static_cast<double>(total_bytes);
    }

    [[nodiscard]] auto get_health() -> monitoring_adapter::health_status {
        monitoring_adapter::health_status status;
        status.healthy = true;
        status.status = "healthy";

        std::shared_lock lock(health_checks_mutex_);
        for (const auto& [component, check] : health_checks_) {
            try {
                bool component_healthy = check();
                status.components[component] = component_healthy ? "healthy" : "unhealthy";
                if (!component_healthy) {
                    status.healthy = false;
                    status.status = "degraded";
                }
            } catch (const std::exception& e) {
                status.components[component] = std::string("error: ") + e.what();
                status.healthy = false;
                status.status = "degraded";
            }
        }

        return status;
    }

    void register_health_check(std::string_view component,
                               std::function<bool()> check) {
        std::unique_lock lock(health_checks_mutex_);
        health_checks_[std::string(component)] = std::move(check);
    }

    void unregister_health_check(std::string_view component) {
        std::unique_lock lock(health_checks_mutex_);
        health_checks_.erase(std::string(component));
    }

    [[nodiscard]] auto get_config() const -> const monitoring_config& {
        return config_;
    }

private:
    mutable std::mutex mutex_;
    mutable std::mutex counters_mutex_;
    mutable std::mutex gauges_mutex_;
    mutable std::shared_mutex health_checks_mutex_;

    std::atomic<bool> initialized_{false};
    monitoring_config config_;

    // Metric storage
    std::unordered_map<std::string, std::int64_t> counters_;
    std::unordered_map<std::string, double> gauges_;
    std::unordered_map<std::string, std::function<bool()>> health_checks_;
};

// =============================================================================
// Static Member Initialization
// =============================================================================

std::unique_ptr<monitoring_adapter::impl> monitoring_adapter::pimpl_ =
    std::make_unique<monitoring_adapter::impl>();

// =============================================================================
// Initialization
// =============================================================================

void monitoring_adapter::initialize(const monitoring_config& config) {
    pimpl_->initialize(config);
}

void monitoring_adapter::shutdown() { pimpl_->shutdown(); }

auto monitoring_adapter::is_initialized() noexcept -> bool {
    return pimpl_->is_initialized();
}

// =============================================================================
// Metrics
// =============================================================================

void monitoring_adapter::increment_counter(std::string_view name,
                                           std::int64_t value) {
    pimpl_->increment_counter(name, value);
}

void monitoring_adapter::set_gauge(std::string_view name, double value) {
    pimpl_->set_gauge(name, value);
}

void monitoring_adapter::record_histogram(std::string_view name, double value) {
    pimpl_->record_histogram(name, value);
}

void monitoring_adapter::record_timing(std::string_view name,
                                       std::chrono::nanoseconds duration) {
    pimpl_->record_timing(name, duration);
}

// =============================================================================
// DICOM-Specific Metrics
// =============================================================================

void monitoring_adapter::record_c_store(std::chrono::nanoseconds duration,
                                        std::size_t bytes,
                                        bool success) {
    pimpl_->record_c_store(duration, bytes, success);
}

void monitoring_adapter::record_c_find(std::chrono::nanoseconds duration,
                                       std::size_t matches,
                                       query_level level) {
    pimpl_->record_c_find(duration, matches, level);
}

void monitoring_adapter::record_association(const std::string& calling_ae,
                                            bool established) {
    pimpl_->record_association(calling_ae, established);
}

void monitoring_adapter::update_storage_stats(std::size_t total_instances,
                                              std::size_t total_bytes) {
    pimpl_->update_storage_stats(total_instances, total_bytes);
}

// =============================================================================
// Distributed Tracing
// =============================================================================

auto monitoring_adapter::start_span(std::string_view operation) -> span {
    return span(operation);
}

// =============================================================================
// Health Check
// =============================================================================

auto monitoring_adapter::get_health() -> health_status {
    return pimpl_->get_health();
}

void monitoring_adapter::register_health_check(std::string_view component,
                                               std::function<bool()> check) {
    pimpl_->register_health_check(component, std::move(check));
}

void monitoring_adapter::unregister_health_check(std::string_view component) {
    pimpl_->unregister_health_check(component);
}

// =============================================================================
// Configuration
// =============================================================================

auto monitoring_adapter::get_config() -> const monitoring_config& {
    return pimpl_->get_config();
}

// =============================================================================
// Private Helpers
// =============================================================================

auto monitoring_adapter::query_level_to_string(query_level level) -> std::string {
    switch (level) {
        case query_level::patient:
            return "PATIENT";
        case query_level::study:
            return "STUDY";
        case query_level::series:
            return "SERIES";
        case query_level::image:
            return "IMAGE";
        default:
            return "UNKNOWN";
    }
}

}  // namespace pacs::integration
