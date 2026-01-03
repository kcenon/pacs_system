/**
 * @file storage_scu_test.cpp
 * @brief Unit tests for Storage SCU service
 */

#include <pacs/services/storage_scu.hpp>
#include <pacs/services/storage_scp.hpp>
#include <pacs/services/storage_status.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

// KCENON_HAS_COMMON_SYSTEM is defined by CMake when common_system is available
#ifndef KCENON_HAS_COMMON_SYSTEM
#define KCENON_HAS_COMMON_SYSTEM 0
#endif

using namespace pacs::services;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::core;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/// SOP Class UID tag (0008,0016)
constexpr dicom_tag tag_sop_class_uid{0x0008, 0x0016};

/// SOP Instance UID tag (0008,0018)
constexpr dicom_tag tag_sop_instance_uid{0x0008, 0x0018};

/// Patient Name tag (0010,0010)
constexpr dicom_tag tag_patient_name{0x0010, 0x0010};

/// Create a test dataset with required DICOM attributes
[[nodiscard]] dicom_dataset create_test_dataset(
    const std::string& sop_class_uid,
    const std::string& sop_instance_uid) {

    dicom_dataset ds;
    ds.set_string(tag_sop_class_uid, pacs::encoding::vr_type::UI, sop_class_uid);
    ds.set_string(tag_sop_instance_uid, pacs::encoding::vr_type::UI, sop_instance_uid);
    ds.set_string(tag_patient_name, pacs::encoding::vr_type::PN, "TEST^PATIENT");
    return ds;
}

/// Create a temporary directory for tests
struct temp_directory {
    std::filesystem::path path;

    temp_directory() {
        path = std::filesystem::temp_directory_path() / "pacs_test_storage_scu";
        std::filesystem::create_directories(path);
    }

    ~temp_directory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

}  // namespace

// =============================================================================
// store_result Tests
// =============================================================================

