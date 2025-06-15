/**
 * @file performance_test_framework.h
 * @brief Performance testing framework for PACS System
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#pragma once

#include <string>
#include <chrono>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <atomic>

#include "core/result.h"

namespace pacs {
namespace perf {

/**
 * @brief Performance metric
 */
struct PerfMetric {
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double median = 0.0;
    double stddev = 0.0;
    double p95 = 0.0;  // 95th percentile
    double p99 = 0.0;  // 99th percentile
    size_t count = 0;
    double totalTime = 0.0;
};

/**
 * @brief Performance test configuration
 */
struct PerfTestConfig {
    size_t iterations = 1000;
    size_t warmupIterations = 100;
    std::chrono::seconds maxDuration{60};
    size_t concurrentThreads = 1;
    bool measureMemory = false;
    bool measureCpu = false;
    std::string outputFormat = "console";  // console, json, csv
};

/**
 * @brief Memory usage statistics
 */
struct MemoryStats {
    size_t initialMemory = 0;
    size_t peakMemory = 0;
    size_t finalMemory = 0;
    size_t allocations = 0;
    size_t deallocations = 0;
};

/**
 * @brief Performance test result
 */
struct PerfTestResult {
    std::string testName;
    PerfMetric timing;
    MemoryStats memory;
    double throughput = 0.0;  // operations per second
    std::map<std::string, double> customMetrics;
    bool passed = true;
    std::string errorMessage;
};

/**
 * @brief Base class for performance tests
 */
class PerfTest {
public:
    explicit PerfTest(const std::string& name) : name_(name) {}
    virtual ~PerfTest() = default;
    
    /**
     * @brief Setup before test execution
     */
    virtual core::Result<void> setup() { return core::Result<void>::ok(); }
    
    /**
     * @brief Teardown after test execution
     */
    virtual void teardown() {}
    
    /**
     * @brief Execute single test iteration
     */
    virtual core::Result<void> execute() = 0;
    
    /**
     * @brief Get test name
     */
    const std::string& getName() const { return name_; }
    
protected:
    std::string name_;
};

/**
 * @brief Performance test runner
 */
class PerfTestRunner {
public:
    PerfTestRunner();
    ~PerfTestRunner();
    
    /**
     * @brief Add performance test
     */
    void addTest(std::shared_ptr<PerfTest> test);
    
    /**
     * @brief Run all tests
     */
    std::vector<PerfTestResult> runAll(const PerfTestConfig& config = {});
    
    /**
     * @brief Run specific test
     */
    PerfTestResult runTest(const std::string& testName, 
                           const PerfTestConfig& config = {});
    
    /**
     * @brief Generate report
     */
    void generateReport(const std::vector<PerfTestResult>& results,
                        const std::string& outputPath = "");
    
    /**
     * @brief Set baseline results for comparison
     */
    void setBaseline(const std::vector<PerfTestResult>& baseline);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    
    PerfTestResult runSingleTest(std::shared_ptr<PerfTest> test,
                                  const PerfTestConfig& config);
    
    PerfMetric calculateMetrics(const std::vector<double>& measurements);
    MemoryStats measureMemory(std::function<void()> func);
};

/**
 * @brief Macro to define performance test
 */
#define PERF_TEST(TestName) \
    class PerfTest_##TestName : public pacs::perf::PerfTest { \
    public: \
        PerfTest_##TestName() : PerfTest(#TestName) {} \
        core::Result<void> execute() override; \
    }; \
    core::Result<void> PerfTest_##TestName::execute()

/**
 * @brief Macro to register performance test
 */
#define REGISTER_PERF_TEST(TestName) \
    runner.addTest(std::make_shared<PerfTest_##TestName>())

/**
 * @brief Performance benchmark helpers
 */
class PerfBenchmark {
public:
    /**
     * @brief Measure execution time
     */
    template<typename Func>
    static double measureTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
    
    /**
     * @brief Measure throughput
     */
    template<typename Func>
    static double measureThroughput(Func&& func, size_t operations, 
                                    std::chrono::seconds duration) {
        auto start = std::chrono::steady_clock::now();
        size_t completed = 0;
        
        while (std::chrono::steady_clock::now() - start < duration) {
            func();
            completed++;
        }
        
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        return completed / elapsed;
    }
    
    /**
     * @brief Measure latency distribution
     */
    template<typename Func>
    static std::vector<double> measureLatencies(Func&& func, size_t iterations) {
        std::vector<double> latencies;
        latencies.reserve(iterations);
        
        for (size_t i = 0; i < iterations; ++i) {
            latencies.push_back(measureTime(func));
        }
        
        return latencies;
    }
};

/**
 * @brief Load generator for stress testing
 */
class LoadGenerator {
public:
    /**
     * @brief Generate constant load
     */
    template<typename Func>
    static void constantLoad(Func&& func, size_t rps, 
                             std::chrono::seconds duration) {
        auto interval = std::chrono::microseconds(1000000 / rps);
        auto endTime = std::chrono::steady_clock::now() + duration;
        
        while (std::chrono::steady_clock::now() < endTime) {
            auto start = std::chrono::steady_clock::now();
            func();
            auto elapsed = std::chrono::steady_clock::now() - start;
            
            if (elapsed < interval) {
                std::this_thread::sleep_for(interval - elapsed);
            }
        }
    }
    
    /**
     * @brief Generate ramp-up load
     */
    template<typename Func>
    static void rampUpLoad(Func&& func, size_t startRps, size_t endRps,
                           std::chrono::seconds rampDuration) {
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + rampDuration;
        
        while (std::chrono::steady_clock::now() < endTime) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            auto progress = elapsed.count() / (double)rampDuration.count();
            auto currentRps = startRps + (endRps - startRps) * progress;
            
            auto interval = std::chrono::microseconds(
                static_cast<long>(1000000 / currentRps));
            auto iterStart = std::chrono::steady_clock::now();
            
            func();
            
            auto iterElapsed = std::chrono::steady_clock::now() - iterStart;
            if (iterElapsed < interval) {
                std::this_thread::sleep_for(interval - iterElapsed);
            }
        }
    }
};

} // namespace perf
} // namespace pacs