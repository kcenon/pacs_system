#include "performance.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <iostream>

namespace pacs {
namespace common {

PerformanceTracker& PerformanceTracker::getInstance() {
    static PerformanceTracker instance;
    return instance;
}

PerformanceTracker::PerformanceTracker() 
    : m_trackingEnabled(false), m_nextOperationId(0) {
}

PerformanceTracker::~PerformanceTracker() {
    // Automatically log final statistics on shutdown
    if (m_trackingEnabled) {
        std::cout << "Final Performance Statistics: " << getStatisticsJson() << std::endl;
    }
}

void PerformanceTracker::initialize(bool enableTracking) {
    m_trackingEnabled = enableTracking;
    std::cout << "Performance tracking " << (enableTracking ? "enabled" : "disabled") << std::endl;
}

int PerformanceTracker::startOperation(const std::string& operationName) {
    if (!m_trackingEnabled) {
        return -1;
    }
    
    int operationId = m_nextOperationId++;
    
    OperationData data;
    data.name = operationName;
    data.startTime = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(m_operationMutex);
    m_activeOperations.push_back(data);
    
    return operationId;
}

void PerformanceTracker::endOperation(int operationId, size_t dataSize) {
    if (!m_trackingEnabled || operationId < 0) {
        return;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    
    std::string operationName;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    
    {
        std::lock_guard<std::mutex> lock(m_operationMutex);
        // Find operation by index position
        if (operationId >= 0 && operationId < static_cast<int>(m_activeOperations.size())) {
            operationName = m_activeOperations[operationId].name;
            startTime = m_activeOperations[operationId].startTime;
            m_activeOperations.erase(m_activeOperations.begin() + operationId);
        } else {
            return; // Invalid operation ID
        }
    }
    
    recordExecutionTime(operationName, startTime, endTime, dataSize);
}

void PerformanceTracker::recordExecutionTime(
    const std::string& operationName,
    std::chrono::time_point<std::chrono::high_resolution_clock> start,
    std::chrono::time_point<std::chrono::high_resolution_clock> end,
    size_t dataSize) {
    
    if (!m_trackingEnabled) {
        return;
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0; // convert to ms
    
    std::lock_guard<std::mutex> lock(m_statsMutex);
    auto& stats = m_statistics[operationName];
    stats.totalTime += duration;
    stats.minTime = std::min(stats.minTime, duration);
    stats.maxTime = std::max(stats.maxTime, duration);
    stats.count++;
    stats.totalDataSize += dataSize;
}

void PerformanceTracker::recordMetric(const std::string& metricName, double value) {
    if (!m_trackingEnabled) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_statsMutex);
    auto& stats = m_statistics[metricName];
    stats.totalTime += value;  // Using totalTime to accumulate values
    stats.minTime = std::min(stats.minTime, value);
    stats.maxTime = std::max(stats.maxTime, value);
    stats.count++;
}

std::string PerformanceTracker::getStatisticsJson() const {
    if (!m_trackingEnabled) {
        return "{}";
    }
    
    std::ostringstream oss;
    oss << "{";
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        bool first = true;
        
        for (const auto& pair : m_statistics) {
            if (!first) {
                oss << ",";
            }
            first = false;
            
            const auto& name = pair.first;
            const auto& stats = pair.second;
            
            double avgTime = stats.count > 0 ? stats.totalTime / stats.count : 0;
            double throughput = 0;
            
            if (stats.totalTime > 0 && stats.totalDataSize > 0) {
                throughput = stats.totalDataSize / (stats.totalTime / 1000.0); // bytes per second
            }
            
            oss << "\"" << name << "\":{";
            oss << "\"count\":" << stats.count << ",";
            oss << "\"total_ms\":" << std::fixed << std::setprecision(2) << stats.totalTime << ",";
            oss << "\"avg_ms\":" << std::fixed << std::setprecision(2) << avgTime << ",";
            oss << "\"min_ms\":" << std::fixed << std::setprecision(2) << stats.minTime << ",";
            oss << "\"max_ms\":" << std::fixed << std::setprecision(2) << stats.maxTime;
            
            if (stats.totalDataSize > 0) {
                oss << ",\"data_size\":" << stats.totalDataSize;
                oss << ",\"throughput_bps\":" << std::fixed << std::setprecision(2) << throughput;
            }
            
            oss << "}";
        }
    }
    
    oss << "}";
    return oss.str();
}

void PerformanceTracker::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_statistics.clear();
}

ScopedPerformanceTracker::ScopedPerformanceTracker(const std::string& operationName, size_t dataSize)
    : m_dataSize(dataSize) {
    m_operationId = PerformanceTracker::getInstance().startOperation(operationName);
}

ScopedPerformanceTracker::~ScopedPerformanceTracker() {
    PerformanceTracker::getInstance().endOperation(m_operationId, m_dataSize);
}

void ScopedPerformanceTracker::setDataSize(size_t dataSize) {
    m_dataSize = dataSize;
}

} // namespace common
} // namespace pacs