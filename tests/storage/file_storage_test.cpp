/**
 * @file file_storage_test.cpp
 * @brief Unit tests for file_storage class
 *
 * Tests the file_storage implementation for filesystem-based DICOM storage.
 */

#include <pacs/storage/file_storage.hpp>

#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

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
        path_ = temp / ("pacs_test_" + std::to_string(
                            std::chrono::steady_clock::now()
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
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.2");
    ds.set_string(tags::patient_id, vr_type::LO, patient_id);
    ds.set_string(tags::patient_name, vr_type::PN, patient_name);
    ds.set_string(tags::modality, vr_type::CS, "CT");
    return ds;
}

}  // namespace

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("file_storage: construction with config", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();
    config.naming = naming_scheme::uid_hierarchical;
    config.duplicate = duplicate_policy::reject;
    config.create_directories = true;

    REQUIRE_NOTHROW(file_storage{config});
}

TEST_CASE("file_storage: auto-creates root directory",
          "[storage][file_storage]") {
    temp_directory temp_dir;
    auto storage_path = temp_dir.path() / "new_storage";

    file_storage_config config;
    config.root_path = storage_path;
    config.create_directories = true;

    file_storage storage{config};

    CHECK(std::filesystem::exists(storage_path));
}

// ============================================================================
// Store and Retrieve Tests
// ============================================================================

TEST_CASE("file_storage: store and retrieve", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    auto dataset = create_test_dataset("1.2.3.100", "1.2.3.100.1",
                                        "1.2.3.100.1.1", "PAT001", "DOE^JOHN");

    SECTION("store returns success") {
        auto result = storage.store(dataset);
        REQUIRE(result.is_ok());
    }

    SECTION("retrieve after store returns dataset") {
        REQUIRE(storage.store(dataset).is_ok());

        auto result = storage.retrieve("1.2.3.100.1.1");
        REQUIRE(result.is_ok());
        CHECK(result.value().get_string(tags::patient_id) == "PAT001");
        CHECK(result.value().get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("retrieve non-existent returns error") {
        auto result = storage.retrieve("nonexistent.uid");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("file_storage: store requires UIDs", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    SECTION("missing study UID") {
        dicom_dataset ds;
        ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.2");

        auto result = storage.store(ds);
        CHECK(result.is_err());
    }

    SECTION("missing series UID") {
        dicom_dataset ds;
        ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3");
        ds.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5");
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.2");

        auto result = storage.store(ds);
        CHECK(result.is_err());
    }

    SECTION("missing SOP Instance UID") {
        dicom_dataset ds;
        ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3");
        ds.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4");
        ds.set_string(tags::sop_class_uid, vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.2");

        auto result = storage.store(ds);
        CHECK(result.is_err());
    }
}

// ============================================================================
// Exists Tests
// ============================================================================

TEST_CASE("file_storage: exists check", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");

    CHECK_FALSE(storage.exists("1.2.3.4.5"));

    REQUIRE(storage.store(dataset).is_ok());

    CHECK(storage.exists("1.2.3.4.5"));
    CHECK_FALSE(storage.exists("nonexistent"));
}

// ============================================================================
// Remove Tests
// ============================================================================

TEST_CASE("file_storage: remove", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(dataset).is_ok());
    CHECK(storage.exists("1.2.3.4.5"));

    auto result = storage.remove("1.2.3.4.5");
    REQUIRE(result.is_ok());
    CHECK_FALSE(storage.exists("1.2.3.4.5"));

    // Remove non-existent should not error
    result = storage.remove("nonexistent");
    CHECK(result.is_ok());
}

// ============================================================================
// Duplicate Policy Tests
// ============================================================================

TEST_CASE("file_storage: duplicate policy - reject",
          "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();
    config.duplicate = duplicate_policy::reject;

    file_storage storage{config};

    auto dataset1 = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5",
                                         "PAT001", "ORIGINAL");
    auto dataset2 = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5",
                                         "PAT002", "DUPLICATE");

    REQUIRE(storage.store(dataset1).is_ok());

    auto result = storage.store(dataset2);
    CHECK(result.is_err());

    // Verify original is still there
    auto retrieved = storage.retrieve("1.2.3.4.5");
    REQUIRE(retrieved.is_ok());
    CHECK(retrieved.value().get_string(tags::patient_name) == "ORIGINAL");
}

TEST_CASE("file_storage: duplicate policy - replace",
          "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();
    config.duplicate = duplicate_policy::replace;

    file_storage storage{config};

    auto dataset1 = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5",
                                         "PAT001", "ORIGINAL");
    auto dataset2 = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5",
                                         "PAT002", "REPLACED");

    REQUIRE(storage.store(dataset1).is_ok());
    REQUIRE(storage.store(dataset2).is_ok());

    auto retrieved = storage.retrieve("1.2.3.4.5");
    REQUIRE(retrieved.is_ok());
    CHECK(retrieved.value().get_string(tags::patient_name) == "REPLACED");
}

