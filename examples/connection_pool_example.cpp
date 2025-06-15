/**
 * @file connection_pool_example.cpp
 * @brief Example demonstrating connection pooling and resilience features
 *
 * Copyright (c) 2024 PACS System
 * Licensed under BSD License
 */

#include <iostream>
#include <thread>
#include <chrono>

#include "core/result.h"
#include "common/logger/log_module.h"
#include "common/network/dicom_connection_pool.h"
#include "common/network/retry_policy.h"

// Simulate DICOM operations
core::Result<std::string> performDicomOperation(DicomConnection* conn) {
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // In real usage, this would perform actual DICOM operations
    return core::Result<std::string>::ok("Operation successful");
}

int main() {
    // Initialize logger
    logger::initialize("connection_pool_example", Logger::LogLevel::Info);
    
    logger::logInfo("Connection Pool Example");
    logger::logInfo("======================");
    
    // Configure DICOM connection parameters
    DicomConnection::Parameters params;
    params.remoteHost = "127.0.0.1";  // Localhost for testing
    params.remotePort = 11112;
    params.remoteAeTitle = "TEST_PACS";
    params.localAeTitle = "POOL_CLIENT";
    params.maxPduSize = 16384;
    params.timeout = std::chrono::seconds(30);
    
    // Configure connection pool
    ConnectionPool<DicomConnection>::Config poolConfig;
    poolConfig.minSize = 2;
    poolConfig.maxSize = 5;
    poolConfig.maxIdleTime = 300;  // 5 minutes
    poolConfig.validateOnBorrow = true;
    poolConfig.validationInterval = 60;  // 1 minute
    
    logger::logInfo("Creating connection pool with:");
    logger::logInfo("  Min size: {}", poolConfig.minSize);
    logger::logInfo("  Max size: {}", poolConfig.maxSize);
    logger::logInfo("  Max idle time: {}s", poolConfig.maxIdleTime);
    
    // Get connection pool
    auto& poolManager = DicomConnectionPoolManager::getInstance();
    auto pool = poolManager.getPool("TEST_POOL", params, poolConfig);
    
    // Example 1: Simple operation with automatic connection management
    logger::logInfo("\nExample 1: Simple operation");
    auto result = pool->executeWithConnection("test_operation",
        [](DicomConnection* conn) -> core::Result<std::string> {
            return performDicomOperation(conn);
        });
    
    if (result.isOk()) {
        logger::logInfo("Operation result: {}", result.getValue());
    } else {
        logger::logError("Operation failed: {}", result.getError());
    }
    
    // Example 2: Multiple concurrent operations
    logger::logInfo("\nExample 2: Concurrent operations");
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&pool, i]() {
            auto opName = "operation_" + std::to_string(i);
            auto result = pool->executeWithConnection(opName,
                [i](DicomConnection* conn) -> core::Result<std::string> {
                    logger::logInfo("Thread {} using connection", i);
                    return performDicomOperation(conn);
                });
            
            if (result.isOk()) {
                logger::logInfo("Thread {} completed successfully", i);
            } else {
                logger::logError("Thread {} failed: {}", i, result.getError());
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    // Show pool statistics
    logger::logInfo("\nPool Statistics:");
    auto stats = pool->getPoolStats();
    logger::logInfo("  Total connections: {}", stats.totalSize);
    logger::logInfo("  Active connections: {}", stats.activeSize);
    logger::logInfo("  Available connections: {}", stats.availableSize);
    logger::logInfo("  Total borrows: {}", stats.totalBorrows);
    logger::logInfo("  Total returns: {}", stats.totalReturns);
    
    // Example 3: Retry policy with connection pool
    logger::logInfo("\nExample 3: Retry policy");
    RetryConfig retryConfig;
    retryConfig.maxAttempts = 3;
    retryConfig.initialDelay = std::chrono::milliseconds(500);
    retryConfig.strategy = RetryStrategy::ExponentialJitter;
    retryConfig.addRetryableError("connection failed");
    
    RetryPolicy retry(retryConfig);
    
    auto retryResult = retry.execute([&pool]() -> core::Result<std::string> {
        static int attempt = 0;
        attempt++;
        
        if (attempt < 2) {
            // Simulate failure on first attempt
            return core::Result<std::string>::error("connection failed");
        }
        
        return pool->executeWithConnection("retry_operation",
            [](DicomConnection* conn) -> core::Result<std::string> {
                return performDicomOperation(conn);
            });
    });
    
    if (retryResult.isOk()) {
        logger::logInfo("Retry operation succeeded: {}", retryResult.getValue());
    } else {
        logger::logError("Retry operation failed: {}", retryResult.getError());
    }
    
    // Example 4: Circuit breaker
    logger::logInfo("\nExample 4: Circuit breaker");
    CircuitBreaker::Config cbConfig;
    cbConfig.failureThreshold = 3;
    cbConfig.successThreshold = 2;
    cbConfig.openDuration = std::chrono::seconds(5);
    
    CircuitBreaker cb("test_service", cbConfig);
    
    // Simulate some failures
    for (int i = 0; i < 5; ++i) {
        auto cbResult = cb.execute([i]() -> core::Result<void> {
            if (i < 3) {
                return core::Result<void>::error("Service unavailable");
            }
            return core::Result<void>::ok();
        });
        
        logger::logInfo("Circuit breaker attempt {}: {} (State: {})", 
                        i + 1,
                        cbResult.isOk() ? "Success" : "Failed",
                        cb.getState() == CircuitBreaker::State::Closed ? "Closed" :
                        cb.getState() == CircuitBreaker::State::Open ? "Open" : "Half-Open");
    }
    
    // Example 5: Resilient executor (combines retry + circuit breaker)
    logger::logInfo("\nExample 5: Resilient executor");
    ResilientExecutor executor("resilient_service", retryConfig, cbConfig);
    
    auto resilientResult = executor.execute([&pool]() -> core::Result<std::string> {
        return pool->executeWithConnection("resilient_operation",
            [](DicomConnection* conn) -> core::Result<std::string> {
                return performDicomOperation(conn);
            });
    });
    
    if (resilientResult.isOk()) {
        logger::logInfo("Resilient operation succeeded: {}", resilientResult.getValue());
    } else {
        logger::logError("Resilient operation failed: {}", resilientResult.getError());
    }
    
    // Show final statistics
    logger::logInfo("\nFinal Pool Statistics:");
    auto finalStats = poolManager.getAllPoolStats();
    for (const auto& [name, stats] : finalStats) {
        logger::logInfo("Pool '{}': Total={}, Active={}, Available={}", 
                        name, stats.totalSize, stats.activeSize, stats.availableSize);
    }
    
    logger::logInfo("\nConnection pool example completed");
    
    return 0;
}