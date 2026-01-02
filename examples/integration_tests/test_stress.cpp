/**
 * @file test_stress.cpp
 * @brief Scenario 4: Multi-Association Stress Tests
 *
 * Tests system behavior under load:
 * 1. Start Storage SCP
 * 2. Launch multiple concurrent Storage SCUs
 * 3. Each SCU sends multiple files
 * 4. Verify all files stored
 * 5. Verify database consistency
 * 6. Stop Storage SCP
 *
 * @see Issue #111 - Integration Test Suite
 */

#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/storage/index_database.hpp"

#include <atomic>
#include <future>
#include <latch>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
#include <set>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::services;
using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;
using namespace pacs::network::dimse;

// =============================================================================
// Helper: Stress Test Storage Server
// =============================================================================

namespace {

/**
 * @brief Storage server optimized for stress testing
 *
 * Tracks all stored instances and provides thread-safe counters.
 */
class stress_test_server {
public:
    explicit stress_test_server(uint16_t port, const std::string& ae_title = "STRESS_SCP")
        : port_(port)
        , ae_title_(ae_title)
        , test_dir_("stress_test_")
        , storage_dir_(test_dir_.path() / "archive")
        , db_path_(test_dir_.path() / "index.db") {

        std::filesystem::create_directories(storage_dir_);

        server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 50;  // High limit for stress testing
        config.idle_timeout = std::chrono::seconds{120};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.7";
        config.implementation_version_name = "STRESS_SCP";

        server_ = std::make_unique<dicom_server>(config);
        
        file_storage_config fs_conf;
        fs_conf.root_path = storage_dir_;
        file_storage_ = std::make_unique<file_storage>(fs_conf);
        
        auto db_result = index_database::open(db_path_.string());
        if (db_result.is_ok()) {
            database_ = std::move(db_result.value());
        } else {
            throw std::runtime_error("Failed to open database: " + db_result.error().message);
        }
    }

    bool initialize() {
        server_->register_service(std::make_shared<verification_scp>());

        auto storage_scp_ptr = std::make_shared<storage_scp>();
        storage_scp_ptr->set_handler([this](
            const dicom_dataset& dataset,
            const std::string& calling_ae,
            const std::string& sop_class_uid,
            const std::string& sop_instance_uid) {
            return handle_store(dataset, calling_ae, sop_class_uid, sop_instance_uid);
        });
        server_->register_service(storage_scp_ptr);
        return true;
    }

    bool start() {
        auto result = server_->start();
        if (result.is_ok()) {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            return true;
        }
        return false;
    }

    void stop() {
        server_->stop();
    }

    uint16_t port() const { return port_; }
    const std::string& ae_title() const { return ae_title_; }

    size_t stored_count() const { return stored_count_.load(); }
    size_t failed_count() const { return failed_count_.load(); }

    std::vector<std::string> stored_instance_uids() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stored_instance_uids_;
    }

    bool verify_consistency() const {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check that stored count matches unique instance UIDs
        std::set<std::string> unique_uids(
            stored_instance_uids_.begin(),
            stored_instance_uids_.end());

        return unique_uids.size() == stored_count_.load();
    }

private:
    storage_status handle_store(
        const dicom_dataset& dataset,
        const std::string& /* calling_ae */,
        const std::string& /* sop_class_uid */,
        const std::string& sop_instance_uid) {

        // Store to filesystem
        auto store_result = file_storage_->store(dataset);
        if (store_result.is_err()) {
            ++failed_count_;
            return storage_status::storage_error;
        }

        // 1. Upsert Patient
        auto patient_res = database_->upsert_patient(
            dataset.get_string(tags::patient_id),
            dataset.get_string(tags::patient_name),
            dataset.get_string(tags::patient_birth_date),
            dataset.get_string(tags::patient_sex));
        
        if (patient_res.is_err()) {
            ++failed_count_;
            return storage_status::storage_error;
        }
        int64_t patient_pk = patient_res.value();

        // 2. Upsert Study
        auto study_res = database_->upsert_study(
            patient_pk,
            dataset.get_string(tags::study_instance_uid),
            dataset.get_string(tags::study_id),
            dataset.get_string(tags::study_date),
            dataset.get_string(tags::study_time),
            dataset.get_string(tags::accession_number));
            
        if (study_res.is_err()) {
            ++failed_count_;
            return storage_status::storage_error;
        }
        int64_t study_pk = study_res.value();

        // 3. Upsert Series
        // Parse series number safely
        std::optional<int> series_number;
        try {
            std::string sn = dataset.get_string(tags::series_number);
            if (!sn.empty()) series_number = std::stoi(sn);
        } catch (...) {}

        auto series_res = database_->upsert_series(
            study_pk,
            dataset.get_string(tags::series_instance_uid),
            dataset.get_string(tags::modality),
            series_number);
            
        if (series_res.is_err()) {
            ++failed_count_;
            return storage_status::storage_error;
        }
        int64_t series_pk = series_res.value();

        // 4. Upsert Instance
        auto file_path = file_storage_->get_file_path(sop_instance_uid);
        
        // Parse instance number safely
        std::optional<int> instance_number;
        try {
            std::string in = dataset.get_string(tags::instance_number);
            if (!in.empty()) instance_number = std::stoi(in);
        } catch (...) {}

        auto instance_res = database_->upsert_instance(
            series_pk,
            sop_instance_uid,
            dataset.get_string(tags::sop_class_uid),
            file_path.string(),
            static_cast<int64_t>(std::filesystem::file_size(file_path)),
            "", // transfer syntax
            instance_number);
            
        if (instance_res.is_err()) {
            ++failed_count_;
            return storage_status::storage_error;
        }

        // Track stored instance
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stored_instance_uids_.push_back(sop_instance_uid);
        }

