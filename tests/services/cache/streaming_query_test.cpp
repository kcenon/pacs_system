/**
 * @file streaming_query_test.cpp
 * @brief Unit tests for streaming query functionality
 *
 * @note These tests require PACS_WITH_DATABASE_SYSTEM to be defined.
 *       The streaming query functionality depends on the unified database
 *       adapter API from database_system.
 */

#include <pacs/services/cache/database_cursor.hpp>

// Only compile tests when PACS_WITH_DATABASE_SYSTEM is defined
#ifdef PACS_WITH_DATABASE_SYSTEM

#include <pacs/services/cache/query_result_stream.hpp>
#include <pacs/services/cache/streaming_query_handler.hpp>
#include <pacs/services/query_scp.hpp>
#include <pacs/storage/index_database.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>

using namespace pacs::services;
using namespace pacs::storage;
using namespace pacs::core;
using namespace pacs::encoding;

// Helper macro to get cursor handle (always uses db_adapter since we're inside PACS_WITH_DATABASE_SYSTEM)
#define GET_CURSOR_HANDLE(db) (db)->db_adapter()

// ============================================================================
// Test Fixtures
// ============================================================================

namespace {

/**
 * @brief Create a test database with sample data
 */
class test_database_fixture {
public:
    test_database_fixture() {
        // Create in-memory database (schema is auto-initialized)
        auto result = index_database::open(":memory:");
        REQUIRE(result.is_ok());
        db_ = std::move(result.value());

        // Insert test data
        insert_test_data();
    }

    [[nodiscard]] auto db() const -> index_database* {
        return db_.get();
    }

private:
    void insert_test_data() {
        // Insert 10 test patients
        for (int i = 1; i <= 10; ++i) {
            patient_record patient;
            patient.patient_id = "PAT" + std::to_string(i);
            patient.patient_name = "TEST^PATIENT^" + std::to_string(i);
            patient.birth_date = "19800101";
            patient.sex = (i % 2 == 0) ? "M" : "F";

            auto result = db_->upsert_patient(patient);
            REQUIRE(result.is_ok());
            auto patient_pk = result.value();

            // Insert 3 studies per patient
            for (int j = 1; j <= 3; ++j) {
                study_record study;
                study.patient_pk = patient_pk;
                study.study_uid = "1.2.3." + std::to_string(i) + "." + std::to_string(j);
                study.study_id = "STUDY" + std::to_string(j);
                study.study_date = "2024010" + std::to_string(j);
                study.study_time = "120000";
                study.accession_number = "ACC" + std::to_string(i * 10 + j);
                study.modalities_in_study = "CT";

                auto study_result = db_->upsert_study(study);
                REQUIRE(study_result.is_ok());
                auto study_pk = study_result.value();

                // Insert 2 series per study
                for (int k = 1; k <= 2; ++k) {
                    series_record series;
                    series.study_pk = study_pk;
                    series.series_uid = "1.2.3." + std::to_string(i) + "." +
                                        std::to_string(j) + "." + std::to_string(k);
                    series.modality = "CT";
                    series.series_number = k;
                    series.series_description = "Series " + std::to_string(k);

                    auto series_result = db_->upsert_series(series);
                    REQUIRE(series_result.is_ok());
                    auto series_pk = series_result.value();

                    // Insert 5 instances per series
                    for (int l = 1; l <= 5; ++l) {
                        instance_record instance;
                        instance.series_pk = series_pk;
                        instance.sop_uid = "1.2.3." + std::to_string(i) + "." +
                                           std::to_string(j) + "." + std::to_string(k) +
                                           "." + std::to_string(l);
                        instance.sop_class_uid = "1.2.840.10008.5.1.4.1.1.2";  // CT
                        instance.file_path = "/test/path/" + instance.sop_uid + ".dcm";
                        instance.file_size = 1024 * l;
                        instance.instance_number = l;

                        auto instance_result = db_->upsert_instance(instance);
                        REQUIRE(instance_result.is_ok());
                    }
                }
            }
        }
    }

    std::unique_ptr<index_database> db_;
};

}  // namespace

// ============================================================================
// database_cursor Tests
// ============================================================================

