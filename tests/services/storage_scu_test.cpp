/**
 * @file storage_scu_test.cpp
 * @brief Unit tests for Storage SCU service
 */

#include <kcenon/pacs/services/storage_scu.h>
#include <kcenon/pacs/services/scp_service.h>
#include <kcenon/pacs/services/storage_scp.h>
#include <kcenon/pacs/services/storage_status.h>
#include <kcenon/pacs/network/dicom_server.h>
#include <kcenon/pacs/network/dimse/command_field.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/status_codes.h>
#include <kcenon/pacs/network/server_config.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/result.h>
#include <kcenon/pacs/encoding/transfer_syntax.h>
#include <kcenon/pacs/encoding/vr_type.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <random>

// KCENON_HAS_COMMON_SYSTEM is defined by CMake when common_system is available
#ifndef KCENON_HAS_COMMON_SYSTEM
#define KCENON_HAS_COMMON_SYSTEM 0
#endif

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;

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
    ds.set_string(tag_sop_class_uid, kcenon::pacs::encoding::vr_type::UI, sop_class_uid);
    ds.set_string(tag_sop_instance_uid, kcenon::pacs::encoding::vr_type::UI, sop_instance_uid);
    ds.set_string(tag_patient_name, kcenon::pacs::encoding::vr_type::PN, "TEST^PATIENT");
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

// =============================================================================
// Presentation Context Negotiation Tests (Issue #1274)
// =============================================================================

namespace {

using kcenon::pacs::encoding::transfer_syntax;

constexpr const char* implicit_vr_le_uid = "1.2.840.10008.1.2";
constexpr const char* explicit_vr_le_uid = "1.2.840.10008.1.2.1";
constexpr const char* explicit_vr_be_uid = "1.2.840.10008.1.2.2";
constexpr const char* rle_lossless_uid = "1.2.840.10008.1.2.5";

/// Port of the in-process dicom_server; no other test uses it
constexpr uint16_t negotiation_test_port = 11274;

/// No dicom_server listens here, so connect() returns an unpaired SCU
constexpr uint16_t unserved_port = 104;

/// A temporary directory that no other test process shares
struct unique_temp_directory {
    std::filesystem::path path;

    explicit unique_temp_directory(const std::string& prefix) {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               (prefix + "_" + std::to_string(stamp) + "_" +
                std::to_string(std::random_device{}()));
        std::filesystem::create_directories(path);
    }

    ~unique_temp_directory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

/**
 * @brief Write a CT Image file in a transfer syntax.
 *
 * The file carries no Pixel Data: storage_scu picks the presentation context
 * from the transfer syntax in the File Meta Information, so the tests need
 * no codec.
 */
std::filesystem::path write_ct_file(const std::filesystem::path& directory,
                                    const std::string& sop_instance_uid,
                                    const char* transfer_syntax_uid) {
    transfer_syntax syntax{transfer_syntax_uid};
    REQUIRE(syntax.is_valid());

    auto file = dicom_file::create(
        create_test_dataset(std::string(ct_image_storage_uid), sop_instance_uid),
        syntax);
    auto path = directory / (sop_instance_uid + ".dcm");
    REQUIRE(file.save(path).is_ok());
    return path;
}

/**
 * @brief Storage SCP that records the transfer syntax of the presentation
 *        context each C-STORE request arrives on, then stores it like
 *        storage_scp.
 */
class context_recording_storage_scp final : public scp_service {
public:
    [[nodiscard]] std::vector<std::string> supported_sop_classes() const override {
        return {std::string(ct_image_storage_uid)};
    }

    [[nodiscard]] kcenon::pacs::network::Result<std::monostate> handle_message(
        association& assoc,
        uint8_t context_id,
        const dimse_message& request) override {
        auto syntax = assoc.context_transfer_syntax(context_id);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            received_[request.affected_sop_instance_uid()] =
                syntax.is_ok() ? std::string(syntax.value().uid()) : std::string{};
        }
        return storage_.handle_message(assoc, context_id, request);
    }

    [[nodiscard]] std::string_view service_name() const noexcept override {
        return "Context Recording Storage SCP";
    }

    /// Transfer syntax of the context each SOP Instance arrived on
    [[nodiscard]] std::map<std::string, std::string> received() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return received_;
    }

private:
    storage_scp storage_;
    mutable std::mutex mutex_;
    std::map<std::string, std::string> received_;
};

/// An SCU paired in memory with an SCP that accepts only some transfer
/// syntaxes, the way dicom_server pairs them
struct paired_storage_associations {
    std::unique_ptr<association> scu;
    std::unique_ptr<association> scp;
};