        ++stored_count_;
        return storage_status::success;
    }

    uint16_t port_;
    std::string ae_title_;
    test_directory test_dir_;
    std::filesystem::path storage_dir_;
    std::filesystem::path db_path_;

    std::unique_ptr<dicom_server> server_;
    std::unique_ptr<file_storage> file_storage_;
    std::unique_ptr<index_database> database_;

    std::atomic<size_t> stored_count_{0};
    std::atomic<size_t> failed_count_{0};

    mutable std::mutex mutex_;
    std::vector<std::string> stored_instance_uids_;
};

/**
 * @brief Result of a stress test worker
 */
struct worker_result {
    size_t success_count{0};
    size_t failure_count{0};
    std::chrono::milliseconds duration{0};
    std::string error_message;
};

/**
 * @brief Storage SCU worker for concurrent testing
 */
worker_result run_storage_worker(
    uint16_t server_port,
    const std::string& server_ae,
    const std::string& worker_id,
    int file_count,
    std::latch& start_latch) {

    worker_result result;
    auto start_time = std::chrono::steady_clock::now();

    // Wait for all workers to be ready
    start_latch.arrive_and_wait();

    try {
        // Configure association
        association_config config;
        config.calling_ae_title = "SCU_" + worker_id;
        config.called_ae_title = server_ae;
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.8";
        config.proposed_contexts.push_back({
            1,
            "1.2.840.10008.5.1.4.1.1.2",  // CT Image Storage
            {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
        });

        auto connect_result = association::connect(
            "localhost", server_port, config, default_timeout() * 2);

        if (connect_result.is_err()) {
            result.error_message = "Connection failed: " + connect_result.error().message;
            result.failure_count = file_count;
            return result;
        }

        auto& assoc = connect_result.value();
        storage_scu_config scu_config;
        scu_config.response_timeout = default_timeout();
        storage_scu scu{scu_config};

        // Generate unique study for this worker
        auto study_uid = generate_uid();

        // Send files
        for (int i = 0; i < file_count; ++i) {
            auto dataset = generate_ct_dataset(study_uid);
            dataset.set_string(tags::instance_number, vr_type::IS, std::to_string(i + 1));

            auto store_result = scu.store(assoc, dataset);
            if (store_result.is_ok() && store_result.value().is_success()) {
                ++result.success_count;
            } else {
                ++result.failure_count;
            }
        }

        (void)assoc.release(default_timeout());

    } catch (const std::exception& e) {
        result.error_message = "Exception: " + std::string(e.what());
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

}  // namespace

// =============================================================================
// Scenario 4: Multi-Association Stress Tests
// =============================================================================

TEST_CASE("Concurrent storage from multiple SCUs", "[stress][concurrent]") {
    auto port = find_available_port();
    stress_test_server server(port, "STRESS_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Test parameters - reduced for CI/testing
    constexpr int num_workers = 5;
    constexpr int files_per_worker = 10;
    constexpr int total_expected = num_workers * files_per_worker;

    std::latch start_latch(num_workers + 1);  // +1 for main thread
    std::vector<std::future<worker_result>> futures;

    // Launch workers
    for (int i = 0; i < num_workers; ++i) {
        futures.push_back(std::async(
            std::launch::async,
            run_storage_worker,
            port,
            server.ae_title(),
            std::to_string(i),
            files_per_worker,
            std::ref(start_latch)
        ));
    }

    // Release all workers simultaneously
    start_latch.arrive_and_wait();

    // Collect results
    size_t total_success = 0;
    size_t total_failure = 0;
    std::chrono::milliseconds max_duration{0};

    for (auto& future : futures) {
        auto result = future.get();
        total_success += result.success_count;
        total_failure += result.failure_count;
        max_duration = (std::max)(max_duration, result.duration);

        if (!result.error_message.empty()) {
            INFO("Worker error: " << result.error_message);
        }
    }

    INFO("Total success: " << total_success);
    INFO("Total failure: " << total_failure);
    INFO("Max duration: " << max_duration.count() << " ms");
    INFO("Server stored: " << server.stored_count());

    // Verify results
    REQUIRE(total_success == total_expected);
    REQUIRE(total_failure == 0);
    REQUIRE(server.stored_count() == total_expected);
    REQUIRE(server.verify_consistency());

    server.stop();
}

TEST_CASE("Rapid sequential connections", "[stress][sequential]") {
    auto port = find_available_port();
    stress_test_server server(port, "STRESS_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    constexpr int num_connections = 20;
    size_t success_count = 0;

    for (int i = 0; i < num_connections; ++i) {
        association_config config;
        config.calling_ae_title = "RAPID_" + std::to_string(i);
        config.called_ae_title = server.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.9";
        config.proposed_contexts.push_back({
            1,
            std::string(verification_sop_class_uid),
            {"1.2.840.10008.1.2.1"}
        });

        auto connect_result = association::connect(
            "localhost", port, config, default_timeout());

        if (connect_result.is_ok()) {
            auto& assoc = connect_result.value();
            (void)assoc.release(std::chrono::milliseconds{500});
            ++success_count;
        }
    }

    REQUIRE(success_count == num_connections);

    server.stop();
}

TEST_CASE("Large dataset storage", "[stress][large]") {
    auto port = find_available_port();
    stress_test_server server(port, "STRESS_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Create larger dataset (512x512 instead of 64x64)
    dicom_dataset large_ds;

    // Standard patient/study info
    large_ds.set_string(tags::patient_name, vr_type::PN, "LARGE^DATASET");
    large_ds.set_string(tags::patient_id, vr_type::LO, "LARGE001");
    large_ds.set_string(tags::study_instance_uid, vr_type::UI, generate_uid());
    large_ds.set_string(tags::series_instance_uid, vr_type::UI, generate_uid());
    large_ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    large_ds.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
    large_ds.set_string(tags::modality, vr_type::CS, "CT");

    // Large image (512x512 16-bit = 512KB pixel data)
    constexpr int rows = 512;
    constexpr int cols = 512;
    large_ds.set_numeric<uint16_t>(tags::rows, vr_type::US, rows);
    large_ds.set_numeric<uint16_t>(tags::columns, vr_type::US, cols);
    large_ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
    large_ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 12);
    large_ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 11);
    large_ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);
    large_ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    large_ds.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");

    // Generate pixel data
    std::vector<uint16_t> pixel_data(rows * cols);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 4095);
    for (auto& pixel : pixel_data) {
        pixel = dist(gen);
    }
    large_ds.insert(dicom_element(
        tags::pixel_data,
        vr_type::OW,
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(pixel_data.data()),
            pixel_data.size() * sizeof(uint16_t))));

    // Store the large dataset
    association_config config;
    config.calling_ae_title = "LARGE_SCU";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.10";
    config.proposed_contexts.push_back({
        1,
        "1.2.840.10008.5.1.4.1.1.2",
        {"1.2.840.10008.1.2.1"}
    });

    auto connect_result = association::connect(
        "localhost", port, config, std::chrono::milliseconds{10000});
    REQUIRE(connect_result.is_ok());

    auto& assoc = connect_result.value();
    storage_scu_config scu_config;
    scu_config.response_timeout = std::chrono::milliseconds{10000};
    storage_scu scu{scu_config};

    auto start = std::chrono::steady_clock::now();
    auto result = scu.store(assoc, large_ds);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    INFO("Large dataset storage took: " << duration.count() << " ms");

    REQUIRE(result.is_ok());
    REQUIRE(result.value().is_success());

    (void)assoc.release(default_timeout());
    server.stop();
}

