/**
 * @file test_support.hpp
 * @brief Test support utilities for PACS dependency injection
 *
 * This file provides mock implementations and test utilities for
 * unit testing code that depends on PACS services. Use these mocks
 * to isolate code under test from actual storage, network, and
 * codec implementations.
 *
 * @see Issue #312 - ServiceContainer based DI Integration
 */

#pragma once

#include "service_interfaces.hpp"
#include "service_registration.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

#include <kcenon/common/di/service_container.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pacs::di::test {

// =============================================================================
// Mock Storage Implementation
// =============================================================================

/**
 * @brief In-memory mock storage for testing
 *
 * Provides a simple in-memory implementation of IDicomStorage for
 * unit testing. Stores datasets in memory without any file system access.
 *
 * Thread Safety: All methods are thread-safe.
 *
 * @example
 * @code
 * auto mock_storage = std::make_shared<MockStorage>();
 *
 * // Store a dataset
 * core::dicom_dataset ds;
 * ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4");
 * mock_storage->store(ds);
 *
 * // Verify storage
 * EXPECT_TRUE(mock_storage->exists("1.2.3.4"));
 * EXPECT_EQ(mock_storage->store_count(), 1);
 * @endcode
 */
class MockStorage : public IDicomStorage {
public:
    MockStorage() = default;
    ~MockStorage() override = default;

    // =========================================================================
    // storage_interface Implementation
    // =========================================================================

    [[nodiscard]] storage::VoidResult store(const core::dicom_dataset& dataset) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto uid = dataset.get_string(core::tags::sop_instance_uid);
        if (uid.empty()) {
            return kcenon::common::make_error<std::monostate>(
                -1, "Dataset missing SOP Instance UID", "MockStorage");
        }

        datasets_[uid] = dataset;
        store_calls_.push_back(uid);

        if (on_store_) {
            on_store_(dataset);
        }

        return storage::VoidResult::ok({});
    }

    [[nodiscard]] storage::Result<core::dicom_dataset>
        retrieve(std::string_view sop_instance_uid) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = datasets_.find(std::string(sop_instance_uid));
        if (it == datasets_.end()) {
            return kcenon::common::make_error<core::dicom_dataset>(
                -1, "Instance not found", "MockStorage");
        }

        return storage::Result<core::dicom_dataset>::ok(it->second);
    }

    [[nodiscard]] storage::VoidResult remove(std::string_view sop_instance_uid) override {
        std::lock_guard<std::mutex> lock(mutex_);
        datasets_.erase(std::string(sop_instance_uid));
        return storage::VoidResult::ok({});
    }

    [[nodiscard]] bool exists(std::string_view sop_instance_uid) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return datasets_.find(std::string(sop_instance_uid)) != datasets_.end();
    }

    [[nodiscard]] storage::Result<std::vector<core::dicom_dataset>>
        find(const core::dicom_dataset& /*query*/) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<core::dicom_dataset> results;
        for (const auto& [uid, dataset] : datasets_) {
            results.push_back(dataset);
        }
        return storage::Result<std::vector<core::dicom_dataset>>::ok(std::move(results));
    }

    [[nodiscard]] storage::storage_statistics get_statistics() const override {
        std::lock_guard<std::mutex> lock(mutex_);

        storage::storage_statistics stats;
        stats.total_instances = datasets_.size();
        return stats;
    }

    [[nodiscard]] storage::VoidResult verify_integrity() override {
        return storage::VoidResult::ok({});
    }

    // =========================================================================
    // Test Utilities
    // =========================================================================

    /// Get the number of store() calls
    [[nodiscard]] std::size_t store_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return store_calls_.size();
    }

    /// Get all stored UIDs in order
    [[nodiscard]] std::vector<std::string> stored_uids() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return store_calls_;
    }

    /// Clear all stored data and call history
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        datasets_.clear();
        store_calls_.clear();
    }

    /// Set callback for store operations (for assertions)
    void on_store(std::function<void(const core::dicom_dataset&)> callback) {
        on_store_ = std::move(callback);
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, core::dicom_dataset> datasets_;
    std::vector<std::string> store_calls_;
    std::function<void(const core::dicom_dataset&)> on_store_;
};

// =============================================================================
// Mock Network Implementation
// =============================================================================

/**
 * @brief Mock network service for testing
 *
 * Provides a mock implementation of IDicomNetwork that records calls
 * and can be configured to return specific results for testing.
 *
 * Thread Safety: All methods are thread-safe.
 */
class MockNetwork : public IDicomNetwork {
public:
    MockNetwork() = default;
    ~MockNetwork() override = default;

