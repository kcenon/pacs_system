/**
 * @file network_performance_tests.cpp
 * @brief Network and connection pooling performance tests
 *
 * Copyright (c) 2024 PACS System
 * All rights reserved.
 */

#include "performance_test_framework.h"
#include "common/network/connection_pool.h"
#include "common/network/retry_policy.h"
#include "common/network/dicom_connection_pool.h"

#include <thread>
#include <atomic>

using namespace pacs::perf;

namespace {

// Mock connection for testing
class MockConnection {
public:
    MockConnection() : id_(nextId_++) {
        // Simulate connection establishment time
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    ~MockConnection() = default;
    
    bool isValid() const { return valid_; }
    void invalidate() { valid_ = false; }
    int getId() const { return id_; }
    
    void doWork() {
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
    
private:
    static std::atomic<int> nextId_;
    int id_;
    bool valid_ = true;
};

std::atomic<int> MockConnection::nextId_{0};

// Shared connection pool
std::shared_ptr<ConnectionPool<MockConnection>> testPool;

} // anonymous namespace

// Test: Connection pool creation and warmup
PERF_TEST(ConnectionPoolCreation) {
    ConnectionPool<MockConnection>::Config config;
    config.minSize = 5;
    config.maxSize = 10;
    config.maxIdleTime = 300;
    
    auto pool = std::make_shared<ConnectionPool<MockConnection>>(
        []() -> core::Result<std::unique_ptr<MockConnection>> {
            return core::Result<std::unique_ptr<MockConnection>>::ok(
                std::make_unique<MockConnection>()
            );
        },
        config
    );
    
    // Initialize pool (creates minSize connections)
    auto initResult = pool->initialize();
    if (!initResult.isOk()) {
        return core::Result<void>::error("Pool initialization failed");
    }
    
    // Verify pool size
    auto stats = pool->getPoolStats();
    if (stats.totalSize < config.minSize) {
        return core::Result<void>::error("Pool not properly initialized");
    }
    
    return core::Result<void>::ok();
}

// Test: Single-threaded connection borrow/return
PERF_TEST(ConnectionBorrowReturn) {
    if (!testPool) {
        ConnectionPool<MockConnection>::Config config;
        config.minSize = 10;
        config.maxSize = 20;
        
        testPool = std::make_shared<ConnectionPool<MockConnection>>(
            []() -> core::Result<std::unique_ptr<MockConnection>> {
                return core::Result<std::unique_ptr<MockConnection>>::ok(
                    std::make_unique<MockConnection>()
                );
            },
            config
        );
        testPool->initialize();
    }
    
    // Borrow connection
    auto borrowResult = testPool->borrowConnection();
    if (!borrowResult.isOk()) {
        return core::Result<void>::error("Failed to borrow connection");
    }
    
    auto conn = borrowResult.getValue();
    
    // Use connection
    conn->doWork();
    
    // Connection automatically returned when shared_ptr goes out of scope
    return core::Result<void>::ok();
}

// Test: Concurrent connection borrowing
PERF_TEST(ConcurrentConnectionBorrow) {
    if (!testPool) {
        return core::Result<void>::error("Pool not initialized");
    }
    
    const size_t numThreads = 10;
    std::vector<std::future<bool>> futures;
    
    for (size_t i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, [i]() {
            auto borrowResult = testPool->borrowConnection();
            if (!borrowResult.isOk()) {
                return false;
            }
            
            auto conn = borrowResult.getValue();
            conn->doWork();
            
            // Hold connection briefly to increase contention
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            
            return true;
        }));
    }
    
    // Wait for all threads
    for (auto& future : futures) {
        if (!future.get()) {
            return core::Result<void>::error("Concurrent borrow failed");
        }
    }
    
    return core::Result<void>::ok();
}