paired_storage_associations pair_with_scp(
    std::vector<proposed_presentation_context> proposed,
    std::vector<std::string> scp_transfer_syntaxes) {
    association_config scu_config;
    scu_config.calling_ae_title = "STORESCU";
    scu_config.called_ae_title = "STRICT_SCP";
    scu_config.proposed_contexts = std::move(proposed);
    auto connect_result =
        association::connect("localhost", unserved_port, scu_config);
    REQUIRE(connect_result.is_ok());
    auto scu = std::make_unique<association>(std::move(connect_result.value()));

    scp_config config;
    config.ae_title = "STRICT_SCP";
    config.supported_abstract_syntaxes = {std::string(ct_image_storage_uid)};
    config.supported_transfer_syntaxes = std::move(scp_transfer_syntaxes);
    auto scp = std::make_unique<association>(
        association::accept(scu->build_associate_rq(), config));
    REQUIRE(scp->is_established());
    REQUIRE(scu->process_associate_ac(scp->build_associate_ac()));

    scp->set_peer(scu.get());
    return {std::move(scu), std::move(scp)};
}

/// Answer one C-STORE request on the SCP end with storage_scp; the future
/// holds the transfer syntax of the context the request arrived on
std::future<std::string> answer_one_store(association& scp, storage_scp& storage) {
    return std::async(std::launch::async, [&scp, &storage]() -> std::string {
        auto received = scp.receive_dimse(std::chrono::seconds{10});
        if (received.is_err()) {
            return {};
        }
        const auto& [context_id, request] = received.value();
        auto syntax = scp.context_transfer_syntax(context_id);
        (void)storage.handle_message(scp, context_id, request);
        return syntax.is_ok() ? std::string(syntax.value().uid()) : std::string{};
    });
}

}  // namespace

TEST_CASE("storage_scu proposes only transfer syntaxes the DIMSE layer can send",
          "[services][storage_scu][negotiation]") {
    using syntax_list = std::vector<std::string>;

    SECTION("an encapsulated transfer syntax is proposed on its own") {
        for (const char* uid : {"1.2.840.10008.1.2.4.201", "1.2.840.10008.1.2.4.202",
                                "1.2.840.10008.1.2.4.203", "1.2.840.10008.1.2.4.90",
                                "1.2.840.10008.1.2.4.80", rle_lossless_uid}) {
            INFO(uid);
            CHECK(storage_scu::proposed_transfer_syntaxes(transfer_syntax{uid}) ==
                  syntax_list{uid});
        }
    }

    SECTION("a native transfer syntax is proposed as Little Endian, own syntax first") {
        CHECK(storage_scu::proposed_transfer_syntaxes(transfer_syntax{implicit_vr_le_uid}) ==
              syntax_list{implicit_vr_le_uid, explicit_vr_le_uid});
        CHECK(storage_scu::proposed_transfer_syntaxes(transfer_syntax{explicit_vr_le_uid}) ==
              syntax_list{explicit_vr_le_uid, implicit_vr_le_uid});
        CHECK(storage_scu::proposed_transfer_syntaxes(transfer_syntax{explicit_vr_be_uid}) ==
              syntax_list{explicit_vr_le_uid, implicit_vr_le_uid});
    }

    SECTION("an unknown transfer syntax is not proposed") {
        CHECK(storage_scu::proposed_transfer_syntaxes(transfer_syntax{"1.2.3.4"}).empty());
    }

    SECTION("no registered transfer syntax leads to a Big Endian proposal") {
        for (const auto& syntax : kcenon::pacs::encoding::all_transfer_syntaxes()) {
            INFO(syntax.uid());
            const auto proposed = storage_scu::proposed_transfer_syntaxes(syntax);
            REQUIRE_FALSE(proposed.empty());
            CHECK(std::find(proposed.begin(), proposed.end(), explicit_vr_be_uid) ==
                  proposed.end());
            if (!syntax.is_encapsulated()) {
                for (const auto& uid : proposed) {
                    CHECK((uid == implicit_vr_le_uid || uid == explicit_vr_le_uid));
                }
            }
        }
    }
}

TEST_CASE("storage_scu sends an HTJ2K file on an HTJ2K context of the pacs_system SCP",
          "[services][storage_scu][negotiation][htj2k]") {
    const std::string ct{ct_image_storage_uid};
    const std::string htj2k_instance = "1.2.826.0.1.3680043.2.1545.1274.1";
    const std::string native_instance = "1.2.826.0.1.3680043.2.1545.1274.2";

    for (const char* htj2k_uid : {"1.2.840.10008.1.2.4.201", "1.2.840.10008.1.2.4.202",
                                  "1.2.840.10008.1.2.4.203"}) {
        DYNAMIC_SECTION("file in " << htj2k_uid) {
            unique_temp_directory temp_dir{"pacs_test_storage_scu_htj2k"};
            const auto htj2k_file = write_ct_file(temp_dir.path, htj2k_instance, htj2k_uid);
            const auto native_file =
                write_ct_file(temp_dir.path, native_instance, explicit_vr_le_uid);

            server_config config;
            config.ae_title = "NEGOTIATION_SCP";
            config.port = negotiation_test_port;
            auto service = std::make_shared<context_recording_storage_scp>();
            dicom_server server{config};
            server.register_service(service);
            REQUIRE(server.start().is_ok());

            // One context per (SOP Class, transfer syntax) pair of the files,
            // as store_scu proposes them
            association_config scu_config;
            scu_config.calling_ae_title = "STORESCU";
            scu_config.called_ae_title = "NEGOTIATION_SCP";
            scu_config.proposed_contexts.push_back(
                {1, ct, storage_scu::proposed_transfer_syntaxes(transfer_syntax{htj2k_uid})});
            scu_config.proposed_contexts.push_back(
                {3, ct,
                 storage_scu::proposed_transfer_syntaxes(transfer_syntax{explicit_vr_le_uid})});

            auto connect_result =
                association::connect("localhost", negotiation_test_port, scu_config);
            REQUIRE(connect_result.is_ok());
            auto& assoc = connect_result.value();

            auto htj2k_context = assoc.context_transfer_syntax(1);
            REQUIRE(htj2k_context.is_ok());
            CHECK(htj2k_context.value().uid() == htj2k_uid);

            storage_scu scu;
            auto results = scu.store_files(assoc, {htj2k_file, native_file});
            REQUIRE(results.size() == 2);
            CHECK(results[0].is_success());
            CHECK(results[1].is_success());

            // The SCP side saw each file on a context in the file's syntax
            const auto received = service->received();
            REQUIRE(received.size() == 2);
            CHECK(received.at(htj2k_instance) == htj2k_uid);
            CHECK(received.at(native_instance) == explicit_vr_le_uid);

            CHECK(assoc.release().is_ok());
            server.stop();
        }
    }
}

