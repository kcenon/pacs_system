#pragma once

#include <chrono>
#include <string>
#include <functional>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <vector>

namespace pacs {
namespace common {

/**
 * @class PerformanceTracker
 * @brief Utility for tracking and logging performance metrics
 * 
 * This class provides functionality for measuring execution time,
 * throughput, and resource usage of different PACS operations.
 */
class PerformanceTracker {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static PerformanceTracker& getInstance();

    /**
     * @brief Initialize the performance tracker
     * @param enableTracking Whether to enable performance tracking
     */
    void initialize(bool enableTracking = true);

    /**
     * @brief Start tracking a specific operation
     * @param operationName Name of the operation to track
     * @return Operation ID for later reference
     */
    int startOperation(const std::string& operationName);

    /**
     * @brief End tracking for a specific operation
     * @param operationId ID of the operation to end tracking
     * @param dataSize Size of data processed (for throughput calculation)
     */
    void endOperation(int operationId, size_t dataSize = 0);

    /**
     * @brief Record a metric without timing
     * @param metricName Name of the metric
     * @param value Value to record
     */
    void recordMetric(const std::string& metricName, double value);

    /**
     * @brief Execute a function and measure its performance
     * @param operationName Name of the operation
     * @param func Function to execute
     * @param dataSize Size of data processed (for throughput calculation)
     * @return Result of the function execution
     */
    template <typename Func, typename... Args>
    auto measureOperation(const std::string& operationName, Func&& func, size_t dataSize = 0, Args&&... args) {
        auto start = std::chrono::high_resolution_clock::now();
        
        if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            recordExecutionTime(operationName, start, end, dataSize);
            return;
        } else {
            auto result = std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            recordExecutionTime(operationName, start, end, dataSize);
            return result;
        }
    }

    /**
     * @brief Get performance statistics
     * @return JSON string containing performance statistics
     */
    std::string getStatisticsJson() const;

    /**
     * @brief Reset all performance statistics
     */
    void resetStatistics();

private:
    PerformanceTracker();
    ~PerformanceTracker();
    
    void recordExecutionTime(const std::string& operationName, 
                           std::chrono::time_point<std::chrono::high_resolution_clock> start,
                           std::chrono::time_point<std::chrono::high_resolution_clock> end,
                           size_t dataSize);
    
    struct OperationData {
        std::string name;
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    };

    struct OperationStats {
        double totalTime = 0.0;
        double minTime = std::numeric_limits<double>::max();
        double maxTime = 0.0;
        uint64_t count = 0;
        uint64_t totalDataSize = 0;
    };

    std::atomic<bool> m_trackingEnabled;
    std::atomic<int> m_nextOperationId;
    
    std::mutex m_operationMutex;
    std::vector<OperationData> m_activeOperations;
    
    mutable std::mutex m_statsMutex;
    mutable std::unordered_map<std::string, OperationStats> m_statistics;
};

/**
 * @class ScopedPerformanceTracker
 * @brief RAII wrapper for automatic performance tracking
 * 
 * This class automatically starts tracking when constructed
 * and ends tracking when destructed.
 */
class ScopedPerformanceTracker {
public:
    /**
     * @brief Constructor
     * @param operationName Name of the operation to track
     * @param dataSize Size of data to be processed
     */
    ScopedPerformanceTracker(const std::string& operationName, size_t dataSize = 0);
    
    /**
     * @brief Destructor
     */
    ~ScopedPerformanceTracker();
    
    /**
     * @brief Set the data size (after construction)
     * @param dataSize Size of data processed
     */
    void setDataSize(size_t dataSize);
    
private:
    int m_operationId;
    size_t m_dataSize;
};

} // namespace common
} // namespace pacs