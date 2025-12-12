/**
 * @file dicomweb_endpoints_test.cpp
 * @brief Unit tests for DICOMweb (WADO-RS) API utilities
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "pacs/web/endpoints/dicomweb_endpoints.hpp"
#include "pacs/storage/study_record.hpp"
#include "pacs/storage/series_record.hpp"
#include "pacs/storage/instance_record.hpp"

using namespace pacs::web;
using namespace pacs::web::dicomweb;

// ============================================================================
// Accept Header Parsing Tests
// ============================================================================

TEST_CASE("parse_accept_header - empty header", "[dicomweb][accept]") {
    auto result = parse_accept_header("");

    REQUIRE(result.size() == 1);
    REQUIRE(result[0].media_type == "application/dicom");
    REQUIRE(result[0].quality == 1.0f);
}

TEST_CASE("parse_accept_header - single media type", "[dicomweb][accept]") {
    auto result = parse_accept_header("application/dicom+json");

    REQUIRE(result.size() == 1);
    REQUIRE(result[0].media_type == "application/dicom+json");
    REQUIRE(result[0].quality == 1.0f);
}

TEST_CASE("parse_accept_header - with quality", "[dicomweb][accept]") {
    auto result = parse_accept_header("application/dicom;q=0.8");

    REQUIRE(result.size() == 1);
    REQUIRE(result[0].media_type == "application/dicom");
    REQUIRE(result[0].quality == Catch::Approx(0.8f));
}

TEST_CASE("parse_accept_header - multiple types sorted by quality",
          "[dicomweb][accept]") {
    auto result = parse_accept_header(
        "application/dicom;q=0.5, application/dicom+json;q=1.0, */*;q=0.1");

    REQUIRE(result.size() == 3);
    // Should be sorted by quality (descending)
    REQUIRE(result[0].media_type == "application/dicom+json");
    REQUIRE(result[0].quality == Catch::Approx(1.0f));
    REQUIRE(result[1].media_type == "application/dicom");
    REQUIRE(result[1].quality == Catch::Approx(0.5f));
    REQUIRE(result[2].media_type == "*/*");
    REQUIRE(result[2].quality == Catch::Approx(0.1f));
}

TEST_CASE("parse_accept_header - with transfer-syntax parameter",
          "[dicomweb][accept]") {
    auto result = parse_accept_header(
        "application/dicom;transfer-syntax=\"1.2.840.10008.1.2.1\"");

    REQUIRE(result.size() == 1);
    REQUIRE(result[0].media_type == "application/dicom");
    REQUIRE(result[0].transfer_syntax == "1.2.840.10008.1.2.1");
}

// ============================================================================
// is_acceptable Tests
// ============================================================================

TEST_CASE("is_acceptable - empty accept list accepts all",
          "[dicomweb][accept]") {
    std::vector<accept_info> empty;

    REQUIRE(is_acceptable(empty, "application/dicom") == true);
    REQUIRE(is_acceptable(empty, "application/dicom+json") == true);
    REQUIRE(is_acceptable(empty, "image/jpeg") == true);
}

TEST_CASE("is_acceptable - exact match", "[dicomweb][accept]") {
    auto infos = parse_accept_header("application/dicom+json");

    REQUIRE(is_acceptable(infos, "application/dicom+json") == true);
    REQUIRE(is_acceptable(infos, "application/dicom") == false);
}

TEST_CASE("is_acceptable - wildcard match", "[dicomweb][accept]") {
    auto infos = parse_accept_header("*/*");

    REQUIRE(is_acceptable(infos, "application/dicom") == true);
    REQUIRE(is_acceptable(infos, "application/dicom+json") == true);
    REQUIRE(is_acceptable(infos, "image/jpeg") == true);
}

TEST_CASE("is_acceptable - type wildcard", "[dicomweb][accept]") {
    auto infos = parse_accept_header("application/*");

    REQUIRE(is_acceptable(infos, "application/dicom") == true);
    REQUIRE(is_acceptable(infos, "application/dicom+json") == true);
    REQUIRE(is_acceptable(infos, "image/jpeg") == false);
}

// ============================================================================
// Multipart Builder Tests
// ============================================================================

TEST_CASE("multipart_builder - empty builder", "[dicomweb][multipart]") {
    multipart_builder builder;

    REQUIRE(builder.empty() == true);
    REQUIRE(builder.size() == 0);
}

TEST_CASE("multipart_builder - add single part", "[dicomweb][multipart]") {
    multipart_builder builder;

    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    builder.add_part(data);

    REQUIRE(builder.empty() == false);
    REQUIRE(builder.size() == 1);
}

