/**
 * @file lockfree_stress_test.cpp
 * @brief Stress tests for lock-free structures
 *
 * Comprehensive concurrency tests for the lock-free queue implementation.
 * These tests verify thread safety under high contention scenarios.
 *
 * Test Categories:
 * - ThreadSanitizer verification tests
 * - High-contention stress tests
 * - MPMC (Multi-Producer Multi-Consumer) scenarios
 * - Benchmark comparisons (mutex vs lock-free)
 *
 * @see Issue #337 - Add lock-free structure stress tests
 * @see Issue #314 - Apply Lock-free Structures
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include <kcenon/thread/lockfree/lockfree_queue.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <latch>
#include <memory>
#include <mutex>
#include <numeric>
#include <queue>
#include <random>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// =============================================================================
// Test Constants
// =============================================================================

namespace {

constexpr int DEFAULT_THREAD_COUNT = 8;
constexpr int HIGH_THREAD_COUNT = 16;
constexpr int ITERATIONS_PER_THREAD = 10000;
constexpr int HIGH_CONTENTION_ITERATIONS = 50000;
constexpr auto STRESS_TEST_DURATION = 2s;
constexpr auto BENCHMARK_DURATION = 1s;

}  // namespace

// =============================================================================
// Helper Types
// =============================================================================

namespace {

/**
 * @brief Simple payload for stress testing
 */
struct test_payload {
    uint64_t id;
    uint64_t producer_id;
    std::array<uint8_t, 64> data;

    test_payload() : id(0), producer_id(0) {
        data.fill(0);
    }

    test_payload(uint64_t i, uint64_t p) : id(i), producer_id(p) {
        data.fill(static_cast<uint8_t>(i & 0xFF));
    }
};

/**
 * @brief Mutex-based queue for benchmark comparison
 */
template <typename T>
class mutex_queue {
public:
    void enqueue(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
    }

    [[nodiscard]] auto try_dequeue() -> std::optional<T> {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
};

}  // namespace

// =============================================================================
// ThreadSanitizer Verification Tests
// =============================================================================