TEST_CASE("file_storage: duplicate policy - ignore",
          "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();
    config.duplicate = duplicate_policy::ignore;

    file_storage storage{config};

    auto dataset1 = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5",
                                         "PAT001", "ORIGINAL");
    auto dataset2 = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5",
                                         "PAT002", "IGNORED");

    REQUIRE(storage.store(dataset1).is_ok());
    REQUIRE(storage.store(dataset2).is_ok());  // Should succeed silently

    auto retrieved = storage.retrieve("1.2.3.4.5");
    REQUIRE(retrieved.is_ok());
    CHECK(retrieved.value().get_string(tags::patient_name) == "ORIGINAL");
}

// ============================================================================
// Naming Scheme Tests
// ============================================================================

TEST_CASE("file_storage: naming scheme - uid_hierarchical",
          "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();
    config.naming = naming_scheme::uid_hierarchical;

    file_storage storage{config};

    auto dataset = create_test_dataset("1.2.3.study", "1.2.3.series",
                                        "1.2.3.instance");
    REQUIRE(storage.store(dataset).is_ok());

    auto file_path = storage.get_file_path("1.2.3.instance");
    CHECK(file_path.string().find("1.2.3.study") != std::string::npos);
    CHECK(file_path.string().find("1.2.3.series") != std::string::npos);
}

TEST_CASE("file_storage: naming scheme - flat", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();
    config.naming = naming_scheme::flat;

    file_storage storage{config};

    auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(dataset).is_ok());

    auto file_path = storage.get_file_path("1.2.3.4.5");
    CHECK(file_path.parent_path() == temp_dir.path());
}

// ============================================================================
// Find Tests
// ============================================================================

TEST_CASE("file_storage: find", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    auto ds1 = create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1",
                                    "PAT001", "SMITH^JOHN");
    auto ds2 = create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1",
                                    "PAT001", "SMITH^JANE");
    auto ds3 = create_test_dataset("1.2.3.3", "1.2.3.3.1", "1.2.3.3.1.1",
                                    "PAT002", "DOE^JOHN");

    REQUIRE(storage.store(ds1).is_ok());
    REQUIRE(storage.store(ds2).is_ok());
    REQUIRE(storage.store(ds3).is_ok());

    SECTION("find all") {
        dicom_dataset empty_query;
        auto result = storage.find(empty_query);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 3);
    }

    SECTION("find by patient ID") {
        dicom_dataset query;
        query.set_string(tags::patient_id, vr_type::LO, "PAT001");

        auto result = storage.find(query);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 2);
    }

    SECTION("find with wildcard") {
        dicom_dataset query;
        query.set_string(tags::patient_name, vr_type::PN, "SMITH*");

        auto result = storage.find(query);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 2);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("file_storage: get_statistics", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    auto stats = storage.get_statistics();
    CHECK(stats.total_instances == 0);

    auto ds1 = create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1",
                                    "PAT001");
    auto ds2 = create_test_dataset("1.2.3.1", "1.2.3.1.2", "1.2.3.1.2.1",
                                    "PAT001");
    auto ds3 = create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1",
                                    "PAT002");

    REQUIRE(storage.store(ds1).is_ok());
    REQUIRE(storage.store(ds2).is_ok());
    REQUIRE(storage.store(ds3).is_ok());

    stats = storage.get_statistics();
    CHECK(stats.total_instances == 3);
    CHECK(stats.studies_count == 2);
    CHECK(stats.series_count == 3);
    CHECK(stats.patients_count == 2);
    CHECK(stats.total_bytes > 0);
}

