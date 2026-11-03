// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file instance_archive_test.cpp
 * @brief One object, its file and its rows, whichever protocol brought it
 */

#include <kcenon/pacs/storage/instance_archive.h>

#include <kcenon/pacs/core/dicom_element.h>
#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/vr_type.h>
#include <kcenon/pacs/storage/file_storage.h>
#include <kcenon/pacs/storage/index_database.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::pacs::storage;
namespace core = kcenon::pacs::core;
namespace tags = kcenon::pacs::core::tags;
using kcenon::pacs::encoding::transfer_syntax;
using kcenon::pacs::encoding::vr_type;

namespace {

class test_archive {
public:
    explicit test_archive(duplicate_policy policy = duplicate_policy::reject) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() / ("instance_archive_" + std::to_string(stamp));
        std::filesystem::create_directories(root_);
        file_storage_config config;
        config.root_path = root_ / "files";
        config.duplicate = policy;
        files_ = std::make_unique<file_storage>(config);
        auto opened = index_database::open((root_ / "index.db").string());
        REQUIRE(opened.is_ok());
        database_ = std::move(opened.value());
    }
    ~test_archive() {
        files_.reset();
        database_.reset();
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }
    test_archive(const test_archive&) = delete;
    test_archive& operator=(const test_archive&) = delete;

    [[nodiscard]] file_storage& files() { return *files_; }
    [[nodiscard]] index_database& database() { return *database_; }

    [[nodiscard]] archive_result offer(const core::dicom_dataset& dataset) {
        return archive_instance(*files_, *database_, dataset, transfer_syntax::explicit_vr_little_endian);
    }

    /// Every regular file under the storage root.
    [[nodiscard]] std::size_t file_count() const {
        std::size_t count = 0;
        std::error_code ignored;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root_ / "files", ignored)) {
            count += entry.is_regular_file() ? 1 : 0;
        }
        return count;
    }

private:
    std::filesystem::path root_;
    std::unique_ptr<file_storage> files_;
    std::unique_ptr<index_database> database_;
};

[[nodiscard]] core::dicom_dataset an_object(std::string_view study = "1.2.3", std::string_view series = "1.2.3.1", std::string_view sop = "1.2.3.1.1") {
    core::dicom_dataset dataset;
    dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.7");
    dataset.set_string(tags::sop_instance_uid, vr_type::UI, sop);
    dataset.set_string(tags::study_instance_uid, vr_type::UI, study);
    dataset.set_string(tags::series_instance_uid, vr_type::UI, series);
    dataset.set_string(tags::patient_id, vr_type::LO, "PATIENT");
    dataset.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    dataset.set_string(tags::modality, vr_type::CS, "OT");
    dataset.set_string(tags::series_number, vr_type::IS, "4");
    dataset.set_string(tags::instance_number, vr_type::IS, "5");
    return dataset;
}

}  // namespace

TEST_CASE("is_valid_uid is PS3.5 9.1", "[storage][archive]") {
    CHECK(is_valid_uid("1.2.840.10008.1.2"));
    CHECK(is_valid_uid(std::string(64, '1')));
    for (const auto* bad : {"", ".", "..", "1.", ".1", "1..2", "1.2.a", "../x", "1/2", "1 2"}) {
        CAPTURE(bad);
        CHECK_FALSE(is_valid_uid(bad));
    }
    CHECK_FALSE(is_valid_uid(std::string(65, '1')));
}

TEST_CASE("archive_instance keeps the file and the rows together", "[storage][archive]") {
    test_archive archive;
    auto dataset = an_object();
    dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, 6);
    dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, 8);
    dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
    dataset.set_string(core::dicom_tag{0x0028, 0x0008}, vr_type::IS, "3");
    dataset.set_string(tags::study_id, vr_type::SH, "S1");
    dataset.set_string(tags::accession_number, vr_type::SH, "A1");

    const auto result = archive.offer(dataset);
    REQUIRE(result.status == archive_status::stored);
    CHECK(std::filesystem::exists(result.file_path));

    const auto instance = archive.database().find_instance("1.2.3.1.1");
    REQUIRE(instance);
    CHECK(instance->file_path == result.file_path.string());
    CHECK(instance->file_size == static_cast<int64_t>(std::filesystem::file_size(result.file_path)));
    CHECK(instance->transfer_syntax == transfer_syntax::explicit_vr_little_endian.uid());
    CHECK(instance->instance_number == 5);
    CHECK(instance->rows == 6);
    CHECK(instance->columns == 8);
    CHECK(instance->bits_allocated == 8);
    CHECK(instance->number_of_frames == 3);

    const auto series = archive.database().find_series("1.2.3.1");
    REQUIRE(series);
    CHECK(series->series_number == 4);
    const auto study = archive.database().find_study("1.2.3");
    REQUIRE(study);
    CHECK(study->accession_number == "A1");
    CHECK(archive.database().find_patient("PATIENT").has_value());
}

TEST_CASE("archive_instance indexes UTF-8, whatever the object uses", "[storage][archive]") {
    test_archive archive;
    auto dataset = an_object();
    dataset.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 100");
    dataset.set_string(tags::patient_name, vr_type::PN, "M\xFCller");
    REQUIRE(archive.offer(dataset).status == archive_status::stored);
    const auto patient = archive.database().find_patient("PATIENT");
    REQUIRE(patient);
    CHECK(patient->patient_name == "M\xC3\xBCller");
}