TEST_CASE("store_result status checks", "[services][storage_scu]") {
    SECTION("success status") {
        store_result result;
        result.sop_instance_uid = "1.2.3.4.5";
        result.status = 0x0000;

        CHECK(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK_FALSE(result.is_error());
    }

    SECTION("warning status (0xB000 range)") {
        store_result result;
        result.sop_instance_uid = "1.2.3.4.5";
        result.status = 0xB000;  // Coercion of data elements

        CHECK_FALSE(result.is_success());
        CHECK(result.is_warning());
        CHECK_FALSE(result.is_error());
    }

    SECTION("warning status (0xB006)") {
        store_result result;
        result.sop_instance_uid = "1.2.3.4.5";
        result.status = 0xB006;  // Elements discarded

        CHECK_FALSE(result.is_success());
        CHECK(result.is_warning());
        CHECK_FALSE(result.is_error());
    }

    SECTION("error status (0xC000 range)") {
        store_result result;
        result.sop_instance_uid = "1.2.3.4.5";
        result.status = 0xC001;  // Storage error

        CHECK_FALSE(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK(result.is_error());
    }

    SECTION("error status (0xA700 range)") {
        store_result result;
        result.sop_instance_uid = "1.2.3.4.5";
        result.status = 0xA700;  // Out of resources

        CHECK_FALSE(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK(result.is_error());
    }

    SECTION("result with error comment") {
        store_result result;
        result.sop_instance_uid = "1.2.3.4.5";
        result.status = 0xC001;
        result.error_comment = "Storage failure: disk full";

        CHECK(result.error_comment == "Storage failure: disk full");
    }
}

// =============================================================================
// storage_scu Construction Tests
// =============================================================================

TEST_CASE("storage_scu default construction", "[services][storage_scu]") {
    storage_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.images_sent() == 0);
        CHECK(scu.failures() == 0);
        CHECK(scu.bytes_sent() == 0);
    }
}

TEST_CASE("storage_scu construction with config", "[services][storage_scu]") {
    storage_scu_config config;
    config.default_priority = priority_high;
    config.response_timeout = std::chrono::milliseconds{60000};
    config.continue_on_error = false;

    storage_scu scu{config};

    SECTION("initial statistics are still zero") {
        CHECK(scu.images_sent() == 0);
        CHECK(scu.failures() == 0);
        CHECK(scu.bytes_sent() == 0);
    }
}

// =============================================================================
// storage_scu Statistics Tests
// =============================================================================

TEST_CASE("storage_scu statistics", "[services][storage_scu]") {
    storage_scu scu;

    SECTION("initial values are zero") {
        CHECK(scu.images_sent() == 0);
        CHECK(scu.failures() == 0);
        CHECK(scu.bytes_sent() == 0);
    }

    SECTION("reset clears statistics") {
        scu.reset_statistics();
        CHECK(scu.images_sent() == 0);
        CHECK(scu.failures() == 0);
        CHECK(scu.bytes_sent() == 0);
    }
}

// =============================================================================
// storage_scu Non-movable Verification
// =============================================================================

TEST_CASE("storage_scu is non-copyable and non-movable", "[services][storage_scu]") {
    // Verify compile-time constraints
    // storage_scu is non-copyable and non-movable due to atomic members
    CHECK_FALSE(std::is_copy_constructible_v<storage_scu>);
    CHECK_FALSE(std::is_copy_assignable_v<storage_scu>);
    CHECK_FALSE(std::is_move_constructible_v<storage_scu>);
    CHECK_FALSE(std::is_move_assignable_v<storage_scu>);
}

// =============================================================================
// Multiple Instance Tests
// =============================================================================

TEST_CASE("multiple storage_scu instances are independent", "[services][storage_scu]") {
    storage_scu scu1;
    storage_scu scu2;

    // Statistics are independent
    CHECK(scu1.images_sent() == 0);
    CHECK(scu2.images_sent() == 0);

    scu1.reset_statistics();
    CHECK(scu1.images_sent() == 0);
    CHECK(scu2.images_sent() == 0);
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_CASE("storage_scu_config defaults", "[services][storage_scu]") {
    storage_scu_config config;

    CHECK(config.default_priority == 0);  // Medium priority
    CHECK(config.response_timeout == std::chrono::milliseconds{30000});
    CHECK(config.continue_on_error == true);
}

TEST_CASE("storage_scu_config priority values", "[services][storage_scu]") {
    SECTION("medium priority (default)") {
        storage_scu_config config;
        config.default_priority = priority_medium;
        CHECK(config.default_priority == 0);
    }

    SECTION("high priority") {
        storage_scu_config config;
        config.default_priority = priority_high;
        CHECK(config.default_priority == 1);
    }

    SECTION("low priority") {
        storage_scu_config config;
        config.default_priority = priority_low;
        CHECK(config.default_priority == 2);
    }
}

// =============================================================================
// Progress Callback Tests
// =============================================================================

TEST_CASE("store_progress_callback type", "[services][storage_scu]") {
    SECTION("lambda callback") {
        std::vector<std::pair<size_t, size_t>> progress_log;

        store_progress_callback callback = [&](size_t completed, size_t total) {
            progress_log.emplace_back(completed, total);
        };

        // Simulate progress
        callback(1, 10);
        callback(2, 10);
        callback(10, 10);

        REQUIRE(progress_log.size() == 3);
        CHECK(progress_log[0] == std::make_pair(size_t{1}, size_t{10}));
        CHECK(progress_log[1] == std::make_pair(size_t{2}, size_t{10}));
        CHECK(progress_log[2] == std::make_pair(size_t{10}, size_t{10}));
    }

    SECTION("null callback is valid") {
        store_progress_callback callback = nullptr;
        CHECK(callback == nullptr);
    }
}

// =============================================================================
// Test Dataset Creation Tests
// =============================================================================

TEST_CASE("create_test_dataset helper", "[services][storage_scu]") {
    auto ds = create_test_dataset(
        "1.2.840.10008.5.1.4.1.1.2",  // CT Image Storage
        "1.2.3.4.5.6.7.8.9.10"
    );

    CHECK(ds.get_string(tag_sop_class_uid) == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(ds.get_string(tag_sop_instance_uid) == "1.2.3.4.5.6.7.8.9.10");
    CHECK(ds.get_string(tag_patient_name) == "TEST^PATIENT");
}

// =============================================================================
// File Operations Tests (Directory handling)
// =============================================================================

TEST_CASE("store_directory with non-existent directory", "[services][storage_scu]") {
    storage_scu scu;
    association assoc;

    auto results = scu.store_directory(
        assoc,
        "/non/existent/directory/path",
        true
    );

    // Should return empty results for non-existent directory
    CHECK(results.empty());
}

TEST_CASE("store_directory with empty directory", "[services][storage_scu]") {
    temp_directory temp_dir;
    storage_scu scu;
    association assoc;

    auto results = scu.store_directory(
        assoc,
        temp_dir.path,
        true
    );

    // Should return empty results for empty directory
    CHECK(results.empty());
}

// =============================================================================
// Integration with storage_status Tests
// =============================================================================

TEST_CASE("storage_scu integrates with storage_status", "[services][storage_scu]") {
    SECTION("success status code") {
        store_result result;
        result.status = static_cast<uint16_t>(storage_status::success);
        CHECK(result.is_success());
    }

    SECTION("warning status codes") {
        store_result result;

        result.status = static_cast<uint16_t>(storage_status::coercion_of_data_elements);
        CHECK(result.is_warning());

        result.status = static_cast<uint16_t>(storage_status::elements_discarded);
        CHECK(result.is_warning());
    }

    SECTION("failure status codes") {
        store_result result;

        result.status = static_cast<uint16_t>(storage_status::storage_error);
        CHECK(result.is_error());

        result.status = static_cast<uint16_t>(storage_status::cannot_understand);
        CHECK(result.is_error());

        result.status = static_cast<uint16_t>(storage_status::out_of_resources);
        CHECK(result.is_error());
    }
}

// =============================================================================
// C-STORE Message Factory Integration Tests
// =============================================================================

TEST_CASE("storage_scu uses make_c_store_rq correctly", "[services][storage_scu]") {
    // Verify the C-STORE request message format expected by storage_scu
    auto request = make_c_store_rq(
        1,
        std::string(ct_image_storage_uid),
        "1.2.3.4.5"
    );

    CHECK(request.command() == command_field::c_store_rq);
    CHECK(request.message_id() == 1);
    CHECK(request.affected_sop_class_uid() == std::string(ct_image_storage_uid));
    CHECK(request.affected_sop_instance_uid() == "1.2.3.4.5");
    CHECK(request.priority() == priority_medium);
}

TEST_CASE("storage_scu understands C-STORE responses", "[services][storage_scu]") {
    // Verify storage_scu can interpret C-STORE response messages
    SECTION("success response") {
        auto response = make_c_store_rsp(
            1,
            std::string(ct_image_storage_uid),
            "1.2.3.4.5",
            status_success
        );

        CHECK(response.command() == command_field::c_store_rsp);
        CHECK(response.status() == status_success);
    }

    SECTION("error response") {
        auto response = make_c_store_rsp(
            1,
            std::string(ct_image_storage_uid),
            "1.2.3.4.5",
            static_cast<status_code>(storage_status::storage_error)
        );

        CHECK(response.command() == command_field::c_store_rsp);
        CHECK(static_cast<uint16_t>(response.status()) == 0xC001);
    }
}

// =============================================================================
// Batch Result Analysis Tests
// =============================================================================

TEST_CASE("analyzing batch store results", "[services][storage_scu]") {
    std::vector<store_result> results;

    // Simulate mixed results
    store_result r1;
    r1.sop_instance_uid = "1.1.1";
    r1.status = 0x0000;  // Success
    results.push_back(r1);

    store_result r2;
    r2.sop_instance_uid = "2.2.2";
    r2.status = 0xB000;  // Warning
    results.push_back(r2);

    store_result r3;
    r3.sop_instance_uid = "3.3.3";
    r3.status = 0xC001;  // Error
    results.push_back(r3);

    SECTION("count successes") {
        auto success_count = std::count_if(results.begin(), results.end(),
            [](const store_result& r) { return r.is_success(); });
        CHECK(success_count == 1);
    }

    SECTION("count warnings") {
        auto warning_count = std::count_if(results.begin(), results.end(),
            [](const store_result& r) { return r.is_warning(); });
        CHECK(warning_count == 1);
    }

    SECTION("count errors") {
        auto error_count = std::count_if(results.begin(), results.end(),
            [](const store_result& r) { return r.is_error(); });
        CHECK(error_count == 1);
    }

    SECTION("count non-failures") {
        auto non_failure_count = std::count_if(results.begin(), results.end(),
            [](const store_result& r) { return r.is_success() || r.is_warning(); });
        CHECK(non_failure_count == 2);
    }
}

// =============================================================================
// SOP Class UID Constants Availability Tests
// =============================================================================

TEST_CASE("storage_scu can use SOP Class UIDs from storage_scp", "[services][storage_scu]") {
    // Verify we can access the SOP Class UID constants
    CHECK(std::string(ct_image_storage_uid) == "1.2.840.10008.5.1.4.1.1.2");
    CHECK(std::string(mr_image_storage_uid) == "1.2.840.10008.5.1.4.1.1.4");
    CHECK(std::string(us_image_storage_uid) == "1.2.840.10008.5.1.4.1.1.6.1");

    // Create a test dataset using these constants
    auto ds = create_test_dataset(
        std::string(ct_image_storage_uid),
        "1.2.3.4.5"
    );

    CHECK(ds.get_string(tag_sop_class_uid) == std::string(ct_image_storage_uid));
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_CASE("store_result default construction", "[services][storage_scu]") {
    store_result result;

    CHECK(result.sop_instance_uid.empty());
    CHECK(result.status == 0);
    CHECK(result.error_comment.empty());
    CHECK(result.is_success());  // Status 0 is success
}

TEST_CASE("store_file with non-existent file", "[services][storage_scu]") {
    storage_scu scu;
    association assoc;

    auto result = scu.store_file(assoc, "/non/existent/file.dcm");

    CHECK(result.is_err());
#if KCENON_HAS_COMMON_SYSTEM
    CHECK(result.error().message.find("File not found") != std::string::npos);
#else
    CHECK(result.error().find("File not found") != std::string::npos);
#endif
}

// =============================================================================
// store_files Tests
// =============================================================================

TEST_CASE("store_files with empty vector", "[services][storage_scu]") {
    storage_scu scu;
    association assoc;

    std::vector<std::filesystem::path> empty_paths;
    auto results = scu.store_files(assoc, empty_paths);

    CHECK(results.empty());
}

TEST_CASE("store_files with non-existent files", "[services][storage_scu]") {
    storage_scu scu;
    association assoc;

    std::vector<std::filesystem::path> paths = {
        "/non/existent/file1.dcm",
        "/non/existent/file2.dcm",
        "/non/existent/file3.dcm"
    };

    auto results = scu.store_files(assoc, paths);

    // All should fail with file not found
    REQUIRE(results.size() == 3);
    for (const auto& result : results) {
        CHECK(result.is_error());
    }
}

TEST_CASE("store_files progress callback", "[services][storage_scu]") {
    storage_scu scu;
    association assoc;

    std::vector<std::filesystem::path> paths = {
        "/non/existent/file1.dcm",
        "/non/existent/file2.dcm"
    };

    std::vector<std::pair<size_t, size_t>> progress_log;
    auto results = scu.store_files(assoc, paths,
        [&progress_log](size_t completed, size_t total) {
            progress_log.emplace_back(completed, total);
        });

    // Progress callback should be called for each file
    REQUIRE(progress_log.size() == 2);
    CHECK(progress_log[0] == std::make_pair(size_t{1}, size_t{2}));
    CHECK(progress_log[1] == std::make_pair(size_t{2}, size_t{2}));
}

TEST_CASE("store_files with null progress callback", "[services][storage_scu]") {
    storage_scu scu;
    association assoc;

    std::vector<std::filesystem::path> paths = {"/non/existent/file.dcm"};

    // Should not crash with null callback
    auto results = scu.store_files(assoc, paths, nullptr);

    REQUIRE(results.size() == 1);
}