TEST_CASE("multipart_builder - add multiple parts", "[dicomweb][multipart]") {
    multipart_builder builder;

    std::vector<uint8_t> data1 = {0x01, 0x02};
    std::vector<uint8_t> data2 = {0x03, 0x04};
    std::vector<uint8_t> data3 = {0x05, 0x06};

    builder.add_part(data1);
    builder.add_part(data2);
    builder.add_part(data3);

    REQUIRE(builder.size() == 3);
}

TEST_CASE("multipart_builder - content type header format",
          "[dicomweb][multipart]") {
    multipart_builder builder;

    auto content_type = builder.content_type_header();

    REQUIRE(content_type.find("multipart/related") != std::string::npos);
    REQUIRE(content_type.find("type=\"application/dicom\"") != std::string::npos);
    REQUIRE(content_type.find("boundary=") != std::string::npos);
}

TEST_CASE("multipart_builder - custom content type", "[dicomweb][multipart]") {
    multipart_builder builder("application/dicom+json");

    auto content_type = builder.content_type_header();

    REQUIRE(content_type.find("type=\"application/dicom+json\"") !=
            std::string::npos);
}

TEST_CASE("multipart_builder - build output format", "[dicomweb][multipart]") {
    multipart_builder builder;

    std::vector<uint8_t> data = {'T', 'E', 'S', 'T'};
    builder.add_part(data);

    auto output = builder.build();
    auto boundary = std::string(builder.boundary());

    // Check multipart structure
    REQUIRE(output.find("--" + boundary) != std::string::npos);
    REQUIRE(output.find("Content-Type: application/dicom") != std::string::npos);
    REQUIRE(output.find("--" + boundary + "--") != std::string::npos);
    REQUIRE(output.find("TEST") != std::string::npos);
}

TEST_CASE("multipart_builder - part with location", "[dicomweb][multipart]") {
    multipart_builder builder;

    std::vector<uint8_t> data = {'D', 'A', 'T', 'A'};
    builder.add_part_with_location(data, "/dicomweb/studies/1.2.3/instances/4.5.6");

    auto output = builder.build();

    REQUIRE(output.find("Content-Location: /dicomweb/studies/1.2.3/instances/4.5.6") !=
            std::string::npos);
}

// ============================================================================
// Bulk Data Tag Tests
// ============================================================================

TEST_CASE("is_bulk_data_tag - pixel data", "[dicomweb][bulkdata]") {
    REQUIRE(is_bulk_data_tag(0x7FE00010) == true);  // Pixel Data
    REQUIRE(is_bulk_data_tag(0x7FE00008) == true);  // Float Pixel Data
    REQUIRE(is_bulk_data_tag(0x7FE00009) == true);  // Double Float Pixel Data
}

TEST_CASE("is_bulk_data_tag - encapsulated document", "[dicomweb][bulkdata]") {
    REQUIRE(is_bulk_data_tag(0x00420011) == true);  // Encapsulated Document
}

TEST_CASE("is_bulk_data_tag - regular tags", "[dicomweb][bulkdata]") {
    REQUIRE(is_bulk_data_tag(0x00100010) == false);  // Patient Name
    REQUIRE(is_bulk_data_tag(0x00100020) == false);  // Patient ID
    REQUIRE(is_bulk_data_tag(0x00080018) == false);  // SOP Instance UID
}

TEST_CASE("is_bulk_data_tag - audio sample data range",
          "[dicomweb][bulkdata]") {
    // Audio Sample Data is in group 0x50xx
    REQUIRE(is_bulk_data_tag(0x50003000) == true);
    REQUIRE(is_bulk_data_tag(0x50FF3000) == true);
    REQUIRE(is_bulk_data_tag(0x50003001) == false);  // Wrong element
}

// ============================================================================
// Media Type Constants Tests
// ============================================================================

TEST_CASE("media_type constants", "[dicomweb][constants]") {
    REQUIRE(media_type::dicom == "application/dicom");
    REQUIRE(media_type::dicom_json == "application/dicom+json");
    REQUIRE(media_type::dicom_xml == "application/dicom+xml");
    REQUIRE(media_type::octet_stream == "application/octet-stream");
    REQUIRE(media_type::jpeg == "image/jpeg");
    REQUIRE(media_type::png == "image/png");
    REQUIRE(media_type::multipart_related == "multipart/related");
}

// ============================================================================
// STOW-RS Multipart Parser Tests
// ============================================================================

TEST_CASE("multipart_parser::extract_boundary - simple boundary",
          "[dicomweb][stowrs][parser]") {
    auto boundary = multipart_parser::extract_boundary(
        "multipart/related; boundary=----=_Part_123");

    REQUIRE(boundary.has_value());
    REQUIRE(*boundary == "----=_Part_123");
}

TEST_CASE("multipart_parser::extract_boundary - quoted boundary",
          "[dicomweb][stowrs][parser]") {
    auto boundary = multipart_parser::extract_boundary(
        "multipart/related; type=\"application/dicom\"; boundary=\"----=_Part_456\"");

    REQUIRE(boundary.has_value());
    REQUIRE(*boundary == "----=_Part_456");
}

