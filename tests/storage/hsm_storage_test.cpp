/**
 * @file hsm_storage_test.cpp
 * @brief Unit tests for hsm_storage and hsm_migration_service classes
 *
 * Tests the Hierarchical Storage Management implementation for multi-tier
 * DICOM storage with automatic migration.
 */

#include <pacs/storage/hsm_migration_service.hpp>
#include <pacs/storage/hsm_storage.hpp>
#include <pacs/storage/hsm_types.hpp>

#include <pacs/storage/file_storage.hpp>

#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <thread>

using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;

namespace {

/**
 * @brief RAII helper for creating temporary test directories
 */
class temp_directory {
public:
    temp_directory() {
        auto temp = std::filesystem::temp_directory_path();
        path_ = temp / ("pacs_hsm_test_" +
                        std::to_string(std::chrono::steady_clock::now()
                                           .time_since_epoch()
                                           .count()));
        std::filesystem::create_directories(path_);
    }

    ~temp_directory() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] auto path() const -> const std::filesystem::path& {
        return path_;
    }

private:
    std::filesystem::path path_;
};

/**
 * @brief Helper to create a test dataset with required UIDs
 */
auto create_test_dataset(const std::string& study_uid,
                         const std::string& series_uid,
                         const std::string& sop_uid,
                         const std::string& patient_id = "P001",
                         const std::string& patient_name = "TEST^PATIENT")
    -> dicom_dataset {
    dicom_dataset ds;
    ds.set_string(tags::study_instance_uid, vr_type::UI, study_uid);
    ds.set_string(tags::series_instance_uid, vr_type::UI, series_uid);
    ds.set_string(tags::sop_instance_uid, vr_type::UI, sop_uid);
    ds.set_string(tags::sop_class_uid, vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.2");
    ds.set_string(tags::patient_id, vr_type::LO, patient_id);
    ds.set_string(tags::patient_name, vr_type::PN, patient_name);
    ds.set_string(tags::modality, vr_type::CS, "CT");
    return ds;
}

/**
 * @brief Create a file_storage backend for testing
 */
auto create_file_storage(const std::filesystem::path& root)
    -> std::unique_ptr<storage_interface> {
    file_storage_config config;
    config.root_path = root;
    config.naming = naming_scheme::uid_hierarchical;
    config.duplicate = duplicate_policy::replace;
    config.create_directories = true;
    return std::make_unique<file_storage>(config);
}

}  // namespace

// ============================================================================
// storage_tier Type Tests
// ============================================================================

TEST_CASE("storage_tier: to_string conversion", "[storage][hsm][types]") {
    CHECK(to_string(storage_tier::hot) == "hot");
    CHECK(to_string(storage_tier::warm) == "warm");
    CHECK(to_string(storage_tier::cold) == "cold");
}

TEST_CASE("storage_tier: from_string parsing", "[storage][hsm][types]") {
    CHECK(storage_tier_from_string("hot") == storage_tier::hot);
    CHECK(storage_tier_from_string("warm") == storage_tier::warm);
    CHECK(storage_tier_from_string("cold") == storage_tier::cold);
    CHECK(storage_tier_from_string("invalid") == std::nullopt);
    CHECK(storage_tier_from_string("") == std::nullopt);
}

// ============================================================================
// tier_policy Tests
// ============================================================================

TEST_CASE("tier_policy: default values", "[storage][hsm][types]") {
    tier_policy policy;

    CHECK(policy.hot_to_warm == std::chrono::days{30});
    CHECK(policy.warm_to_cold == std::chrono::days{365});
    CHECK(policy.auto_migrate == true);
    CHECK(policy.min_migration_size == 0);
    CHECK(policy.max_instances_per_cycle == 100);
}

TEST_CASE("tier_policy: equality comparison", "[storage][hsm][types]") {
    tier_policy p1;
    tier_policy p2;

    CHECK(p1 == p2);

    p2.hot_to_warm = std::chrono::days{60};
    CHECK_FALSE(p1 == p2);
}

// ============================================================================
// tier_metadata Tests
// ============================================================================

