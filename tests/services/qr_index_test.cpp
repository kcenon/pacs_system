// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file qr_index_test.cpp
 * @brief Query/Retrieve answers computed from the index database
 *
 * The expectations here are literal. validation/dcmtk/tests/qr_matrix_probe.py repeats them over
 * the network against independent peers; these keep the rules covered where no peer exists.
 */

#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/transfer_syntax.h>
#include <kcenon/pacs/encoding/vr_type.h>
#include <kcenon/pacs/services/qr_index.h>
#include <kcenon/pacs/storage/index_database.h>
#include <kcenon/pacs/storage/sql_literal.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace kcenon::pacs;
using namespace kcenon::pacs::services;
using core::dicom_dataset;
using encoding::vr_type;
namespace tags = core::tags;

namespace {

constexpr auto kAe = "PACS_QR";
constexpr auto kCt = "1.2.840.10008.5.1.4.1.1.2";

/// Two patients: P1 has a two-series study and one more, P2 has a study with no accession number.
std::unique_ptr<storage::index_database> seeded_database() {
    auto opened = storage::index_database::open(":memory:");
    REQUIRE(opened.is_ok());
    auto database = std::move(opened.value());
    const auto p1 = database->upsert_patient("P1", "DOE^JOHN", "19700101", "M").value();
    const auto p2 = database->upsert_patient("P_2", "\xED\x99\x8D^\xEA\xB8\xB8\xEB\x8F\x99", "19800202", "F").value();
    const auto s1 = database->upsert_study(p1, "1.1", "S1", "20240115", "101500", "ACC-1", "HOUSE^GREG", "CHEST CT").value();
    const auto s2 = database->upsert_study(p1, "1.2", "S2", "20240220", "143000", "ACC-2", "WILSON^JAMES", "BRAIN MR").value();
    const auto s3 = database->upsert_study(p2, "1.3", "S3", "20230310", "090000", "", "", "O'BRIEN").value();
    const auto a = database->upsert_series(s1, "1.1.1", "CT", 1, "AXIAL", "", "").value();
    const auto b = database->upsert_series(s1, "1.1.2", "OT", 2, "SCOUT", "", "").value();
    const auto c = database->upsert_series(s2, "1.2.1", "MR", 1, "T1", "", "").value();
    const auto d = database->upsert_series(s3, "1.3.1", "CT", 1, "PLAIN", "", "").value();
    REQUIRE(database->upsert_instance(a, "1.1.1.1", kCt, "/nonexistent/a1", 1, "", 1).is_ok());
    REQUIRE(database->upsert_instance(a, "1.1.1.2", kCt, "/nonexistent/a2", 1, "", 2).is_ok());
    REQUIRE(database->upsert_instance(b, "1.1.2.1", kCt, "/nonexistent/b1", 1, "", 1).is_ok());
    REQUIRE(database->upsert_instance(c, "1.2.1.1", kCt, "/nonexistent/c1", 1, "", 1).is_ok());
    REQUIRE(database->upsert_instance(d, "1.3.1.1", kCt, "/nonexistent/d1", 1, "", 1).is_ok());
    return database;
}

find_outcome find(storage::index_database& database, qr_model model, query_level level, const dicom_dataset& keys) {
    return find_in_index(database, find_request{model, level, keys, "SCU", kAe});
}

std::vector<std::string> values(const find_outcome& outcome, core::dicom_tag tag) {
    std::vector<std::string> found;
    for (const auto& match : outcome.matches) {
        found.push_back(match.get_string(tag));
    }
    std::sort(found.begin(), found.end());
    return found;
}

using strings = std::vector<std::string>;

}  // namespace