TEST_CASE("multipart_parser::extract_boundary - no boundary",
          "[dicomweb][stowrs][parser]") {
    auto boundary = multipart_parser::extract_boundary(
        "multipart/related; type=\"application/dicom\"");

    REQUIRE(!boundary.has_value());
}

TEST_CASE("multipart_parser::extract_type - quoted type",
          "[dicomweb][stowrs][parser]") {
    auto type = multipart_parser::extract_type(
        "multipart/related; type=\"application/dicom\"; boundary=xyz");

    REQUIRE(type.has_value());
    REQUIRE(*type == "application/dicom");
}

TEST_CASE("multipart_parser::extract_type - no type",
          "[dicomweb][stowrs][parser]") {
    auto type = multipart_parser::extract_type(
        "multipart/related; boundary=xyz");

    REQUIRE(!type.has_value());
}

TEST_CASE("multipart_parser::parse - single part",
          "[dicomweb][stowrs][parser]") {
    std::string content_type = "multipart/related; boundary=BOUNDARY123";
    std::string body =
        "--BOUNDARY123\r\n"
        "Content-Type: application/dicom\r\n"
        "\r\n"
        "DICOM_DATA_HERE\r\n"
        "--BOUNDARY123--\r\n";

    auto result = multipart_parser::parse(content_type, body);

    REQUIRE(result.success());
    REQUIRE(result.parts.size() == 1);
    REQUIRE(result.parts[0].content_type == "application/dicom");
    REQUIRE(result.parts[0].data.size() == 15);  // "DICOM_DATA_HERE"
}

TEST_CASE("multipart_parser::parse - multiple parts",
          "[dicomweb][stowrs][parser]") {
    std::string content_type = "multipart/related; boundary=BOUNDARY";
    std::string body =
        "--BOUNDARY\r\n"
        "Content-Type: application/dicom\r\n"
        "\r\n"
        "PART1\r\n"
        "--BOUNDARY\r\n"
        "Content-Type: application/dicom\r\n"
        "\r\n"
        "PART2\r\n"
        "--BOUNDARY\r\n"
        "Content-Type: application/dicom\r\n"
        "\r\n"
        "PART3\r\n"
        "--BOUNDARY--\r\n";

    auto result = multipart_parser::parse(content_type, body);

    REQUIRE(result.success());
    REQUIRE(result.parts.size() == 3);
}

TEST_CASE("multipart_parser::parse - with content-location header",
          "[dicomweb][stowrs][parser]") {
    std::string content_type = "multipart/related; boundary=BOUND";
    std::string body =
        "--BOUND\r\n"
        "Content-Type: application/dicom\r\n"
        "Content-Location: /studies/1.2.3/instances/4.5.6\r\n"
        "\r\n"
        "DATA\r\n"
        "--BOUND--\r\n";

    auto result = multipart_parser::parse(content_type, body);

    REQUIRE(result.success());
    REQUIRE(result.parts.size() == 1);
    REQUIRE(result.parts[0].content_location == "/studies/1.2.3/instances/4.5.6");
}

TEST_CASE("multipart_parser::parse - invalid boundary",
          "[dicomweb][stowrs][parser]") {
    std::string content_type = "multipart/related; type=\"application/dicom\"";
    std::string body = "some data";

    auto result = multipart_parser::parse(content_type, body);

    REQUIRE(!result.success());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->code == "INVALID_BOUNDARY");
}

TEST_CASE("multipart_parser::parse - no parts found",
          "[dicomweb][stowrs][parser]") {
    std::string content_type = "multipart/related; boundary=MISSING";
    std::string body = "no boundaries here";

    auto result = multipart_parser::parse(content_type, body);

    REQUIRE(!result.success());
    REQUIRE(result.error.has_value());
    REQUIRE(result.error->code == "NO_PARTS");
}

TEST_CASE("multipart_parser::parse - case insensitive header names",
          "[dicomweb][stowrs][parser]") {
    std::string content_type = "multipart/related; boundary=B";
    std::string body =
        "--B\r\n"
        "content-type: application/dicom\r\n"
        "CONTENT-LOCATION: /some/path\r\n"
        "\r\n"
        "DATA\r\n"
        "--B--\r\n";

    auto result = multipart_parser::parse(content_type, body);

    REQUIRE(result.success());
    REQUIRE(result.parts.size() == 1);
    REQUIRE(result.parts[0].content_type == "application/dicom");
    REQUIRE(result.parts[0].content_location == "/some/path");
}

// ============================================================================
// STOW-RS Store Response Tests
// ============================================================================

