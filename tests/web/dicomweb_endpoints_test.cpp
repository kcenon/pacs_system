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