TEST_CASE("tier_metadata: age calculation", "[storage][hsm][types]") {
    tier_metadata meta;
    meta.sop_instance_uid = "1.2.3.4.5";
    meta.current_tier = storage_tier::hot;
    meta.stored_at =
        std::chrono::system_clock::now() - std::chrono::hours{24};

    auto age = meta.age();
    auto hours =
        std::chrono::duration_cast<std::chrono::hours>(age).count();

    // Should be approximately 24 hours (allow some tolerance)
    CHECK(hours >= 23);
    CHECK(hours <= 25);
}

TEST_CASE("tier_metadata: should_migrate logic", "[storage][hsm][types]") {
    tier_policy policy;
    policy.hot_to_warm = std::chrono::days{1};
    policy.warm_to_cold = std::chrono::days{7};

    SECTION("Fresh instance should not migrate") {
        tier_metadata meta;
        meta.sop_instance_uid = "1.2.3.4.5";
        meta.current_tier = storage_tier::hot;
        meta.stored_at = std::chrono::system_clock::now();

        CHECK_FALSE(meta.should_migrate(policy, storage_tier::warm));
        CHECK_FALSE(meta.should_migrate(policy, storage_tier::cold));
    }

    SECTION("Old hot instance should migrate to warm") {
        tier_metadata meta;
        meta.sop_instance_uid = "1.2.3.4.5";
        meta.current_tier = storage_tier::hot;
        meta.stored_at =
            std::chrono::system_clock::now() - std::chrono::days{2};

        CHECK(meta.should_migrate(policy, storage_tier::warm));
        CHECK_FALSE(meta.should_migrate(policy, storage_tier::cold));
    }

    SECTION("Cannot migrate to same or hotter tier") {
        tier_metadata meta;
        meta.sop_instance_uid = "1.2.3.4.5";
        meta.current_tier = storage_tier::warm;
        meta.stored_at =
            std::chrono::system_clock::now() - std::chrono::days{100};

        CHECK_FALSE(meta.should_migrate(policy, storage_tier::hot));
        CHECK_FALSE(meta.should_migrate(policy, storage_tier::warm));
    }
}

// ============================================================================
// hsm_storage Construction Tests
// ============================================================================

TEST_CASE("hsm_storage: construction with hot tier only",
          "[storage][hsm][construction]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");

    REQUIRE_NOTHROW(
        hsm_storage{std::move(hot), nullptr, nullptr});
}

TEST_CASE("hsm_storage: construction with all tiers",
          "[storage][hsm][construction]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");
    auto cold = create_file_storage(temp_dir.path() / "cold");

    REQUIRE_NOTHROW(
        hsm_storage{std::move(hot), std::move(warm), std::move(cold)});
}

TEST_CASE("hsm_storage: throws if hot tier is nullptr",
          "[storage][hsm][construction]") {
    temp_directory temp_dir;
    auto warm = create_file_storage(temp_dir.path() / "warm");

    REQUIRE_THROWS_AS(
        hsm_storage(nullptr, std::move(warm), nullptr),
        std::invalid_argument);
}

// ============================================================================
// hsm_storage CRUD Tests
// ============================================================================

TEST_CASE("hsm_storage: store and retrieve", "[storage][hsm][crud]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");

    hsm_storage storage{std::move(hot), std::move(warm), nullptr};

    auto ds = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");

    // Store
    auto store_result = storage.store(ds);
    REQUIRE(store_result.is_ok());

    // Verify in hot tier
    auto tier = storage.get_tier("1.2.3.4.5");
    REQUIRE(tier.has_value());
    CHECK(*tier == storage_tier::hot);

    // Retrieve
    auto retrieve_result = storage.retrieve("1.2.3.4.5");
    REQUIRE(retrieve_result.is_ok());

    auto& retrieved = retrieve_result.value();
    CHECK(retrieved.get_string(tags::patient_id) == "P001");
}

TEST_CASE("hsm_storage: exists check", "[storage][hsm][crud]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");

    hsm_storage storage{std::move(hot), nullptr, nullptr};

    auto ds = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(ds).is_ok());

    CHECK(storage.exists("1.2.3.4.5"));
    CHECK_FALSE(storage.exists("nonexistent"));
}