TEST_CASE("store_response - all success", "[dicomweb][stowrs][response]") {
    store_response response;
    response.referenced_instances.push_back({
        true, "1.2.3", "4.5.6", "/dicomweb/studies/...", std::nullopt, std::nullopt
    });

    REQUIRE(response.all_success() == true);
    REQUIRE(response.all_failed() == false);
    REQUIRE(response.partial_success() == false);
}

TEST_CASE("store_response - all failed", "[dicomweb][stowrs][response]") {
    store_response response;
    response.failed_instances.push_back({
        false, "1.2.3", "4.5.6", "", "ERROR", "Some error"
    });

    REQUIRE(response.all_success() == false);
    REQUIRE(response.all_failed() == true);
    REQUIRE(response.partial_success() == false);
}

TEST_CASE("store_response - partial success", "[dicomweb][stowrs][response]") {
    store_response response;
    response.referenced_instances.push_back({
        true, "1.2.3", "4.5.6", "/dicomweb/studies/...", std::nullopt, std::nullopt
    });
    response.failed_instances.push_back({
        false, "1.2.3", "7.8.9", "", "DUPLICATE", "Already exists"
    });

    REQUIRE(response.all_success() == false);
    REQUIRE(response.all_failed() == false);
    REQUIRE(response.partial_success() == true);
}

TEST_CASE("build_store_response_json - success response",
          "[dicomweb][stowrs][response]") {
    store_response response;
    response.referenced_instances.push_back({
        true,
        "1.2.840.10008.5.1.4.1.1.2",  // CT Image Storage
        "1.2.3.4.5.6.7.8.9",
        "/dicomweb/studies/1.2.3/series/4.5.6/instances/1.2.3.4.5.6.7.8.9",
        std::nullopt,
        std::nullopt
    });

    auto json = build_store_response_json(response, "http://example.com");

    // Check for Referenced SOP Sequence
    REQUIRE(json.find("00081199") != std::string::npos);
    // Check for Referenced SOP Class UID
    REQUIRE(json.find("00081150") != std::string::npos);
    REQUIRE(json.find("1.2.840.10008.5.1.4.1.1.2") != std::string::npos);
    // Check for Referenced SOP Instance UID
    REQUIRE(json.find("00081155") != std::string::npos);
    REQUIRE(json.find("1.2.3.4.5.6.7.8.9") != std::string::npos);
    // Check for Retrieve URL
    REQUIRE(json.find("00081190") != std::string::npos);
}

TEST_CASE("build_store_response_json - failure response",
          "[dicomweb][stowrs][response]") {
    store_response response;
    response.failed_instances.push_back({
        false,
        "1.2.840.10008.5.1.4.1.1.2",
        "1.2.3.4.5.6.7.8.9",
        "",
        "DUPLICATE",
        "Instance already exists"
    });

    auto json = build_store_response_json(response, "");

    // Check for Failed SOP Sequence
    REQUIRE(json.find("00081198") != std::string::npos);
    // Check for Failure Reason (273 = Duplicate)
    REQUIRE(json.find("00081197") != std::string::npos);
    REQUIRE(json.find("273") != std::string::npos);
}

// ============================================================================
// STOW-RS Validation Result Tests
// ============================================================================

TEST_CASE("validation_result - ok result", "[dicomweb][stowrs][validation]") {
    auto result = validation_result::ok();

    REQUIRE(result.valid == true);
    REQUIRE(static_cast<bool>(result) == true);
    REQUIRE(result.error_code.empty());
}

TEST_CASE("validation_result - error result", "[dicomweb][stowrs][validation]") {
    auto result = validation_result::error("MISSING_TAG", "Required tag missing");

    REQUIRE(result.valid == false);
    REQUIRE(static_cast<bool>(result) == false);
    REQUIRE(result.error_code == "MISSING_TAG");
    REQUIRE(result.error_message == "Required tag missing");
}

// ============================================================================
// QIDO-RS Query Parameter Parsing Tests
// ============================================================================

TEST_CASE("parse_study_query_params - empty params", "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("");

    REQUIRE(!query.patient_id.has_value());
    REQUIRE(!query.study_uid.has_value());
    REQUIRE(query.limit == 0);
    REQUIRE(query.offset == 0);
}

TEST_CASE("parse_study_query_params - PatientID", "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?PatientID=12345");

    REQUIRE(query.patient_id.has_value());
    REQUIRE(*query.patient_id == "12345");
}

TEST_CASE("parse_study_query_params - PatientName with wildcard",
          "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?PatientName=DOE*");

    REQUIRE(query.patient_name.has_value());
    REQUIRE(*query.patient_name == "DOE*");
}

TEST_CASE("parse_study_query_params - StudyDate exact", "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?StudyDate=20231215");

    REQUIRE(query.study_date.has_value());
    REQUIRE(*query.study_date == "20231215");
    REQUIRE(!query.study_date_from.has_value());
    REQUIRE(!query.study_date_to.has_value());
}

