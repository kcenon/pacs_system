#pragma once

#include <chrono>
#include <functional>
#include <random>
#include <algorithm>
#include <string>
#include <vector>
#include <thread>
#include <future>

#include "core/result/result.h"
#include "common/logger/logger.h"

namespace pacs {
namespace common {
namespace network {

/**
 * @brief Retry strategy types
 */
enum class RetryStrategy {
    Fixed,              // Fixed delay between retries
    Exponential,        // Exponential backoff
    ExponentialJitter,  // Exponential backoff with jitter
    Linear,             // Linear increase
    Fibonacci          // Fibonacci sequence
};

/**
 * @brief Retry policy configuration
 */
struct RetryConfig {
    size_t maxAttempts = 3;
    std::chrono::milliseconds initialDelay{1000};
    std::chrono::milliseconds maxDelay{30000};
    double backoffMultiplier = 2.0;
    double jitterFactor = 0.1;
    RetryStrategy strategy = RetryStrategy::ExponentialJitter;
    
    // Retryable error conditions
    std::vector<std::function<bool(const std::string&)>> retryableErrors;
    
    /**
     * @brief Add a retryable error pattern
     * @param pattern Error message pattern to match
     */
    void addRetryableError(const std::string& pattern) {
        retryableErrors.push_back([pattern](const std::string& error) {
            return error.find(pattern) != std::string::npos;
        });
    }
    
    /**
     * @brief Check if an error is retryable
     * @param error Error message
     * @return True if the error is retryable
     */
    bool isRetryable(const std::string& error) const {
        if (retryableErrors.empty()) {
            // If no specific errors configured, retry all
            return true;
        }
        
        return std::any_of(retryableErrors.begin(), retryableErrors.end(),
                          [&error](const auto& checker) { return checker(error); });
    }
};

/**
 * @brief Retry policy implementation
 */
class RetryPolicy {
public:
    /**
     * @brief Constructor with configuration
     * @param config Retry configuration
     */
    explicit RetryPolicy(const RetryConfig& config = RetryConfig())
        : config_(config), rng_(std::random_device{}()) {}
    
    /**
     * @brief Execute a function with retry logic
     * @tparam Func Function type
     * @tparam Args Argument types
     * @param func Function to execute
     * @param args Function arguments
     * @return Result of the function or error
     */
    template<typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) 
        -> decltype(func(std::forward<Args>(args)...)) {
        
        using ReturnType = decltype(func(std::forward<Args>(args)...));
        using ValueType = typename ReturnType::value_type;
        
        size_t attempt = 0;
        std::chrono::milliseconds delay = config_.initialDelay;
        
        while (attempt < config_.maxAttempts) {
            attempt++;
            
            // Try to execute the function
            auto result = func(std::forward<Args>(args)...);
            
            if (result.isOk()) {
                if (attempt > 1) {
                    logger::logInfo("Operation succeeded after {} attempts", attempt);
                }
                return result;
            }
            
            // Check if error is retryable
            if (!config_.isRetryable(result.getError())) {
                logger::logError("Non-retryable error: {}", result.getError());
                return result;
            }
            
            // Check if we have more attempts
            if (attempt >= config_.maxAttempts) {
                logger::logError("Max retry attempts ({}) exceeded. Last error: {}", 
                                config_.maxAttempts, result.getError());
                return ReturnType::error("Max retry attempts exceeded: " + result.getError());
            }
            
            // Calculate delay
            delay = calculateDelay(attempt, delay);
            
            logger::logWarning("Attempt {} failed: {}. Retrying in {} ms...", 
                              attempt, result.getError(), delay.count());
            
            // Sleep before retry
            std::this_thread::sleep_for(delay);
        }
        
        return ReturnType::error("Retry policy execution failed");
    }
    
    /**
     * @brief Execute an async function with retry logic
     * @tparam Func Function type
     * @tparam Args Argument types
     * @param func Async function to execute
     * @param args Function arguments
     * @return Future with the result
     */
    template<typename Func, typename... Args>
    auto executeAsync(Func&& func, Args&&... args) {
        return std::async(std::launch::async, [this, func, args...]() {
            return this->execute(func, args...);
        });
    }
    
