/**
 * @file signal_handler.cpp
 * @brief Implementation of cross-platform signal handling
 */

#include "signal_handler.hpp"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace pacs::samples {

// ============================================================================
// Static Member Definitions
// ============================================================================

std::atomic<bool> signal_handler::shutdown_requested_{false};
signal_handler::shutdown_callback signal_handler::callback_;
std::mutex signal_handler::mutex_;
std::condition_variable signal_handler::cv_;
std::atomic<bool> signal_handler::callback_invoked_{false};

// ============================================================================
// Signal Handler Implementation
// ============================================================================

void signal_handler::install(shutdown_callback callback) {
    callback_ = std::move(callback);
    shutdown_requested_.store(false, std::memory_order_release);
    callback_invoked_.store(false, std::memory_order_release);

#ifdef _WIN32
    std::signal(SIGINT, handler);
    std::signal(SIGBREAK, handler);
#else
    std::signal(SIGINT, handler);
    std::signal(SIGTERM, handler);
#endif
}

auto signal_handler::should_shutdown() noexcept -> bool {
    return shutdown_requested_.load(std::memory_order_acquire);
}

void signal_handler::wait_for_shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, []() {
        return shutdown_requested_.load(std::memory_order_acquire);
    });
}

void signal_handler::request_shutdown() {
    bool expected = false;
    if (shutdown_requested_.compare_exchange_strong(expected, true,
            std::memory_order_release, std::memory_order_relaxed)) {
        // Invoke callback only once
        if (callback_ && !callback_invoked_.exchange(true, std::memory_order_acq_rel)) {
            callback_();
        }
        cv_.notify_all();
    }
}

void signal_handler::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_requested_.store(false, std::memory_order_release);
    callback_invoked_.store(false, std::memory_order_release);
    callback_ = nullptr;
}

void signal_handler::handler(int /*signal*/) {
    request_shutdown();
}

// ============================================================================
// Scoped Signal Handler Implementation
// ============================================================================

scoped_signal_handler::scoped_signal_handler(signal_handler::shutdown_callback callback) {
    signal_handler::install(std::move(callback));
}

scoped_signal_handler::~scoped_signal_handler() {
    signal_handler::reset();
}

void scoped_signal_handler::wait() {
    signal_handler::wait_for_shutdown();
}

auto scoped_signal_handler::should_shutdown() const noexcept -> bool {
    return signal_handler::should_shutdown();
}

}  // namespace pacs::samples
