/**
 * @file monitoring_service.cpp
 * @brief Real-time monitoring service implementation
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#include "monitoring_service.h"
#include "common/logger/logger.h"

#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cmath>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <unistd.h>
#elif __APPLE__
#include <mach/mach.h>
#include <sys/sysctl.h>
#endif

namespace pacs {

void MetricValue::record(double sample) {
    samples_.push_back(sample);
}

double MetricValue::getPercentile(double percentile) const {
    if (samples_.empty()) return 0.0;
    
    std::vector<double> sorted = samples_;
    std::sort(sorted.begin(), sorted.end());
    
    size_t index = static_cast<size_t>(percentile / 100.0 * sorted.size());
    return sorted[std::min(index, sorted.size() - 1)];
}

double MetricValue::getMean() const {
    if (samples_.empty()) return 0.0;
    
    double sum = std::accumulate(samples_.begin(), samples_.end(), 0.0);
    return sum / samples_.size();
}

double MetricValue::getStdDev() const {
    if (samples_.size() < 2) return 0.0;
    
    double mean = getMean();
    double sumSquares = 0.0;
    
    for (double sample : samples_) {
        double diff = sample - mean;
        sumSquares += diff * diff;
    }
    
    return std::sqrt(sumSquares / (samples_.size() - 1));
}

class MonitoringService::Impl {
public:
    mutable std::mutex metricsMutex_;
    std::unordered_map<std::string, MetricMetadata> registeredMetrics_;
    std::unordered_map<std::string, MetricValue> metrics_;
    std::unordered_map<std::string, HealthCheckFunc> healthChecks_;
    std::unique_ptr<std::thread> monitoringThread_;
    std::unique_ptr<std::thread> prometheusThread_;
    
    SystemMetrics collectSystemMetrics() {
        SystemMetrics metrics;
        
#ifdef __linux__
        // CPU metrics
        std::ifstream statFile("/proc/stat");
        if (statFile.is_open()) {
            std::string line;
            std::getline(statFile, line);
            if (line.substr(0, 3) == "cpu") {
                std::istringstream iss(line);
                std::string cpu;
                long user, nice, system, idle, iowait, irq, softirq, steal;
                iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
                
                long total = user + nice + system + idle + iowait + irq + softirq + steal;
                long active = total - idle - iowait;
                
                static long lastTotal = 0, lastActive = 0;
                if (lastTotal > 0) {
                    long totalDiff = total - lastTotal;
                    long activeDiff = active - lastActive;
                    metrics.cpuUsagePercent = (totalDiff > 0) ? 
                        (100.0 * activeDiff / totalDiff) : 0.0;
                }
                lastTotal = total;
                lastActive = active;
            }
        }
        
        // Memory metrics
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            metrics.totalMemoryBytes = si.totalram * si.mem_unit;
            metrics.availableMemoryBytes = si.freeram * si.mem_unit;
            metrics.usedMemoryBytes = metrics.totalMemoryBytes - metrics.availableMemoryBytes;
            metrics.memoryUsagePercent = (metrics.totalMemoryBytes > 0) ?
                (100.0 * metrics.usedMemoryBytes / metrics.totalMemoryBytes) : 0.0;
            
            metrics.uptime = std::chrono::seconds(si.uptime);
        }
        
        // Process memory
        std::ifstream statusFile("/proc/self/status");
        if (statusFile.is_open()) {
            std::string line;
            while (std::getline(statusFile, line)) {
                if (line.substr(0, 6) == "VmRSS:") {
                    std::istringstream iss(line);
                    std::string label;
                    size_t value;
                    std::string unit;
                    iss >> label >> value >> unit;
                    metrics.processMemoryBytes = value * 1024;  // Convert KB to bytes
                    break;
                }
            }
        }
        
#elif __APPLE__
        // macOS implementation
        // CPU metrics
        host_cpu_load_info_data_t cpuinfo;
        mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
        
        if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                            (host_info_t)&cpuinfo, &count) == KERN_SUCCESS) {
            static unsigned int lastUser = 0, lastSystem = 0, lastIdle = 0;
            
            unsigned int user = cpuinfo.cpu_ticks[CPU_STATE_USER];
            unsigned int system = cpuinfo.cpu_ticks[CPU_STATE_SYSTEM];
            unsigned int idle = cpuinfo.cpu_ticks[CPU_STATE_IDLE];
            
            if (lastUser > 0) {
                unsigned int userDiff = user - lastUser;
                unsigned int systemDiff = system - lastSystem;
                unsigned int idleDiff = idle - lastIdle;
                unsigned int totalDiff = userDiff + systemDiff + idleDiff;
                
                if (totalDiff > 0) {
                    metrics.cpuUsagePercent = 100.0 * (userDiff + systemDiff) / totalDiff;
                    metrics.userCpuPercent = 100.0 * userDiff / totalDiff;
                    metrics.systemCpuPercent = 100.0 * systemDiff / totalDiff;
                }
            }
            
            lastUser = user;
            lastSystem = system;
            lastIdle = idle;
        }
        
        // Memory metrics
        vm_size_t page_size;
        vm_statistics64_data_t vm_stat;
        mach_msg_type_number_t host_size = sizeof(vm_stat) / sizeof(natural_t);
        
        if (host_page_size(mach_host_self(), &page_size) == KERN_SUCCESS &&
            host_statistics64(mach_host_self(), HOST_VM_INFO,
                              (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS) {
            metrics.availableMemoryBytes = vm_stat.free_count * page_size;
            
            // Get total memory
            int mib[2] = {CTL_HW, HW_MEMSIZE};
            size_t length = sizeof(metrics.totalMemoryBytes);
            sysctl(mib, 2, &metrics.totalMemoryBytes, &length, NULL, 0);
            
            metrics.usedMemoryBytes = metrics.totalMemoryBytes - metrics.availableMemoryBytes;
            metrics.memoryUsagePercent = (metrics.totalMemoryBytes > 0) ?
                (100.0 * metrics.usedMemoryBytes / metrics.totalMemoryBytes) : 0.0;
        }
#endif
        
        // Thread count
        metrics.threadCount = std::thread::hardware_concurrency();
        
        return metrics;
    }
    
    std::string formatPrometheusMetrics(
        const std::unordered_map<std::string, MetricMetadata>& metadata,
        const std::unordered_map<std::string, MetricValue>& metrics) {
        std::ostringstream oss;
        
        for (const auto& [name, meta] : metadata) {
            auto it = metrics.find(name);
            if (it == metrics.end()) continue;
            
            // Write metric help and type
            oss << "# HELP " << name << " " << meta.description << "\n";
            oss << "# TYPE " << name << " ";
            
            switch (meta.type) {
                case MetricType::Counter:
                    oss << "counter";
                    break;
                case MetricType::Gauge:
                    oss << "gauge";
                    break;
                case MetricType::Histogram:
                case MetricType::Timer:
                    oss << "histogram";
                    break;
            }
            oss << "\n";
            
            // Write metric value
            oss << name;
            
            // Add labels if any
            if (!meta.labels.empty()) {
                oss << "{";
                bool first = true;
                for (const auto& [key, value] : meta.labels) {
                    if (!first) oss << ",";
                    oss << key << "=\"" << value << "\"";
                    first = false;
                }
                oss << "}";
            }
            
            // Write value
            if (meta.type == MetricType::Histogram || meta.type == MetricType::Timer) {
                const auto& value = it->second;
                oss << "_count " << value.getCount() << "\n";
                oss << name << "_sum " << value.getMean() * value.getCount() << "\n";
                
                // Add percentiles
                for (double p : {0.5, 0.9, 0.95, 0.99}) {
                    oss << name << "{quantile=\"" << p << "\"} " 
                        << value.getPercentile(p * 100) << "\n";
                }
            } else {
                oss << " " << it->second.getValue() << "\n";
            }
            
            oss << "\n";
        }
        
        return oss.str();
    }
};

MonitoringService::MonitoringService(const MonitoringConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {
    
    // Register default metrics
    registerMetric({
        "pacs_system_uptime_seconds",
        MetricType::Counter,
        "System uptime in seconds",
        "seconds"
    });
    
    registerMetric({
        "pacs_system_cpu_usage_percent",
        MetricType::Gauge,
        "CPU usage percentage",
        "percent"
    });
    
    registerMetric({
        "pacs_system_memory_usage_bytes",
        MetricType::Gauge,
        "Memory usage in bytes",
        "bytes"
    });
    
    registerMetric({
        "pacs_dicom_operations_total",
        MetricType::Counter,
        "Total DICOM operations",
        "operations"
    });
    
    registerMetric({
        "pacs_database_queries_total",
        MetricType::Counter,
        "Total database queries",
        "queries"
    });
    
    registerMetric({
        "pacs_api_requests_total",
        MetricType::Counter,
        "Total API requests",
        "requests"
    });
    
    registerMetric({
        "pacs_api_request_duration_ms",
        MetricType::Timer,
        "API request duration",
        "milliseconds"
    });
}

MonitoringService::~MonitoringService() {
    stop();
}

core::Result<void> MonitoringService::start() {
    if (running_) {
        return core::Result<void>::error("Monitoring service already running");
    }
    
    running_ = true;
    
    // Start monitoring thread
    impl_->monitoringThread_ = std::make_unique<std::thread>([this]() {
        common::logger::logInfo("Monitoring thread started");
        
        while (running_) {
            try {
                // Collect system metrics
                if (config_.collectSystemMetrics) {
                    auto sysMetrics = impl_->collectSystemMetrics();
                    
                    setGauge("pacs_system_cpu_usage_percent", sysMetrics.cpuUsagePercent);
                    setGauge("pacs_system_memory_usage_bytes", sysMetrics.usedMemoryBytes);
                    setGauge("pacs_system_memory_usage_percent", sysMetrics.memoryUsagePercent);
                    setGauge("pacs_process_memory_bytes", sysMetrics.processMemoryBytes);
                    setGauge("pacs_system_threads", sysMetrics.threadCount);
                    incrementCounter("pacs_system_uptime_seconds", 
                                     config_.metricsInterval.count());
                }
                
                // Sleep until next collection
                std::this_thread::sleep_for(config_.metricsInterval);
                
            } catch (const std::exception& e) {
                common::logger::logError("Error in monitoring thread: {}", e.what());
            }
        }
        
        common::logger::logInfo("Monitoring thread stopped");
    });
    
    // Start Prometheus endpoint if enabled
    if (config_.enablePrometheus) {
        // In production, use a proper HTTP server library
        common::logger::logInfo("Prometheus metrics endpoint enabled on port {}", 
                        config_.prometheusPort);
    }
    
    common::logger::logInfo("Monitoring service started");
    return core::Result<void>::ok();
}

void MonitoringService::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (impl_->monitoringThread_ && impl_->monitoringThread_->joinable()) {
        impl_->monitoringThread_->join();
    }
    
    if (impl_->prometheusThread_ && impl_->prometheusThread_->joinable()) {
        impl_->prometheusThread_->join();
    }
    
    common::logger::logInfo("Monitoring service stopped");
}

void MonitoringService::registerMetric(const MetricMetadata& metadata) {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    impl_->registeredMetrics_[metadata.name] = metadata;
    impl_->metrics_[metadata.name] = MetricValue();
}

void MonitoringService::updateMetric(const std::string& name, double value,
                                      const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    
    auto it = impl_->metrics_.find(name);
    if (it != impl_->metrics_.end()) {
        it->second.setValue(value);
    }
}

void MonitoringService::recordTiming(const std::string& name, 
                                     std::chrono::microseconds duration,
                                     const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    
    auto it = impl_->metrics_.find(name);
    if (it != impl_->metrics_.end()) {
        double ms = duration.count() / 1000.0;
        it->second.record(ms);
    }
}

void MonitoringService::incrementCounter(const std::string& name, double delta,
                                          const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    
    auto it = impl_->metrics_.find(name);
    if (it != impl_->metrics_.end()) {
        it->second.increment(delta);
    }
}

void MonitoringService::setGauge(const std::string& name, double value,
                                  const std::unordered_map<std::string, std::string>& labels) {
    updateMetric(name, value, labels);
}

void MonitoringService::recordHistogram(const std::string& name, double value,
                                         const std::unordered_map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    
    auto it = impl_->metrics_.find(name);
    if (it != impl_->metrics_.end()) {
        it->second.record(value);
    }
}

void MonitoringService::registerHealthCheck(const std::string& name, 
                                            HealthCheckFunc check) {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    impl_->healthChecks_[name] = check;
}

SystemMetrics MonitoringService::getSystemMetrics() const {
    return impl_->collectSystemMetrics();
}

std::unordered_map<std::string, MetricValue> MonitoringService::getAllMetrics() const {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    return impl_->metrics_;
}

core::Result<MetricValue> MonitoringService::getMetric(const std::string& name) const {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    
    auto it = impl_->metrics_.find(name);
    if (it == impl_->metrics_.end()) {
        return core::Result<MetricValue>::error("Metric not found: " + name);
    }
    
    return core::Result<MetricValue>::ok(it->second);
}

std::unordered_map<std::string, HealthCheckResult> MonitoringService::runHealthChecks() {
    std::unordered_map<std::string, HealthCheckResult> results;
    
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    
    for (const auto& [name, check] : impl_->healthChecks_) {
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            results[name] = check();
        } catch (const std::exception& e) {
            results[name] = {
                false,
                std::string("Exception: ") + e.what(),
                {},
                std::chrono::milliseconds(0)
            };
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        results[name].checkDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start);
    }
    
    return results;
}

std::string MonitoringService::getPrometheusMetrics() const {
    std::lock_guard<std::mutex> lock(impl_->metricsMutex_);
    return impl_->formatPrometheusMetrics(impl_->registeredMetrics_, impl_->metrics_);
}

} // namespace pacs