TEST_CASE("parse_study_query_params - StudyDate range", "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?StudyDate=20230101-20231231");

    REQUIRE(!query.study_date.has_value());
    REQUIRE(query.study_date_from.has_value());
    REQUIRE(*query.study_date_from == "20230101");
    REQUIRE(query.study_date_to.has_value());
    REQUIRE(*query.study_date_to == "20231231");
}

TEST_CASE("parse_study_query_params - StudyDate from only",
          "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?StudyDate=20230101-");

    REQUIRE(query.study_date_from.has_value());
    REQUIRE(*query.study_date_from == "20230101");
    REQUIRE(!query.study_date_to.has_value());
}

TEST_CASE("parse_study_query_params - StudyDate to only",
          "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?StudyDate=-20231231");

    REQUIRE(!query.study_date_from.has_value());
    REQUIRE(query.study_date_to.has_value());
    REQUIRE(*query.study_date_to == "20231231");
}

TEST_CASE("parse_study_query_params - multiple params",
          "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params(
        "?PatientID=12345&ModalitiesInStudy=CT&limit=50&offset=10");

    REQUIRE(query.patient_id.has_value());
    REQUIRE(*query.patient_id == "12345");
    REQUIRE(query.modality.has_value());
    REQUIRE(*query.modality == "CT");
    REQUIRE(query.limit == 50);
    REQUIRE(query.offset == 10);
}

TEST_CASE("parse_study_query_params - DICOM tag format",
          "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?00100020=PATIENT123&0020000D=1.2.3.4");

    REQUIRE(query.patient_id.has_value());
    REQUIRE(*query.patient_id == "PATIENT123");
    REQUIRE(query.study_uid.has_value());
    REQUIRE(*query.study_uid == "1.2.3.4");
}

TEST_CASE("parse_study_query_params - URL encoded values",
          "[dicomweb][qidors][query]") {
    auto query = parse_study_query_params("?PatientName=DOE%5EJOHN");

    REQUIRE(query.patient_name.has_value());
    REQUIRE(*query.patient_name == "DOE^JOHN");
}

TEST_CASE("parse_series_query_params - basic params", "[dicomweb][qidors][query]") {
    auto query = parse_series_query_params(
        "?Modality=CT&SeriesNumber=1&limit=25");

    REQUIRE(query.modality.has_value());
    REQUIRE(*query.modality == "CT");
    REQUIRE(query.series_number.has_value());
    REQUIRE(*query.series_number == 1);
    REQUIRE(query.limit == 25);
}

TEST_CASE("parse_series_query_params - DICOM tag format",
          "[dicomweb][qidors][query]") {
    auto query = parse_series_query_params("?00080060=MR&0020000E=1.2.3.4.5");

    REQUIRE(query.modality.has_value());
    REQUIRE(*query.modality == "MR");
    REQUIRE(query.series_uid.has_value());
    REQUIRE(*query.series_uid == "1.2.3.4.5");
}

TEST_CASE("parse_instance_query_params - basic params",
          "[dicomweb][qidors][query]") {
    auto query = parse_instance_query_params(
        "?SOPClassUID=1.2.840.10008.5.1.4.1.1.2&InstanceNumber=5");

    REQUIRE(query.sop_class_uid.has_value());
    REQUIRE(*query.sop_class_uid == "1.2.840.10008.5.1.4.1.1.2");
    REQUIRE(query.instance_number.has_value());
    REQUIRE(*query.instance_number == 5);
}

TEST_CASE("parse_instance_query_params - DICOM tag format",
          "[dicomweb][qidors][query]") {
    auto query = parse_instance_query_params("?00080018=1.2.3.4.5.6.7.8.9");

    REQUIRE(query.sop_uid.has_value());
    REQUIRE(*query.sop_uid == "1.2.3.4.5.6.7.8.9");
}

// ============================================================================
// QIDO-RS DicomJSON Response Building Tests
// ============================================================================

TEST_CASE("study_record_to_dicom_json - basic record",
          "[dicomweb][qidors][json]") {
    pacs::storage::study_record record;
    record.study_uid = "1.2.3.4.5.6.7";
    record.study_date = "20231215";
    record.study_time = "143025";
    record.accession_number = "ACC123";
    record.study_description = "CT Chest";
    record.num_series = 3;
    record.num_instances = 150;

    auto json = study_record_to_dicom_json(record, "PAT001", "DOE^JOHN");

    // Check Study Instance UID
    REQUIRE(json.find("0020000D") != std::string::npos);
    REQUIRE(json.find("1.2.3.4.5.6.7") != std::string::npos);

    // Check Study Date
    REQUIRE(json.find("00080020") != std::string::npos);
    REQUIRE(json.find("20231215") != std::string::npos);

    // Check Patient ID
    REQUIRE(json.find("00100020") != std::string::npos);
    REQUIRE(json.find("PAT001") != std::string::npos);

    // Check Patient Name (PN VR format)
    REQUIRE(json.find("00100010") != std::string::npos);
    REQUIRE(json.find("Alphabetic") != std::string::npos);
    REQUIRE(json.find("DOE^JOHN") != std::string::npos);

    // Check Number of Series
    REQUIRE(json.find("00201206") != std::string::npos);
    REQUIRE(json.find("3") != std::string::npos);

    // Check Number of Instances
    REQUIRE(json.find("00201208") != std::string::npos);
    REQUIRE(json.find("150") != std::string::npos);
}

