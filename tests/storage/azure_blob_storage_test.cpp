/**
 * @file azure_blob_storage_test.cpp
 * @brief Unit tests for azure_blob_storage class
 *
 * Tests the azure_blob_storage implementation for Azure Blob DICOM storage.
 * Uses the mock Azure client for testing without Azure SDK dependency.
 */

#include <pacs/storage/azure_blob_storage.hpp>

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
 * @brief Helper to create a default Azure storage config for testing
 */
auto create_test_config() -> azure_storage_config {
  azure_storage_config config;
  config.container_name = "test-dicom-container";
  config.connection_string =
      "DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;"
      "AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6t"
      "q/K1SZFPTOtr/KBHBeksoGMGw==;"
      "BlobEndpoint=http://127.0.0.1:10000/devstoreaccount1";
  config.endpoint_url = "http://127.0.0.1:10000/devstoreaccount1"; // Azurite
  return config;
}

} // namespace

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("azure_blob_storage: construction with config",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();

  REQUIRE_NOTHROW(azure_blob_storage{config});
}

TEST_CASE("azure_blob_storage: container_name accessor",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  CHECK(storage.container_name() == "test-dicom-container");
}

TEST_CASE("azure_blob_storage: is_connected returns true",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  CHECK(storage.is_connected());
}

// ============================================================================
// Store and Retrieve Tests
// ============================================================================

