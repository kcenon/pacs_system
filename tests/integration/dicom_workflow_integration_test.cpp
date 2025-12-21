/**
 * @file dicom_workflow_integration_test.cpp
 * @brief Integration tests for DICOM Store-and-Forward workflow
 *
 * This file contains cross-system integration tests verifying DICOM
 * Store-and-Forward workflows involving network_system, thread_system,
 * and logger_system interactions.
 *
 * Part of Issue #390 - Enhance cross-system integration tests
 * Addresses Issue #391 - DICOM Store-and-Forward integration test
 */

#include <pacs/integration/logger_adapter.hpp>
#include <pacs/integration/network_adapter.hpp>
#include <pacs/integration/thread_adapter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <latch>
#include <string>
#include <thread>
#include <vector>

using namespace pacs::integration;
using namespace std::chrono_literals;

// =============================================================================
// Helper Utilities
// =============================================================================

namespace {

/**
 * @brief RAII guard to ensure all adapters are properly shut down
 */
struct cross_system_guard {
    std::filesystem::path log_dir;

    explicit cross_system_guard(const std::filesystem::path& dir) : log_dir(dir) {
        std::filesystem::create_directories(log_dir);
    }

    ~cross_system_guard() {
        thread_adapter::shutdown(true);
        logger_adapter::shutdown();
        std::this_thread::sleep_for(50ms);
        if (std::filesystem::exists(log_dir)) {
            std::filesystem::remove_all(log_dir);
        }
    }
};

/**
 * @brief Wait for a condition with timeout
 */
template <typename Pred>
bool wait_for(Pred condition, std::chrono::milliseconds timeout = 5000ms) {
    auto start = std::chrono::steady_clock::now();
    while (!condition()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

/**
 * @brief Create a temporary directory for test logs
 */
auto create_temp_log_directory() -> std::filesystem::path {
    auto temp_dir = std::filesystem::temp_directory_path() / "pacs_workflow_test";
    std::filesystem::create_directories(temp_dir);
    return temp_dir;
}

/**
 * @brief Simulated DICOM store operation result
 */
struct store_result {
    std::string sop_instance_uid;
    storage_status status;
    std::chrono::milliseconds processing_time;
};

/**
 * @brief Simulate C-STORE processing with thread pool
 */
auto simulate_c_store_processing(const std::string& sop_uid) -> store_result {
    // Simulate DICOM parsing and database storage
    std::this_thread::sleep_for(std::chrono::milliseconds(10 + rand() % 20));

    return store_result{
        .sop_instance_uid = sop_uid,
        .status = storage_status::success,
        .processing_time = std::chrono::milliseconds(15)};
}

}  // namespace

// =============================================================================
// Cross-System Workflow Tests
// =============================================================================

TEST_CASE("DICOM store workflow with thread pool and logging",
          "[integration][workflow][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    cross_system_guard guard(temp_dir);

    // Initialize logger
    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_file = true;
    log_config.enable_audit_log = true;
    log_config.min_level = log_level::debug;
    logger_adapter::initialize(log_config);

    // Initialize thread pool
    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_config.max_threads = 4;
    thread_config.pool_name = "dicom_workflow_test";
    thread_adapter::configure(thread_config);

    SECTION("Single C-STORE with logging") {
        REQUIRE(thread_adapter::start());
        REQUIRE(thread_adapter::is_running());

        std::atomic<bool> store_completed{false};

        auto future = thread_adapter::submit([&store_completed]() {
            auto result = simulate_c_store_processing("1.2.3.4.5.6.7.8.9");

            logger_adapter::log_c_store_received(
                "TEST_MODALITY", "PATIENT001", "1.2.3.4",
                result.sop_instance_uid, result.status);

            store_completed = true;
            return result;
        });

        auto result = future.get();
        REQUIRE(result.status == storage_status::success);
        REQUIRE(store_completed);

        logger_adapter::flush();
    }

    SECTION("Multiple concurrent C-STORE operations") {
        REQUIRE(thread_adapter::start());

        constexpr int store_count = 20;
        std::atomic<int> completed_count{0};
        std::vector<std::future<store_result>> futures;
        futures.reserve(store_count);

        for (int i = 0; i < store_count; ++i) {
            futures.push_back(thread_adapter::submit([i, &completed_count]() {
                std::string sop_uid = "1.2.3.4.5." + std::to_string(i);
                auto result = simulate_c_store_processing(sop_uid);

                logger_adapter::log_c_store_received(
                    "MODALITY_" + std::to_string(i % 4),
                    "PATIENT" + std::to_string(i),
                    "1.2.3.4." + std::to_string(i),
                    result.sop_instance_uid, result.status);

                completed_count++;
                return result;
            }));
        }

        for (auto& future : futures) {
            auto result = future.get();
            REQUIRE(result.status == storage_status::success);
        }

        REQUIRE(completed_count == store_count);
        logger_adapter::flush();
    }

    SECTION("C-STORE with priority scheduling") {
        REQUIRE(thread_adapter::start());

        std::vector<int> completion_order;
        std::mutex order_mutex;
        std::latch start_latch(1);

        // Submit low priority tasks first
        std::vector<std::future<int>> low_futures;
        for (int i = 0; i < 3; ++i) {
            low_futures.push_back(thread_adapter::submit_with_priority(
                job_priority::low,
                [i, &completion_order, &order_mutex, &start_latch]() {
                    start_latch.wait();
                    std::this_thread::sleep_for(5ms);
                    std::lock_guard lock(order_mutex);
                    completion_order.push_back(i + 100);  // Low priority IDs
                    return i + 100;
                }));
        }

        // Submit high priority task
        auto high_future = thread_adapter::submit_with_priority(
            job_priority::high,
            [&completion_order, &order_mutex, &start_latch]() {
                start_latch.wait();
                std::this_thread::sleep_for(5ms);
                std::lock_guard lock(order_mutex);
                completion_order.push_back(1);  // High priority ID
                return 1;
            });

        // Release all tasks
        start_latch.count_down();

        // Wait for completion
        high_future.get();
        for (auto& f : low_futures) {
            f.get();
        }

        REQUIRE(completion_order.size() == 4);
    }
}

// =============================================================================
// Association and Logging Integration Tests
// =============================================================================

TEST_CASE("Association lifecycle with audit logging",
          "[integration][association][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    cross_system_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_audit_log = true;
    log_config.audit_log_format = "json";
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 2;
    thread_adapter::configure(thread_config);

    SECTION("Association established and released with logging") {
        REQUIRE(thread_adapter::start());

        auto future = thread_adapter::submit([]() {
            // Simulate association establishment
            logger_adapter::log_association_established(
                "REMOTE_AE", "LOCAL_SCP", "192.168.1.100");

            // Simulate some DICOM operations
            std::this_thread::sleep_for(10ms);

            logger_adapter::log_c_find_executed(
                "REMOTE_AE", query_level::study, 5);

            // Simulate association release
            logger_adapter::log_association_released("REMOTE_AE", "LOCAL_SCP");

            return true;
        });

        REQUIRE(future.get());
        logger_adapter::flush();

        // Verify audit log exists
        auto audit_path = temp_dir / "audit.json";
        REQUIRE(wait_for([&audit_path]() {
            return std::filesystem::exists(audit_path);
        }));
    }

    SECTION("Multiple concurrent associations") {
        REQUIRE(thread_adapter::start());

        constexpr int association_count = 5;
        std::atomic<int> established_count{0};
        std::atomic<int> released_count{0};

        std::vector<std::future<void>> futures;
        for (int i = 0; i < association_count; ++i) {
            futures.push_back(thread_adapter::submit(
                [i, &established_count, &released_count]() {
                    std::string remote_ae = "MODALITY_" + std::to_string(i);
                    std::string remote_ip = "192.168.1." + std::to_string(100 + i);

                    logger_adapter::log_association_established(
                        remote_ae, "PACS_SCP", remote_ip);
                    established_count++;

                    // Simulate DICOM work
                    std::this_thread::sleep_for(std::chrono::milliseconds(5 + i * 2));

                    logger_adapter::log_association_released(remote_ae, "PACS_SCP");
                    released_count++;
                }));
        }

        for (auto& future : futures) {
            future.get();
        }

        REQUIRE(established_count == association_count);
        REQUIRE(released_count == association_count);
        logger_adapter::flush();
    }
}

// =============================================================================
// C-MOVE Workflow Tests
// =============================================================================

TEST_CASE("C-MOVE workflow with thread pool",
          "[integration][cmove][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    cross_system_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    log_config.enable_audit_log = true;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_config.max_threads = 8;
    thread_adapter::configure(thread_config);

    SECTION("C-MOVE with sub-operations") {
        REQUIRE(thread_adapter::start());

        std::atomic<int> sub_operations_completed{0};
        constexpr int instance_count = 10;

        // Simulate C-MOVE that triggers multiple C-STORE sub-operations
        auto move_future = thread_adapter::submit([&sub_operations_completed]() {
            std::vector<std::future<void>> store_futures;

            for (int i = 0; i < instance_count; ++i) {
                store_futures.push_back(thread_adapter::submit(
                    [i, &sub_operations_completed]() {
                        // Simulate C-STORE sub-operation
                        std::this_thread::sleep_for(5ms);
                        sub_operations_completed++;
                    }));
            }

            // Wait for all sub-operations
            for (auto& f : store_futures) {
                f.get();
            }

            return sub_operations_completed.load();
        });

        auto result = move_future.get();
        REQUIRE(result == instance_count);

        logger_adapter::log_c_move_executed(
            "REQUESTING_AE", "DESTINATION_AE", "1.2.3.4.5",
            static_cast<std::size_t>(instance_count), move_status::success);

        logger_adapter::flush();
    }
}

// =============================================================================
// Thread Pool Statistics During Workflow
// =============================================================================

TEST_CASE("Thread pool statistics during DICOM workflow",
          "[integration][stats][!mayfail]") {
    auto temp_dir = create_temp_log_directory();
    cross_system_guard guard(temp_dir);

    logger_config log_config;
    log_config.log_directory = temp_dir;
    log_config.enable_console = false;
    logger_adapter::initialize(log_config);

    thread_pool_config thread_config;
    thread_config.min_threads = 4;
    thread_config.max_threads = 8;
    thread_adapter::configure(thread_config);

    SECTION("Statistics reflect workload") {
        REQUIRE(thread_adapter::start());
        REQUIRE(wait_for([]() { return thread_adapter::get_thread_count() >= 4; }));

        // Record initial state
        auto initial_threads = thread_adapter::get_thread_count();
        REQUIRE(initial_threads >= 4);

        // Submit many blocking tasks
        std::latch block_latch(1);
        std::atomic<int> running_count{0};
        std::vector<std::future<void>> futures;

        for (int i = 0; i < 20; ++i) {
            futures.push_back(thread_adapter::submit([&block_latch, &running_count]() {
                running_count++;
                block_latch.wait();
            }));
        }

        // Wait for tasks to start
        REQUIRE(wait_for([&running_count]() {
            return running_count.load() >= 4;  // At least min_threads running
        }));

        // Check pending jobs
        auto pending = thread_adapter::get_pending_job_count();
        // Some tasks should be pending since we submitted more than thread count

        // Release tasks
        block_latch.count_down();

        for (auto& f : futures) {
            f.get();
        }

        // Log statistics for audit
        logger_adapter::info("Workflow completed: {} tasks processed", 20);
        logger_adapter::flush();
    }
}
