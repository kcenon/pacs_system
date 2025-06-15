/**
 * @file performance_test_framework.cpp
 * @brief Performance testing framework implementation
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#include "performance_test_framework.h"
#include "common/logger/log_module.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <thread>
#include <future>

namespace pacs {
namespace perf {

class PerfTestRunner::Impl {
public:
    std::map<std::string, std::shared_ptr<PerfTest>> tests_;
    std::vector<PerfTestResult> baseline_;
    
    void generateConsoleReport(const std::vector<PerfTestResult>& results) {
        std::cout << "\n================ Performance Test Results ================\n";
        std::cout << std::setw(30) << std::left << "Test Name"
                  << std::setw(12) << "Iterations"
                  << std::setw(12) << "Mean (ms)"
                  << std::setw(12) << "P95 (ms)"
                  << std::setw(12) << "P99 (ms)"
                  << std::setw(15) << "Throughput"
                  << std::setw(10) << "Status"
                  << "\n";
        std::cout << std::string(100, '-') << "\n";
        
        for (const auto& result : results) {
            std::cout << std::setw(30) << std::left << result.testName
                      << std::setw(12) << result.timing.count
                      << std::setw(12) << std::fixed << std::setprecision(2) 
                      << result.timing.mean
                      << std::setw(12) << result.timing.p95
                      << std::setw(12) << result.timing.p99
                      << std::setw(15) << std::fixed << std::setprecision(0)
                      << result.throughput << " ops/s"
                      << std::setw(10) << (result.passed ? "PASSED" : "FAILED")
                      << "\n";
            
            if (!result.passed) {
                std::cout << "  Error: " << result.errorMessage << "\n";
            }
            
            // Compare with baseline if available
            auto baselineIt = std::find_if(baseline_.begin(), baseline_.end(),
                [&](const PerfTestResult& b) { return b.testName == result.testName; });
            
            if (baselineIt != baseline_.end()) {
                double improvement = ((baselineIt->timing.mean - result.timing.mean) / 
                                     baselineIt->timing.mean) * 100.0;
                std::cout << "  vs Baseline: " 
                          << (improvement >= 0 ? "+" : "") 
                          << std::fixed << std::setprecision(1) << improvement << "%\n";
            }
        }
        
        std::cout << std::string(100, '=') << "\n\n";
    }
    
    void generateJsonReport(const std::vector<PerfTestResult>& results,
                            const std::string& outputPath) {
        std::ofstream file(outputPath);
        file << "{\n  \"results\": [\n";
        
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& result = results[i];
            file << "    {\n";
            file << "      \"name\": \"" << result.testName << "\",\n";
            file << "      \"timing\": {\n";
            file << "        \"mean\": " << result.timing.mean << ",\n";
            file << "        \"min\": " << result.timing.min << ",\n";
            file << "        \"max\": " << result.timing.max << ",\n";
            file << "        \"p95\": " << result.timing.p95 << ",\n";
            file << "        \"p99\": " << result.timing.p99 << ",\n";
            file << "        \"stddev\": " << result.timing.stddev << ",\n";
            file << "        \"count\": " << result.timing.count << "\n";
            file << "      },\n";
            file << "      \"throughput\": " << result.throughput << ",\n";
            file << "      \"passed\": " << (result.passed ? "true" : "false");
            
            if (result.measureMemory) {
                file << ",\n      \"memory\": {\n";
                file << "        \"initial\": " << result.memory.initialMemory << ",\n";
                file << "        \"peak\": " << result.memory.peakMemory << ",\n";
                file << "        \"final\": " << result.memory.finalMemory << "\n";
                file << "      }";
            }
            
            file << "\n    }";
            if (i < results.size() - 1) file << ",";
            file << "\n";
        }
        
        file << "  ]\n}\n";
    }
    
    void generateCsvReport(const std::vector<PerfTestResult>& results,
                           const std::string& outputPath) {
        std::ofstream file(outputPath);
        
        // Header
        file << "Test Name,Iterations,Mean (ms),Min (ms),Max (ms),StdDev,P95 (ms),P99 (ms),"
             << "Throughput (ops/s),Status\n";
        
        // Data
        for (const auto& result : results) {
            file << result.testName << ","
                 << result.timing.count << ","
                 << result.timing.mean << ","
                 << result.timing.min << ","
                 << result.timing.max << ","
                 << result.timing.stddev << ","
                 << result.timing.p95 << ","
                 << result.timing.p99 << ","
                 << result.throughput << ","
                 << (result.passed ? "PASSED" : "FAILED") << "\n";
        }
    }
};

PerfTestRunner::PerfTestRunner() : impl_(std::make_unique<Impl>()) {}
PerfTestRunner::~PerfTestRunner() = default;

void PerfTestRunner::addTest(std::shared_ptr<PerfTest> test) {
    impl_->tests_[test->getName()] = test;
}

std::vector<PerfTestResult> PerfTestRunner::runAll(const PerfTestConfig& config) {
    std::vector<PerfTestResult> results;
    
    for (const auto& [name, test] : impl_->tests_) {
        logger::logInfo("Running performance test: {}", name);
        results.push_back(runSingleTest(test, config));
    }
    
    return results;
}

PerfTestResult PerfTestRunner::runTest(const std::string& testName,
                                        const PerfTestConfig& config) {
    auto it = impl_->tests_.find(testName);
    if (it == impl_->tests_.end()) {
        PerfTestResult result;
        result.testName = testName;
        result.passed = false;
        result.errorMessage = "Test not found";
        return result;
    }
    
    return runSingleTest(it->second, config);
}

PerfTestResult PerfTestRunner::runSingleTest(std::shared_ptr<PerfTest> test,
                                              const PerfTestConfig& config) {
    PerfTestResult result;
    result.testName = test->getName();
    
    // Setup
    auto setupResult = test->setup();
    if (!setupResult.isOk()) {
        result.passed = false;
        result.errorMessage = "Setup failed: " + setupResult.getError();
        return result;
    }
    
    try {
        std::vector<double> measurements;
        measurements.reserve(config.iterations);
        
        // Warmup
        logger::logDebug("Running {} warmup iterations", config.warmupIterations);
        for (size_t i = 0; i < config.warmupIterations; ++i) {
            auto execResult = test->execute();
            if (!execResult.isOk()) {
                result.passed = false;
                result.errorMessage = "Warmup failed: " + execResult.getError();
                test->teardown();
                return result;
            }
        }
        
        // Actual test
        if (config.concurrentThreads == 1) {
            // Single-threaded execution
            auto startTime = std::chrono::steady_clock::now();
            
            for (size_t i = 0; i < config.iterations; ++i) {
                auto iterStart = std::chrono::high_resolution_clock::now();
                auto execResult = test->execute();
                auto iterEnd = std::chrono::high_resolution_clock::now();
                
                if (!execResult.isOk()) {
                    result.passed = false;
                    result.errorMessage = "Execution failed: " + execResult.getError();
                    break;
                }
                
                double elapsed = std::chrono::duration<double, std::milli>(
                    iterEnd - iterStart).count();
                measurements.push_back(elapsed);
                
                // Check timeout
                if (std::chrono::steady_clock::now() - startTime > config.maxDuration) {
                    logger::logWarning("Test timeout reached after {} iterations", i);
                    break;
                }
            }
        } else {
            // Multi-threaded execution
            std::atomic<size_t> completedIterations{0};
            std::vector<std::future<std::vector<double>>> futures;
            
            for (size_t t = 0; t < config.concurrentThreads; ++t) {
                futures.push_back(std::async(std::launch::async, [&, t]() {
                    std::vector<double> threadMeasurements;
                    size_t iterationsPerThread = config.iterations / config.concurrentThreads;
                    
                    for (size_t i = 0; i < iterationsPerThread; ++i) {
                        auto iterStart = std::chrono::high_resolution_clock::now();
                        auto execResult = test->execute();
                        auto iterEnd = std::chrono::high_resolution_clock::now();
                        
                        if (execResult.isOk()) {
                            double elapsed = std::chrono::duration<double, std::milli>(
                                iterEnd - iterStart).count();
                            threadMeasurements.push_back(elapsed);
                            completedIterations++;
                        }
                    }
                    
                    return threadMeasurements;
                }));
            }
            
            // Collect results from all threads
            for (auto& future : futures) {
                auto threadMeasurements = future.get();
                measurements.insert(measurements.end(),
                                    threadMeasurements.begin(),
                                    threadMeasurements.end());
            }
        }
        
        // Calculate metrics
        if (!measurements.empty()) {
            result.timing = calculateMetrics(measurements);
            result.throughput = 1000.0 / result.timing.mean;  // ops/s
            result.passed = true;
        }
        
    } catch (const std::exception& e) {
        result.passed = false;
        result.errorMessage = std::string("Exception: ") + e.what();
    }
    
    // Teardown
    test->teardown();
    
    return result;
}

PerfMetric PerfTestRunner::calculateMetrics(const std::vector<double>& measurements) {
    PerfMetric metric;
    metric.count = measurements.size();
    
    if (measurements.empty()) {
        return metric;
    }
    
    // Sort for percentiles
    std::vector<double> sorted = measurements;
    std::sort(sorted.begin(), sorted.end());
    
    // Basic statistics
    metric.min = sorted.front();
    metric.max = sorted.back();
    metric.totalTime = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    metric.mean = metric.totalTime / metric.count;
    
    // Median
    if (metric.count % 2 == 0) {
        metric.median = (sorted[metric.count/2 - 1] + sorted[metric.count/2]) / 2.0;
    } else {
        metric.median = sorted[metric.count/2];
    }
    
    // Standard deviation
    double sumSquares = 0.0;
    for (double val : measurements) {
        double diff = val - metric.mean;
        sumSquares += diff * diff;
    }
    metric.stddev = std::sqrt(sumSquares / metric.count);
    
    // Percentiles
    size_t p95Index = static_cast<size_t>(metric.count * 0.95);
    size_t p99Index = static_cast<size_t>(metric.count * 0.99);
    metric.p95 = sorted[std::min(p95Index, metric.count - 1)];
    metric.p99 = sorted[std::min(p99Index, metric.count - 1)];
    
    return metric;
}

void PerfTestRunner::generateReport(const std::vector<PerfTestResult>& results,
                                    const std::string& outputPath) {
    if (outputPath.empty()) {
        impl_->generateConsoleReport(results);
    } else if (outputPath.ends_with(".json")) {
        impl_->generateJsonReport(results, outputPath);
    } else if (outputPath.ends_with(".csv")) {
        impl_->generateCsvReport(results, outputPath);
    } else {
        impl_->generateConsoleReport(results);
    }
}

void PerfTestRunner::setBaseline(const std::vector<PerfTestResult>& baseline) {
    impl_->baseline_ = baseline;
}

} // namespace perf
} // namespace pacs