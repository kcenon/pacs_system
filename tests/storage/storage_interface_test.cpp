/**
 * @file storage_interface_test.cpp
 * @brief Unit tests for storage_interface abstract class
 *
 * Tests the storage_interface contract using a mock implementation.
 */

#include <pacs/storage/storage_interface.hpp>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>

using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;

namespace {

/**
 * @brief Mock storage implementation for testing
 *
 * Implements storage_interface using an in-memory map.
 */
class mock_storage : public storage_interface {
public:
    mock_storage() = default;

    auto store(const dicom_dataset& dataset) -> VoidResult override {
        // Extract SOP Instance UID for key
        auto uid = dataset.get_string(dicom_tag{0x0008, 0x0018});
        if (uid.empty()) {
            return kcenon::common::make_error<std::monostate>(
                -1, "SOP Instance UID is required", "storage");
        }

        storage_[uid] = dataset;
        return kcenon::common::ok();
    }

    auto retrieve(std::string_view sop_instance_uid)
        -> Result<dicom_dataset> override {
        auto it = storage_.find(std::string{sop_instance_uid});
        if (it == storage_.end()) {
            return kcenon::common::make_error<dicom_dataset>(
                -1, "Instance not found", "storage");
        }
        return it->second;
    }

    auto remove(std::string_view sop_instance_uid) -> VoidResult override {
        storage_.erase(std::string{sop_instance_uid});
        return kcenon::common::ok();
    }

    auto exists(std::string_view sop_instance_uid) const -> bool override {
        return storage_.contains(std::string{sop_instance_uid});
    }

    auto find(const dicom_dataset& query)
        -> Result<std::vector<dicom_dataset>> override {
        std::vector<dicom_dataset> results;

        // Simple implementation: return all if query is empty
        auto query_patient_id = query.get_string(dicom_tag{0x0010, 0x0020});

        for (const auto& [uid, dataset] : storage_) {
            if (query_patient_id.empty()) {
                results.push_back(dataset);
            } else {
                auto ds_patient_id =
                    dataset.get_string(dicom_tag{0x0010, 0x0020});
                if (ds_patient_id == query_patient_id) {
                    results.push_back(dataset);
                }
            }
        }

        return results;
    }

    auto get_statistics() const -> storage_statistics override {
        storage_statistics stats;
        stats.total_instances = storage_.size();
        return stats;
    }

    auto verify_integrity() -> VoidResult override {
        return kcenon::common::ok();
    }

private:
    std::map<std::string, dicom_dataset> storage_;
};

/**
 * @brief Helper to create a test dataset with SOP Instance UID
 */
auto create_test_dataset(const std::string& sop_uid,
                         const std::string& patient_id = "P001")
    -> dicom_dataset {
    dicom_dataset ds;
    ds.set_string(dicom_tag{0x0008, 0x0018}, vr_type::UI, sop_uid);
    ds.set_string(dicom_tag{0x0010, 0x0020}, vr_type::LO, patient_id);
    return ds;
}

}  // namespace

// ============================================================================
// Interface Contract Tests
// ============================================================================