TEST_CASE("study_record_to_dicom_json - with modalities",
          "[dicomweb][qidors][json]") {
    pacs::storage::study_record record;
    record.study_uid = "1.2.3";
    record.modalities_in_study = "CT\\MR\\US";
    record.num_series = 0;
    record.num_instances = 0;

    auto json = study_record_to_dicom_json(record, "", "");

    // Check Modalities in Study
    REQUIRE(json.find("00080061") != std::string::npos);
    REQUIRE(json.find("CT") != std::string::npos);
    REQUIRE(json.find("MR") != std::string::npos);
    REQUIRE(json.find("US") != std::string::npos);
}

TEST_CASE("series_record_to_dicom_json - basic record",
          "[dicomweb][qidors][json]") {
    pacs::storage::series_record record;
    record.series_uid = "1.2.3.4.5.6.7.8";
    record.modality = "CT";
    record.series_number = 1;
    record.series_description = "Axial";
    record.body_part_examined = "CHEST";
    record.num_instances = 50;

    auto json = series_record_to_dicom_json(record, "1.2.3.4.5.6.7");

    // Check Series Instance UID
    REQUIRE(json.find("0020000E") != std::string::npos);
    REQUIRE(json.find("1.2.3.4.5.6.7.8") != std::string::npos);

    // Check Study Instance UID
    REQUIRE(json.find("0020000D") != std::string::npos);
    REQUIRE(json.find("1.2.3.4.5.6.7") != std::string::npos);

    // Check Modality
    REQUIRE(json.find("00080060") != std::string::npos);
    REQUIRE(json.find("\"CT\"") != std::string::npos);

    // Check Series Number
    REQUIRE(json.find("00200011") != std::string::npos);

    // Check Body Part Examined
    REQUIRE(json.find("00180015") != std::string::npos);
    REQUIRE(json.find("CHEST") != std::string::npos);

    // Check Number of Instances
    REQUIRE(json.find("00201209") != std::string::npos);
    REQUIRE(json.find("50") != std::string::npos);
}

TEST_CASE("instance_record_to_dicom_json - basic record",
          "[dicomweb][qidors][json]") {
    pacs::storage::instance_record record;
    record.sop_uid = "1.2.3.4.5.6.7.8.9";
    record.sop_class_uid = "1.2.840.10008.5.1.4.1.1.2";
    record.instance_number = 10;
    record.rows = 512;
    record.columns = 512;
    record.number_of_frames = 1;

    auto json = instance_record_to_dicom_json(record, "1.2.3.4.5.6.7.8", "1.2.3.4.5.6.7");

    // Check SOP Instance UID
    REQUIRE(json.find("00080018") != std::string::npos);
    REQUIRE(json.find("1.2.3.4.5.6.7.8.9") != std::string::npos);

    // Check SOP Class UID
    REQUIRE(json.find("00080016") != std::string::npos);
    REQUIRE(json.find("1.2.840.10008.5.1.4.1.1.2") != std::string::npos);

    // Check Study Instance UID
    REQUIRE(json.find("0020000D") != std::string::npos);
    REQUIRE(json.find("1.2.3.4.5.6.7") != std::string::npos);

    // Check Series Instance UID
    REQUIRE(json.find("0020000E") != std::string::npos);
    REQUIRE(json.find("1.2.3.4.5.6.7.8") != std::string::npos);

    // Check Instance Number
    REQUIRE(json.find("00200013") != std::string::npos);

    // Check Rows and Columns
    REQUIRE(json.find("00280010") != std::string::npos);
    REQUIRE(json.find("00280011") != std::string::npos);
    REQUIRE(json.find("512") != std::string::npos);
}

TEST_CASE("instance_record_to_dicom_json - optional fields not present",
          "[dicomweb][qidors][json]") {
    pacs::storage::instance_record record;
    record.sop_uid = "1.2.3";
    record.sop_class_uid = "1.2.840.10008.5.1.4.1.1.2";
    // No rows, columns, instance_number, number_of_frames

    auto json = instance_record_to_dicom_json(record, "", "");

    // SOP UIDs should be present
    REQUIRE(json.find("00080018") != std::string::npos);
    REQUIRE(json.find("00080016") != std::string::npos);

    // Rows/Columns should NOT be present
    REQUIRE(json.find("00280010") == std::string::npos);
    REQUIRE(json.find("00280011") == std::string::npos);
}