    /**
     * @brief Reset the retry policy state
     */
    void reset() {
        // Reset any internal state if needed
    }
    
    /**
     * @brief Get the configuration
     * @return Retry configuration
     */
    const RetryConfig& getConfig() const { return config_; }
    
    /**
     * @brief Set the configuration
     * @param config New configuration
     */
    void setConfig(const RetryConfig& config) { config_ = config; }

private:
    /**
     * @brief Calculate delay for next retry
     * @param attempt Current attempt number
     * @param previousDelay Previous delay
     * @return Delay for next retry
     */
    std::chrono::milliseconds calculateDelay(size_t attempt, 
                                             std::chrono::milliseconds previousDelay) {
        std::chrono::milliseconds delay;
        
        switch (config_.strategy) {
            case RetryStrategy::Fixed:
                delay = config_.initialDelay;
                break;
                
            case RetryStrategy::Exponential:
                delay = std::chrono::milliseconds(static_cast<long>(
                    config_.initialDelay.count() * std::pow(config_.backoffMultiplier, attempt - 1)));
                break;
                
            case RetryStrategy::ExponentialJitter: {
                // Calculate exponential delay
                long baseDelay = static_cast<long>(
                    config_.initialDelay.count() * std::pow(config_.backoffMultiplier, attempt - 1));
                
                // Add jitter
                std::uniform_real_distribution<double> dist(-config_.jitterFactor, config_.jitterFactor);
                double jitter = 1.0 + dist(rng_);
                
                delay = std::chrono::milliseconds(static_cast<long>(baseDelay * jitter));
                break;
            }
            
            case RetryStrategy::Linear:
                delay = std::chrono::milliseconds(config_.initialDelay.count() * attempt);
                break;
                
            case RetryStrategy::Fibonacci:
                if (attempt <= 2) {
                    delay = config_.initialDelay;
                } else {
                    // Simple Fibonacci calculation
                    long fib = config_.initialDelay.count();
                    long prev = config_.initialDelay.count();
                    for (size_t i = 3; i <= attempt; ++i) {
                        long temp = fib;
                        fib = fib + prev;
                        prev = temp;
                    }
                    delay = std::chrono::milliseconds(fib);
                }
                break;
        }
        
        // Cap at max delay
        if (delay > config_.maxDelay) {
            delay = config_.maxDelay;
        }
        
        return delay;
    }
    
    RetryConfig config_;
    mutable std::mt19937 rng_;
};

/**
 * @brief Circuit breaker pattern implementation
 */
class CircuitBreaker {
public:
    /**
     * @brief Circuit breaker states
     */
    enum class State {
        Closed,     // Normal operation
        Open,       // Failing, reject all calls
        HalfOpen    // Testing if service recovered
    };
    
    /**
     * @brief Circuit breaker configuration
     */
    struct Config {
        size_t failureThreshold;          // Failures before opening
        size_t successThreshold;          // Successes before closing
        std::chrono::seconds openDuration; // Time to stay open
        std::chrono::seconds halfOpenTimeout; // Timeout for half-open test
        
        Config() 
            : failureThreshold(5)
            , successThreshold(2)
            , openDuration(60)
            , halfOpenTimeout(10) {}
    };
    
    /**
     * @brief Constructor
     * @param name Circuit breaker name
     * @param config Configuration
     */
    CircuitBreaker(const std::string& name, const Config& config = Config())
        : name_(name), config_(config), state_(State::Closed) {}
    
    /**
     * @brief Execute a function with circuit breaker protection
     * @tparam Func Function type
     * @tparam Args Argument types
     * @param func Function to execute
     * @param args Function arguments
     * @return Result of the function or circuit breaker error
     */
    template<typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) 
        -> decltype(func(std::forward<Args>(args)...)) {
        
        using ReturnType = decltype(func(std::forward<Args>(args)...));
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check circuit state
        auto currentState = getState();
        
        if (currentState == State::Open) {
            return ReturnType::error("Circuit breaker is open for: " + name_);
        }
        
        // Execute the function
        auto result = func(std::forward<Args>(args)...);
        
        if (result.isOk()) {
            onSuccess();
        } else {
            onFailure();
        }
        
        return result;
    }
    
