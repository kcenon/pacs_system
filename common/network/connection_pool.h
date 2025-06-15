#pragma once

#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>

#include "core/result/result.h"

namespace pacs {
namespace common {
namespace network {

/**
 * @brief Connection interface for pooled connections
 */
class PooledConnection {
public:
    virtual ~PooledConnection() = default;
    
    /**
     * @brief Check if connection is alive
     * @return True if connection is valid
     */
    virtual bool isAlive() const = 0;
    
    /**
     * @brief Reset connection state
     */
    virtual void reset() = 0;
    
    /**
     * @brief Get unique connection ID
     * @return Connection identifier
     */
    virtual std::string getId() const = 0;
    
    /**
     * @brief Get last activity time
     * @return Time point of last activity
     */
    std::chrono::steady_clock::time_point getLastActivity() const {
        return lastActivity_;
    }
    
    /**
     * @brief Update last activity time
     */
    void updateActivity() {
        lastActivity_ = std::chrono::steady_clock::now();
    }

protected:
    std::chrono::steady_clock::time_point lastActivity_{std::chrono::steady_clock::now()};
};

/**
 * @brief Connection factory interface
 */
template<typename ConnectionType>
class ConnectionFactory {
public:
    virtual ~ConnectionFactory() = default;
    
    /**
     * @brief Create a new connection
     * @return New connection or nullptr on failure
     */
    virtual std::unique_ptr<ConnectionType> createConnection() = 0;
    
    /**
     * @brief Validate a connection
     * @param conn Connection to validate
     * @return True if connection is valid
     */
    virtual bool validateConnection(ConnectionType* conn) = 0;
};

/**
 * @brief Generic connection pool implementation
 */
template<typename ConnectionType>
class ConnectionPool {
public:
    /**
     * @brief Connection pool configuration
     */
    struct Config {
        size_t minSize = 2;          // Minimum pool size
        size_t maxSize = 10;         // Maximum pool size
        size_t maxIdleTime = 300;    // Max idle time in seconds
        size_t connectionTimeout = 10; // Connection timeout in seconds
        size_t validationInterval = 60; // Validation interval in seconds
        bool validateOnBorrow = true;  // Validate connection when borrowing
        bool validateOnReturn = false; // Validate connection when returning
    };
    
    /**
     * @brief Constructor
     * @param factory Connection factory
     * @param config Pool configuration
     */
    ConnectionPool(std::unique_ptr<ConnectionFactory<ConnectionType>> factory,
                   const Config& config = Config())
        : factory_(std::move(factory)), config_(config) {}
    
    ~ConnectionPool() {
        shutdown();
    }
    
    /**
     * @brief Initialize the connection pool
     * @return Result indicating success or failure
     */
    core::Result<void> initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (initialized_) {
            return core::Result<void>::error("Connection pool already initialized");
        }
        
        // Create initial connections
        for (size_t i = 0; i < config_.minSize; ++i) {
            auto conn = factory_->createConnection();
            if (!conn) {
                // Clean up any created connections
                clear();
                return core::Result<void>::error("Failed to create initial connections");
            }
            availableConnections_.push(std::move(conn));
        }
        
        currentSize_ = config_.minSize;
        initialized_ = true;
        running_ = true;
        
        // Start maintenance thread
        maintenanceThread_ = std::thread(&ConnectionPool::maintenanceLoop, this);
        
        return core::Result<void>::ok();
    }
    
    /**
     * @brief Shutdown the connection pool
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!initialized_) {
                return;
            }
            
            running_ = false;
            initialized_ = false;
        }
        
        // Wake up maintenance thread
        maintenanceCondition_.notify_all();
        
        // Wait for maintenance thread
        if (maintenanceThread_.joinable()) {
            maintenanceThread_.join();
        }
        
        // Clear all connections
        clear();
    }
    
    /**
     * @brief Borrow a connection from the pool
     * @param timeout Timeout for waiting
     * @return Connection or error
     */
    core::Result<std::shared_ptr<ConnectionType>> borrowConnection(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!initialized_) {
            return core::Result<std::shared_ptr<ConnectionType>>::error("Connection pool not initialized");
        }
        
        auto deadline = std::chrono::steady_clock::now() + timeout;
        