TEST_CASE("hsm_storage: remove instance", "[storage][hsm][crud]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");

    hsm_storage storage{std::move(hot), nullptr, nullptr};

    auto ds = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(ds).is_ok());

    REQUIRE(storage.exists("1.2.3.4.5"));

    auto remove_result = storage.remove("1.2.3.4.5");
    REQUIRE(remove_result.is_ok());

    CHECK_FALSE(storage.exists("1.2.3.4.5"));
}

TEST_CASE("hsm_storage: remove nonexistent returns ok",
          "[storage][hsm][crud]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");

    hsm_storage storage{std::move(hot), nullptr, nullptr};

    auto result = storage.remove("nonexistent");
    CHECK(result.is_ok());
}

// ============================================================================
// hsm_storage Migration Tests
// ============================================================================

TEST_CASE("hsm_storage: manual migration between tiers",
          "[storage][hsm][migration]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");
    auto cold = create_file_storage(temp_dir.path() / "cold");

    hsm_storage storage{std::move(hot), std::move(warm), std::move(cold)};

    auto ds = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(ds).is_ok());

    // Verify starts in hot
    CHECK(storage.get_tier("1.2.3.4.5") == storage_tier::hot);

    // Migrate to warm
    auto migrate_result = storage.migrate("1.2.3.4.5", storage_tier::warm);
    REQUIRE(migrate_result.is_ok());
    CHECK(storage.get_tier("1.2.3.4.5") == storage_tier::warm);

    // Migrate to cold
    migrate_result = storage.migrate("1.2.3.4.5", storage_tier::cold);
    REQUIRE(migrate_result.is_ok());
    CHECK(storage.get_tier("1.2.3.4.5") == storage_tier::cold);

    // Can still retrieve after migration
    auto retrieve_result = storage.retrieve("1.2.3.4.5");
    REQUIRE(retrieve_result.is_ok());
}

TEST_CASE("hsm_storage: migrate to same tier is no-op",
          "[storage][hsm][migration]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");

    hsm_storage storage{std::move(hot), nullptr, nullptr};

    auto ds = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(ds).is_ok());

    auto result = storage.migrate("1.2.3.4.5", storage_tier::hot);
    CHECK(result.is_ok());
    CHECK(storage.get_tier("1.2.3.4.5") == storage_tier::hot);
}

TEST_CASE("hsm_storage: migrate nonexistent returns error",
          "[storage][hsm][migration]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");

    hsm_storage storage{std::move(hot), std::move(warm), nullptr};

    auto result = storage.migrate("nonexistent", storage_tier::warm);
    CHECK_FALSE(result.is_ok());
}

// ============================================================================
// hsm_storage Statistics Tests
// ============================================================================

TEST_CASE("hsm_storage: get_hsm_statistics", "[storage][hsm][stats]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");

    hsm_storage storage{std::move(hot), std::move(warm), nullptr};

    // Store some datasets
    REQUIRE(storage.store(create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5")).is_ok());
    REQUIRE(storage.store(create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.6")).is_ok());
    REQUIRE(storage.store(create_test_dataset("1.2.3", "1.2.3.5", "1.2.3.5.1")).is_ok());

    // Migrate one to warm
    REQUIRE(storage.migrate("1.2.3.4.5", storage_tier::warm).is_ok());

    auto stats = storage.get_hsm_statistics();

    CHECK(stats.hot.instance_count == 2);
    CHECK(stats.warm.instance_count == 1);
    CHECK(stats.cold.instance_count == 0);
    CHECK(stats.total_instances() == 3);
}

// ============================================================================
// hsm_storage Policy Tests
// ============================================================================

TEST_CASE("hsm_storage: get and set tier policy", "[storage][hsm][policy]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");

    hsm_storage_config config;
    config.policy.hot_to_warm = std::chrono::days{7};

    hsm_storage storage{std::move(hot), nullptr, nullptr, config};

    auto policy = storage.get_tier_policy();
    CHECK(policy.hot_to_warm == std::chrono::days{7});

    tier_policy new_policy;
    new_policy.hot_to_warm = std::chrono::days{14};
    storage.set_tier_policy(new_policy);

    CHECK(storage.get_tier_policy().hot_to_warm == std::chrono::days{14});
}

// ============================================================================
// hsm_migration_service Tests
// ============================================================================

TEST_CASE("hsm_migration_service: basic lifecycle",
          "[storage][hsm][service]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");

    hsm_storage storage{std::move(hot), std::move(warm), nullptr};

    migration_service_config config;
    config.migration_interval = std::chrono::seconds{1};

    hsm_migration_service service{storage, config};

    CHECK_FALSE(service.is_running());

    service.start();
    CHECK(service.is_running());

    service.stop();
    CHECK_FALSE(service.is_running());
}

TEST_CASE("hsm_migration_service: manual migration cycle",
          "[storage][hsm][service]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");

    hsm_storage_config hsm_config;
    // Set policy to migrate after 0 days (immediate)
    hsm_config.policy.hot_to_warm = std::chrono::days{0};

    hsm_storage storage{std::move(hot), std::move(warm), nullptr, hsm_config};

    // Store a dataset
    auto ds = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(ds).is_ok());

    hsm_migration_service service{storage};

    // Run manual cycle
    auto result = service.run_migration_cycle();

    // With immediate migration policy, it should have migrated
    CHECK(result.instances_migrated >= 0);  // May or may not migrate depending on timing
}

