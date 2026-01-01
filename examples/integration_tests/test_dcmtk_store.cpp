/**
 * @file test_dcmtk_store.cpp
 * @brief C-STORE interoperability tests with DCMTK
 *
 * Tests bidirectional C-STORE compatibility between pacs_system and DCMTK:
 * - Scenario A: pacs_system SCP <- DCMTK storescu
 * - Scenario B: DCMTK storescp <- pacs_system SCU
 *
 * @see Issue #452 - C-STORE Bidirectional Interoperability Test with DCMTK
 * @see Issue #449 - DCMTK Interoperability Test Automation Epic
 */

#include <catch2/catch_test_macros.hpp>

#include "dcmtk_tool.hpp"
#include "test_fixtures.hpp"

#include "pacs/core/dicom_file.hpp"
#include "pacs/encoding/transfer_syntax.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/services/storage_scu.hpp"

#include <atomic>
#include <filesystem>
#include <future>
#include <mutex>
#include <vector>

namespace fs = std::filesystem;

using namespace pacs::integration_test;
using namespace pacs::network;
using namespace pacs::network::dimse;
using namespace pacs::services;
using namespace pacs::core;
using namespace pacs::encoding;

// =============================================================================
// Helper: Storage Test Server
// =============================================================================

namespace {

/**
 * @brief Simple storage server for testing C-STORE
 */
class storage_test_server {
public:
    explicit storage_test_server(uint16_t port, const std::string& ae_title = "STORE_SCP")
        : port_(port)
        , ae_title_(ae_title)
        , storage_dir_("dcmtk_store_test_") {

        fs::create_directories(storage_dir_.path());

        server_ = std::make_unique<test_server>(port_, ae_title_);

        auto storage_scp_ptr = std::make_shared<storage_scp>();
        storage_scp_ptr->set_handler([this](
            const dicom_dataset& dataset,
            const std::string& /* calling_ae */,
            const std::string& /* sop_class_uid */,
            const std::string& sop_instance_uid) -> storage_status {

            return handle_store(dataset, sop_instance_uid);
        });

        server_->register_service(storage_scp_ptr);
    }

    bool start() {
        if (!server_->start()) {
            return false;
        }
        return wait_for([this]() {
            return process_launcher::is_port_listening(port_);
        }, server_ready_timeout());
    }

    void stop() {
        server_->stop();
    }

    uint16_t port() const { return port_; }
    const std::string& ae_title() const { return ae_title_; }
    size_t stored_count() const { return stored_count_.load(); }
    const fs::path& storage_path() const { return storage_dir_.path(); }

    std::vector<fs::path> stored_files() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stored_files_;
    }

private:
    storage_status handle_store(const dicom_dataset& dataset,
                                const std::string& sop_instance_uid) {
        try {
            auto file_path = storage_dir_.path() / (sop_instance_uid + ".dcm");
            auto file = dicom_file::create(
                dicom_dataset{dataset},
                transfer_syntax::explicit_vr_little_endian);
            auto result = file.save(file_path);

            if (result.is_ok()) {
                std::lock_guard<std::mutex> lock(mutex_);
                stored_files_.push_back(file_path);
                ++stored_count_;
                return storage_status::success;
            }
            return storage_status::storage_error;
        } catch (...) {
            return storage_status::storage_error;
        }
    }

    uint16_t port_;
    std::string ae_title_;
    test_directory storage_dir_;
    std::unique_ptr<test_server> server_;
    std::atomic<size_t> stored_count_{0};
    mutable std::mutex mutex_;
    std::vector<fs::path> stored_files_;
};

/**
 * @brief Find all DICOM files in a directory
 */
std::vector<fs::path> find_dicom_files(const fs::path& dir) {
    std::vector<fs::path> result;
    if (!fs::exists(dir)) {
        return result;
    }

    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".dcm" || ext.empty()) {
                result.push_back(entry.path());
            }
        }
    }
    return result;
}

/**
 * @brief Create a test DICOM file
 */