TEST_CASE("database_cursor basic operations", "[services][streaming]") {
    test_database_fixture fixture;

    SECTION("create_patient_cursor creates valid cursor") {
        patient_query query;
        auto result =
            database_cursor::create_patient_cursor(GET_CURSOR_HANDLE(fixture.db()), query);

        REQUIRE(result.is_ok());
        auto& cursor = result.value();

        CHECK(cursor->has_more());
        CHECK(cursor->position() == 0);
        CHECK(cursor->type() == database_cursor::record_type::patient);
    }

    SECTION("fetch_next returns records") {
        patient_query query;
        auto result =
            database_cursor::create_patient_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();
        auto record = cursor->fetch_next();

        REQUIRE(record.has_value());
        CHECK(cursor->position() == 1);

        // Verify it's a patient record
        auto* patient = std::get_if<patient_record>(&record.value());
        REQUIRE(patient != nullptr);
        CHECK_FALSE(patient->patient_id.empty());
    }

    SECTION("fetch_batch returns multiple records") {
        patient_query query;
        auto result =
            database_cursor::create_patient_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();
        auto batch = cursor->fetch_batch(5);

        CHECK(batch.size() == 5);
        CHECK(cursor->position() == 5);
    }

    SECTION("cursor exhaustion works correctly") {
        patient_query query;
        auto result =
            database_cursor::create_patient_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();

        // Fetch all 10 patients
        auto batch = cursor->fetch_batch(20);

        CHECK(batch.size() == 10);
        CHECK(cursor->position() == 10);
        CHECK_FALSE(cursor->has_more());
    }

    SECTION("cursor with filter returns filtered results") {
        patient_query query;
        query.sex = "M";

        auto result =
            database_cursor::create_patient_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();
        auto batch = cursor->fetch_batch(20);

        // Should return only male patients (5 out of 10)
        CHECK(batch.size() == 5);

        for (const auto& record : batch) {
            auto* patient = std::get_if<patient_record>(&record);
            REQUIRE(patient != nullptr);
            CHECK(patient->sex == "M");
        }
    }

    SECTION("reset allows re-iteration") {
        patient_query query;
        auto result =
            database_cursor::create_patient_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();

        // Fetch some records
        (void)cursor->fetch_batch(5);
        CHECK(cursor->position() == 5);

        // Reset
        auto reset_result = cursor->reset();
        REQUIRE(reset_result.is_ok());

        CHECK(cursor->position() == 0);
        CHECK(cursor->has_more());

        // Should be able to fetch again from start
        auto batch = cursor->fetch_batch(3);
        CHECK(batch.size() == 3);
        CHECK(cursor->position() == 3);
    }

    SECTION("serialize creates valid state string") {
        patient_query query;
        auto result =
            database_cursor::create_patient_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();
        (void)cursor->fetch_batch(3);

        auto state = cursor->serialize();
        CHECK_FALSE(state.empty());

        // Format should be "type:position"
        CHECK(state.find(':') != std::string::npos);
    }
}

TEST_CASE("database_cursor study queries", "[services][streaming]") {
    test_database_fixture fixture;

    SECTION("create_study_cursor returns all studies") {
        study_query query;
        auto result =
            database_cursor::create_study_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();
        auto batch = cursor->fetch_batch(100);

        // 10 patients * 3 studies = 30 studies
        CHECK(batch.size() == 30);
    }

    SECTION("study query with patient filter") {
        study_query query;
        query.patient_id = "PAT1";

        auto result =
            database_cursor::create_study_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();
        auto batch = cursor->fetch_batch(100);

        CHECK(batch.size() == 3);  // 3 studies for PAT1
    }

    SECTION("study query with date range") {
        study_query query;
        query.study_date_from = "20240101";
        query.study_date_to = "20240102";

        auto result =
            database_cursor::create_study_cursor(GET_CURSOR_HANDLE(fixture.db()), query);
        REQUIRE(result.is_ok());

        auto& cursor = result.value();
        auto batch = cursor->fetch_batch(100);

        // Should only return studies from dates 01 and 02
        CHECK(batch.size() == 20);  // 10 patients * 2 studies
    }
}

// ============================================================================
// query_result_stream Tests
// ============================================================================

TEST_CASE("query_result_stream basic operations", "[services][streaming]") {
    test_database_fixture fixture;

    SECTION("create stream for patient queries") {
        dicom_dataset query_keys;
        query_keys.set_string(tags::query_retrieve_level, vr_type::CS, "PATIENT");

        auto result =
            query_result_stream::create(fixture.db(), query_level::patient, query_keys);

        REQUIRE(result.is_ok());
        auto& stream = result.value();

        CHECK(stream->has_more());
        CHECK(stream->level() == query_level::patient);
    }

    SECTION("next_batch returns DICOM datasets") {
        dicom_dataset query_keys;

        auto result =
            query_result_stream::create(fixture.db(), query_level::patient, query_keys);
        REQUIRE(result.is_ok());

        auto& stream = result.value();
        auto batch = stream->next_batch();

        REQUIRE(batch.has_value());
        CHECK_FALSE(batch->empty());

        // Verify dataset has patient tags
        auto& first = batch->front();
        CHECK_FALSE(first.get_string(tags::patient_id).empty());
        CHECK(first.get_string(tags::query_retrieve_level) == "PATIENT");
    }

    SECTION("stream with query filter") {
        dicom_dataset query_keys;
        query_keys.set_string(tags::patient_sex, vr_type::CS, "F");

        auto result =
            query_result_stream::create(fixture.db(), query_level::patient, query_keys);
        REQUIRE(result.is_ok());

        auto& stream = result.value();

        size_t count = 0;
        while (stream->has_more()) {
            auto batch = stream->next_batch();
            if (batch.has_value()) {
                count += batch->size();
            }
        }

        CHECK(count == 5);  // 5 female patients
    }

    SECTION("cursor() returns serializable state") {
        dicom_dataset query_keys;

        auto result =
            query_result_stream::create(fixture.db(), query_level::patient, query_keys);
        REQUIRE(result.is_ok());

        auto& stream = result.value();
        (void)stream->next_batch();

        auto cursor = stream->cursor();
        CHECK_FALSE(cursor.empty());
    }
}

