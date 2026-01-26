/**
 * @file patient_study_endpoints_test.cpp
 * @brief Unit tests for patient and study API endpoints
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/storage/index_database.hpp"
#include "pacs/storage/patient_record.hpp"
#include "pacs/storage/series_record.hpp"
#include "pacs/storage/study_record.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_types.hpp"

using namespace pacs::storage;
using namespace pacs::web;

TEST_CASE("Patient query structure", "[web][patient]") {
  patient_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("with patient_id") {
    query.patient_id = "12345";
    REQUIRE(query.has_criteria());
  }

  SECTION("with patient_name") {
    query.patient_name = "Doe*";
    REQUIRE(query.has_criteria());
  }

  SECTION("with birth_date range") {
    query.birth_date_from = "19800101";
    query.birth_date_to = "19901231";
    REQUIRE(query.has_criteria());
  }

  SECTION("with sex") {
    query.sex = "M";
    REQUIRE(query.has_criteria());
  }
}

TEST_CASE("Study query structure", "[web][study]") {
  study_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("with patient_id") {
    query.patient_id = "12345";
    REQUIRE(query.has_criteria());
  }

  SECTION("with study_date range") {
    query.study_date_from = "20230101";
    query.study_date_to = "20231231";
    REQUIRE(query.has_criteria());
  }

  SECTION("with modality") {
    query.modality = "CT";
    REQUIRE(query.has_criteria());
  }

  SECTION("with accession_number") {
    query.accession_number = "ACC*";
    REQUIRE(query.has_criteria());
  }
}

TEST_CASE("Series query structure", "[web][series]") {
  series_query query;

  SECTION("default values") {
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
    REQUIRE_FALSE(query.has_criteria());
  }

  SECTION("with study_uid") {
    query.study_uid = "1.2.840.123456";
    REQUIRE(query.has_criteria());
  }

  SECTION("with modality") {
    query.modality = "MR";
    REQUIRE(query.has_criteria());
  }

  SECTION("with body_part_examined") {
    query.body_part_examined = "HEAD";
    REQUIRE(query.has_criteria());
  }
}

TEST_CASE("Patient record validation", "[web][patient]") {
  patient_record patient;

  SECTION("invalid when patient_id is empty") {
    REQUIRE_FALSE(patient.is_valid());
  }

  SECTION("valid when patient_id is set") {
    patient.patient_id = "12345";
    REQUIRE(patient.is_valid());
  }
}

TEST_CASE("Study record validation", "[web][study]") {
  study_record study;

  SECTION("invalid when study_uid is empty") {
    REQUIRE_FALSE(study.is_valid());
  }

  SECTION("valid when study_uid is set") {
    study.study_uid = "1.2.840.123456.789";
    REQUIRE(study.is_valid());
  }
}

TEST_CASE("Series record validation", "[web][series]") {
  series_record series;

  SECTION("invalid when series_uid is empty") {
    REQUIRE_FALSE(series.is_valid());
  }

  SECTION("valid when series_uid is set") {
    series.series_uid = "1.2.840.123456.789.1";
    REQUIRE(series.is_valid());
  }
}

TEST_CASE("Index database patient operations", "[web][patient][database]") {
  // Open in-memory database for testing
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  SECTION("insert and find patient") {
    auto pk_result = db->upsert_patient("P001", "Doe^John", "19800115", "M");
    REQUIRE(pk_result.is_ok());
    REQUIRE(pk_result.value() > 0);

    auto patient = db->find_patient("P001");
    REQUIRE(patient.has_value());
    REQUIRE(patient->patient_id == "P001");
    REQUIRE(patient->patient_name == "Doe^John");
    REQUIRE(patient->birth_date == "19800115");
    REQUIRE(patient->sex == "M");
  }

  SECTION("search patients with wildcard") {
    (void)db->upsert_patient("P001", "Doe^John", "19800115", "M");
    (void)db->upsert_patient("P002", "Doe^Jane", "19850220", "F");
    (void)db->upsert_patient("P003", "Smith^Bob", "19900305", "M");

    patient_query query;
    query.patient_name = "Doe*";
    auto results_result = db->search_patients(query);
    REQUIRE(results_result.is_ok());
    REQUIRE(results_result.value().size() == 2);
  }

  SECTION("search patients with pagination") {
    for (int i = 1; i <= 10; ++i) {
      (void)db->upsert_patient("P" + std::to_string(i), "Patient" + std::to_string(i),
                         "", "");
    }

    patient_query query;
    query.limit = 5;
    query.offset = 0;
    auto page1_result = db->search_patients(query);
    REQUIRE(page1_result.is_ok());
    REQUIRE(page1_result.value().size() == 5);

    query.offset = 5;
    auto page2_result = db->search_patients(query);
    REQUIRE(page2_result.is_ok());
    REQUIRE(page2_result.value().size() == 5);
  }
}

TEST_CASE("Index database study operations", "[web][study][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  // Create patient first
  auto patient_pk = db->upsert_patient("P001", "Doe^John", "19800115", "M");
  REQUIRE(patient_pk.is_ok());

  SECTION("insert and find study") {
    auto study_pk = db->upsert_study(patient_pk.value(), "1.2.840.123456",
                                     "S001", "20231015", "103000", "ACC001",
                                     "Dr. Smith", "CT HEAD");
    REQUIRE(study_pk.is_ok());

    auto study = db->find_study("1.2.840.123456");
    REQUIRE(study.has_value());
    REQUIRE(study->study_uid == "1.2.840.123456");
    REQUIRE(study->study_id == "S001");
    REQUIRE(study->accession_number == "ACC001");
  }

  SECTION("list studies for patient") {
    (void)db->upsert_study(patient_pk.value(), "1.2.840.1", "S001", "20231001", "",
                     "ACC001", "", "Study 1");
    (void)db->upsert_study(patient_pk.value(), "1.2.840.2", "S002", "20231002", "",
                     "ACC002", "", "Study 2");

    auto studies_result = db->list_studies("P001");
    REQUIRE(studies_result.is_ok());
    REQUIRE(studies_result.value().size() == 2);
  }

  SECTION("delete study") {
    (void)db->upsert_study(patient_pk.value(), "1.2.840.delete", "S001", "", "", "",
                     "", "");

    auto result = db->delete_study("1.2.840.delete");
    REQUIRE(result.is_ok());

    auto study = db->find_study("1.2.840.delete");
    REQUIRE_FALSE(study.has_value());
  }
}

TEST_CASE("Index database series operations", "[web][series][database]") {
  auto db_result = index_database::open(":memory:");
  REQUIRE(db_result.is_ok());
  auto &db = db_result.value();

  // Create patient and study first
  auto patient_pk = db->upsert_patient("P001", "Doe^John", "", "");
  REQUIRE(patient_pk.is_ok());

  auto study_pk = db->upsert_study(patient_pk.value(), "1.2.840.study", "", "",
                                   "", "", "", "");
  REQUIRE(study_pk.is_ok());

  SECTION("insert and find series") {
    auto series_pk =
        db->upsert_series(study_pk.value(), "1.2.840.series", "CT", 1,
                          "CT Head", "HEAD", "CT-Scanner-1");
    REQUIRE(series_pk.is_ok());

    auto series = db->find_series("1.2.840.series");
    REQUIRE(series.has_value());
    REQUIRE(series->series_uid == "1.2.840.series");
    REQUIRE(series->modality == "CT");
    REQUIRE(series->series_number.value_or(-1) == 1);
  }

  SECTION("list series for study") {
    (void)db->upsert_series(study_pk.value(), "1.2.840.series1", "CT", 1, "", "", "");
    (void)db->upsert_series(study_pk.value(), "1.2.840.series2", "CT", 2, "", "", "");

    auto series_list_result = db->list_series("1.2.840.study");
    REQUIRE(series_list_result.is_ok());
    REQUIRE(series_list_result.value().size() == 2);
  }
}