TEST_CASE("storage_scu never sends a compressed file on an Explicit VR LE context",
          "[services][storage_scu][negotiation]") {
    unique_temp_directory temp_dir{"pacs_test_storage_scu_strict"};
    const auto rle_file =
        write_ct_file(temp_dir.path, "1.2.826.0.1.3680043.2.1545.1274.11", rle_lossless_uid);
    const auto native_file =
        write_ct_file(temp_dir.path, "1.2.826.0.1.3680043.2.1545.1274.12", implicit_vr_le_uid);

    // A context that also offers native fallbacks for the compressed file,
    // as store_scu proposed before #1274; the SCP accepts only Explicit VR LE
    auto pair = pair_with_scp(
        {{1, std::string(ct_image_storage_uid),
          {rle_lossless_uid, explicit_vr_le_uid, implicit_vr_le_uid}}},
        {explicit_vr_le_uid});
    auto accepted = pair.scu->context_transfer_syntax(1);
    REQUIRE(accepted.is_ok());
    REQUIRE(accepted.value().uid() == explicit_vr_le_uid);

    storage_scu scu;
    storage_scp storage;

    SECTION("the compressed file fails cleanly and nothing is sent") {
        auto result = scu.store_file(*pair.scu, rle_file);
        REQUIRE(result.is_err());
        CHECK(result.error().code == kcenon::pacs::error_codes::store_no_accepted_context);
        CHECK(result.error().message.find(rle_lossless_uid) != std::string::npos);

        auto received = pair.scp->receive_dimse(std::chrono::milliseconds{200});
        REQUIRE(received.is_err());
        CHECK(received.error().code == kcenon::pacs::error_codes::receive_timeout);
    }

    SECTION("a batch reports the compressed file and still sends the native file") {
        auto answered = answer_one_store(*pair.scp, storage);
        auto results = scu.store_files(*pair.scu, {rle_file, native_file});
        REQUIRE(results.size() == 2);
        CHECK(results[0].is_error());
        CHECK(results[0].error_comment.find(rle_lossless_uid) != std::string::npos);
        CHECK(results[1].is_success());

        // The native file went out on the Explicit VR LE context, and it was
        // the only request the SCP received
        REQUIRE(answered.wait_for(std::chrono::seconds{10}) == std::future_status::ready);
        CHECK(answered.get() == explicit_vr_le_uid);
        CHECK(pair.scp->receive_dimse(std::chrono::milliseconds{200}).is_err());
    }
}

TEST_CASE("storage_scu never sends a native dataset on a compressed context",
          "[services][storage_scu][negotiation]") {
    const std::string ct{ct_image_storage_uid};
    auto pair = pair_with_scp({{1, ct, {rle_lossless_uid}}}, {rle_lossless_uid});
    auto dataset = create_test_dataset(ct, "1.2.826.0.1.3680043.2.1545.1274.21");
    storage_scu scu;

    SECTION("store() treats the dataset as native and does not send it") {
        auto result = scu.store(*pair.scu, dataset);
        REQUIRE(result.is_err());
        CHECK(result.error().code == kcenon::pacs::error_codes::store_no_accepted_context);

        auto received = pair.scp->receive_dimse(std::chrono::milliseconds{200});
        REQUIRE(received.is_err());
        CHECK(received.error().code == kcenon::pacs::error_codes::receive_timeout);
    }

    SECTION("store() with the dataset's transfer syntax sends it on the matching context") {
        storage_scp storage;
        auto answered = answer_one_store(*pair.scp, storage);
        auto result = scu.store(*pair.scu, dataset, transfer_syntax::rle_lossless);
        REQUIRE(result.is_ok());
        CHECK(result.value().is_success());

        REQUIRE(answered.wait_for(std::chrono::seconds{10}) == std::future_status::ready);
        CHECK(answered.get() == rle_lossless_uid);
    }
}