TEST_CASE("query_result_stream study level", "[services][streaming]") {
    test_database_fixture fixture;

    SECTION("stream returns study datasets") {
        dicom_dataset query_keys;

        stream_config config;
        config.page_size = 10;

        auto result =
            query_result_stream::create(fixture.db(), query_level::study, query_keys, config);
        REQUIRE(result.is_ok());

        auto& stream = result.value();
        auto batch = stream->next_batch();

        REQUIRE(batch.has_value());
        CHECK(batch->size() == 10);

        // Verify dataset has study tags
        auto& first = batch->front();
        CHECK_FALSE(first.get_string(tags::study_instance_uid).empty());
        CHECK(first.get_string(tags::query_retrieve_level) == "STUDY");
    }
}

// ============================================================================
// streaming_query_handler Tests
// ============================================================================

TEST_CASE("streaming_query_handler configuration", "[services][streaming]") {
    test_database_fixture fixture;
    streaming_query_handler handler(fixture.db());

    SECTION("default page size is 100") {
        CHECK(handler.page_size() == 100);
    }

    SECTION("set_page_size updates page size") {
        handler.set_page_size(50);
        CHECK(handler.page_size() == 50);
    }

    SECTION("default max_results is unlimited (0)") {
        CHECK(handler.max_results() == 0);
    }

    SECTION("set_max_results updates limit") {
        handler.set_max_results(500);
        CHECK(handler.max_results() == 500);
    }
}

TEST_CASE("streaming_query_handler create_stream", "[services][streaming]") {
    test_database_fixture fixture;
    streaming_query_handler handler(fixture.db());
    handler.set_page_size(5);

    SECTION("creates functional stream") {
        dicom_dataset query_keys;

        auto result =
            handler.create_stream(query_level::patient, query_keys, "TEST_AE");

        REQUIRE(result.is_ok());
        auto& stream = result.value();

        CHECK(stream->has_more());

        auto batch = stream->next_batch();
        REQUIRE(batch.has_value());
        CHECK(batch->size() == 5);  // page_size = 5
    }
}

TEST_CASE("streaming_query_handler as_query_handler", "[services][streaming]") {
    test_database_fixture fixture;
    streaming_query_handler handler(fixture.db());

    SECTION("returns query_handler compatible function") {
        auto query_fn = handler.as_query_handler();

        dicom_dataset query_keys;
        auto results = query_fn(query_level::patient, query_keys, "TEST_AE");

        CHECK(results.size() == 10);  // All 10 patients
    }

    SECTION("respects max_results limit") {
        handler.set_max_results(5);
        auto query_fn = handler.as_query_handler();

        dicom_dataset query_keys;
        auto results = query_fn(query_level::patient, query_keys, "TEST_AE");

        CHECK(results.size() == 5);  // Limited to 5
    }

    SECTION("can be used with query_scp") {
        query_scp scp;

        auto query_fn = handler.as_query_handler();
        scp.set_handler(query_fn);

        // Handler is set but not called (no mock association)
        CHECK(scp.max_results() == 0);
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("streaming query end-to-end", "[services][streaming][integration]") {
    test_database_fixture fixture;

    SECTION("full pagination workflow") {
        streaming_query_handler handler(fixture.db());
        handler.set_page_size(7);

        dicom_dataset query_keys;
        query_keys.set_string(tags::modality, vr_type::CS, "CT");

        auto stream_result =
            handler.create_stream(query_level::study, query_keys, "TEST_AE");
        REQUIRE(stream_result.is_ok());

        auto stream = std::move(stream_result.value());

        std::vector<std::string> study_uids;
        while (stream->has_more()) {
            auto batch = stream->next_batch();
            if (!batch.has_value()) {
                break;
            }

            for (const auto& ds : batch.value()) {
                auto uid = ds.get_string(tags::study_instance_uid);
                study_uids.push_back(std::string(uid));
            }
        }

        // All 30 studies (10 patients * 3 studies)
        CHECK(study_uids.size() == 30);

        // Verify uniqueness
        std::sort(study_uids.begin(), study_uids.end());
        auto last = std::unique(study_uids.begin(), study_uids.end());
        CHECK(std::distance(study_uids.begin(), last) == 30);
    }

    SECTION("wildcard query filtering") {
        streaming_query_handler handler(fixture.db());

        dicom_dataset query_keys;
        query_keys.set_string(tags::patient_name, vr_type::PN, "TEST^PATIENT^1*");

        auto stream_result =
            handler.create_stream(query_level::patient, query_keys, "TEST_AE");
        REQUIRE(stream_result.is_ok());

        auto stream = std::move(stream_result.value());
        size_t count = 0;

        while (stream->has_more()) {
            auto batch = stream->next_batch();
            if (batch.has_value()) {
                count += batch->size();
            }
        }

        // PAT1 and PAT10 match "TEST^PATIENT^1*"
        CHECK(count == 2);
    }
}

#endif  // PACS_WITH_DATABASE_SYSTEM