TEST_CASE("archive_instance refuses what nothing could find, writing nothing", "[storage][archive]") {
    test_archive archive;
    const auto refused = [&](core::dicom_dataset dataset) {
        const auto result = archive.offer(dataset);
        CHECK(result.status == archive_status::rejected);
        CHECK_FALSE(result.message.empty());
        CHECK(archive.file_count() == 0);
    };

    SECTION("an empty Patient ID") {
        auto dataset = an_object();
        dataset.set_string(tags::patient_id, vr_type::LO, "");
        refused(dataset);
    }
    for (const auto tag : {tags::sop_class_uid, tags::sop_instance_uid, tags::study_instance_uid, tags::series_instance_uid}) {
        SECTION("a missing or invalid UID " + std::to_string(tag.combined())) {
            auto missing = an_object();
            missing.remove(tag);
            refused(missing);
            for (const auto* bad : {"..", "../../x", "1.2.a"}) {
                auto invalid = an_object();
                invalid.set_string(tag, vr_type::UI, bad);
                refused(invalid);
            }
        }
    }
}

TEST_CASE("archive_instance removes a file the index would not take", "[storage][archive]") {
    test_archive archive;
    auto dataset = an_object();
    // The index holds at most the 64 characters of LO, so this object can be written and not indexed.
    dataset.set_string(tags::patient_id, vr_type::LO, std::string(65, 'P'));
    const auto result = archive.offer(dataset);
    CHECK(result.status == archive_status::failed);
    CHECK(archive.file_count() == 0);
    CHECK_FALSE(archive.files().exists("1.2.3.1.1"));

    // Nothing is left to refuse the next copy as a duplicate.
    CHECK(archive.offer(an_object()).status == archive_status::stored);
}

TEST_CASE("archive_instance applies the duplicate policy before anything changes", "[storage][archive]") {
    SECTION("reject") {
        test_archive archive(duplicate_policy::reject);
        const auto first = archive.offer(an_object());
        REQUIRE(first.status == archive_status::stored);
        const auto second = archive.offer(an_object("1.9", "1.9.1"));
        CHECK(second.status == archive_status::duplicate_refused);
        CHECK(archive.database().find_instance("1.2.3.1.1")->file_path == first.file_path.string());
        CHECK_FALSE(archive.database().find_study("1.9").has_value());
        CHECK(archive.file_count() == 1);
    }

    SECTION("ignore keeps what is held, however the new copy differs") {
        test_archive archive(duplicate_policy::ignore);
        const auto first = archive.offer(an_object());
        REQUIRE(first.status == archive_status::stored);
        const auto second = archive.offer(an_object("1.9", "1.9.1"));
        CHECK(second.status == archive_status::duplicate_ignored);
        const auto instance = archive.database().find_instance("1.2.3.1.1");
        REQUIRE(instance);
        CHECK(archive.database().find_series_by_pk(instance->series_pk)->series_uid == "1.2.3.1");
        CHECK_FALSE(archive.database().find_study("1.9").has_value());
        CHECK(archive.file_count() == 1);
    }

    SECTION("replace moves the object, and leaves no file behind") {
        test_archive archive(duplicate_policy::replace);
        const auto first = archive.offer(an_object());
        REQUIRE(first.status == archive_status::stored);
        const auto second = archive.offer(an_object("1.9", "1.9.1"));
        REQUIRE(second.status == archive_status::stored);
        CHECK(archive.file_count() == 1);
        CHECK_FALSE(std::filesystem::exists(first.file_path));
        const auto instance = archive.database().find_instance("1.2.3.1.1");
        REQUIRE(instance);
        CHECK(instance->file_path == second.file_path.string());
        CHECK(archive.database().find_series_by_pk(instance->series_pk)->series_uid == "1.9.1");
    }
}

TEST_CASE("archive_instance loses nothing to many writers at once", "[storage][archive]") {
    test_archive archive;
    constexpr int writers = 8;
    constexpr int objects = 25;
    std::atomic<int> stored{0};
    std::atomic<int> refused{0};
    std::vector<std::thread> threads;
    for (int writer = 0; writer < writers; ++writer) {
        threads.emplace_back([&, writer] {
            for (int object = 0; object < objects; ++object) {
                // One new patient, study and series row everyone wants at once, and objects shared by two writers.
                const auto series = "1.2.3." + std::to_string(writer / 2);
                const auto sop = series + "." + std::to_string(object);
                const auto result = archive.offer(an_object("1.2.3", series, sop));
                if (result.status == archive_status::stored) {
                    stored.fetch_add(1);
                } else if (result.status == archive_status::duplicate_refused) {
                    refused.fetch_add(1);
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(stored.load() == writers / 2 * objects);
    CHECK(refused.load() == writers / 2 * objects);
    CHECK(archive.file_count() == static_cast<std::size_t>(writers / 2 * objects));
    for (int series = 0; series < writers / 2; ++series) {
        const auto files = archive.database().get_series_files("1.2.3." + std::to_string(series));
        REQUIRE(files.is_ok());
        CHECK(files.value().size() == objects);
    }
}

TEST_CASE("file_storage refuses a UID that would climb out of the archive", "[storage][archive]") {
    test_archive archive;
    auto dataset = an_object("..", "..", "..");
    CHECK(archive.files().store(dataset, transfer_syntax::explicit_vr_little_endian).is_err());
    CHECK(archive.file_count() == 0);
}
