/**
 * @file s3_storage_test.cpp
 * @brief Unit tests for s3_storage class
 *
 * Tests the s3_storage implementation for S3-compatible DICOM storage.
 * Uses the mock S3 client for testing without AWS SDK dependency.
 */

#include <pacs/storage/s3_storage.hpp>

#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <atomic>

using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;

namespace {

/**
 * @brief Helper to create a test dataset with required UIDs
 */
auto create_test_dataset(const std::string &study_uid,
                         const std::string &series_uid,
                         const std::string &sop_uid,
                         const std::string &patient_id = "P001",
                         const std::string &patient_name = "TEST^PATIENT")
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

/**
 * @brief Helper to create a default cloud storage config for testing
 */
auto create_test_config() -> cloud_storage_config {
  cloud_storage_config config;
  config.bucket_name = "test-dicom-bucket";
  config.region = "us-east-1";
  config.access_key_id = "test-access-key";
  config.secret_access_key = "test-secret-key";
  config.endpoint_url = "http://localhost:9000"; // MinIO endpoint
  return config;
}

} // namespace

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("s3_storage: construction with config", "[storage][s3_storage]") {
  auto config = create_test_config();

  REQUIRE_NOTHROW(s3_storage{config});
}

TEST_CASE("s3_storage: bucket_name accessor", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

  CHECK(storage.bucket_name() == "test-dicom-bucket");
}

TEST_CASE("s3_storage: is_connected returns true", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

  CHECK(storage.is_connected());
}

// ============================================================================
// Store and Retrieve Tests
// ============================================================================

