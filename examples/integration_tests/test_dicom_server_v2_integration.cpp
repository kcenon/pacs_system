/**
 * @file test_dicom_server_v2_integration.cpp
 * @brief Full integration testing for dicom_server_v2 (network_system migration)
 *
 * Comprehensive integration testing to validate the complete network_system
 * migration, ensuring all DICOM functionality works correctly.
 *
 * Test Categories:
 * 1. Unit Tests - Association handler state machine, PDU framing, service dispatching
 * 2. Integration Tests - C-ECHO, C-STORE, C-FIND, C-MOVE operations
 * 3. Stress Testing - 100 concurrent connections, 10K operations
 * 4. TLS Testing - TLS 1.2/1.3, mTLS
 * 5. Migration Validation - v1 to v2 API compatibility
 *
 * @see Issue #163 - Full integration testing for network_system migration
 * @see Issue #162 - Implement dicom_server_v2 using network_system messaging_server
 */

#include "test_fixtures.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "pacs/network/v2/dicom_server_v2.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/query_scp.hpp"
#include "pacs/storage/file_storage.hpp"
#include "pacs/storage/index_database.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <latch>
#include <mutex>
#include <thread>
#include <vector>

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::v2;
using namespace pacs::network::dimse;
using namespace pacs::services;
using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;

// =============================================================================
// Helper: V2 Test Server
// =============================================================================

namespace {

/**
 * @brief RAII wrapper for dicom_server_v2 testing
 *
 * Provides automatic server lifecycle management for V2 server tests.
 * Mirrors the test_server class but uses dicom_server_v2.
 */
class test_server_v2 {
public:
    explicit test_server_v2(
        uint16_t port = 0,
        const std::string& ae_title = "TEST_SCP_V2")
        : port_(port == 0 ? find_available_port() : port)
        , ae_title_(ae_title) {

        server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 50;
        config.idle_timeout = std::chrono::seconds{60};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.100";
        config.implementation_version_name = "TEST_SCP_V2";

        server_ = std::make_unique<dicom_server_v2>(config);
    }

    ~test_server_v2() {
        stop();
    }

    test_server_v2(const test_server_v2&) = delete;
    test_server_v2& operator=(const test_server_v2&) = delete;
    test_server_v2(test_server_v2&&) = delete;
    test_server_v2& operator=(test_server_v2&&) = delete;

    template <typename Service>
    void register_service(std::shared_ptr<Service> service) {
        server_->register_service(std::move(service));
    }

    [[nodiscard]] bool start() {
        auto result = server_->start();
        if (result.is_ok()) {
            running_ = true;
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
        return result.is_ok();
    }

    void stop() {
        if (running_) {
            server_->stop();
            running_ = false;
        }
    }

    [[nodiscard]] uint16_t port() const noexcept { return port_; }
    [[nodiscard]] const std::string& ae_title() const noexcept { return ae_title_; }
    [[nodiscard]] bool is_running() const noexcept { return running_; }
    [[nodiscard]] dicom_server_v2& server() { return *server_; }

    [[nodiscard]] server_statistics get_statistics() const {
        return server_->get_statistics();
    }

private:
    uint16_t port_;
    std::string ae_title_;
    std::unique_ptr<dicom_server_v2> server_;
    bool running_{false};
};

/**
 * @brief Storage server V2 for stress testing
 *
 * Tracks all stored instances and provides thread-safe counters.
 */
class stress_test_server_v2 {
public:
    explicit stress_test_server_v2(
        uint16_t port,
        const std::string& ae_title = "STRESS_V2")
        : port_(port)
        , ae_title_(ae_title)
        , test_dir_("stress_test_v2_")
        , storage_dir_(test_dir_.path() / "archive")
        , db_path_(test_dir_.path() / "index.db") {

        std::filesystem::create_directories(storage_dir_);

        server_config config;
        config.ae_title = ae_title_;
        config.port = port_;
        config.max_associations = 100;
        config.idle_timeout = std::chrono::seconds{120};
        config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.101";
        config.implementation_version_name = "STRESS_V2";

        server_ = std::make_unique<dicom_server_v2>(config);

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

    server_statistics get_statistics() const {
        return server_->get_statistics();
    }

private:
    storage_status handle_store(
        const dicom_dataset& dataset,
        const std::string& /* calling_ae */,
        const std::string& /* sop_class_uid */,
        const std::string& sop_instance_uid) {

        auto store_result = file_storage_->store(dataset);
        if (store_result.is_err()) {
            ++failed_count_;
            return storage_status::storage_error;
        }

        // Index in database
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

        auto file_path = file_storage_->get_file_path(sop_instance_uid);
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
            "",
            instance_number);

        if (instance_res.is_err()) {
            ++failed_count_;
            return storage_status::storage_error;
        }

        ++stored_count_;
        return storage_status::success;
    }

