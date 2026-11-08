// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace kcenon::pacs::cli {

// Signal handlers must not log, acquire a server lock or join an I/O thread.
static_assert(std::atomic<bool>::is_always_lock_free);
inline std::atomic<bool> shutdown_requested{false};

inline void request_shutdown(int /* signal */) {
    shutdown_requested.store(true, std::memory_order_relaxed);
}

inline void install_shutdown_signals() {
    shutdown_requested.store(false, std::memory_order_relaxed);
    std::signal(SIGINT, request_shutdown);
    std::signal(SIGTERM, request_shutdown);
#ifndef _WIN32
    std::signal(SIGHUP, request_shutdown);
#endif
}

template <typename Server>
void wait_for_shutdown(Server& server) {
    while (!shutdown_requested.load(std::memory_order_relaxed) && server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{50});
    }
    server.stop();
}

}  // namespace kcenon::pacs::cli