TEST_CASE("Connection pool exhaustion recovery", "[stress][exhaustion]") {
    auto port = find_available_port();
    stress_test_server server(port, "STRESS_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    // Hold connections open
    constexpr int num_held = 10;
    std::vector<std::optional<association>> held_connections;

    for (int i = 0; i < num_held; ++i) {
        association_config config;
        config.calling_ae_title = "HOLD_" + std::to_string(i);
        config.called_ae_title = server.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.11";
        config.proposed_contexts.push_back({
            1,
            std::string(verification_sop_class_uid),
            {"1.2.840.10008.1.2.1"}
        });

        auto connect_result = association::connect(
            "localhost", port, config, default_timeout());
        if (connect_result.is_ok()) {
            held_connections.push_back(std::move(connect_result.value()));
        }
    }

    REQUIRE(held_connections.size() == num_held);

    // Try more connections (should still work due to max_associations = 50)
    constexpr int num_additional = 5;
    size_t additional_success = 0;

    for (int i = 0; i < num_additional; ++i) {
        association_config config;
        config.calling_ae_title = "EXTRA_" + std::to_string(i);
        config.called_ae_title = server.ae_title();
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.12";
        config.proposed_contexts.push_back({
            1,
            std::string(verification_sop_class_uid),
            {"1.2.840.10008.1.2.1"}
        });

        auto connect_result = association::connect(
            "localhost", port, config, default_timeout());
        if (connect_result.is_ok()) {
            auto& assoc = connect_result.value();
            (void)assoc.release(std::chrono::milliseconds{500});
            ++additional_success;
        }
    }

    REQUIRE(additional_success == num_additional);

    // Release held connections
    for (auto& opt_assoc : held_connections) {
        if (opt_assoc) {
            (void)opt_assoc->release(std::chrono::milliseconds{500});
        }
    }
    held_connections.clear();

    // Verify new connections work after release
    association_config config;
    config.calling_ae_title = "AFTER_RELEASE";
    config.called_ae_title = server.ae_title();
    config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.13";
    config.proposed_contexts.push_back({
        1,
        std::string(verification_sop_class_uid),
        {"1.2.840.10008.1.2.1"}
    });

    auto final_connect = association::connect(
        "localhost", port, config, default_timeout());
    REQUIRE(final_connect.is_ok());
    (void)final_connect.value().release(default_timeout());

    server.stop();
}

TEST_CASE("Mixed operations stress test", "[stress][mixed]") {
    auto port = find_available_port();
    stress_test_server server(port, "STRESS_SCP");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    constexpr int num_iterations = 10;
    std::atomic<int> echo_success{0};
    std::atomic<int> store_success{0};

    std::vector<std::thread> threads;

    // Echo workers
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < num_iterations; ++j) {
                association_config config;
                config.calling_ae_title = "ECHO_" + std::to_string(i);
                config.called_ae_title = server.ae_title();
                config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.14";
                config.proposed_contexts.push_back({
                    1,
                    std::string(verification_sop_class_uid),
                    {"1.2.840.10008.1.2.1"}
                });

                auto connect = association::connect(
                    "localhost", port, config, default_timeout());
                if (connect.is_ok()) {
                    auto& assoc = connect.value();
                    auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
                    if (ctx) {
                        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
                        if (assoc.send_dimse(*ctx, echo_rq).is_ok()) {
                            auto recv = assoc.receive_dimse(default_timeout());
                            if (recv.is_ok() && recv.value().second.status() == status_success) {
                                ++echo_success;
                            }
                        }
                    }
                    (void)assoc.release(std::chrono::milliseconds{500});
                }
            }
        });
    }

    // Store workers
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < num_iterations; ++j) {
                association_config config;
                config.calling_ae_title = "STORE_" + std::to_string(i);
                config.called_ae_title = server.ae_title();
                config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.15";
                config.proposed_contexts.push_back({
                    1,
                    "1.2.840.10008.5.1.4.1.1.2",
                    {"1.2.840.10008.1.2.1"}
                });

                auto connect = association::connect(
                    "localhost", port, config, default_timeout());
                if (connect.is_ok()) {
                    auto& assoc = connect.value();
                    storage_scu scu;
                    auto ds = generate_ct_dataset();
                    auto result = scu.store(assoc, ds);
                    if (result.is_ok() && result.value().is_success()) {
                        ++store_success;
                    }
                    (void)assoc.release(std::chrono::milliseconds{500});
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    INFO("Echo success: " << echo_success.load());
    INFO("Store success: " << store_success.load());

    REQUIRE(echo_success == 3 * num_iterations);
    REQUIRE(store_success == 2 * num_iterations);

    server.stop();
}