// ============================================================================
// Frame Retrieval Tests
// ============================================================================

TEST_CASE("parse_frame_numbers - single frame", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("1");

    REQUIRE(frames.size() == 1);
    REQUIRE(frames[0] == 1);
}

TEST_CASE("parse_frame_numbers - multiple frames", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("1,3,5");

    REQUIRE(frames.size() == 3);
    REQUIRE(frames[0] == 1);
    REQUIRE(frames[1] == 3);
    REQUIRE(frames[2] == 5);
}

TEST_CASE("parse_frame_numbers - range", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("1-5");

    REQUIRE(frames.size() == 5);
    REQUIRE(frames[0] == 1);
    REQUIRE(frames[1] == 2);
    REQUIRE(frames[2] == 3);
    REQUIRE(frames[3] == 4);
    REQUIRE(frames[4] == 5);
}

TEST_CASE("parse_frame_numbers - mixed format", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("1,3-5,7");

    REQUIRE(frames.size() == 5);
    REQUIRE(frames[0] == 1);
    REQUIRE(frames[1] == 3);
    REQUIRE(frames[2] == 4);
    REQUIRE(frames[3] == 5);
    REQUIRE(frames[4] == 7);
}

TEST_CASE("parse_frame_numbers - empty string", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("");

    REQUIRE(frames.empty());
}

TEST_CASE("parse_frame_numbers - invalid input", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("abc");

    REQUIRE(frames.empty());
}

TEST_CASE("parse_frame_numbers - zero frame is ignored", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("0,1,2");

    REQUIRE(frames.size() == 2);
    REQUIRE(frames[0] == 1);
    REQUIRE(frames[1] == 2);
}

TEST_CASE("parse_frame_numbers - removes duplicates", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers("1,1,2,2,3");

    REQUIRE(frames.size() == 3);
    REQUIRE(frames[0] == 1);
    REQUIRE(frames[1] == 2);
    REQUIRE(frames[2] == 3);
}

TEST_CASE("parse_frame_numbers - whitespace handling", "[dicomweb][frames]") {
    auto frames = parse_frame_numbers(" 1 , 2 , 3 ");

    REQUIRE(frames.size() == 3);
    REQUIRE(frames[0] == 1);
    REQUIRE(frames[1] == 2);
    REQUIRE(frames[2] == 3);
}

TEST_CASE("extract_frame - first frame", "[dicomweb][frames]") {
    // Create test pixel data: 4 frames of 4 bytes each
    std::vector<uint8_t> pixel_data = {
        0x01, 0x02, 0x03, 0x04,  // Frame 1
        0x11, 0x12, 0x13, 0x14,  // Frame 2
        0x21, 0x22, 0x23, 0x24,  // Frame 3
        0x31, 0x32, 0x33, 0x34   // Frame 4
    };

    auto frame = extract_frame(pixel_data, 1, 4);

    REQUIRE(frame.size() == 4);
    REQUIRE(frame[0] == 0x01);
    REQUIRE(frame[1] == 0x02);
    REQUIRE(frame[2] == 0x03);
    REQUIRE(frame[3] == 0x04);
}

TEST_CASE("extract_frame - middle frame", "[dicomweb][frames]") {
    std::vector<uint8_t> pixel_data = {
        0x01, 0x02, 0x03, 0x04,  // Frame 1
        0x11, 0x12, 0x13, 0x14,  // Frame 2
        0x21, 0x22, 0x23, 0x24,  // Frame 3
        0x31, 0x32, 0x33, 0x34   // Frame 4
    };

    auto frame = extract_frame(pixel_data, 3, 4);

    REQUIRE(frame.size() == 4);
    REQUIRE(frame[0] == 0x21);
    REQUIRE(frame[1] == 0x22);
}

TEST_CASE("extract_frame - out of bounds frame", "[dicomweb][frames]") {
    std::vector<uint8_t> pixel_data = {
        0x01, 0x02, 0x03, 0x04  // Only 1 frame
    };

    auto frame = extract_frame(pixel_data, 5, 4);

    REQUIRE(frame.empty());
}

TEST_CASE("extract_frame - zero frame number", "[dicomweb][frames]") {
    std::vector<uint8_t> pixel_data = {0x01, 0x02, 0x03, 0x04};

    auto frame = extract_frame(pixel_data, 0, 4);

    REQUIRE(frame.empty());
}

// ============================================================================
// Rendered Image Parameter Parsing Tests
// ============================================================================

TEST_CASE("parse_rendered_params - default values", "[dicomweb][rendered]") {
    auto params = parse_rendered_params("", "");

    REQUIRE(params.format == rendered_format::jpeg);
    REQUIRE(params.quality == 75);
    REQUIRE(!params.window_center.has_value());
    REQUIRE(!params.window_width.has_value());
    REQUIRE(params.viewport_width == 0);
    REQUIRE(params.viewport_height == 0);
    REQUIRE(params.frame == 1);
}

