// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file soap_envelope_test.cpp
 * @brief Unit tests for the ITI-41 SOAP envelope builder.
 */

#include "../../../src/ihe/xds/common/soap_envelope.h"

#include <kcenon/pacs/ihe/xds/submission_set.h>

#include <catch2/catch_test_macros.hpp>
#include <pugixml.hpp>

using namespace kcenon::pacs::ihe::xds;

namespace {

submission_set make_valid_submission() {
    submission_set s;
    s.submission_set_unique_id = "1.2.3.4.5.6.7.8.9";
    s.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000001";
    s.source_id = "1.2.276.0.7230010.3.0.3.6.1";
    s.patient_id = "P12345^^^&1.2.3.4&ISO";
    s.content_type_code = "History and Physical";
    s.submission_time = "20260422120000";
    s.author_institution = "kcenon.pacs.test";
    s.author_person = "^Doe^John";

    xds_document d;
    d.entry_uuid = "urn:uuid:00000000-0000-0000-0000-000000000002";
    d.unique_id = "1.2.840.10008.5.1.4.1.1.2.99999";
    d.mime_type = "application/dicom";
    d.creation_time = "20260422120000";
    d.language_code = "en-US";
    d.title = "CT Chest";
    d.content = std::vector<std::uint8_t>{0xDE, 0xAD, 0xBE, 0xEF};
    s.documents.push_back(std::move(d));
    return s;
}

}  // namespace

TEST_CASE("ITI-41 envelope contains required SOAP + ebRIM structure",
          "[ihe][xds][iti41]") {
    const auto s = make_valid_submission();
    const std::vector<std::string> cids{"doc-0@test"};

    auto result = detail::build_iti41_envelope(s, cids);
    REQUIRE(result.is_ok());
    const auto& env = result.value();

    pugi::xml_document doc;
    REQUIRE(doc.load_buffer(env.xml.data(), env.xml.size()));

    auto envelope = doc.child("soap:Envelope");
    REQUIRE(envelope);

    auto body = envelope.child("soap:Body");
    REQUIRE(body);
    REQUIRE(std::string(body.attribute("wsu:Id").value()) == env.body_id);

    auto submit = body.child("lcm:SubmitObjectsRequest");
    REQUIRE(submit);

    auto leaf_list = submit.child("rim:RegistryObjectList");
    REQUIRE(leaf_list);

    auto doc_entry = leaf_list.child("rim:ExtrinsicObject");
    REQUIRE(doc_entry);
    REQUIRE(std::string(doc_entry.attribute("mimeType").value()) ==
            "application/dicom");
    auto include = doc_entry.child("xop:Include");
    REQUIRE(include);
    REQUIRE(std::string(include.attribute("href").value()) == "cid:doc-0@test");

    auto registry_package = leaf_list.child("rim:RegistryPackage");
    REQUIRE(registry_package);

    auto security = envelope.child("soap:Header").child("wsse:Security");
    REQUIRE(security);
    REQUIRE(security.child("wsu:Timestamp"));
    REQUIRE(security.child("wsse:BinarySecurityToken"));
}

TEST_CASE("ITI-41 envelope rejects missing metadata with typed errors",
          "[ihe][xds][iti41]") {
    SECTION("empty patient id") {
        auto s = make_valid_submission();
        s.patient_id.clear();
        auto r = detail::build_iti41_envelope(s, {"doc-0"});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(error_code::metadata_missing_patient_id));
    }
    SECTION("no documents") {
        auto s = make_valid_submission();
        s.documents.clear();
        auto r = detail::build_iti41_envelope(s, {});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(error_code::metadata_no_documents));
    }
    SECTION("document with empty unique id") {
        auto s = make_valid_submission();
        s.documents[0].unique_id.clear();
        auto r = detail::build_iti41_envelope(s, {"doc-0"});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(
                    error_code::metadata_document_missing_unique_id));
    }
    SECTION("document with empty content") {
        auto s = make_valid_submission();
        s.documents[0].content.clear();
        auto r = detail::build_iti41_envelope(s, {"doc-0"});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(error_code::metadata_document_empty));
    }
    SECTION("cid list length mismatch") {
        auto s = make_valid_submission();
        auto r = detail::build_iti41_envelope(s, {});
        REQUIRE(r.is_err());
        REQUIRE(r.error().code ==
                static_cast<int>(error_code::envelope_build_failed));
    }
}