fs::path create_test_dicom(const fs::path& dir, const std::string& filename,
                           const std::string& modality = "CT") {
    fs::create_directories(dir);

    dicom_dataset ds;
    if (modality == "CT") {
        ds = generate_ct_dataset();
    } else if (modality == "MR") {
        ds = generate_mr_dataset();
    } else if (modality == "XA") {
        ds = generate_xa_dataset();
    } else {
        ds = generate_ct_dataset();
    }

    auto file_path = dir / filename;
    auto file = dicom_file::create(
        std::move(ds),
        transfer_syntax::explicit_vr_little_endian);
    auto result = file.save(file_path);
    if (!result.is_ok()) {
        throw std::runtime_error("Failed to write test DICOM file");
    }

    return file_path;
}

}  // namespace

// =============================================================================
// Test: pacs_system SCP receives from DCMTK storescu
// =============================================================================

TEST_CASE("C-STORE: pacs_system SCP receives from DCMTK storescu",
          "[dcmtk][interop][store]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    auto port = find_available_port();
    test_directory input_dir;

    // Setup storage server
    storage_test_server server(port, "PACS_STORE");
    REQUIRE(server.start());

    SECTION("Single CT image storage") {
        auto test_file = create_test_dicom(input_dir.path(), "test_ct.dcm", "CT");

        auto result = dcmtk_tool::storescu(
            "localhost", port, "PACS_STORE", {test_file});

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
        REQUIRE(server.stored_count() >= 1);
    }

    SECTION("MR image storage") {
        auto test_file = create_test_dicom(input_dir.path(), "test_mr.dcm", "MR");

        auto result = dcmtk_tool::storescu(
            "localhost", port, "PACS_STORE", {test_file});

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
        REQUIRE(server.stored_count() >= 1);
    }

    SECTION("Multiple images in single association") {
        std::vector<fs::path> files;
        for (int i = 0; i < 3; ++i) {
            auto file = create_test_dicom(
                input_dir.path(),
                "test_" + std::to_string(i) + ".dcm",
                "CT");
            files.push_back(file);
        }

        auto result = dcmtk_tool::storescu(
            "localhost", port, "PACS_STORE", files);

        INFO("stdout: " << result.stdout_output);
        INFO("stderr: " << result.stderr_output);

        REQUIRE(result.success());
        REQUIRE(server.stored_count() >= 3);
    }

    SECTION("Multiple modality images") {
        std::vector<fs::path> files;
        files.push_back(create_test_dicom(input_dir.path(), "ct.dcm", "CT"));
        files.push_back(create_test_dicom(input_dir.path(), "mr.dcm", "MR"));
        files.push_back(create_test_dicom(input_dir.path(), "xa.dcm", "XA"));

        auto result = dcmtk_tool::storescu(
            "localhost", port, "PACS_STORE", files);

        REQUIRE(result.success());
        REQUIRE(server.stored_count() >= 3);
    }
}

// =============================================================================
// Test: DCMTK storescp receives from pacs_system SCU
// =============================================================================

TEST_CASE("C-STORE: DCMTK storescp receives from pacs_system SCU",
          "[dcmtk][interop][store]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed - skipping interoperability test");
    }

    auto port = find_available_port();
    test_directory storage_dir;
    test_directory input_dir;

    // Start DCMTK storescp
    auto dcmtk_server = dcmtk_tool::storescp(port, "DCMTK_SCP", storage_dir.path());
    REQUIRE(dcmtk_server.is_running());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(port);
    }, dcmtk_server_ready_timeout()));

    SECTION("Single image via storage_scu") {
        auto test_file = create_test_dicom(input_dir.path(), "test.dcm", "CT");

        auto file_result = dicom_file::open(test_file);
        REQUIRE(file_result.is_ok());

        // Get SOP Class UID from the dataset
        auto sop_class = file_result.value().dataset().get_string(tags::sop_class_uid);
        REQUIRE_FALSE(sop_class.empty());

        // Establish association with DCMTK storescp
        auto connect_result = test_association::connect(
            "localhost", port, "DCMTK_SCP", "PACS_SCU", {sop_class});
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();
        storage_scu scu;
        auto send_result = scu.store(assoc, file_result.value().dataset());
        REQUIRE(send_result.is_ok());

        (void)assoc.release(default_timeout);

        // Wait for DCMTK to write the file
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        auto received = find_dicom_files(storage_dir.path());
        REQUIRE(received.size() >= 1);
    }

    SECTION("Multiple images via storage_scu") {
        // Load all datasets first to get SOP Class UIDs
        std::vector<std::pair<fs::path, dicom_file>> files;
        for (int i = 0; i < 3; ++i) {
            auto test_file = create_test_dicom(
                input_dir.path(),
                "test_" + std::to_string(i) + ".dcm",
                "CT");

            auto file_result = dicom_file::open(test_file);
            REQUIRE(file_result.is_ok());
            files.emplace_back(test_file, std::move(file_result.value()));
        }

        // Establish association
        auto connect_result = test_association::connect(
            "localhost", port, "DCMTK_SCP", "PACS_SCU",
            {"1.2.840.10008.5.1.4.1.1.2"});  // CT Image Storage
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();
        storage_scu scu;

        for (size_t i = 0; i < files.size(); ++i) {
            auto send_result = scu.store(assoc, files[i].second.dataset());
            INFO("Sending file " << i);
            REQUIRE(send_result.is_ok());
        }

        (void)assoc.release(default_timeout);

        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        auto received = find_dicom_files(storage_dir.path());
        REQUIRE(received.size() >= 3);
    }
}

