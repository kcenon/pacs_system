/**
 * BSD 3-Clause License
 * Copyright (c) 2024, kcenon
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "core/result/result.h"
#include <string>
#include <chrono>
#include <memory>
#include <map>

namespace pacs {
namespace common {
namespace performance {

/**
 * @brief Performance optimization strategies for PACS operations
 */
class PerformanceOptimizer {
public:
    /**
     * @brief Performance optimization strategy
     */
    enum class OptimizationStrategy {
        Memory,         // Memory usage optimization
        Network,        // Network throughput optimization
        Database,       // Database query optimization
        Threading,      // Thread pool optimization
        Cache           // Caching strategy optimization
    };
    
    /**
     * @brief Initialize performance optimizer
     * @param strategy Primary optimization strategy
     * @return Result indicating success or failure
     */
    static core::Result<void> initialize(OptimizationStrategy strategy);
    
    /**
     * @brief Optimize DICOM file processing
     * @param filePath Path to DICOM file
     * @return Result with optimization metrics
     */
    static core::Result<std::map<std::string, double>> optimizeDicomProcessing(const std::string& filePath);
    
    /**
     * @brief Optimize database query performance
     * @param query SQL query to optimize
     * @return Result with optimized query
     */
    static core::Result<std::string> optimizeQuery(const std::string& query);
    
    /**
     * @brief Get performance recommendations
     * @return Result with performance recommendations
     */
    static core::Result<std::vector<std::string>> getRecommendations();

private:
    static OptimizationStrategy currentStrategy_;
    static bool initialized_;
};

} // namespace performance
} // namespace common
} // namespace pacs