TEST_CASE("find_in_index evaluates each level", "[services][qr][index]") {
    auto database = seeded_database();

    SECTION("PATIENT with return keys only is universal") {
        dicom_dataset keys;
        keys.set_string(tags::patient_id, vr_type::LO, "");
        keys.set_string(tags::patient_name, vr_type::PN, "");
        const auto outcome = find(*database, qr_model::patient_root, query_level::patient, keys);
        CHECK(outcome.failure == qr_failure::none);
        CHECK(values(outcome, tags::patient_id) == strings{"P1", "P_2"});
    }
    SECTION("STUDY under Study Root takes Patient keys") {
        dicom_dataset keys;
        keys.set_string(tags::patient_name, vr_type::PN, "doe*");
        keys.set_string(tags::study_date, vr_type::DA, "20240201-");
        keys.set_string(tags::study_instance_uid, vr_type::UI, "");
        const auto outcome = find(*database, qr_model::study_root, query_level::study, keys);
        CHECK(values(outcome, tags::study_instance_uid) == strings{"1.2"});
    }
    SECTION("a list of UIDs selects each study") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1\\1.3\\9.9");
        const auto outcome = find(*database, qr_model::study_root, query_level::study, keys);
        CHECK(values(outcome, tags::study_instance_uid) == strings{"1.1", "1.3"});
    }
    SECTION("a Required Key with no stored value matches any key") {
        dicom_dataset keys;
        keys.set_string(tags::accession_number, vr_type::SH, "ACC-1");
        keys.set_string(tags::study_instance_uid, vr_type::UI, "");
        const auto outcome = find(*database, qr_model::study_root, query_level::study, keys);
        CHECK(values(outcome, tags::study_instance_uid) == strings{"1.1", "1.3"});
    }
    SECTION("SERIES stays inside its study") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        keys.set_string(tags::series_instance_uid, vr_type::UI, "");
        keys.set_string(tags::modality, vr_type::CS, "C*");
        const auto outcome = find(*database, qr_model::study_root, query_level::series, keys);
        CHECK(values(outcome, tags::series_instance_uid) == strings{"1.1.1"});
    }
    SECTION("IMAGE needs its series in its study") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.2");
        keys.set_string(tags::series_instance_uid, vr_type::UI, "1.1.1");
        keys.set_string(tags::sop_instance_uid, vr_type::UI, "");
        const auto outcome = find(*database, qr_model::study_root, query_level::image, keys);
        CHECK(outcome.failure == qr_failure::none);
        CHECK(outcome.matches.empty());
    }
    SECTION("Patient Root keeps a study under its patient") {
        dicom_dataset keys;
        keys.set_string(tags::patient_id, vr_type::LO, "P_2");
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        keys.set_string(tags::series_instance_uid, vr_type::UI, "");
        CHECK(find(*database, qr_model::patient_root, query_level::series, keys).matches.empty());
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.3");
        CHECK(values(find(*database, qr_model::patient_root, query_level::series, keys), tags::series_instance_uid) ==
              strings{"1.3.1"});
    }
    SECTION("a Patient ID above the level is one exact value, not a pattern") {
        dicom_dataset keys;
        keys.set_string(tags::patient_id, vr_type::LO, "P*");
        keys.set_string(tags::study_instance_uid, vr_type::UI, "");
        const auto outcome = find(*database, qr_model::patient_root, query_level::study, keys);
        CHECK(outcome.failure == qr_failure::none);
        CHECK(outcome.matches.empty());
    }
}

TEST_CASE("find_in_index refuses Identifiers without the Unique Keys above the level", "[services][qr][index]") {
    auto database = seeded_database();
    dicom_dataset keys;
    keys.set_string(tags::study_date, vr_type::DA, "20240115");

    SECTION("Patient Root STUDY needs a Patient ID") {
        CHECK(find(*database, qr_model::patient_root, query_level::study, keys).failure == qr_failure::identifier_mismatch);
    }
    SECTION("SERIES needs a Study Instance UID") {
        CHECK(find(*database, qr_model::study_root, query_level::series, keys).failure == qr_failure::identifier_mismatch);
    }
    SECTION("and exactly one") {
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1\\1.2");
        CHECK(find(*database, qr_model::study_root, query_level::series, keys).failure == qr_failure::identifier_mismatch);
    }
    SECTION("IMAGE needs a Series Instance UID") {
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        CHECK(find(*database, qr_model::study_root, query_level::image, keys).failure == qr_failure::identifier_mismatch);
    }
}