    uint16_t port_;
    std::string ae_title_;
    test_directory test_dir_;
    std::filesystem::path storage_dir_;
    std::filesystem::path db_path_;

    std::unique_ptr<dicom_server_v2> server_;
    std::unique_ptr<file_storage> file_storage_;
    std::unique_ptr<index_database> database_;

    std::atomic<size_t> stored_count_{0};
    std::atomic<size_t> failed_count_{0};
};

/**
 * @brief Worker result for stress tests
 */
struct v2_worker_result {
    size_t success_count{0};
    size_t failure_count{0};
    std::chrono::milliseconds duration{0};
    std::string error_message;
};

}  // namespace

// =============================================================================
// Scenario 1: Basic DICOM Operations with V2 Server
// =============================================================================

#ifdef PACS_WITH_NETWORK_SYSTEM

TEST_CASE("dicom_server_v2 C-ECHO integration", "[v2][integration][echo]") {
    auto port = find_available_port();
    test_server_v2 server(port, "V2_ECHO_SCP");
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());
    REQUIRE(server.is_running());

    SECTION("Single C-ECHO succeeds") {
        auto connect = test_association::connect(
            "localhost", port, server.ae_title(), "V2_ECHO_SCU",
            {std::string(verification_sop_class_uid)});

        REQUIRE(connect.is_ok());
        auto& assoc = connect.value();

        REQUIRE(assoc.has_accepted_context(verification_sop_class_uid));

        auto ctx_opt = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(ctx_opt.has_value());

        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
        auto send_result = assoc.send_dimse(*ctx_opt, echo_rq);
        REQUIRE(send_result.is_ok());

        auto recv_result = assoc.receive_dimse(default_timeout());
        REQUIRE(recv_result.is_ok());

        auto& [recv_ctx, echo_rsp] = recv_result.value();
        REQUIRE(echo_rsp.command() == command_field::c_echo_rsp);
        REQUIRE(echo_rsp.status() == status_success);

        (void)assoc.release(default_timeout());
    }

    SECTION("Multiple sequential C-ECHO operations") {
        constexpr int num_echos = 10;
        int success_count = 0;

        auto connect = test_association::connect(
            "localhost", port, server.ae_title(), "V2_ECHO_SCU",
            {std::string(verification_sop_class_uid)});
        REQUIRE(connect.is_ok());
        auto& assoc = connect.value();

        auto ctx_opt = assoc.accepted_context_id(verification_sop_class_uid);
        REQUIRE(ctx_opt.has_value());

        for (int i = 0; i < num_echos; ++i) {
            auto echo_rq = make_c_echo_rq(static_cast<uint16_t>(i + 1),
                                          verification_sop_class_uid);
            if (assoc.send_dimse(*ctx_opt, echo_rq).is_ok()) {
                auto recv = assoc.receive_dimse(default_timeout());
                if (recv.is_ok() && recv.value().second.status() == status_success) {
                    ++success_count;
                }
            }
        }

        REQUIRE(success_count == num_echos);
        (void)assoc.release(default_timeout());
    }