// ============================================================================
// Integrity Verification Tests
// ============================================================================

TEST_CASE("file_storage: verify_integrity", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(dataset).is_ok());

    auto result = storage.verify_integrity();
    CHECK(result.is_ok());
}

// ============================================================================
// File Path Tests
// ============================================================================

TEST_CASE("file_storage: get_file_path", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    // Non-existent returns empty path
    auto path = storage.get_file_path("nonexistent");
    CHECK(path.empty());

    // After store, returns actual path
    auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
    REQUIRE(storage.store(dataset).is_ok());

    path = storage.get_file_path("1.2.3.4.5");
    CHECK(!path.empty());
    CHECK(std::filesystem::exists(path));
}

// ============================================================================
// Root Path Tests
// ============================================================================

TEST_CASE("file_storage: root_path accessor", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    CHECK(storage.root_path() == temp_dir.path());
}

// ============================================================================
// Rebuild Index Tests
// ============================================================================

TEST_CASE("file_storage: rebuild_index", "[storage][file_storage]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    // First, store some data
    {
        file_storage storage1{config};

        auto ds1 = create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1");
        auto ds2 = create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1");

        REQUIRE(storage1.store(ds1).is_ok());
        REQUIRE(storage1.store(ds2).is_ok());
    }

    // Create new storage instance (should rebuild index from files)
    file_storage storage2{config};

    CHECK(storage2.exists("1.2.3.1.1.1"));
    CHECK(storage2.exists("1.2.3.2.1.1"));

    auto stats = storage2.get_statistics();
    CHECK(stats.total_instances == 2);
}

// ============================================================================
// Batch Operation Tests (inherited from storage_interface)
// ============================================================================

TEST_CASE("file_storage: store_batch", "[storage][file_storage][batch]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    std::vector<dicom_dataset> datasets;
    datasets.push_back(
        create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1"));
    datasets.push_back(
        create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1"));
    datasets.push_back(
        create_test_dataset("1.2.3.3", "1.2.3.3.1", "1.2.3.3.1.1"));

    auto result = storage.store_batch(datasets);

    REQUIRE(result.is_ok());
    CHECK(storage.exists("1.2.3.1.1.1"));
    CHECK(storage.exists("1.2.3.2.1.1"));
    CHECK(storage.exists("1.2.3.3.1.1"));
}

TEST_CASE("file_storage: retrieve_batch", "[storage][file_storage][batch]") {
    temp_directory temp_dir;

    file_storage_config config;
    config.root_path = temp_dir.path();

    file_storage storage{config};

    REQUIRE(storage
                .store(create_test_dataset("1.2.3.1", "1.2.3.1.1",
                                            "1.2.3.1.1.1"))
                .is_ok());
    REQUIRE(storage
                .store(create_test_dataset("1.2.3.2", "1.2.3.2.1",
                                            "1.2.3.2.1.1"))
                .is_ok());

    SECTION("retrieve existing instances") {
        std::vector<std::string> uids{"1.2.3.1.1.1", "1.2.3.2.1.1"};
        auto result = storage.retrieve_batch(uids);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 2);
    }

    SECTION("retrieve with some missing") {
        std::vector<std::string> uids{"1.2.3.1.1.1", "nonexistent",
                                       "1.2.3.2.1.1"};
        auto result = storage.retrieve_batch(uids);

        REQUIRE(result.is_ok());
        CHECK(result.value().size() == 2);
    }
}