// =============================================================================
// Test: Bidirectional store (round-trip)
// =============================================================================

TEST_CASE("C-STORE: Bidirectional round-trip verification",
          "[dcmtk][interop][store]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    test_directory original_dir;
    test_directory dcmtk_storage_dir;

    auto pacs_port = find_available_port();
    auto dcmtk_port = find_available_port();

    // Setup pacs_system storage server
    storage_test_server pacs_server(pacs_port, "PACS_SCP");
    REQUIRE(pacs_server.start());

    // Start DCMTK storescp
    auto dcmtk_server = dcmtk_tool::storescp(
        dcmtk_port, "DCMTK_SCP", dcmtk_storage_dir.path());
    REQUIRE(dcmtk_server.is_running());

    REQUIRE(wait_for([&]() {
        return process_launcher::is_port_listening(dcmtk_port);
    }, dcmtk_server_ready_timeout()));

    SECTION("DCMTK -> pacs_system -> DCMTK round-trip") {
        // Create original test file
        auto original_file = create_test_dicom(
            original_dir.path(), "original.dcm", "CT");

        // Read original for comparison
        auto original_result = dicom_file::open(original_file);
        REQUIRE(original_result.is_ok());
        auto orig_uid = original_result.value().dataset().get_string(
            tags::sop_instance_uid);
        REQUIRE_FALSE(orig_uid.empty());

        // Step 1: DCMTK storescu -> pacs_system SCP
        auto store1 = dcmtk_tool::storescu(
            "localhost", pacs_port, "PACS_SCP", {original_file});
        REQUIRE(store1.success());
        REQUIRE(pacs_server.stored_count() >= 1);

        // Get the stored files
        auto pacs_files = pacs_server.stored_files();
        REQUIRE(pacs_files.size() >= 1);

        // Step 2: pacs_system SCU -> DCMTK storescp
        auto read_result = dicom_file::open(pacs_files[0]);
        REQUIRE(read_result.is_ok());

        // Establish association with DCMTK storescp
        auto connect_result = test_association::connect(
            "localhost", dcmtk_port, "DCMTK_SCP", "PACS_SCU",
            {"1.2.840.10008.5.1.4.1.1.2"});  // CT Image Storage
        REQUIRE(connect_result.is_ok());

        auto& assoc = connect_result.value();
        storage_scu scu;
        auto send_result = scu.store(assoc, read_result.value().dataset());
        REQUIRE(send_result.is_ok());
        (void)assoc.release(default_timeout);

        // Verify DCMTK received the file
        std::this_thread::sleep_for(std::chrono::milliseconds{500});
        auto dcmtk_files = find_dicom_files(dcmtk_storage_dir.path());
        REQUIRE(dcmtk_files.size() >= 1);

        // Verify data integrity through round-trip
        auto final_result = dicom_file::open(dcmtk_files[0]);
        REQUIRE(final_result.is_ok());

        auto final_uid = final_result.value().dataset().get_string(
            tags::sop_instance_uid);
        REQUIRE_FALSE(final_uid.empty());

        // UIDs should match
        REQUIRE(final_uid == orig_uid);
    }
}

// =============================================================================
// Test: Concurrent store operations
// =============================================================================

