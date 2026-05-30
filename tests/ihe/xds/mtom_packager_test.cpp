// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file mtom_packager_test.cpp
 * @brief Unit tests for the multipart/related + MTOM packager.
 */

#include "../../../src/ihe/xds/common/mtom_packager.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace kcenon::pacs::ihe::xds::detail;

namespace {

std::string extract_boundary(const std::string& content_type) {
    const auto pos = content_type.find("boundary=\"");
    if (pos == std::string::npos) return {};
    const auto start = pos + std::string_view{"boundary=\""}.size();
    const auto end = content_type.find('"', start);
    if (end == std::string::npos) return {};
    return content_type.substr(start, end - start);
}

}  // namespace

TEST_CASE("MTOM packager produces well-formed multipart/related body",
          "[ihe][xds][mtom]") {
    const std::string envelope =
        R"(<?xml version="1.0" encoding="UTF-8"?><soap:Envelope/>)";

    mtom_part part;
    part.content_id = "doc-0@kcenon.pacs";
    part.mime_type = "application/dicom";
    part.content = std::vector<std::uint8_t>{
        0x44, 0x49, 0x43, 0x4D, 0x00, 0x01, 0x02, 0x03, 0xFF, 0xFE};

    auto r = package_mtom(envelope, {part});
    REQUIRE(r.is_ok());
    const auto& out = r.value();

    REQUIRE(out.content_type.find("multipart/related") == 0);
    REQUIRE(out.content_type.find("type=\"application/xop+xml\"") !=
            std::string::npos);
    REQUIRE(out.content_type.find("start=\"<root.message@cxf.apache.org>\"") !=
            std::string::npos);

    const std::string boundary = extract_boundary(out.content_type);
    REQUIRE_FALSE(boundary.empty());

    REQUIRE(out.body.find("--" + boundary) != std::string::npos);
    REQUIRE(out.body.find("--" + boundary + "--") != std::string::npos);

    REQUIRE(out.body.find("Content-Type: application/xop+xml") !=
            std::string::npos);
    REQUIRE(out.body.find("Content-ID: <" + part.content_id + ">") !=
            std::string::npos);
    REQUIRE(out.body.find(envelope) != std::string::npos);

    const std::string raw_bytes(
        reinterpret_cast<const char*>(part.content.data()), part.content.size());
    REQUIRE(out.body.find(raw_bytes) != std::string::npos);
}

TEST_CASE("MTOM packager rejects empty payload",
          "[ihe][xds][mtom]") {
    SECTION("empty envelope") {
        auto r = package_mtom("", {});
        REQUIRE(r.is_err());
    }
    SECTION("part with empty content-id") {
        mtom_part part;
        part.mime_type = "application/dicom";
        part.content = std::vector<std::uint8_t>{0x01};
        auto r = package_mtom("<x/>", {part});
        REQUIRE(r.is_err());
    }
    SECTION("part with empty content") {
        mtom_part part;
        part.content_id = "doc@x";
        part.mime_type = "application/dicom";
        auto r = package_mtom("<x/>", {part});
        REQUIRE(r.is_err());
    }
}

TEST_CASE("MTOM packager attaches multiple parts in order",
          "[ihe][xds][mtom]") {
    const std::string envelope = "<Envelope/>";
    mtom_part a{"a@x", "application/dicom", {0x01, 0x02}};
    mtom_part b{"b@x", "application/dicom", {0x03, 0x04}};

    auto r = package_mtom(envelope, {a, b});
    REQUIRE(r.is_ok());
    const auto& out = r.value();

    const auto pos_a = out.body.find("<a@x>");
    const auto pos_b = out.body.find("<b@x>");
    REQUIRE(pos_a != std::string::npos);
    REQUIRE(pos_b != std::string::npos);
    REQUIRE(pos_a < pos_b);
}