TEST_CASE("find_in_index shapes each response", "[services][qr][index]") {
    auto database = seeded_database();

    SECTION("only what was asked for, with level and AE title") {
        dicom_dataset keys;
        keys.set_string(tags::patient_id, vr_type::LO, "P1");
        const auto outcome = find(*database, qr_model::patient_root, query_level::patient, keys);
        REQUIRE(outcome.matches.size() == 1);
        const auto& match = outcome.matches.front();
        CHECK(match.get_string(tags::query_retrieve_level) == "PATIENT");
        CHECK(match.get_string(tags::retrieve_ae_title) == kAe);
        CHECK_FALSE(match.contains(tags::patient_name));
        CHECK_FALSE(match.contains(tags::specific_character_set));
        CHECK_FALSE(outcome.optional_keys_ignored);
    }
    SECTION("a key the index does not hold is ignored, not returned, and reported") {
        dicom_dataset keys;
        keys.set_string(tags::patient_id, vr_type::LO, "P1");
        const core::dicom_tag comments{0x0010, 0x4000};
        keys.set_string(comments, vr_type::LT, "NO SUCH TEXT");
        const auto outcome = find(*database, qr_model::patient_root, query_level::patient, keys);
        REQUIRE(outcome.matches.size() == 1);
        CHECK_FALSE(outcome.matches.front().contains(comments));
        CHECK(outcome.optional_keys_ignored);
    }
    SECTION("non-ASCII text declares its character set") {
        dicom_dataset keys;
        keys.set_string(tags::patient_id, vr_type::LO, "P_2");
        keys.set_string(tags::patient_name, vr_type::PN, "");
        const auto outcome = find(*database, qr_model::patient_root, query_level::patient, keys);
        REQUIRE(outcome.matches.size() == 1);
        CHECK(outcome.matches.front().get_string(tags::specific_character_set) == "ISO_IR 192");
    }
    SECTION("a key in another character set finds the same patient") {
        dicom_dataset keys;
        keys.set_string(tags::specific_character_set, vr_type::CS, "\\ISO 2022 IR 149");
        // ESC $ ) C, then the EUC-KR bytes of the family name.
        keys.set_string(tags::patient_name, vr_type::PN, "\x1B$)C\xC8\xAB*");
        keys.set_string(tags::patient_id, vr_type::LO, "");
        const auto outcome = find(*database, qr_model::patient_root, query_level::patient, keys);
        CHECK(values(outcome, tags::patient_id) == strings{"P_2"});
    }
}

TEST_CASE("hostile key values stay values", "[services][qr][index][security]") {
    auto database = seeded_database();
    const std::string hostile = "1' OR '1'='1";

    SECTION("sql_literal keeps one literal") {
        CHECK(storage::sql_literal("plain") == "'plain'");
        CHECK(storage::sql_literal(hostile) == "'1'' OR ''1''=''1'");
    }
    SECTION("through the query service") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, hostile);
        CHECK(find(*database, qr_model::study_root, query_level::study, keys).matches.empty());
        dicom_dataset names;
        names.set_string(tags::patient_name, vr_type::PN, hostile);
        CHECK(find(*database, qr_model::study_root, query_level::study, names).matches.empty());
    }
    SECTION("through every repository that builds SQL text") {
        storage::study_query study;
        study.study_uid = hostile;
        CHECK(database->search_studies(study).value().empty());
        storage::study_query dated;
        dated.study_date = hostile;
        CHECK(database->search_studies(dated).value().empty());
        storage::study_query named;
        named.patient_name = hostile;
        CHECK(database->search_studies(named).value().empty());
        storage::study_query modality;
        modality.modality = hostile;
        CHECK(database->search_studies(modality).value().empty());
        storage::series_query series;
        series.study_uid = hostile;
        CHECK(database->search_series(series).value().empty());
        storage::instance_query instance;
        instance.series_uid = hostile;
        CHECK(database->search_instances(instance).value().empty());
        CHECK(database->list_series(hostile).value().empty());
        CHECK(database->list_instances(hostile).value().empty());
    }
    SECTION("a statement in a key does not run") {
        dicom_dataset keys;
        keys.set_string(tags::patient_name, vr_type::PN, "x'; DROP TABLE studies; --");
        CHECK(find(*database, qr_model::study_root, query_level::study, keys).matches.empty());
        CHECK(database->search_studies(storage::study_query{}).value().size() == 3);
    }
    SECTION("a quote in a stored value is found by the same value") {
        dicom_dataset keys;
        keys.set_string(tags::study_description, vr_type::LO, "O'BRIEN");
        keys.set_string(tags::study_instance_uid, vr_type::UI, "");
        CHECK(values(find(*database, qr_model::study_root, query_level::study, keys), tags::study_instance_uid) == strings{"1.3"});
    }
}