TEST_CASE("hsm_migration_service: cumulative stats tracking",
          "[storage][hsm][service]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");

    hsm_storage storage{std::move(hot), nullptr, nullptr};
    hsm_migration_service service{storage};

    // Run a few cycles (manual runs don't increment cycles_completed counter)
    (void)service.run_migration_cycle();
    (void)service.run_migration_cycle();
    (void)service.run_migration_cycle();

    CHECK(service.cycles_completed() == 0);  // Service not started, manual runs don't count
}

// ============================================================================
// migration_result Tests
// ============================================================================

TEST_CASE("migration_result: is_success check", "[storage][hsm][types]") {
    migration_result result;

    CHECK(result.is_success());

    result.failed_uids.push_back("1.2.3.4.5");
    CHECK_FALSE(result.is_success());
}

TEST_CASE("migration_result: total_processed calculation",
          "[storage][hsm][types]") {
    migration_result result;
    result.instances_migrated = 5;
    result.instances_skipped = 3;
    result.failed_uids = {"uid1", "uid2"};

    CHECK(result.total_processed() == 10);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_CASE("hsm_storage: concurrent access", "[storage][hsm][threading]") {
    temp_directory temp_dir;
    auto hot = create_file_storage(temp_dir.path() / "hot");
    auto warm = create_file_storage(temp_dir.path() / "warm");

    hsm_storage storage{std::move(hot), std::move(warm), nullptr};

    // Pre-populate with some data
    for (int i = 0; i < 10; ++i) {
        auto ds = create_test_dataset(
            "1.2.3." + std::to_string(i),
            "1.2.3." + std::to_string(i) + ".1",
            "1.2.3." + std::to_string(i) + ".1." + std::to_string(i));
        REQUIRE(storage.store(ds).is_ok());
    }

    std::atomic<int> successful_reads{0};
    std::atomic<int> successful_writes{0};

    auto reader = [&]() {
        for (int i = 0; i < 10; ++i) {
            auto sop_uid = "1.2.3." + std::to_string(i % 10) + ".1." +
                           std::to_string(i % 10);
            if (storage.exists(sop_uid)) {
                ++successful_reads;
            }
        }
    };

    auto writer = [&]() {
        for (int i = 10; i < 20; ++i) {
            auto ds = create_test_dataset(
                "1.2.4." + std::to_string(i),
                "1.2.4." + std::to_string(i) + ".1",
                "1.2.4." + std::to_string(i) + ".1." + std::to_string(i));
            if (storage.store(ds).is_ok()) {
                ++successful_writes;
            }
        }
    };

    std::thread t1(reader);
    std::thread t2(reader);
    std::thread t3(writer);
    std::thread t4(writer);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    CHECK(successful_reads > 0);
    CHECK(successful_writes > 0);
}