// Test: Connection pool exhaustion handling
PERF_TEST(ConnectionPoolExhaustion) {
    // Create small pool for testing exhaustion
    ConnectionPool<MockConnection>::Config config;
    config.minSize = 2;
    config.maxSize = 2;  // Very small pool
    
    auto smallPool = std::make_shared<ConnectionPool<MockConnection>>(
        []() -> core::Result<std::unique_ptr<MockConnection>> {
            return core::Result<std::unique_ptr<MockConnection>>::ok(
                std::make_unique<MockConnection>()
            );
        },
        config
    );
    
    smallPool->initialize();
    
    // Borrow all connections
    std::vector<std::shared_ptr<MockConnection>> connections;
    for (size_t i = 0; i < config.maxSize; ++i) {
        auto result = smallPool->borrowConnection(std::chrono::milliseconds(100));
        if (result.isOk()) {
            connections.push_back(result.getValue());
        }
    }
    
    // Try to borrow one more (should timeout)
    auto result = smallPool->borrowConnection(std::chrono::milliseconds(10));
    if (result.isOk()) {
        return core::Result<void>::error("Should have timed out");
    }
    
    // Release one connection
    connections.pop_back();
    
    // Now borrowing should succeed
    result = smallPool->borrowConnection(std::chrono::milliseconds(100));
    if (!result.isOk()) {
        return core::Result<void>::error("Failed to borrow after release");
    }
    
    return core::Result<void>::ok();
}

// Test: Retry policy - fixed delay
PERF_TEST(RetryPolicyFixed) {
    RetryConfig config;
    config.maxAttempts = 3;
    config.initialDelay = std::chrono::milliseconds(10);
    config.strategy = RetryStrategy::Fixed;
    
    RetryPolicy retry(config);
    
    int attempts = 0;
    auto result = retry.execute([&attempts]() -> core::Result<int> {
        attempts++;
        if (attempts < 3) {
            return core::Result<int>::error("Simulated failure");
        }
        return core::Result<int>::ok(42);
    });
    
    if (!result.isOk() || result.getValue() != 42) {
        return core::Result<void>::error("Retry failed");
    }
    
    return core::Result<void>::ok();
}

// Test: Retry policy - exponential backoff
PERF_TEST(RetryPolicyExponential) {
    RetryConfig config;
    config.maxAttempts = 4;
    config.initialDelay = std::chrono::milliseconds(5);
    config.strategy = RetryStrategy::Exponential;
    config.backoffMultiplier = 2.0;
    
    RetryPolicy retry(config);
    
    auto start = std::chrono::steady_clock::now();
    int attempts = 0;
    
    auto result = retry.execute([&attempts]() -> core::Result<void> {
        attempts++;
        if (attempts < 4) {
            return core::Result<void>::error("Retry needed");
        }
        return core::Result<void>::ok();
    });
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    // Should have delays: 5ms, 10ms, 20ms = 35ms minimum
    if (elapsed < std::chrono::milliseconds(35)) {
        return core::Result<void>::error("Exponential backoff too fast");
    }
    
    return core::Result<void>::ok();
}

// Test: Circuit breaker
PERF_TEST(CircuitBreaker) {
    CircuitBreaker::Config config;
    config.failureThreshold = 3;
    config.successThreshold = 2;
    config.openDuration = std::chrono::milliseconds(50);
    
    CircuitBreaker cb("test_service", config);
    
    // Cause failures to open circuit
    for (int i = 0; i < 3; ++i) {
        cb.execute([]() -> core::Result<void> {
            return core::Result<void>::error("Service failure");
        });
    }
    
    // Circuit should be open
    if (cb.getState() != CircuitBreaker::State::Open) {
        return core::Result<void>::error("Circuit should be open");
    }
    
    // Requests should fail fast
    auto result = cb.execute([]() -> core::Result<void> {
        return core::Result<void>::ok();
    });
    
    if (result.isOk()) {
        return core::Result<void>::error("Circuit breaker should reject requests");
    }
    
    // Wait for half-open state
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    
    // Success should move to closed state
    cb.execute([]() -> core::Result<void> {
        return core::Result<void>::ok();
    });
    cb.execute([]() -> core::Result<void> {
        return core::Result<void>::ok();
    });
    
    if (cb.getState() != CircuitBreaker::State::Closed) {
        return core::Result<void>::error("Circuit should be closed");
    }
    
    return core::Result<void>::ok();
}