TEST_CASE("select_from_index follows the Unique Keys", "[services][qr][index]") {
    static std::atomic<int> runs{0};
    const auto root = std::filesystem::temp_directory_path()/("qr_index_test_"+std::to_string(++runs));
    std::filesystem::create_directories(root);
    auto opened = storage::index_database::open(":memory:");
    REQUIRE(opened.is_ok());
    auto database = std::move(opened.value());
    const auto patient = database->upsert_patient("P1", "DOE^JOHN", "", "").value();
    const auto study = database->upsert_study(patient, "1.1", "", "", "", "", "", "").value();
    const auto series = database->upsert_series(study, "1.1.1", "CT", 1, "", "", "").value();
    for (const auto* uid : {"1.1.1.1", "1.1.1.2"}) {
        dicom_dataset object;
        object.set_string(tags::sop_class_uid, vr_type::UI, kCt);
        object.set_string(tags::sop_instance_uid, vr_type::UI, uid);
        const auto path = root/uid;
        REQUIRE(core::dicom_file::create(object, encoding::transfer_syntax::explicit_vr_little_endian).save(path).is_ok());
        REQUIRE(database->upsert_instance(series, uid, kCt, path.string(), 1, "", 1).is_ok());
    }
    const auto select = [&](qr_model model, const char* level, const dicom_dataset& extra) {
        dicom_dataset keys = extra;
        keys.set_string(tags::query_retrieve_level, vr_type::CS, level);
        return select_from_index(*database, retrieve_request{model, keys});
    };

    SECTION("STUDY delivers the study") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        const auto selection = select(qr_model::study_root, "STUDY", keys);
        CHECK(selection.failure == qr_failure::none);
        CHECK(selection.files.size() == 2);
    }
    SECTION("a list repeating a UID stores each object once") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1\\1.1");
        CHECK(select(qr_model::study_root, "STUDY", keys).files.size() == 2);
    }
    SECTION("IMAGE delivers the named object") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        keys.set_string(tags::series_instance_uid, vr_type::UI, "1.1.1");
        keys.set_string(tags::sop_instance_uid, vr_type::UI, "1.1.1.2");
        const auto selection = select(qr_model::study_root, "IMAGE", keys);
        REQUIRE(selection.files.size() == 1);
        CHECK(selection.files.front().dataset().get_string(tags::sop_instance_uid) == "1.1.1.2");
    }
    SECTION("an object that cannot be read is named, not dropped") {
        std::filesystem::remove(root/"1.1.1.2");
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        const auto selection = select(qr_model::study_root, "STUDY", keys);
        CHECK(selection.files.size() == 1);
        CHECK(selection.unavailable_uids == strings{"1.1.1.2"});
    }
    SECTION("Required Keys are ignored") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        keys.set_string(tags::study_date, vr_type::DA, "19000101");
        CHECK(select(qr_model::study_root, "STUDY", keys).files.size() == 2);
    }
    SECTION("an unknown study is an empty success") {
        dicom_dataset keys;
        keys.set_string(tags::study_instance_uid, vr_type::UI, "9.9");
        const auto selection = select(qr_model::study_root, "STUDY", keys);
        CHECK(selection.failure == qr_failure::none);
        CHECK(selection.files.empty());
        CHECK(selection.unavailable_uids.empty());
    }
    SECTION("what cannot name objects is refused") {
        dicom_dataset none;
        CHECK(select(qr_model::study_root, "STUDY", none).failure == qr_failure::identifier_mismatch);
        CHECK(select(qr_model::study_root, "PATIENT", none).failure == qr_failure::identifier_mismatch);
        CHECK(select(qr_model::study_root, "BOGUS", none).failure == qr_failure::identifier_mismatch);
        dicom_dataset two_patients;
        two_patients.set_string(tags::patient_id, vr_type::LO, "P1\\P2");
        CHECK(select(qr_model::patient_root, "PATIENT", two_patients).failure == qr_failure::identifier_mismatch);
        dicom_dataset below;
        below.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        below.set_string(tags::series_instance_uid, vr_type::UI, "1.1.1");
        CHECK(select(qr_model::study_root, "STUDY", below).failure == qr_failure::identifier_mismatch);
        dicom_dataset no_patient;
        no_patient.set_string(tags::study_instance_uid, vr_type::UI, "1.1");
        CHECK(select(qr_model::patient_root, "STUDY", no_patient).failure == qr_failure::identifier_mismatch);
    }
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

TEST_CASE("level_in_model follows Annex C.6", "[services][qr]") {
    CHECK(level_in_model(qr_model::patient_root, query_level::patient));
    CHECK(level_in_model(qr_model::patient_root, query_level::image));
    CHECK_FALSE(level_in_model(qr_model::study_root, query_level::patient));
    CHECK(level_in_model(qr_model::study_root, query_level::study));
    CHECK(level_in_model(qr_model::patient_study_only, query_level::study));
    CHECK_FALSE(level_in_model(qr_model::patient_study_only, query_level::series));
}
