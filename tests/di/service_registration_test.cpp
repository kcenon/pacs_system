/**
 * @file service_registration_test.cpp
 * @brief Unit tests for PACS DI service registration
 *
 * @see Issue #312 - ServiceContainer based DI Integration
 */

#include <pacs/di/service_registration.hpp>
#include <pacs/di/test_support.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

using namespace pacs::di;
using namespace pacs::di::test;
using namespace pacs::core;
using namespace kcenon::common::di;

// =============================================================================
// Service Registration Tests
// =============================================================================

TEST_CASE("register_services registers default services", "[di][registration]") {
    service_container container;

    SECTION("registers all services with default config") {
        auto result = register_services(container);
        REQUIRE(result.is_ok());

        REQUIRE(container.is_registered<IDicomStorage>());
        REQUIRE(container.is_registered<IDicomNetwork>());
    }

    SECTION("can resolve registered services") {
        auto result = register_services(container);
        REQUIRE(result.is_ok());

        auto storage_result = container.resolve<IDicomStorage>();
        REQUIRE(storage_result.is_ok());
        REQUIRE(storage_result.value() != nullptr);

        auto network_result = container.resolve<IDicomNetwork>();
        REQUIRE(network_result.is_ok());
        REQUIRE(network_result.value() != nullptr);
    }

    SECTION("fails on duplicate registration") {
        auto result1 = register_services(container);
        REQUIRE(result1.is_ok());

        auto result2 = register_services(container);
        REQUIRE(result2.is_err());
    }
}

TEST_CASE("register_services respects configuration", "[di][registration]") {
    service_container container;

    SECTION("respects custom storage path") {
        registration_config config;
        config.storage_path = std::filesystem::temp_directory_path() / "pacs_test_custom";

        auto result = register_services(container, config);
        REQUIRE(result.is_ok());

        auto storage = container.resolve<IDicomStorage>().value();
        REQUIRE(storage != nullptr);
    }

    SECTION("respects enable_network = false") {
        registration_config config;
        config.enable_network = false;

        auto result = register_services(container, config);
        REQUIRE(result.is_ok());

        REQUIRE(container.is_registered<IDicomStorage>());
        REQUIRE_FALSE(container.is_registered<IDicomNetwork>());
    }

    SECTION("respects transient lifetime") {
        registration_config config;
        config.use_singletons = false;

        auto result = register_services(container, config);
        REQUIRE(result.is_ok());

        // With transient lifetime, each resolve returns a new instance
        auto storage1 = container.resolve<IDicomStorage>().value();
        auto storage2 = container.resolve<IDicomStorage>().value();
        REQUIRE(storage1.get() != storage2.get());
    }

    SECTION("singleton lifetime returns same instance") {
        registration_config config;
        config.use_singletons = true;

        auto result = register_services(container, config);
        REQUIRE(result.is_ok());

        auto storage1 = container.resolve<IDicomStorage>().value();
        auto storage2 = container.resolve<IDicomStorage>().value();
        REQUIRE(storage1.get() == storage2.get());
    }
}

TEST_CASE("create_container creates configured container", "[di][registration]") {
    SECTION("creates container with default config") {
        auto container = create_container();
        REQUIRE(container != nullptr);
        REQUIRE(container->is_registered<IDicomStorage>());
        REQUIRE(container->is_registered<IDicomNetwork>());
    }

    SECTION("creates container with custom config") {
        registration_config config;
        config.enable_network = false;

        auto container = create_container(config);
        REQUIRE(container != nullptr);
        REQUIRE(container->is_registered<IDicomStorage>());
        REQUIRE_FALSE(container->is_registered<IDicomNetwork>());
    }
}

// =============================================================================
// Custom Registration Tests
// =============================================================================

TEST_CASE("register_storage_instance registers pre-created instance", "[di][registration]") {
    service_container container;

    auto mock_storage = std::make_shared<MockStorage>();
    auto result = register_storage_instance(container, mock_storage);
    REQUIRE(result.is_ok());

    auto resolved = container.resolve<IDicomStorage>().value();
    REQUIRE(resolved.get() == mock_storage.get());
}

TEST_CASE("register_network_instance registers pre-created instance", "[di][registration]") {
    service_container container;

    auto mock_network = std::make_shared<MockNetwork>();
    auto result = register_network_instance(container, mock_network);
    REQUIRE(result.is_ok());

    auto resolved = container.resolve<IDicomNetwork>().value();
    REQUIRE(resolved.get() == mock_network.get());
}

// =============================================================================
// Mock Storage Tests
// =============================================================================