TEST_CASE("parse_rendered_params - png format from accept header",
          "[dicomweb][rendered]") {
    auto params = parse_rendered_params("", "image/png");

    REQUIRE(params.format == rendered_format::png);
}

TEST_CASE("parse_rendered_params - jpeg format from accept header",
          "[dicomweb][rendered]") {
    auto params = parse_rendered_params("", "image/jpeg");

    REQUIRE(params.format == rendered_format::jpeg);
}

TEST_CASE("parse_rendered_params - quality parameter", "[dicomweb][rendered]") {
    auto params = parse_rendered_params("?quality=90", "");

    REQUIRE(params.quality == 90);
}

TEST_CASE("parse_rendered_params - quality clamped to 1-100",
          "[dicomweb][rendered]") {
    auto params1 = parse_rendered_params("?quality=0", "");
    REQUIRE(params1.quality == 1);

    auto params2 = parse_rendered_params("?quality=150", "");
    REQUIRE(params2.quality == 100);
}

TEST_CASE("parse_rendered_params - window center and width",
          "[dicomweb][rendered]") {
    auto params = parse_rendered_params("?windowcenter=400&windowwidth=1500", "");

    REQUIRE(params.window_center.has_value());
    REQUIRE(*params.window_center == Catch::Approx(400.0));
    REQUIRE(params.window_width.has_value());
    REQUIRE(*params.window_width == Catch::Approx(1500.0));
}

TEST_CASE("parse_rendered_params - viewport parameter", "[dicomweb][rendered]") {
    auto params = parse_rendered_params("?viewport=512x512", "");

    REQUIRE(params.viewport_width == 512);
    REQUIRE(params.viewport_height == 512);
}

TEST_CASE("parse_rendered_params - rows and columns parameters",
          "[dicomweb][rendered]") {
    auto params = parse_rendered_params("?rows=256&columns=512", "");

    REQUIRE(params.viewport_height == 256);
    REQUIRE(params.viewport_width == 512);
}

TEST_CASE("parse_rendered_params - frame parameter", "[dicomweb][rendered]") {
    auto params = parse_rendered_params("?frame=5", "");

    REQUIRE(params.frame == 5);
}

TEST_CASE("parse_rendered_params - frame 0 defaults to 1",
          "[dicomweb][rendered]") {
    auto params = parse_rendered_params("?frame=0", "");

    REQUIRE(params.frame == 1);
}

// ============================================================================
// Window/Level Application Tests
// ============================================================================

TEST_CASE("apply_window_level - 8-bit unsigned", "[dicomweb][rendered]") {
    // Create simple 2x2 image with values 0, 64, 128, 255
    std::vector<uint8_t> pixel_data = {0, 64, 128, 255};

    auto output = apply_window_level(
        pixel_data,
        2, 2,       // width, height
        8,          // bits_stored
        false,      // is_signed
        127.5,      // window_center (for proper 8-bit identity mapping)
        255.0,      // window_width (255 for [0,255] range)
        1.0, 0.0    // rescale slope/intercept
    );

    REQUIRE(output.size() == 4);
    // Values should map linearly with center=127.5, width=255: 0->0, 255->255
    REQUIRE(output[0] == 0);
    REQUIRE(output[3] == 255);
}

TEST_CASE("apply_window_level - window clipping", "[dicomweb][rendered]") {
    std::vector<uint8_t> pixel_data = {0, 100, 200, 255};

    // Window from 100 to 200 (center=150, width=100)
    auto output = apply_window_level(
        pixel_data,
        2, 2,
        8,
        false,
        150.0,      // window_center
        100.0,      // window_width
        1.0, 0.0
    );

    REQUIRE(output.size() == 4);
    REQUIRE(output[0] == 0);     // Below window min
    REQUIRE(output[1] == 0);     // At window min
    REQUIRE(output[2] == 255);   // At window max
    REQUIRE(output[3] == 255);   // Above window max
}

TEST_CASE("apply_window_level - rescale slope and intercept",
          "[dicomweb][rendered]") {
    std::vector<uint8_t> pixel_data = {0, 50, 100, 150};

    // With slope=2, intercept=-100:
    // 0 -> -100 (below window)
    // 50 -> 0 (at window min with center=128, width=256 -> min=0)
    // 100 -> 100 (in window)
    // 150 -> 200 (in window)
    auto output = apply_window_level(
        pixel_data,
        2, 2,
        8,
        false,
        128.0,
        256.0,
        2.0, -100.0
    );

    REQUIRE(output.size() == 4);
    REQUIRE(output[0] == 0);     // -100 clipped to 0
}