TEST_CASE("azure_blob_storage: store and retrieve",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  auto dataset = create_test_dataset("1.2.3.100", "1.2.3.100.1", "1.2.3.100.1.1",
                                     "PAT001", "DOE^JOHN");

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

TEST_CASE("azure_blob_storage: store requires UIDs",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

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

TEST_CASE("azure_blob_storage: exists check", "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");

  CHECK_FALSE(storage.exists("1.2.3.4.5"));

  REQUIRE(storage.store(dataset).is_ok());

  CHECK(storage.exists("1.2.3.4.5"));
  CHECK_FALSE(storage.exists("nonexistent"));
}

// ============================================================================
// Remove Tests
// ============================================================================

TEST_CASE("azure_blob_storage: remove", "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

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

TEST_CASE("azure_blob_storage: find", "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  auto ds1 = create_test_dataset("1.2.3.1", "1.2.3.1.1", "1.2.3.1.1.1", "PAT001",
                                 "SMITH^JOHN");
  auto ds2 = create_test_dataset("1.2.3.2", "1.2.3.2.1", "1.2.3.2.1.1", "PAT001",
                                 "SMITH^JANE");
  auto ds3 = create_test_dataset("1.2.3.3", "1.2.3.3.1", "1.2.3.3.1.1", "PAT002",
                                 "DOE^JOHN");

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

TEST_CASE("azure_blob_storage: get_statistics",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

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

TEST_CASE("azure_blob_storage: verify_integrity",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
  REQUIRE(storage.store(dataset).is_ok());

  auto result = storage.verify_integrity();
  CHECK(result.is_ok());
}

// ============================================================================
// Blob Name Tests
// ============================================================================

TEST_CASE("azure_blob_storage: get_blob_name",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  // Non-existent returns empty string
  auto blob_name = storage.get_blob_name("nonexistent");
  CHECK(blob_name.empty());

  // After store, returns actual blob name
  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
  REQUIRE(storage.store(dataset).is_ok());

  blob_name = storage.get_blob_name("1.2.3.4.5");
  CHECK(!blob_name.empty());
  CHECK(blob_name.find("1.2.3") != std::string::npos);     // Contains study UID
  CHECK(blob_name.find("1.2.3.4") != std::string::npos);   // Contains series UID
  CHECK(blob_name.find("1.2.3.4.5") != std::string::npos); // Contains SOP UID
  CHECK(blob_name.ends_with(".dcm"));
}

// ============================================================================
// Rebuild Index Tests
// ============================================================================

TEST_CASE("azure_blob_storage: rebuild_index",
          "[storage][azure_blob_storage]") {
  auto config = create_test_config();

  // Store some data
  {
    azure_blob_storage storage1{config};

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

TEST_CASE("azure_blob_storage: store_batch",
          "[storage][azure_blob_storage][batch]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

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

TEST_CASE("azure_blob_storage: retrieve_batch",
          "[storage][azure_blob_storage][batch]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

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

TEST_CASE("azure_blob_storage: store_with_progress",
          "[storage][azure_blob_storage][progress]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

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

TEST_CASE("azure_blob_storage: retrieve_with_progress",
          "[storage][azure_blob_storage][progress]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

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
// Access Tier Tests
// ============================================================================

TEST_CASE("azure_blob_storage: set_access_tier",
          "[storage][azure_blob_storage][tier]") {
  auto config = create_test_config();
  azure_blob_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");
  REQUIRE(storage.store(dataset).is_ok());

  SECTION("change tier to Cool") {
    auto result = storage.set_access_tier("1.2.3.4.5", "Cool");
    CHECK(result.is_ok());
  }

  SECTION("change tier to Archive") {
    auto result = storage.set_access_tier("1.2.3.4.5", "Archive");
    CHECK(result.is_ok());
  }

  SECTION("change tier for non-existent blob returns error") {
    auto result = storage.set_access_tier("nonexistent", "Cool");
    CHECK(result.is_err());
  }
}

// ============================================================================
// Azure Storage Config Tests
// ============================================================================

TEST_CASE("azure_storage_config: default values",
          "[storage][azure_blob_storage]") {
  azure_storage_config config;

  CHECK(config.container_name.empty());
  CHECK(config.connection_string.empty());
  CHECK_FALSE(config.endpoint_suffix.has_value());
  CHECK_FALSE(config.endpoint_url.has_value());
  CHECK(config.block_upload_threshold == 100 * 1024 * 1024); // 100MB
  CHECK(config.block_size == 4 * 1024 * 1024);               // 4MB
  CHECK(config.max_concurrency == 8);
  CHECK(config.connect_timeout_ms == 3000);
  CHECK(config.request_timeout_ms == 60000);
  CHECK(config.use_https == true);
  CHECK(config.access_tier == "Hot");
  CHECK(config.max_retries == 3);
  CHECK(config.retry_delay_ms == 1000);
}

// ============================================================================
// Block Blob Upload Tests
// ============================================================================

TEST_CASE("azure_blob_storage: block blob upload for large files",
          "[storage][azure_blob_storage][block]") {
  azure_storage_config config = create_test_config();
  // Set a very low threshold to trigger block upload
  config.block_upload_threshold = 100;
  config.block_size = 50;

  azure_blob_storage storage{config};

  // Create a dataset that will be larger than threshold
  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");

  std::atomic<int> progress_calls{0};
  auto callback = [&](std::size_t /*bytes_transferred*/,
                      std::size_t /*total_bytes*/) -> bool {
    progress_calls++;
    return true;
  };

  auto result = storage.store_with_progress(dataset, callback);

  REQUIRE(result.is_ok());
  // With block upload, we should have multiple progress callbacks
  CHECK(progress_calls > 1);

  // Verify retrieval works
  auto retrieve_result = storage.retrieve("1.2.3.4.5");
  REQUIRE(retrieve_result.is_ok());
  CHECK(retrieve_result.value().get_string(tags::sop_instance_uid) ==
        "1.2.3.4.5");
}

TEST_CASE("azure_blob_storage: block blob upload cancellation",
          "[storage][azure_blob_storage][block]") {
  azure_storage_config config = create_test_config();
  config.block_upload_threshold = 100;
  config.block_size = 50;

  azure_blob_storage storage{config};

  auto dataset = create_test_dataset("1.2.3", "1.2.3.4", "1.2.3.4.5");

  std::atomic<int> call_count{0};
  auto callback = [&](std::size_t /*bytes_transferred*/,
                      std::size_t /*total_bytes*/) -> bool {
    return ++call_count < 2; // Cancel after first callback
  };

  auto result = storage.store_with_progress(dataset, callback);

  CHECK(result.is_err());
  CHECK_FALSE(storage.exists("1.2.3.4.5"));
}