TEST_CASE("lockfree_queue ThreadSanitizer verification", "[concurrency][tsan]") {
    SECTION("concurrent enqueue operations") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int threads = DEFAULT_THREAD_COUNT;
        constexpr int items_per_thread = 1000;

        std::vector<std::thread> producers;
        std::latch start_latch(1);

        for (int t = 0; t < threads; ++t) {
            producers.emplace_back([&queue, &start_latch, t]() {
                start_latch.wait();
                for (int i = 0; i < items_per_thread; ++i) {
                    queue.enqueue(t * items_per_thread + i);
                }
            });
        }

        start_latch.count_down();

        for (auto& p : producers) {
            p.join();
        }

        CHECK(queue.size() == threads * items_per_thread);
    }

    SECTION("concurrent dequeue operations") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int total_items = 10000;

        // Pre-fill the queue
        for (int i = 0; i < total_items; ++i) {
            queue.enqueue(i);
        }

        constexpr int threads = DEFAULT_THREAD_COUNT;
        std::atomic<int> dequeued_count{0};
        std::vector<std::thread> consumers;
        std::latch start_latch(1);

        for (int t = 0; t < threads; ++t) {
            consumers.emplace_back([&queue, &dequeued_count, &start_latch]() {
                start_latch.wait();
                while (auto val = queue.try_dequeue()) {
                    dequeued_count.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        start_latch.count_down();

        for (auto& c : consumers) {
            c.join();
        }

        CHECK(dequeued_count.load() == total_items);
        CHECK(queue.empty());
    }

    SECTION("mixed producer-consumer operations") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int producers = 4;
        constexpr int consumers = 4;
        constexpr int items_per_producer = 2500;

        std::atomic<int> produced_count{0};
        std::atomic<int> consumed_count{0};
        std::atomic<bool> producers_done{false};

        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;
        std::latch start_latch(1);

        // Start producers
        for (int p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&queue, &produced_count, &start_latch, p]() {
                start_latch.wait();
                for (int i = 0; i < items_per_producer; ++i) {
                    queue.enqueue(p * items_per_producer + i);
                    produced_count.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Start consumers
        for (int c = 0; c < consumers; ++c) {
            consumer_threads.emplace_back([&queue, &consumed_count, &producers_done, &start_latch]() {
                start_latch.wait();
                while (!producers_done.load(std::memory_order_acquire) || !queue.empty()) {
                    if (auto val = queue.try_dequeue()) {
                        consumed_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        std::this_thread::yield();
                    }
                }
                // Final drain
                while (auto val = queue.try_dequeue()) {
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        start_latch.count_down();

        for (auto& p : producer_threads) {
            p.join();
        }
        producers_done.store(true, std::memory_order_release);

        for (auto& c : consumer_threads) {
            c.join();
        }

        CHECK(produced_count.load() == producers * items_per_producer);
        CHECK(consumed_count.load() == producers * items_per_producer);
    }

    SECTION("rapid size queries during modifications") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int threads = DEFAULT_THREAD_COUNT;
        constexpr int iterations = 5000;

        std::atomic<bool> running{true};
        std::atomic<uint64_t> size_queries{0};

        std::vector<std::thread> modifiers;
        std::vector<std::thread> observers;
        std::latch start_latch(1);

        // Modifier threads
        for (int t = 0; t < threads; ++t) {
            modifiers.emplace_back([&queue, &start_latch, t]() {
                start_latch.wait();
                for (int i = 0; i < iterations; ++i) {
                    if (i % 2 == 0) {
                        queue.enqueue(t * iterations + i);
                    } else {
                        (void)queue.try_dequeue();
                    }
                }
            });
        }

        // Observer threads
        for (int t = 0; t < threads / 2; ++t) {
            observers.emplace_back([&queue, &running, &size_queries, &start_latch]() {
                start_latch.wait();
                while (running.load(std::memory_order_relaxed)) {
                    [[maybe_unused]] auto s = queue.size();
                    [[maybe_unused]] auto e = queue.empty();
                    size_queries.fetch_add(2, std::memory_order_relaxed);
                }
            });
        }

        start_latch.count_down();

        for (auto& m : modifiers) {
            m.join();
        }
        running.store(false);

        for (auto& o : observers) {
            o.join();
        }

        CHECK(size_queries.load() > 0);
    }
}

// =============================================================================
// High-Contention Stress Tests
// =============================================================================

TEST_CASE("lockfree_queue high-contention stress", "[concurrency][stress]") {
    SECTION("high-throughput PDU processing simulation") {
        // Simulates multiple producers sending PDUs, single consumer processing
        kcenon::thread::lockfree_queue<test_payload> queue;

        constexpr int producer_count = HIGH_THREAD_COUNT;
        constexpr int items_per_producer = HIGH_CONTENTION_ITERATIONS / producer_count;

        std::atomic<uint64_t> produced{0};
        std::atomic<uint64_t> consumed{0};
        std::atomic<bool> producers_done{false};

        std::vector<std::thread> producers;
        std::latch start_latch(1);

        // Multiple producers
        for (int p = 0; p < producer_count; ++p) {
            producers.emplace_back([&queue, &produced, &start_latch, p]() {
                start_latch.wait();
                for (int i = 0; i < items_per_producer; ++i) {
                    queue.enqueue(test_payload(i, p));
                    produced.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Single consumer
        std::thread consumer([&queue, &consumed, &producers_done]() {
            while (!producers_done.load(std::memory_order_acquire) || !queue.empty()) {
                if (auto val = queue.try_dequeue()) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
            // Final drain
            while (auto val = queue.try_dequeue()) {
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });

        start_latch.count_down();

        for (auto& p : producers) {
            p.join();
        }
        producers_done.store(true, std::memory_order_release);
        consumer.join();

        CHECK(produced.load() == static_cast<uint64_t>(producer_count * items_per_producer));
        CHECK(consumed.load() == produced.load());
    }

    SECTION("saturated queue operations") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int threads = HIGH_THREAD_COUNT;

        std::atomic<bool> running{true};
        std::atomic<uint64_t> operations{0};

        std::vector<std::thread> workers;
        std::latch start_latch(1);

        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&queue, &running, &operations, &start_latch, t]() {
                start_latch.wait();
                std::mt19937 rng(t);
                std::uniform_int_distribution<int> op_dist(0, 1);

                while (running.load(std::memory_order_relaxed)) {
                    if (op_dist(rng) == 0) {
                        queue.enqueue(t);
                    } else {
                        (void)queue.try_dequeue();
                    }
                    operations.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        start_latch.count_down();
        std::this_thread::sleep_for(STRESS_TEST_DURATION);
        running.store(false);

        for (auto& w : workers) {
            w.join();
        }

        // Drain remaining items
        while (queue.try_dequeue()) {
        }

        CHECK(operations.load() > 100000);
        INFO("Total operations: " << operations.load());
    }

    SECTION("wait_dequeue under contention") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int producers = 4;
        constexpr int consumers = 4;
        constexpr int items_per_producer = 5000;

        std::atomic<int> produced{0};
        std::atomic<int> consumed{0};
        std::atomic<bool> done{false};

        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;
        std::latch start_latch(1);

        // Producers
        for (int p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&queue, &produced, &start_latch]() {
                start_latch.wait();
                for (int i = 0; i < items_per_producer; ++i) {
                    queue.enqueue(i);
                    produced.fetch_add(1, std::memory_order_relaxed);
                    if (i % 100 == 0) {
                        std::this_thread::yield();
                    }
                }
            });
        }

        // Consumers using wait_dequeue
        for (int c = 0; c < consumers; ++c) {
            consumer_threads.emplace_back([&queue, &consumed, &done, &start_latch]() {
                start_latch.wait();
                while (!done.load(std::memory_order_acquire)) {
                    if (auto val = queue.wait_dequeue(50ms)) {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                // Final drain
                while (auto val = queue.try_dequeue()) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        start_latch.count_down();

        for (auto& p : producer_threads) {
            p.join();
        }

        // Wait for consumers to process remaining items
        std::this_thread::sleep_for(200ms);
        done.store(true, std::memory_order_release);
        queue.notify_all();

        for (auto& c : consumer_threads) {
            c.join();
        }

        CHECK(consumed.load() == producers * items_per_producer);
    }

    SECTION("shutdown under load") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int threads = DEFAULT_THREAD_COUNT;

        std::atomic<bool> running{true};
        std::atomic<int> shutdown_responses{0};

        std::vector<std::thread> workers;

        for (int t = 0; t < threads; ++t) {
            workers.emplace_back([&queue, &shutdown_responses, t]() {
                // Wait for data with longer timeout
                auto val = queue.wait_dequeue(5s);
                if (!val) {
                    // Should be null after shutdown
                    shutdown_responses.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Allow workers to start waiting
        std::this_thread::sleep_for(100ms);

        // Trigger shutdown
        queue.shutdown();

        for (auto& w : workers) {
            w.join();
        }

        CHECK(shutdown_responses.load() == threads);
        CHECK(queue.is_shutdown());
    }
}

// =============================================================================
// MPMC (Multi-Producer Multi-Consumer) Tests
// =============================================================================

TEST_CASE("lockfree_queue MPMC correctness", "[concurrency][mpmc]") {
    SECTION("all items are processed exactly once") {
        kcenon::thread::lockfree_queue<uint64_t> queue;
        constexpr int producers = 4;
        constexpr int consumers = 4;
        constexpr int items_per_producer = 10000;
        constexpr uint64_t total_items = producers * items_per_producer;

        std::vector<std::atomic<int>> consumed_counts(total_items);
        for (auto& c : consumed_counts) {
            c.store(0, std::memory_order_relaxed);
        }

        std::atomic<bool> producers_done{false};
        std::vector<std::thread> producer_threads;
        std::vector<std::thread> consumer_threads;
        std::latch start_latch(1);

        // Producers - each produces unique IDs
        for (int p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&queue, &start_latch, p]() {
                start_latch.wait();
                for (int i = 0; i < items_per_producer; ++i) {
                    uint64_t id = static_cast<uint64_t>(p) * items_per_producer + i;
                    queue.enqueue(id);
                }
            });
        }

        // Consumers
        for (int c = 0; c < consumers; ++c) {
            consumer_threads.emplace_back([&queue, &consumed_counts, &producers_done, &start_latch]() {
                start_latch.wait();
                while (!producers_done.load(std::memory_order_acquire) || !queue.empty()) {
                    if (auto val = queue.try_dequeue()) {
                        if (*val < consumed_counts.size()) {
                            consumed_counts[*val].fetch_add(1, std::memory_order_relaxed);
                        }
                    } else {
                        std::this_thread::yield();
                    }
                }
                // Final drain
                while (auto val = queue.try_dequeue()) {
                    if (*val < consumed_counts.size()) {
                        consumed_counts[*val].fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start_latch.count_down();

        for (auto& p : producer_threads) {
            p.join();
        }
        producers_done.store(true, std::memory_order_release);

        for (auto& c : consumer_threads) {
            c.join();
        }

        // Verify each item was consumed exactly once
        bool all_once = true;
        int missing = 0;
        int duplicates = 0;
        for (uint64_t i = 0; i < total_items; ++i) {
            int count = consumed_counts[i].load(std::memory_order_relaxed);
            if (count != 1) {
                all_once = false;
                if (count == 0) {
                    missing++;
                } else {
                    duplicates++;
                }
            }
        }

        INFO("Missing: " << missing << ", Duplicates: " << duplicates);
        CHECK(all_once);
    }

    SECTION("ordering within single producer is preserved") {
        kcenon::thread::lockfree_queue<std::pair<int, int>> queue;
        constexpr int producers = 4;
        constexpr int items_per_producer = 5000;

        std::vector<std::vector<int>> received_by_producer(producers);
        for (auto& v : received_by_producer) {
            v.reserve(items_per_producer);
        }
        std::mutex received_mutex;

        std::atomic<bool> producers_done{false};
        std::vector<std::thread> producer_threads;
        std::latch start_latch(1);

        // Producers
        for (int p = 0; p < producers; ++p) {
            producer_threads.emplace_back([&queue, &start_latch, p]() {
                start_latch.wait();
                for (int i = 0; i < items_per_producer; ++i) {
                    queue.enqueue({p, i});
                }
            });
        }

        // Single consumer to check ordering
        std::thread consumer([&]() {
            start_latch.wait();
            while (!producers_done.load(std::memory_order_acquire) || !queue.empty()) {
                if (auto val = queue.try_dequeue()) {
                    std::lock_guard<std::mutex> lock(received_mutex);
                    received_by_producer[val->first].push_back(val->second);
                } else {
                    std::this_thread::yield();
                }
            }
            // Final drain
            while (auto val = queue.try_dequeue()) {
                std::lock_guard<std::mutex> lock(received_mutex);
                received_by_producer[val->first].push_back(val->second);
            }
        });

        start_latch.count_down();

        for (auto& p : producer_threads) {
            p.join();
        }
        producers_done.store(true, std::memory_order_release);
        consumer.join();

        // Verify ordering for each producer
        bool all_ordered = true;
        for (int p = 0; p < producers; ++p) {
            const auto& received = received_by_producer[p];
            for (size_t i = 1; i < received.size(); ++i) {
                if (received[i] <= received[i - 1]) {
                    all_ordered = false;
                    INFO("Producer " << p << " ordering violated at index " << i);
                }
            }
        }

        CHECK(all_ordered);
    }
}

// =============================================================================
// Benchmark Comparisons (mutex vs lock-free)
// =============================================================================

TEST_CASE("lockfree_queue vs mutex_queue benchmark", "[concurrency][benchmark]") {
    constexpr int items = 100000;

    BENCHMARK("lockfree_queue single-threaded enqueue") {
        kcenon::thread::lockfree_queue<int> queue;
        for (int i = 0; i < items; ++i) {
            queue.enqueue(i);
        }
        return queue.size();
    };

    BENCHMARK("mutex_queue single-threaded enqueue") {
        mutex_queue<int> queue;
        for (int i = 0; i < items; ++i) {
            queue.enqueue(i);
        }
        return queue.size();
    };

    BENCHMARK("lockfree_queue single-threaded enqueue+dequeue") {
        kcenon::thread::lockfree_queue<int> queue;
        for (int i = 0; i < items; ++i) {
            queue.enqueue(i);
        }
        int count = 0;
        while (queue.try_dequeue()) {
            count++;
        }
        return count;
    };

    BENCHMARK("mutex_queue single-threaded enqueue+dequeue") {
        mutex_queue<int> queue;
        for (int i = 0; i < items; ++i) {
            queue.enqueue(i);
        }
        int count = 0;
        while (queue.try_dequeue()) {
            count++;
        }
        return count;
    };
}

TEST_CASE("concurrent benchmark comparison", "[concurrency][benchmark]") {
    constexpr int threads = 4;
    constexpr int items_per_thread = 25000;

    BENCHMARK("lockfree_queue multi-producer") {
        kcenon::thread::lockfree_queue<int> queue;
        std::vector<std::thread> producers;
        std::latch start_latch(1);

        for (int t = 0; t < threads; ++t) {
            producers.emplace_back([&queue, &start_latch]() {
                start_latch.wait();
                for (int i = 0; i < items_per_thread; ++i) {
                    queue.enqueue(i);
                }
            });
        }

        start_latch.count_down();
        for (auto& p : producers) {
            p.join();
        }

        return queue.size();
    };

    BENCHMARK("mutex_queue multi-producer") {
        mutex_queue<int> queue;
        std::vector<std::thread> producers;
        std::latch start_latch(1);

        for (int t = 0; t < threads; ++t) {
            producers.emplace_back([&queue, &start_latch]() {
                start_latch.wait();
                for (int i = 0; i < items_per_thread; ++i) {
                    queue.enqueue(i);
                }
            });
        }

        start_latch.count_down();
        for (auto& p : producers) {
            p.join();
        }

        return queue.size();
    };

    BENCHMARK("lockfree_queue MPMC") {
        kcenon::thread::lockfree_queue<int> queue;
        std::atomic<int> consumed{0};
        std::atomic<bool> done{false};
        std::latch start_latch(1);

        // Pre-fill
        for (int i = 0; i < threads * items_per_thread; ++i) {
            queue.enqueue(i);
        }

        std::vector<std::thread> consumers;
        for (int t = 0; t < threads; ++t) {
            consumers.emplace_back([&queue, &consumed, &done, &start_latch]() {
                start_latch.wait();
                while (!done.load(std::memory_order_acquire)) {
                    if (queue.try_dequeue()) {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start_latch.count_down();

        while (consumed.load() < threads * items_per_thread) {
            std::this_thread::yield();
        }
        done.store(true);

        for (auto& c : consumers) {
            c.join();
        }

        return consumed.load();
    };

    BENCHMARK("mutex_queue MPMC") {
        mutex_queue<int> queue;
        std::atomic<int> consumed{0};
        std::atomic<bool> done{false};
        std::latch start_latch(1);

        // Pre-fill
        for (int i = 0; i < threads * items_per_thread; ++i) {
            queue.enqueue(i);
        }

        std::vector<std::thread> consumers;
        for (int t = 0; t < threads; ++t) {
            consumers.emplace_back([&queue, &consumed, &done, &start_latch]() {
                start_latch.wait();
                while (!done.load(std::memory_order_acquire)) {
                    if (queue.try_dequeue()) {
                        consumed.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        start_latch.count_down();

        while (consumed.load() < threads * items_per_thread) {
            std::this_thread::yield();
        }
        done.store(true);

        for (auto& c : consumers) {
            c.join();
        }

        return consumed.load();
    };
}

// =============================================================================
// Memory Safety Tests
// =============================================================================

TEST_CASE("lockfree_queue memory safety", "[concurrency][memory]") {
    SECTION("no leaks on destruction") {
        for (int cycle = 0; cycle < 10; ++cycle) {
            auto queue = std::make_unique<kcenon::thread::lockfree_queue<test_payload>>();

            std::vector<std::thread> producers;
            for (int t = 0; t < 4; ++t) {
                producers.emplace_back([&queue, t]() {
                    for (int i = 0; i < 1000; ++i) {
                        queue->enqueue(test_payload(i, t));
                    }
                });
            }

            for (auto& p : producers) {
                p.join();
            }

            // Destroy queue with items still in it
            queue.reset();
        }

        CHECK(true);  // No crashes or sanitizer errors
    }

    SECTION("no use-after-free during concurrent operations") {
        kcenon::thread::lockfree_queue<std::shared_ptr<int>> queue;
        std::atomic<bool> running{true};
        std::atomic<int> total_ops{0};

        std::vector<std::thread> workers;
        for (int t = 0; t < DEFAULT_THREAD_COUNT; ++t) {
            workers.emplace_back([&queue, &running, &total_ops, t]() {
                std::mt19937 rng(t);
                std::uniform_int_distribution<int> op_dist(0, 2);

                while (running.load(std::memory_order_relaxed)) {
                    int op = op_dist(rng);
                    if (op == 0) {
                        queue.enqueue(std::make_shared<int>(t));
                    } else if (op == 1) {
                        if (auto val = queue.try_dequeue()) {
                            // Access the shared_ptr to verify no use-after-free
                            [[maybe_unused]] int v = **val;
                        }
                    } else {
                        [[maybe_unused]] auto s = queue.size();
                    }
                    total_ops.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        std::this_thread::sleep_for(STRESS_TEST_DURATION);
        running.store(false);

        for (auto& w : workers) {
            w.join();
        }

        CHECK(total_ops.load() > 0);
    }
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_CASE("lockfree_queue edge cases", "[concurrency][edge]") {
    SECTION("empty queue dequeue") {
        kcenon::thread::lockfree_queue<int> queue;

        std::vector<std::thread> dequeuers;
        std::atomic<int> null_results{0};

        for (int t = 0; t < DEFAULT_THREAD_COUNT; ++t) {
            dequeuers.emplace_back([&queue, &null_results]() {
                for (int i = 0; i < 1000; ++i) {
                    if (!queue.try_dequeue()) {
                        null_results.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }

        for (auto& d : dequeuers) {
            d.join();
        }

        CHECK(null_results.load() == DEFAULT_THREAD_COUNT * 1000);
    }

    SECTION("single item rapid enqueue-dequeue") {
        kcenon::thread::lockfree_queue<int> queue;
        constexpr int iterations = 100000;

        std::atomic<int> successes{0};

        std::thread producer([&queue]() {
            for (int i = 0; i < iterations; ++i) {
                queue.enqueue(i);
            }
        });

        std::thread consumer([&queue, &successes]() {
            for (int i = 0; i < iterations; ) {
                if (queue.try_dequeue()) {
                    successes.fetch_add(1, std::memory_order_relaxed);
                    ++i;
                }
            }
        });

        producer.join();
        consumer.join();

        CHECK(successes.load() == iterations);
    }

    SECTION("wait_dequeue timeout behavior") {
        kcenon::thread::lockfree_queue<int> queue;

        auto start = std::chrono::steady_clock::now();
        auto result = queue.wait_dequeue(100ms);
        auto elapsed = std::chrono::steady_clock::now() - start;

        CHECK_FALSE(result.has_value());
        CHECK(elapsed >= 100ms);
        CHECK(elapsed < 200ms);  // Allow some timing tolerance
    }
}