// Test: Resilient executor (retry + circuit breaker)
PERF_TEST(ResilientExecutor) {
    RetryConfig retryConfig;
    retryConfig.maxAttempts = 3;
    retryConfig.initialDelay = std::chrono::milliseconds(5);
    
    CircuitBreaker::Config cbConfig;
    cbConfig.failureThreshold = 5;
    cbConfig.successThreshold = 2;
    
    ResilientExecutor executor("test_operation", retryConfig, cbConfig);
    
    // Simulate intermittent failures
    std::atomic<int> callCount{0};
    auto result = executor.execute([&callCount]() -> core::Result<std::string> {
        int count = ++callCount;
        if (count % 3 == 0) {
            return core::Result<std::string>::ok("Success");
        }
        return core::Result<std::string>::error("Temporary failure");
    });
    
    if (!result.isOk()) {
        return core::Result<void>::error("Resilient execution failed");
    }
    
    return core::Result<void>::ok();
}

// Test: High-throughput connection usage
PERF_TEST(HighThroughputConnections) {
    if (!testPool) {
        return core::Result<void>::error("Pool not initialized");
    }
    
    const size_t numOperations = 100;
    std::atomic<size_t> completed{0};
    std::atomic<size_t> failed{0};
    
    // Rapid-fire connection usage
    for (size_t i = 0; i < numOperations; ++i) {
        auto result = testPool->borrowConnection(std::chrono::milliseconds(10));
        if (result.isOk()) {
            auto conn = result.getValue();
            conn->doWork();
            completed++;
        } else {
            failed++;
        }
    }
    
    // At least 80% should succeed
    if (completed < numOperations * 0.8) {
        return core::Result<void>::error("Too many failures in high throughput test");
    }
    
    return core::Result<void>::ok();
}

// Main test runner
int main(int argc, char* argv[]) {
    pacs::perf::PerfTestRunner runner;
    
    // Register all tests
    REGISTER_PERF_TEST(ConnectionPoolCreation);
    REGISTER_PERF_TEST(ConnectionBorrowReturn);
    REGISTER_PERF_TEST(ConcurrentConnectionBorrow);
    REGISTER_PERF_TEST(ConnectionPoolExhaustion);
    REGISTER_PERF_TEST(RetryPolicyFixed);
    REGISTER_PERF_TEST(RetryPolicyExponential);
    REGISTER_PERF_TEST(CircuitBreaker);
    REGISTER_PERF_TEST(ResilientExecutor);
    REGISTER_PERF_TEST(HighThroughputConnections);
    
    // Configure test parameters
    pacs::perf::PerfTestConfig config;
    config.iterations = 100;
    config.warmupIterations = 10;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--iterations" && i + 1 < argc) {
            config.iterations = std::stoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            config.concurrentThreads = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            config.outputFormat = argv[++i];
        }
    }
    
    // Run tests
    logger::initialize("network_perf_tests", Logger::LogLevel::Info);
    logger::logInfo("Running network performance tests");
    
    auto results = runner.runAll(config);
    
    // Generate report
    if (config.outputFormat == "json") {
        runner.generateReport(results, "network_perf_results.json");
    } else if (config.outputFormat == "csv") {
        runner.generateReport(results, "network_perf_results.csv");
    } else {
        runner.generateReport(results);
    }
    
    // Cleanup
    testPool.reset();
    
    // Check for failures
    bool allPassed = std::all_of(results.begin(), results.end(),
                                 [](const auto& r) { return r.passed; });
    
    return allPassed ? 0 : 1;
}