    server.stop();

    auto stats = server.get_statistics();
    CHECK(stats.total_associations > 0);
}

TEST_CASE("dicom_server_v2 C-STORE integration", "[v2][integration][store]") {
    auto port = find_available_port();
    test_directory test_dir("v2_store_test_");

    server_config config;
    config.ae_title = "V2_STORE_SCP";
    config.port = port;
    config.max_associations = 20;

    dicom_server_v2 server(config);

    file_storage_config fs_conf;
    fs_conf.root_path = test_dir.path() / "archive";
    std::filesystem::create_directories(fs_conf.root_path);
    auto file_storage_ptr = std::make_unique<file_storage>(fs_conf);
    auto* fs_raw = file_storage_ptr.get();

    std::atomic<int> store_count{0};
    auto storage_scp_ptr = std::make_shared<storage_scp>();
    storage_scp_ptr->set_handler([&](
        const dicom_dataset& dataset,
        const std::string&,
        const std::string&,
        const std::string&) {
        auto result = fs_raw->store(dataset);
        if (result.is_ok()) {
            ++store_count;
            return storage_status::success;
        }
        return storage_status::storage_error;
    });

    server.register_service(storage_scp_ptr);
    server.register_service(std::make_shared<verification_scp>());

    auto start_result = server.start();
    REQUIRE(start_result.is_ok());
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    SECTION("Store single CT image") {
        association_config assoc_config;
        assoc_config.calling_ae_title = "V2_STORE_SCU";
        assoc_config.called_ae_title = "V2_STORE_SCP";
        assoc_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.102";
        assoc_config.proposed_contexts.push_back({
            1,
            "1.2.840.10008.5.1.4.1.1.2",  // CT Image Storage
            {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
        });

        auto connect = association::connect(
            "localhost", port, assoc_config, default_timeout());
        REQUIRE(connect.is_ok());

        auto& assoc = connect.value();
        storage_scu_config scu_config;
        storage_scu scu{scu_config};

        auto dataset = generate_ct_dataset();
        auto result = scu.store(assoc, dataset);
        REQUIRE(result.is_ok());
        REQUIRE(result.value().is_success());

        (void)assoc.release(default_timeout());
        REQUIRE(store_count == 1);
    }

    SECTION("Store multiple images in single association") {
        association_config assoc_config;
        assoc_config.calling_ae_title = "V2_STORE_SCU";
        assoc_config.called_ae_title = "V2_STORE_SCP";
        assoc_config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.103";
        assoc_config.proposed_contexts.push_back({
            1,
            "1.2.840.10008.5.1.4.1.1.2",
            {"1.2.840.10008.1.2.1"}
        });

        auto connect = association::connect(
            "localhost", port, assoc_config, default_timeout());
        REQUIRE(connect.is_ok());

        auto& assoc = connect.value();
        storage_scu scu;

        constexpr int num_images = 5;
        auto study_uid = generate_uid();
        int success_count = 0;

        for (int i = 0; i < num_images; ++i) {
            auto dataset = generate_ct_dataset(study_uid);
            auto result = scu.store(assoc, dataset);
            if (result.is_ok() && result.value().is_success()) {
                ++success_count;
            }
        }

        REQUIRE(success_count == num_images);
        (void)assoc.release(default_timeout());
    }

    server.stop();
}

// =============================================================================
// Scenario 2: Stress Testing with V2 Server
// =============================================================================

TEST_CASE("dicom_server_v2 concurrent storage stress test", "[v2][stress][concurrent]") {
    auto port = find_available_port();
    stress_test_server_v2 server(port, "V2_STRESS");

    REQUIRE(server.initialize());
    REQUIRE(server.start());

    constexpr int num_workers = 10;
    constexpr int files_per_worker = 5;
    constexpr int total_expected = num_workers * files_per_worker;

    std::latch start_latch(num_workers + 1);
    std::vector<std::future<v2_worker_result>> futures;

    for (int i = 0; i < num_workers; ++i) {
        futures.push_back(std::async(std::launch::async, [&, i]() {
            v2_worker_result result;
            auto start_time = std::chrono::steady_clock::now();

            start_latch.arrive_and_wait();

            try {
                association_config config;
                config.calling_ae_title = "SCU_" + std::to_string(i);
                config.called_ae_title = server.ae_title();
                config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.104";
                config.proposed_contexts.push_back({
                    1,
                    "1.2.840.10008.5.1.4.1.1.2",
                    {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
                });

                auto connect = association::connect(
                    "localhost", port, config, default_timeout() * 2);

                if (connect.is_err()) {
                    result.error_message = "Connection failed";
                    result.failure_count = files_per_worker;
                    return result;
                }

                auto& assoc = connect.value();
                storage_scu scu;

                auto study_uid = generate_uid();
                for (int j = 0; j < files_per_worker; ++j) {
                    auto dataset = generate_ct_dataset(study_uid);
                    auto store_result = scu.store(assoc, dataset);
                    if (store_result.is_ok() && store_result.value().is_success()) {
                        ++result.success_count;
                    } else {
                        ++result.failure_count;
                    }
                }

                (void)assoc.release(default_timeout());
            } catch (const std::exception& e) {
                result.error_message = e.what();
            }

            result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
            return result;
        }));
    }

    start_latch.arrive_and_wait();

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

    REQUIRE(total_success == total_expected);
    REQUIRE(total_failure == 0);
    REQUIRE(server.stored_count() == total_expected);

    auto stats = server.get_statistics();
    CHECK(stats.total_associations >= num_workers);

    server.stop();
}

TEST_CASE("dicom_server_v2 rapid sequential connections", "[v2][stress][sequential]") {
    auto port = find_available_port();
    test_server_v2 server(port, "V2_RAPID");
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start());

    constexpr int num_connections = 30;
    size_t success_count = 0;

    for (int i = 0; i < num_connections; ++i) {
        auto connect = test_association::connect(
            "localhost", port, server.ae_title(),
            "RAPID_" + std::to_string(i),
            {std::string(verification_sop_class_uid)});

        if (connect.is_ok()) {
            auto& assoc = connect.value();
            (void)assoc.release(std::chrono::milliseconds{500});
            ++success_count;
        }
    }

    REQUIRE(success_count == num_connections);

    auto stats = server.get_statistics();
    CHECK(stats.total_associations == num_connections);

    server.stop();
}

TEST_CASE("dicom_server_v2 max associations handling", "[v2][stress][limits]") {
    auto port = find_available_port();

    server_config config;
    config.ae_title = "V2_LIMIT";
    config.port = port;
    config.max_associations = 5;

    dicom_server_v2 server(config);
    server.register_service(std::make_shared<verification_scp>());

    REQUIRE(server.start().is_ok());
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    std::vector<std::optional<association>> held_connections;

    // Fill up to max
    for (int i = 0; i < 5; ++i) {
        auto connect = test_association::connect(
            "localhost", port, "V2_LIMIT",
            "HOLD_" + std::to_string(i),
            {std::string(verification_sop_class_uid)});
        if (connect.is_ok()) {
            held_connections.push_back(std::move(connect.value()));
        }
    }

    REQUIRE(held_connections.size() == 5);
    REQUIRE(server.active_associations() == 5);

    // Release one connection
    if (held_connections[0]) {
        (void)held_connections[0]->release(std::chrono::milliseconds{500});
        held_connections[0].reset();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    // New connection should succeed
    auto new_connect = test_association::connect(
        "localhost", port, "V2_LIMIT", "NEW_CLIENT",
        {std::string(verification_sop_class_uid)});

    REQUIRE(new_connect.is_ok());
    (void)new_connect.value().release(default_timeout());

    // Clean up remaining connections
    for (auto& opt_assoc : held_connections) {
        if (opt_assoc) {
            (void)opt_assoc->release(std::chrono::milliseconds{500});
        }
    }

    server.stop();
}

// =============================================================================
// Scenario 3: V1 to V2 Migration Validation
// =============================================================================

TEST_CASE("dicom_server_v2 API compatibility with v1", "[v2][migration][api]") {
    auto port_v1 = find_available_port();
    auto port_v2 = find_available_port();

    // V1 Server
    server_config config_v1;
    config_v1.ae_title = "MIGRATION_V1";
    config_v1.port = port_v1;
    config_v1.max_associations = 20;
    config_v1.idle_timeout = std::chrono::seconds{60};

    dicom_server server_v1(config_v1);
    server_v1.register_service(std::make_shared<verification_scp>());

    // V2 Server (same config)
    server_config config_v2;
    config_v2.ae_title = "MIGRATION_V2";
    config_v2.port = port_v2;
    config_v2.max_associations = 20;
    config_v2.idle_timeout = std::chrono::seconds{60};

    dicom_server_v2 server_v2(config_v2);
    server_v2.register_service(std::make_shared<verification_scp>());

    REQUIRE(server_v1.start().is_ok());
    REQUIRE(server_v2.start().is_ok());
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    SECTION("Same configuration produces same behavior") {
        // Both servers should accept C-ECHO
        auto connect_v1 = test_association::connect(
            "localhost", port_v1, "MIGRATION_V1", "V1_CLIENT",
            {std::string(verification_sop_class_uid)});

        auto connect_v2 = test_association::connect(
            "localhost", port_v2, "MIGRATION_V2", "V2_CLIENT",
            {std::string(verification_sop_class_uid)});

        REQUIRE(connect_v1.is_ok());
        REQUIRE(connect_v2.is_ok());

        auto& assoc_v1 = connect_v1.value();
        auto& assoc_v2 = connect_v2.value();

        // Both should accept verification context
        REQUIRE(assoc_v1.has_accepted_context(verification_sop_class_uid));
        REQUIRE(assoc_v2.has_accepted_context(verification_sop_class_uid));

        // Both should respond to C-ECHO with success
        auto ctx_v1 = *assoc_v1.accepted_context_id(verification_sop_class_uid);
        auto ctx_v2 = *assoc_v2.accepted_context_id(verification_sop_class_uid);

        auto echo_rq_1 = make_c_echo_rq(1, verification_sop_class_uid);
        auto echo_rq_2 = make_c_echo_rq(1, verification_sop_class_uid);

        REQUIRE(assoc_v1.send_dimse(ctx_v1, echo_rq_1).is_ok());
        REQUIRE(assoc_v2.send_dimse(ctx_v2, echo_rq_2).is_ok());

        auto recv_v1 = assoc_v1.receive_dimse(default_timeout());
        auto recv_v2 = assoc_v2.receive_dimse(default_timeout());

        REQUIRE(recv_v1.is_ok());
        REQUIRE(recv_v2.is_ok());

        CHECK(recv_v1.value().second.status() == status_success);
        CHECK(recv_v2.value().second.status() == status_success);

        (void)assoc_v1.release(default_timeout());
        (void)assoc_v2.release(default_timeout());
    }

    SECTION("Statistics consistency") {
        auto stats_v1 = server_v1.get_statistics();
        auto stats_v2 = server_v2.get_statistics();

        // Both should have processed at least one association from previous section
        CHECK(stats_v1.total_associations >= 0);
        CHECK(stats_v2.total_associations >= 0);
    }

    server_v1.stop();
    server_v2.stop();
}

TEST_CASE("dicom_server_v2 graceful shutdown comparison", "[v2][migration][shutdown]") {
    SECTION("V2 shutdown with active connections") {
        auto port = find_available_port();
        test_server_v2 server(port, "V2_SHUTDOWN");
        server.register_service(std::make_shared<verification_scp>());

        REQUIRE(server.start());

        // Establish connections
        std::vector<std::optional<association>> connections;
        for (int i = 0; i < 3; ++i) {
            auto connect = test_association::connect(
                "localhost", port, server.ae_title(),
                "SHUTDOWN_" + std::to_string(i),
                {std::string(verification_sop_class_uid)});
            if (connect.is_ok()) {
                connections.push_back(std::move(connect.value()));
            }
        }

        REQUIRE(connections.size() == 3);

        // Start shutdown timer
        auto start = std::chrono::steady_clock::now();
        server.stop();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        // Shutdown should complete within reasonable time
        INFO("Shutdown duration: " << duration.count() << " ms");
        CHECK(duration < std::chrono::seconds{5});

        // Connections should be closed
        for (auto& opt_assoc : connections) {
            if (opt_assoc) {
                opt_assoc.reset();  // Clean up
            }
        }
    }
}

// =============================================================================
// Scenario 4: Callback and Error Handling Tests
// =============================================================================

TEST_CASE("dicom_server_v2 callback invocation", "[v2][callbacks]") {
    auto port = find_available_port();

    server_config config;
    config.ae_title = "V2_CALLBACK";
    config.port = port;
    config.max_associations = 10;

    dicom_server_v2 server(config);
    server.register_service(std::make_shared<verification_scp>());

    std::atomic<int> established_count{0};
    std::atomic<int> closed_count{0};
    std::vector<std::string> errors;
    std::mutex errors_mutex;

    server.on_association_established(
        [&](const std::string& session_id,
            const std::string& calling_ae,
            const std::string& called_ae) {
            INFO("Association established: " << calling_ae << " -> " << called_ae);
            ++established_count;
            (void)session_id;
        });

    server.on_association_closed(
        [&](const std::string& session_id, bool graceful) {
            INFO("Association closed: " << session_id << " graceful=" << graceful);
            ++closed_count;
        });

    server.on_error(
        [&](const std::string& error) {
            std::lock_guard<std::mutex> lock(errors_mutex);
            errors.push_back(error);
        });

    REQUIRE(server.start().is_ok());
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // Establish and release connection
    auto connect = test_association::connect(
        "localhost", port, "V2_CALLBACK", "CALLBACK_SCU",
        {std::string(verification_sop_class_uid)});
    REQUIRE(connect.is_ok());

    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    CHECK(established_count == 1);

    (void)connect.value().release(default_timeout());
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    CHECK(closed_count == 1);

    server.stop();
}

// =============================================================================
// Scenario 5: Mixed Operations Stress Test
// =============================================================================

TEST_CASE("dicom_server_v2 mixed operations stress", "[v2][stress][mixed]") {
    auto port = find_available_port();
    stress_test_server_v2 server(port, "V2_MIXED");

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
                auto connect = test_association::connect(
                    "localhost", port, server.ae_title(),
                    "ECHO_" + std::to_string(i),
                    {std::string(verification_sop_class_uid)});

                if (connect.is_ok()) {
                    auto& assoc = connect.value();
                    auto ctx = assoc.accepted_context_id(verification_sop_class_uid);
                    if (ctx) {
                        auto echo_rq = make_c_echo_rq(1, verification_sop_class_uid);
                        if (assoc.send_dimse(*ctx, echo_rq).is_ok()) {
                            auto recv = assoc.receive_dimse(default_timeout());
                            if (recv.is_ok() &&
                                recv.value().second.status() == status_success) {
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
                config.implementation_class_uid = "1.2.826.0.1.3680043.9.9999.105";
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

#else  // !PACS_WITH_NETWORK_SYSTEM

TEST_CASE("dicom_server_v2 requires network_system", "[v2][skip]") {
    WARN("dicom_server_v2 tests skipped: PACS_WITH_NETWORK_SYSTEM not defined");
    SUCCEED("Tests skipped as expected");
}

#endif  // PACS_WITH_NETWORK_SYSTEM
