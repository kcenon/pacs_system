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