TEST_CASE("C-STORE: Concurrent store operations", "[dcmtk][interop][store][stress]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();
    test_directory input_dir;

    // Setup storage server
    storage_test_server server(port, "STRESS_SCP");
    REQUIRE(server.start());

    SECTION("3 concurrent DCMTK storescu clients") {
        constexpr int num_clients = 3;

        // Create test files for each client
        std::vector<fs::path> files;
        for (int i = 0; i < num_clients; ++i) {
            auto file = create_test_dicom(
                input_dir.path(),
                "client_" + std::to_string(i) + ".dcm",
                "CT");
            files.push_back(file);
        }

        // Launch concurrent stores
        std::vector<std::future<dcmtk_result>> futures;
        for (int i = 0; i < num_clients; ++i) {
            futures.push_back(std::async(std::launch::async, [&, i]() {
                return dcmtk_tool::storescu(
                    "localhost", port, "STRESS_SCP",
                    {files[static_cast<size_t>(i)]},
                    "CLIENT_" + std::to_string(i));
            }));
        }

        // All should succeed
        for (size_t i = 0; i < futures.size(); ++i) {
            auto result = futures[i].get();

            INFO("Client " << i << " stdout: " << result.stdout_output);
            INFO("Client " << i << " stderr: " << result.stderr_output);

            REQUIRE(result.success());
        }

        // Verify all files were stored
        REQUIRE(server.stored_count() >= static_cast<size_t>(num_clients));
    }
}

// =============================================================================
// Test: Error handling
// =============================================================================

TEST_CASE("C-STORE: Error handling", "[dcmtk][interop][store][error]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    SECTION("storescu to non-existent server fails gracefully") {
        auto port = find_available_port();
        test_directory input_dir;

        REQUIRE_FALSE(process_launcher::is_port_listening(port));

        auto test_file = create_test_dicom(input_dir.path(), "test.dcm", "CT");

        auto result = dcmtk_tool::storescu(
            "localhost", port, "NONEXISTENT", {test_file},
            "STORESCU", std::chrono::seconds{5});

        REQUIRE_FALSE(result.success());
    }

    SECTION("pacs_system SCU to non-existent server fails gracefully") {
        auto port = find_available_port();

        // Ensure nothing is listening on this port
        REQUIRE_FALSE(process_launcher::is_port_listening(port));

        // Connection should fail - no server listening
        auto connect_result = test_association::connect(
            "localhost", port, "NONEXISTENT", "PACS_SCU",
            {"1.2.840.10008.5.1.4.1.1.2"});  // CT Image Storage

        REQUIRE_FALSE(connect_result.is_ok());
    }
}

// =============================================================================
// Test: Data integrity verification
// =============================================================================

TEST_CASE("C-STORE: Data integrity verification", "[dcmtk][interop][store][integrity]") {
    if (!dcmtk_tool::is_available()) {
        SKIP("DCMTK not installed");
    }

    auto port = find_available_port();
    test_directory input_dir;

    // Setup storage server
    storage_test_server server(port, "INTEGRITY_SCP");
    REQUIRE(server.start());

    SECTION("Patient demographics preserved") {
        // Create test file with specific patient data
        dicom_dataset ds = generate_ct_dataset();
        ds.set_string(tags::patient_name, vr_type::PN, "INTEGRITY^TEST^PATIENT");
        ds.set_string(tags::patient_id, vr_type::LO, "INTEG001");

        auto test_file = input_dir.path() / "integrity_test.dcm";
        auto file = dicom_file::create(
            std::move(ds),
            transfer_syntax::explicit_vr_little_endian);
        auto write_result = file.save(test_file);
        REQUIRE(write_result.is_ok());

        // Store via DCMTK
        auto result = dcmtk_tool::storescu(
            "localhost", port, "INTEGRITY_SCP", {test_file});
        REQUIRE(result.success());

        // Verify stored data
        auto stored_files = server.stored_files();
        REQUIRE(stored_files.size() >= 1);

        auto stored_result = dicom_file::open(stored_files[0]);
        REQUIRE(stored_result.is_ok());

        auto& stored_ds = stored_result.value().dataset();

        auto stored_name = stored_ds.get_string(tags::patient_name);
        auto stored_id = stored_ds.get_string(tags::patient_id);

        REQUIRE_FALSE(stored_name.empty());
        REQUIRE_FALSE(stored_id.empty());
        REQUIRE(stored_name == "INTEGRITY^TEST^PATIENT");
        REQUIRE(stored_id == "INTEG001");
    }
}
