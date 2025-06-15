/**
 * @file monitoring_service.h
 * @brief Real-time monitoring service for PACS System
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <vector>

#include "core/result/result.h"

namespace pacs {

/**
 * @brief Metric types
 */
enum class MetricType {
    Counter,    // Monotonically increasing value
    Gauge,      // Value that can go up or down
    Histogram,  // Distribution of values
    Timer       // Timing measurements
};

/**
 * @brief Metric value
 */
class MetricValue {
public:
    MetricValue() = default;
    explicit MetricValue(double value) : value_(value) {}
    
    double getValue() const { return value_; }
    void setValue(double value) { value_ = value; }
    
    // For counters
    void increment(double delta = 1.0) { value_ += delta; }
    
    // For histograms
    void record(double sample);
    double getPercentile(double percentile) const;
    double getMean() const;
    double getStdDev() const;
    size_t getCount() const { return samples_.size(); }
    
private:
    double value_ = 0.0;
    std::vector<double> samples_;  // For histogram metrics
};

/**
 * @brief Metric metadata
 */
struct MetricMetadata {
    std::string name;
    MetricType type;
    std::string description;
    std::string unit;  // e.g., "bytes", "ms", "requests"
    std::unordered_map<std::string, std::string> labels;
};

/**
 * @brief Health check result
 */
struct HealthCheckResult {
    bool healthy = true;
    std::string message;
    std::unordered_map<std::string, std::string> details;
    std::chrono::milliseconds checkDuration{0};
};

/**
 * @brief System metrics
 */
struct SystemMetrics {
    // CPU metrics
    double cpuUsagePercent = 0.0;
    double systemCpuPercent = 0.0;
    double userCpuPercent = 0.0;
    
    // Memory metrics
    size_t totalMemoryBytes = 0;
    size_t usedMemoryBytes = 0;
    size_t availableMemoryBytes = 0;
    double memoryUsagePercent = 0.0;
    
    // Disk metrics
    size_t totalDiskBytes = 0;
    size_t usedDiskBytes = 0;
    size_t availableDiskBytes = 0;
    double diskUsagePercent = 0.0;
    
    // Network metrics
    size_t networkBytesIn = 0;
    size_t networkBytesOut = 0;
    size_t networkPacketsIn = 0;
    size_t networkPacketsOut = 0;
    
    // Process metrics
    size_t processMemoryBytes = 0;
    size_t threadCount = 0;
    size_t fileDescriptorCount = 0;
    std::chrono::seconds uptime{0};
};

/**
 * @brief Monitoring service configuration
 */
struct MonitoringConfig {
    bool enabled = true;
    std::chrono::seconds metricsInterval{60};
    std::chrono::seconds healthCheckInterval{30};
    bool collectSystemMetrics = true;
    bool enablePrometheus = false;
    uint16_t prometheusPort = 9090;
    std::string metricsEndpoint = "/metrics";
    size_t maxMetricAge = 3600;  // seconds
};

/**
 * @brief Monitoring service interface
 */
class MonitoringService {
public:
    explicit MonitoringService(const MonitoringConfig& config);
    ~MonitoringService();
    
    /**
     * @brief Start monitoring service
     */
    core::Result<void> start();
    
    /**
     * @brief Stop monitoring service
     */
    void stop();
    
    /**
     * @brief Register a metric
     */
    void registerMetric(const MetricMetadata& metadata);
    
    /**
     * @brief Update metric value
     */
    void updateMetric(const std::string& name, double value,
                      const std::unordered_map<std::string, std::string>& labels = {});
    
    /**
     * @brief Record timing
     */
    void recordTiming(const std::string& name, std::chrono::microseconds duration,
                      const std::unordered_map<std::string, std::string>& labels = {});
    
    /**
     * @brief Increment counter
     */
    void incrementCounter(const std::string& name, double delta = 1.0,
                          const std::unordered_map<std::string, std::string>& labels = {});
    
    /**
     * @brief Set gauge value
     */
    void setGauge(const std::string& name, double value,
                  const std::unordered_map<std::string, std::string>& labels = {});
    
    /**
     * @brief Record histogram sample
     */
    void recordHistogram(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& labels = {});
    
    /**
     * @brief Register health check
     */
    using HealthCheckFunc = std::function<HealthCheckResult()>;
    void registerHealthCheck(const std::string& name, HealthCheckFunc check);
    
    /**
     * @brief Get current system metrics
     */
    SystemMetrics getSystemMetrics() const;
    
    /**
     * @brief Get all metrics
     */
    std::unordered_map<std::string, MetricValue> getAllMetrics() const;
    
    /**
     * @brief Get metric value
     */
    core::Result<MetricValue> getMetric(const std::string& name) const;
    
    /**
     * @brief Run health checks
     */
    std::unordered_map<std::string, HealthCheckResult> runHealthChecks();
    
    /**
     * @brief Get metrics in Prometheus format
     */
    std::string getPrometheusMetrics() const;
    
    /**
     * @brief Is service running
     */
    bool isRunning() const { return running_; }
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    MonitoringConfig config_;
    std::atomic<bool> running_{false};
};

/**
 * @brief Scoped timer for automatic timing measurement
 */
class ScopedTimer {
public:
    ScopedTimer(MonitoringService& service, const std::string& metricName,
                const std::unordered_map<std::string, std::string>& labels = {})
        : service_(service), metricName_(metricName), labels_(labels),
          startTime_(std::chrono::high_resolution_clock::now()) {}
    
    ~ScopedTimer() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime_);
        service_.recordTiming(metricName_, duration, labels_);
    }
    
private:
    MonitoringService& service_;
    std::string metricName_;
    std::unordered_map<std::string, std::string> labels_;
    std::chrono::high_resolution_clock::time_point startTime_;
};

/**
 * @brief Global monitoring instance
 */
class MonitoringManager {
public:
    static MonitoringManager& getInstance() {
        static MonitoringManager instance;
        return instance;
    }
    
    void initialize(const MonitoringConfig& config) {
        service_ = std::make_unique<MonitoringService>(config);
    }
    
    MonitoringService* getService() { return service_.get(); }
    
private:
    MonitoringManager() = default;
    std::unique_ptr<MonitoringService> service_;
};

/**
 * @brief Convenience macros for monitoring
 */
#define MONITOR_COUNTER(name, delta) \
    if (auto* service = MonitoringManager::getInstance().getService()) { \
        service->incrementCounter(name, delta); \
    }

#define MONITOR_GAUGE(name, value) \
    if (auto* service = MonitoringManager::getInstance().getService()) { \
        service->setGauge(name, value); \
    }

#define MONITOR_TIMING(name) \
    std::unique_ptr<ScopedTimer> _timer; \
    if (auto* service = MonitoringManager::getInstance().getService()) { \
        _timer = std::make_unique<ScopedTimer>(*service, name); \
    }

#define MONITOR_HISTOGRAM(name, value) \
    if (auto* service = MonitoringManager::getInstance().getService()) { \
        service->recordHistogram(name, value); \
    }

} // namespace pacs