    /**
     * @brief Get current state
     * @return Circuit breaker state
     */
    State getState() {
        auto now = std::chrono::steady_clock::now();
        
        if (state_ == State::Open) {
            // Check if we should transition to half-open
            if (now - lastFailureTime_ >= config_.openDuration) {
                state_ = State::HalfOpen;
                halfOpenAttempts_ = 0;
                logger::logInfo("Circuit breaker {} transitioning to half-open", name_);
            }
        }
        
        return state_;
    }
    
    /**
     * @brief Reset the circuit breaker
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = State::Closed;
        failureCount_ = 0;
        halfOpenAttempts_ = 0;
        logger::logInfo("Circuit breaker {} reset to closed", name_);
    }
    
    /**
     * @brief Get circuit breaker statistics
     */
    struct Stats {
        State state;
        size_t failureCount;
        size_t successCount;
        size_t totalCalls;
        std::chrono::steady_clock::time_point lastFailureTime;
    };
    
    Stats getStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {state_, failureCount_, successCount_, totalCalls_, lastFailureTime_};
    }

private:
    void onSuccess() {
        totalCalls_++;
        successCount_++;
        
        switch (state_) {
            case State::Closed:
                // Reset failure count on success
                failureCount_ = 0;
                break;
                
            case State::HalfOpen:
                halfOpenAttempts_++;
                if (halfOpenAttempts_ >= config_.successThreshold) {
                    // Enough successes, close the circuit
                    state_ = State::Closed;
                    failureCount_ = 0;
                    logger::logInfo("Circuit breaker {} closed after successful recovery", name_);
                }
                break;
                
            case State::Open:
                // Shouldn't happen
                break;
        }
    }
    
    void onFailure() {
        totalCalls_++;
        failureCount_++;
        lastFailureTime_ = std::chrono::steady_clock::now();
        
        switch (state_) {
            case State::Closed:
                if (failureCount_ >= config_.failureThreshold) {
                    // Too many failures, open the circuit
                    state_ = State::Open;
                    logger::logWarning("Circuit breaker {} opened after {} failures", 
                                      name_, failureCount_);
                }
                break;
                
            case State::HalfOpen:
                // Failure in half-open state, reopen the circuit
                state_ = State::Open;
                logger::logWarning("Circuit breaker {} reopened after failure in half-open state", 
                                  name_);
                break;
                
            case State::Open:
                // Already open
                break;
        }
    }
    
    std::string name_;
    Config config_;
    State state_;
    
    size_t failureCount_{0};
    size_t successCount_{0};
    size_t totalCalls_{0};
    size_t halfOpenAttempts_{0};
    
    std::chrono::steady_clock::time_point lastFailureTime_;
    
    mutable std::mutex mutex_;
};

/**
 * @brief Combined retry policy with circuit breaker
 */
class ResilientExecutor {
public:
    ResilientExecutor(const std::string& name,
                      const RetryConfig& retryConfig = RetryConfig(),
                      const CircuitBreaker::Config& cbConfig = CircuitBreaker::Config())
        : name_(name),
          retryPolicy_(retryConfig),
          circuitBreaker_(name, cbConfig) {}
    
    /**
     * @brief Execute with retry and circuit breaker protection
     */
    template<typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) 
        -> decltype(func(std::forward<Args>(args)...)) {
        
        // First check circuit breaker
        return circuitBreaker_.execute([this, &func, &args...]() {
            // Then apply retry policy
            return retryPolicy_.execute(func, std::forward<Args>(args)...);
        });
    }
    
    /**
     * @brief Reset both retry policy and circuit breaker
     */
    void reset() {
        retryPolicy_.reset();
        circuitBreaker_.reset();
    }
    
private:
    std::string name_;
    RetryPolicy retryPolicy_;
    CircuitBreaker circuitBreaker_;
};

} // namespace network
} // namespace common
} // namespace pacs