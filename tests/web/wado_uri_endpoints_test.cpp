/**
 * @file wado_uri_endpoints_test.cpp
 * @brief Unit tests for WADO-URI endpoint utilities
 *
 * @see Issue #798 - Add WADO-URI legacy web access support
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/web/endpoints/wado_uri_endpoints.hpp"

using namespace pacs::web::wado_uri;

// ============================================================================
// parse_wado_uri_params Tests
// ============================================================================

TEST_CASE("parse_wado_uri_params - all parameters provided", "[wado-uri][parse]") {
    auto request = parse_wado_uri_params(
        "1.2.3.4",          // studyUID
        "5.6.7.8",          // seriesUID
        "9.10.11.12",       // objectUID
        "image/jpeg",       // contentType
        "1.2.840.10008.1.2.1",  // transferSyntax
        "yes",              // anonymize
        "512",              // rows
        "512",              // columns
        "40",               // windowCenter
        "400",              // windowWidth
        "3");               // frameNumber

    CHECK(request.study_uid == "1.2.3.4");
    CHECK(request.series_uid == "5.6.7.8");
    CHECK(request.object_uid == "9.10.11.12");
    CHECK(request.content_type == "image/jpeg");
    REQUIRE(request.transfer_syntax.has_value());
    CHECK(request.transfer_syntax.value() == "1.2.840.10008.1.2.1");
    CHECK(request.anonymize == true);
    REQUIRE(request.rows.has_value());
    CHECK(request.rows.value() == 512);
    REQUIRE(request.columns.has_value());
    CHECK(request.columns.value() == 512);
    REQUIRE(request.window_center.has_value());
    CHECK(request.window_center.value() == 40.0);
    REQUIRE(request.window_width.has_value());
    CHECK(request.window_width.value() == 400.0);
    REQUIRE(request.frame_number.has_value());
    CHECK(request.frame_number.value() == 3);
}

TEST_CASE("parse_wado_uri_params - required parameters only", "[wado-uri][parse]") {
    auto request = parse_wado_uri_params(
        "1.2.3", "4.5.6", "7.8.9",
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr);

    CHECK(request.study_uid == "1.2.3");
    CHECK(request.series_uid == "4.5.6");
    CHECK(request.object_uid == "7.8.9");
    CHECK(request.content_type == "application/dicom");
    CHECK_FALSE(request.transfer_syntax.has_value());
    CHECK(request.anonymize == false);
    CHECK_FALSE(request.rows.has_value());
    CHECK_FALSE(request.columns.has_value());
    CHECK_FALSE(request.window_center.has_value());
    CHECK_FALSE(request.window_width.has_value());
    CHECK_FALSE(request.frame_number.has_value());
}

TEST_CASE("parse_wado_uri_params - null UIDs", "[wado-uri][parse]") {
    auto request = parse_wado_uri_params(
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr);

    CHECK(request.study_uid.empty());
    CHECK(request.series_uid.empty());
    CHECK(request.object_uid.empty());
    CHECK(request.content_type == "application/dicom");
}

TEST_CASE("parse_wado_uri_params - empty contentType uses default",
          "[wado-uri][parse]") {
    auto request = parse_wado_uri_params(
        "1.2.3", "4.5.6", "7.8.9",
        "",       // empty contentType
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr);

    CHECK(request.content_type == "application/dicom");
}

TEST_CASE("parse_wado_uri_params - anonymize variations", "[wado-uri][parse]") {
    SECTION("yes") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", nullptr, nullptr, "yes",
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.anonymize == true);
    }
    SECTION("true") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", nullptr, nullptr, "true",
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.anonymize == true);
    }
    SECTION("1") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", nullptr, nullptr, "1",
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.anonymize == true);
    }
    SECTION("no") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", nullptr, nullptr, "no",
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.anonymize == false);
    }
}

TEST_CASE("parse_wado_uri_params - invalid numeric values ignored",
          "[wado-uri][parse]") {
    auto request = parse_wado_uri_params(
        "1.2.3", "4.5.6", "7.8.9",
        nullptr, nullptr, nullptr,
        "abc",    // invalid rows
        "-1",     // invalid columns (negative)
        "not_a_number",   // invalid windowCenter
        "xyz",    // invalid windowWidth
        "0");     // invalid frameNumber (must be > 0)

    CHECK_FALSE(request.rows.has_value());
    CHECK_FALSE(request.columns.has_value());
    CHECK_FALSE(request.window_center.has_value());
    CHECK_FALSE(request.window_width.has_value());
    CHECK_FALSE(request.frame_number.has_value());
}

TEST_CASE("parse_wado_uri_params - content type values", "[wado-uri][parse]") {
    SECTION("application/dicom") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", "application/dicom", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.content_type == "application/dicom");
    }
    SECTION("image/jpeg") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", "image/jpeg", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.content_type == "image/jpeg");
    }
    SECTION("image/png") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", "image/png", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.content_type == "image/png");
    }
    SECTION("application/dicom+json") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", "application/dicom+json", nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, nullptr);
        CHECK(r.content_type == "application/dicom+json");
    }
}

// ============================================================================
// validate_wado_uri_request Tests
// ============================================================================

TEST_CASE("validate_wado_uri_request - valid request", "[wado-uri][validate]") {
    wado_uri_request request;
    request.study_uid = "1.2.3";
    request.series_uid = "4.5.6";
    request.object_uid = "7.8.9";
    request.content_type = "application/dicom";

    auto result = validate_wado_uri_request(request);
    CHECK(result.valid);
    CHECK(result.http_status == 200);
}

TEST_CASE("validate_wado_uri_request - missing studyUID",
          "[wado-uri][validate]") {
    wado_uri_request request;
    request.series_uid = "4.5.6";
    request.object_uid = "7.8.9";

    auto result = validate_wado_uri_request(request);
    CHECK_FALSE(result.valid);
    CHECK(result.http_status == 400);
    CHECK(result.error_code == "MISSING_PARAMETER");
}

TEST_CASE("validate_wado_uri_request - missing seriesUID",
          "[wado-uri][validate]") {
    wado_uri_request request;
    request.study_uid = "1.2.3";
    request.object_uid = "7.8.9";

    auto result = validate_wado_uri_request(request);
    CHECK_FALSE(result.valid);
    CHECK(result.http_status == 400);
    CHECK(result.error_code == "MISSING_PARAMETER");
}

TEST_CASE("validate_wado_uri_request - missing objectUID",
          "[wado-uri][validate]") {
    wado_uri_request request;
    request.study_uid = "1.2.3";
    request.series_uid = "4.5.6";

    auto result = validate_wado_uri_request(request);
    CHECK_FALSE(result.valid);
    CHECK(result.http_status == 400);
    CHECK(result.error_code == "MISSING_PARAMETER");
}

TEST_CASE("validate_wado_uri_request - unsupported content type",
          "[wado-uri][validate]") {
    wado_uri_request request;
    request.study_uid = "1.2.3";
    request.series_uid = "4.5.6";
    request.object_uid = "7.8.9";
    request.content_type = "text/html";

    auto result = validate_wado_uri_request(request);
    CHECK_FALSE(result.valid);
    CHECK(result.http_status == 406);
    CHECK(result.error_code == "UNSUPPORTED_MEDIA_TYPE");
}

TEST_CASE("validate_wado_uri_request - all supported content types pass",
          "[wado-uri][validate]") {
    wado_uri_request request;
    request.study_uid = "1.2.3";
    request.series_uid = "4.5.6";
    request.object_uid = "7.8.9";

    SECTION("application/dicom") {
        request.content_type = "application/dicom";
        CHECK(validate_wado_uri_request(request).valid);
    }
    SECTION("image/jpeg") {
        request.content_type = "image/jpeg";
        CHECK(validate_wado_uri_request(request).valid);
    }
    SECTION("image/png") {
        request.content_type = "image/png";
        CHECK(validate_wado_uri_request(request).valid);
    }
    SECTION("application/dicom+json") {
        request.content_type = "application/dicom+json";
        CHECK(validate_wado_uri_request(request).valid);
    }
}

// ============================================================================
// is_supported_content_type Tests
// ============================================================================

TEST_CASE("is_supported_content_type - supported types",
          "[wado-uri][content-type]") {
    CHECK(is_supported_content_type("application/dicom"));
    CHECK(is_supported_content_type("image/jpeg"));
    CHECK(is_supported_content_type("image/png"));
    CHECK(is_supported_content_type("application/dicom+json"));
}

TEST_CASE("is_supported_content_type - unsupported types",
          "[wado-uri][content-type]") {
    CHECK_FALSE(is_supported_content_type("text/html"));
    CHECK_FALSE(is_supported_content_type("application/json"));
    CHECK_FALSE(is_supported_content_type("image/gif"));
    CHECK_FALSE(is_supported_content_type("application/pdf"));
    CHECK_FALSE(is_supported_content_type(""));
    CHECK_FALSE(is_supported_content_type("*/*"));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("parse_wado_uri_params - boundary viewport values",
          "[wado-uri][parse][edge]") {
    SECTION("maximum valid rows") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", nullptr, nullptr, nullptr,
            "65535", nullptr, nullptr, nullptr, nullptr);
        REQUIRE(r.rows.has_value());
        CHECK(r.rows.value() == 65535);
    }
    SECTION("overflow rows ignored") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", nullptr, nullptr, nullptr,
            "70000", nullptr, nullptr, nullptr, nullptr);
        CHECK_FALSE(r.rows.has_value());
    }
    SECTION("frame number 1") {
        auto r = parse_wado_uri_params(
            "1", "2", "3", nullptr, nullptr, nullptr,
            nullptr, nullptr, nullptr, nullptr, "1");
        REQUIRE(r.frame_number.has_value());
        CHECK(r.frame_number.value() == 1);
    }
}

TEST_CASE("parse_wado_uri_params - floating point window values",
          "[wado-uri][parse][edge]") {
    auto r = parse_wado_uri_params(
        "1", "2", "3", nullptr, nullptr, nullptr,
        nullptr, nullptr, "40.5", "400.25", nullptr);

    REQUIRE(r.window_center.has_value());
    CHECK(r.window_center.value() == 40.5);
    REQUIRE(r.window_width.has_value());
    CHECK(r.window_width.value() == 400.25);
}

TEST_CASE("parse_wado_uri_params - negative window center",
          "[wado-uri][parse][edge]") {
    auto r = parse_wado_uri_params(
        "1", "2", "3", nullptr, nullptr, nullptr,
        nullptr, nullptr, "-100", "200", nullptr);

    REQUIRE(r.window_center.has_value());
    CHECK(r.window_center.value() == -100.0);
    REQUIRE(r.window_width.has_value());
    CHECK(r.window_width.value() == 200.0);
}