TEST_CASE("MockStorage stores and retrieves datasets", "[di][test][mock]") {
    auto mock = std::make_shared<MockStorage>();

    SECTION("store increments count") {
        dicom_dataset ds;
        ds.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, "1.2.3.4.5");

        auto result = mock->store(ds);
        REQUIRE(result.is_ok());
        REQUIRE(mock->store_count() == 1);
    }

    SECTION("exists returns correct state") {
        dicom_dataset ds;
        ds.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, "1.2.3.4.5");

        REQUIRE_FALSE(mock->exists("1.2.3.4.5"));

        auto result = mock->store(ds);
        REQUIRE(result.is_ok());

        REQUIRE(mock->exists("1.2.3.4.5"));
    }

    SECTION("retrieve returns stored dataset") {
        dicom_dataset ds;
        ds.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, "1.2.3.4.5");
        ds.set_string(tags::patient_id, pacs::encoding::vr_type::LO, "PATIENT123");

        auto store_result = mock->store(ds);
        REQUIRE(store_result.is_ok());

        auto result = mock->retrieve("1.2.3.4.5");
        REQUIRE(result.is_ok());

        auto patient_id = result.value().get_string(tags::patient_id);
        REQUIRE_FALSE(patient_id.empty());
        REQUIRE(patient_id == "PATIENT123");
    }

    SECTION("remove deletes dataset") {
        dicom_dataset ds;
        ds.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, "1.2.3.4.5");

        auto result = mock->store(ds);
        REQUIRE(result.is_ok());
        REQUIRE(mock->exists("1.2.3.4.5"));

        auto remove_result = mock->remove("1.2.3.4.5");
        REQUIRE(remove_result.is_ok());
        REQUIRE_FALSE(mock->exists("1.2.3.4.5"));
    }

    SECTION("clear removes all data") {
        dicom_dataset ds1;
        ds1.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, "1.2.3.4.1");

        dicom_dataset ds2;
        ds2.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, "1.2.3.4.2");

        auto r1 = mock->store(ds1);
        auto r2 = mock->store(ds2);
        REQUIRE(r1.is_ok());
        REQUIRE(r2.is_ok());

        REQUIRE(mock->store_count() == 2);

        mock->clear();

        REQUIRE(mock->store_count() == 0);
        REQUIRE_FALSE(mock->exists("1.2.3.4.1"));
        REQUIRE_FALSE(mock->exists("1.2.3.4.2"));
    }

    SECTION("on_store callback is invoked") {
        bool callback_called = false;
        std::string captured_uid;

        mock->on_store([&](const dicom_dataset& ds) {
            callback_called = true;
            captured_uid = ds.get_string(tags::sop_instance_uid);
        });

        dicom_dataset ds;
        ds.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, "1.2.3.4.5");

        auto result = mock->store(ds);
        REQUIRE(result.is_ok());

        REQUIRE(callback_called);
        REQUIRE(captured_uid == "1.2.3.4.5");
    }
}

// =============================================================================
// Mock Network Tests
// =============================================================================

TEST_CASE("MockNetwork records connection attempts", "[di][test][mock]") {
    auto mock = std::make_shared<MockNetwork>();

    SECTION("connect increments counter") {
        (void)mock->connect("localhost", 11112, std::chrono::seconds{30});
        (void)mock->connect("192.168.1.100", 104, std::chrono::seconds{30});

        REQUIRE(mock->connection_attempt_count() == 2);
    }

    SECTION("create_server increments counter") {
        pacs::network::server_config config;
        config.ae_title = "TEST_SCP";
        config.port = 11112;

        (void)mock->create_server(config, {});
        (void)mock->create_server(config, {});

        REQUIRE(mock->server_creation_count() == 2);
    }

    SECTION("fail mode returns error") {
        mock->set_fail_connections(true);

        auto result = mock->connect("localhost", 11112, std::chrono::seconds{30});
        REQUIRE(result.is_err());
    }

    SECTION("clear resets counters") {
        (void)mock->connect("localhost", 11112, std::chrono::seconds{30});

        pacs::network::server_config config;
        (void)mock->create_server(config, {});

        mock->clear();

        REQUIRE(mock->connection_attempt_count() == 0);
        REQUIRE(mock->server_creation_count() == 0);
    }
}

// =============================================================================
// TestContainerBuilder Tests
// =============================================================================

TEST_CASE("TestContainerBuilder creates configured containers", "[di][test][builder]") {
    SECTION("with_mock_storage creates mock storage") {
        auto container = TestContainerBuilder()
            .with_mock_storage()
            .build();

        REQUIRE(container->is_registered<IDicomStorage>());

        auto storage = container->resolve<IDicomStorage>().value();
        REQUIRE(storage != nullptr);
    }

    SECTION("with_mock_network creates mock network") {
        auto container = TestContainerBuilder()
            .with_mock_network()
            .build();

        REQUIRE(container->is_registered<IDicomNetwork>());
    }

    SECTION("allows access to mocks for assertions") {
        TestContainerBuilder builder;
        builder.with_mock_storage().with_mock_network();

        auto container = builder.build();

        auto mock_storage = builder.storage();
        auto mock_network = builder.network();

        REQUIRE(mock_storage != nullptr);
        REQUIRE(mock_network != nullptr);

        // Verify same instances are in container
        auto resolved_storage = container->resolve<IDicomStorage>().value();
        REQUIRE(resolved_storage.get() == mock_storage.get());
    }

    SECTION("custom mock instances are used") {
        auto my_storage = std::make_shared<MockStorage>();
        auto my_network = std::make_shared<MockNetwork>();

        auto container = TestContainerBuilder()
            .with_storage(my_storage)
            .with_network(my_network)
            .build();

        auto resolved_storage = container->resolve<IDicomStorage>().value();
        auto resolved_network = container->resolve<IDicomNetwork>().value();

        REQUIRE(resolved_storage.get() == my_storage.get());
        REQUIRE(resolved_network.get() == my_network.get());
    }
}

TEST_CASE("create_test_container creates fully mocked container", "[di][test]") {
    auto container = create_test_container();

    REQUIRE(container != nullptr);
    REQUIRE(container->is_registered<IDicomStorage>());
    REQUIRE(container->is_registered<IDicomNetwork>());
}