    [[nodiscard]] std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& /*tls_cfg*/) override {
        std::lock_guard<std::mutex> lock(mutex_);
        server_configs_.push_back(config);

        // Return nullptr for mock - actual server creation not needed in tests
        return nullptr;
    }

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_attempts_.push_back({config.host, config.port});

        if (fail_connections_) {
            return integration::error_info{"Connection refused (mock)"};
        }

        // Return nullptr session for mock
        return integration::network_adapter::session_ptr{nullptr};
    }

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const std::string& host, uint16_t port,
                std::chrono::milliseconds /*timeout*/) override {
        integration::connection_config config;
        config.host = host;
        config.port = port;
        return connect(config);
    }

    // =========================================================================
    // Test Utilities
    // =========================================================================

    /// Get connection attempt count
    [[nodiscard]] std::size_t connection_attempt_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_attempts_.size();
    }

    /// Get server creation count
    [[nodiscard]] std::size_t server_creation_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return server_configs_.size();
    }

    /// Configure connections to fail
    void set_fail_connections(bool fail) {
        std::lock_guard<std::mutex> lock(mutex_);
        fail_connections_ = fail;
    }

    /// Clear call history
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_attempts_.clear();
        server_configs_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::pair<std::string, uint16_t>> connection_attempts_;
    std::vector<network::server_config> server_configs_;
    bool fail_connections_ = false;
};

// =============================================================================
// Test Container Builder
// =============================================================================

/**
 * @brief Builder for creating test containers with mock services
 *
 * Provides a fluent interface for constructing service containers
 * configured with mock implementations for testing.
 *
 * @example
 * @code
 * auto mock_storage = std::make_shared<MockStorage>();
 * auto container = TestContainerBuilder()
 *     .with_storage(mock_storage)
 *     .with_mock_network()
 *     .build();
 *
 * // Use container in test
 * auto storage = container->resolve<pacs::di::IDicomStorage>().value();
 * @endcode
 */
class TestContainerBuilder {
public:
    TestContainerBuilder() = default;

    /**
     * @brief Use a specific mock storage instance
     */
    TestContainerBuilder& with_storage(std::shared_ptr<MockStorage> storage) {
        mock_storage_ = std::move(storage);
        return *this;
    }

    /**
     * @brief Create and use a new mock storage
     */
    TestContainerBuilder& with_mock_storage() {
        mock_storage_ = std::make_shared<MockStorage>();
        return *this;
    }

    /**
     * @brief Use a specific mock network instance
     */
    TestContainerBuilder& with_network(std::shared_ptr<MockNetwork> network) {
        mock_network_ = std::move(network);
        return *this;
    }

    /**
     * @brief Create and use a new mock network
     */
    TestContainerBuilder& with_mock_network() {
        mock_network_ = std::make_shared<MockNetwork>();
        return *this;
    }

    /**
     * @brief Build the configured container
     *
     * @return Shared pointer to configured service container
     */
    [[nodiscard]] std::shared_ptr<kcenon::common::di::service_container> build() {
        auto container = std::make_shared<kcenon::common::di::service_container>();

        if (mock_storage_) {
            container->register_instance<IDicomStorage>(mock_storage_);
        }

        if (mock_network_) {
            container->register_instance<IDicomNetwork>(mock_network_);
        }

        return container;
    }

    /**
     * @brief Get the mock storage instance (for test assertions)
     */
    [[nodiscard]] std::shared_ptr<MockStorage> storage() const {
        return mock_storage_;
    }

    /**
     * @brief Get the mock network instance (for test assertions)
     */
    [[nodiscard]] std::shared_ptr<MockNetwork> network() const {
        return mock_network_;
    }

private:
    std::shared_ptr<MockStorage> mock_storage_;
    std::shared_ptr<MockNetwork> mock_network_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Create a test container with all mock services
 *
 * @return Shared pointer to container with mock services
 */
[[nodiscard]] inline std::shared_ptr<kcenon::common::di::service_container>
create_test_container() {
    return TestContainerBuilder()
        .with_mock_storage()
        .with_mock_network()
        .build();
}

/**
 * @brief Register mock storage with a container
 *
 * @param container The container to register with
 * @param mock The mock storage instance
 */
inline void register_mock_storage(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<MockStorage> mock) {
    container.register_instance<IDicomStorage>(std::move(mock));
}

/**
 * @brief Register mock network with a container
 *
 * @param container The container to register with
 * @param mock The mock network instance
 */
inline void register_mock_network(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<MockNetwork> mock) {
    container.register_instance<IDicomNetwork>(std::move(mock));
}

}  // namespace pacs::di::test