TEST_CASE("storage_interface: store and retrieve", "[storage][interface]") {
    mock_storage storage;

    auto dataset = create_test_dataset("1.2.3.4.5.6.7", "PAT001");

    SECTION("store returns success") {
        auto result = storage.store(dataset);
        REQUIRE(result.is_ok());
    }

    SECTION("retrieve after store returns dataset") {
        REQUIRE(storage.store(dataset).is_ok());

        auto result = storage.retrieve("1.2.3.4.5.6.7");
        REQUIRE(result.is_ok());
        CHECK(result.value().get_string(dicom_tag{0x0010, 0x0020}) == "PAT001");
    }

    SECTION("retrieve non-existent returns error") {
        auto result = storage.retrieve("nonexistent");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("storage_interface: exists check", "[storage][interface]") {
    mock_storage storage;

    auto dataset = create_test_dataset("1.2.3.4.5.6.7");

    CHECK_FALSE(storage.exists("1.2.3.4.5.6.7"));

    REQUIRE(storage.store(dataset).is_ok());

    CHECK(storage.exists("1.2.3.4.5.6.7"));
    CHECK_FALSE(storage.exists("nonexistent"));
}

TEST_CASE("storage_interface: remove", "[storage][interface]") {
    mock_storage storage;

    auto dataset = create_test_dataset("1.2.3.4.5.6.7");
    REQUIRE(storage.store(dataset).is_ok());
    CHECK(storage.exists("1.2.3.4.5.6.7"));

    auto result = storage.remove("1.2.3.4.5.6.7");
    REQUIRE(result.is_ok());
    CHECK_FALSE(storage.exists("1.2.3.4.5.6.7"));

    // Remove non-existent should not error
    result = storage.remove("nonexistent");
    CHECK(result.is_ok());
}

TEST_CASE("storage_interface: find", "[storage][interface]") {
    mock_storage storage;

    auto ds1 = create_test_dataset("1.2.3.1", "PAT001");
    auto ds2 = create_test_dataset("1.2.3.2", "PAT001");
    auto ds3 = create_test_dataset("1.2.3.3", "PAT002");

    REQUIRE(storage.store(ds1).is_ok());
    REQUIRE(storage.store(ds2).is_ok());
    REQUIRE(storage.store(ds3).is_ok());

    SECTION("find all") {
        dicom_dataset empty_query;
        auto result = storage.find(empty_query);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 3);
    }

    SECTION("find by patient") {
        dicom_dataset query;
        query.set_string(dicom_tag{0x0010, 0x0020}, vr_type::LO,
                         "PAT001");

        auto result = storage.find(query);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 2);
    }
}

// ============================================================================
// Batch Operation Tests
// ============================================================================

TEST_CASE("storage_interface: store_batch default implementation",
          "[storage][interface][batch]") {
    mock_storage storage;

    std::vector<dicom_dataset> datasets;
    datasets.push_back(create_test_dataset("1.2.3.1", "PAT001"));
    datasets.push_back(create_test_dataset("1.2.3.2", "PAT002"));
    datasets.push_back(create_test_dataset("1.2.3.3", "PAT003"));

    auto result = storage.store_batch(datasets);

    REQUIRE(result.is_ok());
    CHECK(storage.exists("1.2.3.1"));
    CHECK(storage.exists("1.2.3.2"));
    CHECK(storage.exists("1.2.3.3"));
}

TEST_CASE("storage_interface: retrieve_batch default implementation",
          "[storage][interface][batch]") {
    mock_storage storage;

    REQUIRE(storage.store(create_test_dataset("1.2.3.1", "PAT001")).is_ok());
    REQUIRE(storage.store(create_test_dataset("1.2.3.2", "PAT002")).is_ok());
    REQUIRE(storage.store(create_test_dataset("1.2.3.3", "PAT003")).is_ok());

    SECTION("retrieve existing instances") {
        std::vector<std::string> uids{"1.2.3.1", "1.2.3.2", "1.2.3.3"};
        auto result = storage.retrieve_batch(uids);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 3);
    }

    SECTION("retrieve with some missing instances") {
        std::vector<std::string> uids{"1.2.3.1", "nonexistent", "1.2.3.3"};
        auto result = storage.retrieve_batch(uids);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 2);  // Missing ones silently skipped
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("storage_interface: get_statistics", "[storage][interface]") {
    mock_storage storage;

    auto stats = storage.get_statistics();
    CHECK(stats.total_instances == 0);

    REQUIRE(storage.store(create_test_dataset("1.2.3.1")).is_ok());
    REQUIRE(storage.store(create_test_dataset("1.2.3.2")).is_ok());

    stats = storage.get_statistics();
    CHECK(stats.total_instances == 2);
}

// ============================================================================
// Integrity Verification Tests
// ============================================================================

TEST_CASE("storage_interface: verify_integrity", "[storage][interface]") {
    mock_storage storage;

    auto result = storage.verify_integrity();
    CHECK(result.is_ok());
}

// ============================================================================
// Storage Statistics Structure Tests
// ============================================================================

TEST_CASE("storage_statistics: default initialization", "[storage][interface]") {
    storage_statistics stats;

    CHECK(stats.total_instances == 0);
    CHECK(stats.total_bytes == 0);
    CHECK(stats.studies_count == 0);
    CHECK(stats.series_count == 0);
    CHECK(stats.patients_count == 0);
}