        while (running_) {
            // Try to get an available connection
            if (!availableConnections_.empty()) {
                auto conn = std::move(availableConnections_.front());
                availableConnections_.pop();
                
                // Validate connection if required
                if (config_.validateOnBorrow && !factory_->validateConnection(conn.get())) {
                    // Connection is invalid, try to create a new one
                    if (currentSize_ < config_.maxSize) {
                        auto newConn = factory_->createConnection();
                        if (newConn) {
                            currentSize_++;
                            conn = std::move(newConn);
                        } else {
                            currentSize_--;
                            continue; // Try again
                        }
                    } else {
                        continue; // Try to get another connection
                    }
                }
                
                // Update activity and add to active connections
                conn->updateActivity();
                auto sharedConn = std::shared_ptr<ConnectionType>(
                    conn.release(),
                    [this](ConnectionType* c) { this->returnConnection(c); });
                
                activeConnections_++;
                return core::Result<std::shared_ptr<ConnectionType>>::ok(sharedConn);
            }
            
            // Try to create a new connection if pool not at max size
            if (currentSize_ < config_.maxSize) {
                auto newConn = factory_->createConnection();
                if (newConn) {
                    currentSize_++;
                    newConn->updateActivity();
                    
                    auto sharedConn = std::shared_ptr<ConnectionType>(
                        newConn.release(),
                        [this](ConnectionType* c) { this->returnConnection(c); });
                    
                    activeConnections_++;
                    return core::Result<std::shared_ptr<ConnectionType>>::ok(sharedConn);
                }
            }
            
            // Wait for a connection to become available
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                return core::Result<std::shared_ptr<ConnectionType>>::error("Connection pool timeout");
            }
        }
        
        return core::Result<std::shared_ptr<ConnectionType>>::error("Connection pool is shutting down");
    }
    
    /**
     * @brief Get pool statistics
     */
    struct PoolStats {
        size_t totalSize;
        size_t availableSize;
        size_t activeSize;
        size_t maxSize;
        size_t totalBorrowed;
        size_t totalCreated;
        size_t totalDestroyed;
    };
    
    PoolStats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        return {
            currentSize_,
            availableConnections_.size(),
            activeConnections_,
            config_.maxSize,
            totalBorrowed_,
            totalCreated_,
            totalDestroyed_
        };
    }

private:
    /**
     * @brief Return a connection to the pool
     * @param conn Connection to return
     */
    void returnConnection(ConnectionType* conn) {
        if (!conn) return;
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        activeConnections_--;
        
        if (!running_) {
            // Pool is shutting down, destroy the connection
            delete conn;
            currentSize_--;
            return;
        }
        
        // Validate connection if required
        if (config_.validateOnReturn && !factory_->validateConnection(conn)) {
            // Connection is invalid, destroy it
            delete conn;
            currentSize_--;
            totalDestroyed_++;
            
            // Notify that a slot is available for new connection
            cv_.notify_one();
            return;
        }
        
        // Reset and return to pool
        conn->reset();
        conn->updateActivity();
        availableConnections_.push(std::unique_ptr<ConnectionType>(conn));
        
        // Notify waiting threads
        cv_.notify_one();
    }
    
    /**
     * @brief Maintenance loop for connection validation and cleanup
     */
    void maintenanceLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // Wait for validation interval
            maintenanceCondition_.wait_for(lock, 
                std::chrono::seconds(config_.validationInterval),
                [this] { return !running_; });
            
            if (!running_) break;
            
            // Validate and clean up idle connections
            std::queue<std::unique_ptr<ConnectionType>> validConnections;
            auto now = std::chrono::steady_clock::now();
            
            while (!availableConnections_.empty()) {
                auto conn = std::move(availableConnections_.front());
                availableConnections_.pop();
                
                auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(
                    now - conn->getLastActivity()).count();
                
                // Check if connection is idle for too long
                if (idleTime > config_.maxIdleTime && currentSize_ > config_.minSize) {
                    // Destroy idle connection
                    currentSize_--;
                    totalDestroyed_++;
                    continue;
                }
                
                // Validate connection
                if (!factory_->validateConnection(conn.get())) {
                    // Connection is invalid
                    currentSize_--;
                    totalDestroyed_++;
                    
                    // Try to create a replacement if below min size
                    if (currentSize_ < config_.minSize) {
                        auto newConn = factory_->createConnection();
                        if (newConn) {
                            currentSize_++;
                            totalCreated_++;
                            validConnections.push(std::move(newConn));
                        }
                    }
                } else {
                    validConnections.push(std::move(conn));
                }
            }
            
            // Put valid connections back
            availableConnections_ = std::move(validConnections);
            
            // Ensure minimum pool size
            while (currentSize_ < config_.minSize) {
                auto conn = factory_->createConnection();
                if (conn) {
                    currentSize_++;
                    totalCreated_++;
                    availableConnections_.push(std::move(conn));
                } else {
                    break; // Failed to create connection
                }
            }
        }
    }
    
    /**
     * @brief Clear all connections
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        while (!availableConnections_.empty()) {
            availableConnections_.pop();
        }
        
        currentSize_ = 0;
        activeConnections_ = 0;
    }
    
    // Members
    std::unique_ptr<ConnectionFactory<ConnectionType>> factory_;
    Config config_;
    
    std::queue<std::unique_ptr<ConnectionType>> availableConnections_;
    size_t currentSize_{0};
    std::atomic<size_t> activeConnections_{0};
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    std::thread maintenanceThread_;
    std::condition_variable maintenanceCondition_;
    
    // Statistics
    std::atomic<size_t> totalBorrowed_{0};
    std::atomic<size_t> totalCreated_{0};
    std::atomic<size_t> totalDestroyed_{0};
};

} // namespace network
} // namespace common
} // namespace pacs