TEST_CASE("s3_storage: store and retrieve", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

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

TEST_CASE("s3_storage: store requires UIDs", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

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

TEST_CASE("s3_storage: exists check", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");

  CHECK_FALSE(storage.exists("1.2.3.4.5"));

  REQUIRE(storage.store(dataset).is_ok());

  CHECK(storage.exists("1.2.3.4.5"));
  CHECK_FALSE(storage.exists("nonexistent"));
}

// ============================================================================
// Remove Tests
// ============================================================================

TEST_CASE("s3_storage: remove", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

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
// Find Tests
// ============================================================================

TEST_CASE("s3_storage: find", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

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

TEST_CASE("s3_storage: get_statistics", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

  auto stats = storage.get_statistics();
  CHECK(stats.total_instances == 0);

  auto ds1 =
      create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1", "PAT001");
  auto ds2 =
      create_test_dataset("1.2.3.1", "1.2.3.1.2", "1.2.3.1.2.1", "PAT001");
  auto ds3 =
      create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1", "PAT002");

  REQUIRE(storage.store(ds1).is_ok());
  REQUIRE(storage.store(ds2).is_ok());
  REQUIRE(storage.store(ds3).is_ok());

  stats = storage.get_statistics();
  CHECK(stats.total_instances == 3);
  CHECK(stats.studies_count == 2);
  CHECK(stats.series_count == 3);
  CHECK(stats.total_bytes > 0);
}

// ============================================================================
// Integrity Verification Tests
// ============================================================================

TEST_CASE("s3_storage: verify_integrity", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
  REQUIRE(storage.store(dataset).is_ok());

  auto result = storage.verify_integrity();
  CHECK(result.is_ok());
}

// ============================================================================
// Object Key Tests
// ============================================================================

TEST_CASE("s3_storage: get_object_key", "[storage][s3_storage]") {
  auto config = create_test_config();
  s3_storage storage{config};

  // Non-existent returns empty string
  auto key = storage.get_object_key("nonexistent");
  CHECK(key.empty());

  // After store, returns actual key
  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
  REQUIRE(storage.store(dataset).is_ok());

  key = storage.get_object_key("1.2.3.4.5");
  CHECK(!key.empty());
  CHECK(key.find("1.2.3") != std::string::npos);     // Contains study UID
  CHECK(key.find("1.2.3.4") != std::string::npos);   // Contains series UID
  CHECK(key.find("1.2.3.4.5") != std::string::npos); // Contains SOP UID
  CHECK(key.ends_with(".dcm"));
}

// ============================================================================
// Rebuild Index Tests
// ============================================================================

TEST_CASE("s3_storage: rebuild_index", "[storage][s3_storage]") {
  auto config = create_test_config();

  // First, store some data
  {
    s3_storage storage1{config};

    auto ds1 = create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1");
    auto ds2 = create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1");

    REQUIRE(storage1.store(ds1).is_ok());
    REQUIRE(storage1.store(ds2).is_ok());

    // Rebuild index should preserve data
    REQUIRE(storage1.rebuild_index().is_ok());

    CHECK(storage1.exists("1.2.3.1.1.1"));
    CHECK(storage1.exists("1.2.3.2.1.1"));
  }
}

// ============================================================================
// Batch Operation Tests (inherited from storage_interface)
// ============================================================================

TEST_CASE("s3_storage: store_batch", "[storage][s3_storage][batch]") {
  auto config = create_test_config();
  s3_storage storage{config};

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

TEST_CASE("s3_storage: retrieve_batch", "[storage][s3_storage][batch]") {
  auto config = create_test_config();
  s3_storage storage{config};

  REQUIRE(
      storage.store(create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1"))
          .is_ok());
  REQUIRE(
      storage.store(create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1"))
          .is_ok());

  SECTION("retrieve existing instances") {
    std::vector<std::string> uids{"1.2.3.1.1.1", "1.2.3.2.1.1"};
    auto result = storage.retrieve_batch(uids);

    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);
  }

  SECTION("retrieve with some missing") {
    std::vector<std::string> uids{"1.2.3.1.1.1", "nonexistent", "1.2.3.2.1.1"};
    auto result = storage.retrieve_batch(uids);

    REQUIRE(result.is_ok());
    CHECK(result.value().size() == 2);
  }
}

// ============================================================================
// Progress Callback Tests
// ============================================================================

TEST_CASE("s3_storage: store_with_progress",
          "[storage][s3_storage][progress]") {
  auto config = create_test_config();
  s3_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");

  std::atomic<std::size_t> last_bytes{0};
  std::atomic<int> callback_count{0};

  auto callback = [&](std::size_t bytes_transferred,
                      std::size_t total_bytes) -> bool {
    (void)total_bytes; // Suppress unused parameter warning
    last_bytes = bytes_transferred;
    callback_count++;
    return true; // Continue upload
  };

  auto result = storage.store_with_progress(dataset, callback);

  REQUIRE(result.is_ok());
  CHECK(callback_count > 0);
  CHECK(last_bytes > 0);
}

TEST_CASE("s3_storage: retrieve_with_progress",
          "[storage][s3_storage][progress]") {
  auto config = create_test_config();
  s3_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
  REQUIRE(storage.store(dataset).is_ok());

  std::atomic<std::size_t> last_bytes{0};
  std::atomic<int> callback_count{0};

  auto callback = [&](std::size_t bytes_transferred,
                      std::size_t total_bytes) -> bool {
    (void)total_bytes; // Suppress unused parameter warning
    last_bytes = bytes_transferred;
    callback_count++;
    return true; // Continue download
  };

  auto result = storage.retrieve_with_progress("1.2.3.4.5", callback);

  REQUIRE(result.is_ok());
  CHECK(callback_count > 0);
}

// ============================================================================
// Cloud Storage Config Tests
// ============================================================================

TEST_CASE("cloud_storage_config: default values", "[storage][s3_storage]") {
  cloud_storage_config config;

  CHECK(config.bucket_name.empty());
  CHECK(config.region == "us-east-1");
  CHECK(config.access_key_id.empty());
  CHECK(config.secret_access_key.empty());
  CHECK_FALSE(config.endpoint_url.has_value());
  CHECK(config.multipart_threshold == 100 * 1024 * 1024); // 100MB
  CHECK(config.part_size == 10 * 1024 * 1024);            // 10MB
  CHECK(config.max_connections == 25);
  CHECK(config.connect_timeout_ms == 3000);
  CHECK(config.request_timeout_ms == 30000);
  CHECK_FALSE(config.enable_encryption);
  CHECK(config.storage_class == "STANDARD");
}
