// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file dicomweb_contract_test.cpp
 * @brief What PS3.18 says of media types, search parameters, frames and DICOM JSON
 *
 * tests in validation/dcmtk/tests/dicomweb_probe.py hold the running server to the same rules through
 * independent clients; these fail closer to the code, and cover what a peer cannot provoke.
 */

#include <catch2/catch_test_macros.hpp>

#include "crow.h"

#ifdef DELETE
#undef DELETE
#endif

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_element.h"
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"
#include "kcenon/pacs/web/endpoints/dicomweb_endpoints.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

using namespace kcenon::pacs::web::dicomweb;
namespace core = kcenon::pacs::core;
namespace tags = kcenon::pacs::core::tags;
using kcenon::pacs::encoding::transfer_syntax;
using kcenon::pacs::encoding::vr_type;

namespace {

constexpr std::string_view dicom_multipart = "multipart/related; type=\"application/dicom\"";
constexpr std::string_view octet_multipart = "multipart/related; type=\"application/octet-stream\"";

constexpr core::dicom_tag number_of_frames_tag{0x0028, 0x0008};

/// A temporary directory, gone with the test.
class temp_directory {
public:
    temp_directory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() / ("dicomweb_contract_" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }
    ~temp_directory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    temp_directory(const temp_directory&) = delete;
    temp_directory& operator=(const temp_directory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

[[nodiscard]] core::dicom_dataset identified_dataset() {
    core::dicom_dataset dataset;
    dataset.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.7");
    dataset.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4");
    dataset.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3");
    dataset.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.1");
    return dataset;
}

/// An 8 bit grayscale object of three 2x2 frames: 0,1,2,3 then 10.. then 20.., with 255 in each to fix the range.
[[nodiscard]] std::filesystem::path write_frames(const temp_directory& directory, std::string_view photometric = "MONOCHROME2") {
    auto dataset = identified_dataset();
    dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, 2);
    dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, 2);
    dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 8);
    dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 8);
    dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 7);
    dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
    dataset.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);
    dataset.set_string(tags::photometric_interpretation, vr_type::CS, photometric);
    dataset.set_string(number_of_frames_tag, vr_type::IS, "3");
    const std::vector<uint8_t> pixels{0, 1, 2, 255, 0, 11, 12, 255, 0, 21, 22, 255};
    dataset.insert(core::dicom_element(tags::pixel_data, vr_type::OB, pixels));
    const auto path = directory.path() / "frames.dcm";
    REQUIRE(core::dicom_file::create(dataset, transfer_syntax::explicit_vr_little_endian).save(path).is_ok());
    return path;
}

/// The samples of an image encode_png wrote: stored blocks, filter 0, one row at a time.
[[nodiscard]] std::vector<uint8_t> png_samples(const std::vector<uint8_t>& png, std::size_t width, std::size_t height) {
    // Signature 8, IHDR chunk 25, IDAT length and type 8, zlib header 2, one stored block's header 5
    const std::size_t start = 8 + 25 + 8 + 2 + 5;
    std::vector<uint8_t> samples;
    for (std::size_t row = 0; row < height; ++row) {
        const auto line = start + row * (width + 1);
        REQUIRE(png[line] == 0);
        samples.insert(samples.end(), png.begin() + static_cast<std::ptrdiff_t>(line + 1),
                       png.begin() + static_cast<std::ptrdiff_t>(line + 1 + width));
    }
    return samples;
}

}  // namespace

TEST_CASE("choose_media_type follows PS3.18 8.7.5", "[dicomweb][negotiation]") {
    SECTION("no Accept header is 406") {
        CHECK(choose_media_type("", wado_resource::collection).status == 406);
        CHECK(choose_media_type("  ", wado_resource::search).status == 406);
    }

    SECTION("DICOM and rendered types together are 400") {
        CHECK(choose_media_type("application/dicom, image/jpeg", wado_resource::instance).status == 400);
        CHECK(choose_media_type("image/png, application/dicom+json", wado_resource::rendered).status == 400);
    }

    SECTION("a Transfer Syntax that is not a UID is 400") {
        const auto choice = choose_media_type(std::string(dicom_multipart) + "; transfer-syntax=not.a.uid", wado_resource::collection);
        CHECK(choice.status == 400);
    }

    SECTION("q=0 excludes a type") {
        const auto choice = choose_media_type("application/dicom;q=0, text/plain", wado_resource::instance);
        CHECK(choice.status == 406);
    }

    SECTION("preference orders what is acceptable") {
        const auto choice = choose_media_type(std::string(dicom_multipart) + ";q=0.5, application/dicom;q=0.9", wado_resource::instance);
        REQUIRE(choice.ok());
        CHECK_FALSE(choice.multipart);
        CHECK(choice.media == media_type::dicom);
    }
}

TEST_CASE("choose_media_type gives each resource its Table 10.4.4-1 types", "[dicomweb][negotiation]") {
    SECTION("a study or series is only multipart") {
        const auto any = choose_media_type("*/*", wado_resource::collection);
        REQUIRE(any.ok());
        CHECK(any.multipart);
        CHECK(any.media == media_type::dicom);
        CHECK(choose_media_type("application/dicom", wado_resource::collection).status == 406);
        CHECK(choose_media_type("multipart/related; type=application/dicom", wado_resource::collection).multipart);
    }

    SECTION("an instance may also be a single part, when that is asked for") {
        CHECK_FALSE(choose_media_type("application/dicom", wado_resource::instance).multipart);
        CHECK(choose_media_type("*/*", wado_resource::instance).multipart);
    }

    SECTION("metadata is JSON or one XML part per object") {
        CHECK(choose_media_type("*/*", wado_resource::metadata).media == media_type::dicom_json);
        const auto xml = choose_media_type("multipart/related; type=\"application/dicom+xml\"", wado_resource::metadata);
        CHECK(xml.multipart);
        CHECK(xml.media == media_type::dicom_xml);
        CHECK(choose_media_type("image/jpeg", wado_resource::metadata).status == 406);
    }

    SECTION("one frame may be a single part, and several may not") {
        CHECK_FALSE(choose_media_type("application/octet-stream", wado_resource::frames, 1).multipart);
        CHECK(choose_media_type("application/octet-stream", wado_resource::frames, 2).status == 406);
        CHECK(choose_media_type(octet_multipart, wado_resource::frames, 2).multipart);
    }

    SECTION("a compressed type is only for what is held in it") {
        constexpr std::string_view rle = "1.2.840.10008.1.2.5";
        constexpr std::string_view jpeg_ls = "1.2.840.10008.1.2.4.80";
        const auto asked = choose_media_type("multipart/related; type=\"image/jls\"", wado_resource::frames, 2, jpeg_ls);
        REQUIRE(asked.ok());
        CHECK(asked.media == "image/jls");
        CHECK(choose_media_type("multipart/related; type=\"image/jls\"", wado_resource::frames, 2, rle).status == 406);
        CHECK(choose_media_type("multipart/related; type=\"image/jls\"", wado_resource::frames, 2, "1.2.840.10008.1.2.1").status == 406);
        CHECK(choose_media_type("multipart/related; type=\"image/dicom-rle\"; transfer-syntax=1.2.840.10008.1.2.5", wado_resource::frames, 1, rle).ok());
        CHECK(choose_media_type("multipart/related; type=\"image/dicom-rle\"; transfer-syntax=1.2.840.10008.1.2.4.80", wado_resource::frames, 1, rle).status == 406);
    }

    SECTION("rendered types") {
        CHECK(choose_media_type("*/*", wado_resource::rendered).media == media_type::jpeg);
        CHECK(choose_media_type("image/*", wado_resource::rendered).media == media_type::jpeg);
        CHECK(choose_media_type("image/png", wado_resource::rendered).media == media_type::png);
        CHECK(choose_media_type("application/dicom", wado_resource::rendered).status == 406);
    }

    SECTION("the response of a store") {
        CHECK(choose_media_type("*/*", wado_resource::store_response).media == media_type::dicom_json);
        CHECK(choose_media_type("application/dicom+xml", wado_resource::store_response).media == media_type::dicom_xml);
        CHECK(choose_media_type("text/plain", wado_resource::store_response).status == 406);
    }
}

TEST_CASE("parse_accept_header keeps the type of a multipart entry", "[dicomweb][negotiation]") {
    const auto entries = parse_accept_header("Multipart/Related; TYPE=x, multipart/related; type=\"Application/DICOM\"; transfer-syntax=*;q=0.5");
    REQUIRE(entries.size() == 2);
    CHECK(entries[0].media_type == "multipart/related");
    CHECK(entries[1].type == "application/dicom");
    CHECK(entries[1].transfer_syntax == "*");
    CHECK(entries[1].quality == 0.5f);
}

TEST_CASE("the first parameter of a request URL is a parameter", "[dicomweb][qido]") {
    CHECK(parse_study_query_params("/dicomweb/studies?PatientID=A&limit=5").patient_id == "A");
    CHECK(parse_study_query_params("/dicomweb/studies?PatientID=A&limit=5").limit == 5);
    CHECK_FALSE(parse_study_query_params("/dicomweb/studies").patient_id.has_value());
    CHECK(parse_study_query_params("PatientID=B").patient_id == "B");
    CHECK(parse_series_query_params("/dicomweb/series?Modality=CT").modality == "CT");
    CHECK(parse_instance_query_params("/dicomweb/instances?SOPInstanceUID=1.2").sop_uid == "1.2");
}

TEST_CASE("parse_search_parameters follows PS3.18 8.3.4", "[dicomweb][qido]") {
    SECTION("keys by Keyword and by Tag") {
        const auto parsed = parse_search_parameters("/dicomweb/studies?PatientID=A&00080050=B");
        REQUIRE_FALSE(parsed.error);
        REQUIRE(parsed.keys.size() == 2);
        CHECK(parsed.keys[0].tag == tags::patient_id);
        CHECK(parsed.keys[1].tag == tags::accession_number);
        CHECK(parsed.keys[1].value == "B");
    }

    SECTION("a comma lists UIDs and is an ordinary character elsewhere") {
        const auto parsed = parse_search_parameters("StudyInstanceUID=1.2,1.3&PatientName=DOE%2CJOHN");
        REQUIRE(parsed.keys.size() == 2);
        CHECK(parsed.keys[0].value == "1.2\\1.3");
        CHECK(parsed.keys[1].value == "DOE,JOHN");
    }

    SECTION("limit and offset are unsigned integers") {
        CHECK(parse_search_parameters("limit=7&offset=3").limit == 7);
        CHECK(parse_search_parameters("limit=7&offset=3").offset == 3);
        for (const auto* bad : {"limit=abc", "limit=-1", "offset=abc", "offset=-1", "limit=", "limit=99999999999999999999999", "offset=1.5"}) {
            CAPTURE(bad);
            CHECK(parse_search_parameters(bad).error.has_value());
        }
    }

    SECTION("what is not an attribute or is repeated is refused") {
        CHECK(parse_search_parameters("NoSuchAttribute=1").error.has_value());
        CHECK(parse_search_parameters("PatientID=A&PatientID=B").error.has_value());
        CHECK(parse_search_parameters("PatientID=A&00100020=B").error.has_value());
    }

    SECTION("boolean parameters") {
        CHECK(parse_search_parameters("fuzzymatching=maybe").error.has_value());
        CHECK(parse_search_parameters("fuzzymatching=false").unsupported_flags.empty());
        CHECK(parse_search_parameters("fuzzymatching=true").unsupported_flags == std::vector<std::string>{"fuzzymatching"});
    }

    SECTION("includefield") {
        const auto parsed = parse_search_parameters("includefield=KVP,00080080&includefield=all");
        REQUIRE_FALSE(parsed.error);
        REQUIRE(parsed.include_fields.size() == 2);
        CHECK(parsed.include_fields[0] == core::dicom_tag{0x0018, 0x0060});
        CHECK(parsed.include_all);
        CHECK(parse_search_parameters("includefield=NoSuchAttribute").error.has_value());
    }

    SECTION("a key inside a sequence is valid and cannot be matched") {
        const auto parsed = parse_search_parameters("00400275.00400009=X");
        CHECK_FALSE(parsed.error);
        CHECK(parsed.keys.empty());
        CHECK(parsed.keys_not_used == std::vector<std::string>{"00400275.00400009"});
    }
}

TEST_CASE("parse_frame_list refuses what would lose a frame", "[dicomweb][frames]") {
    CHECK(parse_frame_list("1") == std::vector<uint32_t>{1});
    CHECK(parse_frame_list("1,3,5") == std::vector<uint32_t>{1, 3, 5});
    CHECK(parse_frame_list("2-4") == std::vector<uint32_t>{2, 3, 4});
    CHECK(parse_frame_list("1,3-4,6") == std::vector<uint32_t>{1, 3, 4, 6});
    for (const auto* bad : {"", "0", "abc", "1,", ",1", "1,,2", "1,abc", "-1", "2,1", "1,1", "4-2", "1-2,2", "1 ,2", "99999999999999999999", "1-4294967295"}) {
        CAPTURE(bad);
        CHECK_FALSE(parse_frame_list(bad).has_value());
    }
}

TEST_CASE("multipart_parser reads whole parts or none", "[dicomweb][stow]") {
    const std::string kind = "multipart/related; type=\"application/dicom\"; boundary=b";

    SECTION("a body without its closing delimiter is an error, not a shorter list") {
        const auto parsed = multipart_parser::parse(kind, "--b\r\nContent-Type: application/dicom\r\n\r\none\r\n--b\r\nContent-Type: application/dicom\r\n\r\ntwo");
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error->code == "TRUNCATED");
        CHECK(parsed.parts.empty());
    }

    SECTION("the boundary's bytes inside a part do not end it") {
        const std::string body = "--b\r\nContent-Type: application/dicom\r\n\r\nxx--b yy\r\n--bz\r\n--b--\r\n";
        const auto parsed = multipart_parser::parse(kind, body);
        REQUIRE(parsed);
        REQUIRE(parsed.parts.size() == 1);
        const std::string data(parsed.parts[0].data.begin(), parsed.parts[0].data.end());
        CHECK(data == "xx--b yy\r\n--bz");
    }

    SECTION("a part may have no headers") {
        const auto parsed = multipart_parser::parse(kind, "--b\r\n\r\ndata\r\n--b--\r\n");
        REQUIRE(parsed);
        REQUIRE(parsed.parts.size() == 1);
        CHECK(parsed.parts[0].content_type == "application/dicom");
    }

    SECTION("a part may be empty") {
        const auto parsed = multipart_parser::parse(kind, "--b\r\nContent-Type: application/dicom\r\n\r\n\r\n--b--\r\n");
        REQUIRE(parsed);
        REQUIRE(parsed.parts.size() == 1);
        CHECK(parsed.parts[0].data.empty());
    }

    SECTION("a part with no blank line after its headers is an error") {
        const auto parsed = multipart_parser::parse(kind, "--b\r\nContent-Type: application/dicom\r\n--b--\r\n");
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error->code == "MALFORMED_PART");
    }

    SECTION("no boundary, or another one") {
        CHECK_FALSE(multipart_parser::parse("multipart/related", "--b\r\n\r\nx\r\n--b--\r\n"));
        CHECK_FALSE(multipart_parser::parse(kind, "--c\r\n\r\nx\r\n--c--\r\n"));
        CHECK_FALSE(multipart_parser::parse(kind, ""));
    }

    SECTION("padding after a delimiter and a preamble before the first") {
        const auto parsed = multipart_parser::parse(kind, "preamble\r\n--b  \r\n\r\none\r\n--b--\r\n");
        REQUIRE(parsed);
        CHECK(parsed.parts.size() == 1);
    }

    SECTION("a quoted boundary holding the characters only quoting allows") {
        multipart_builder builder;
        builder.add_part({1, 2, 3});
        const auto parsed = multipart_parser::parse(builder.content_type_header(), builder.build());
        REQUIRE(parsed);
        CHECK(parsed.parts[0].data == std::vector<uint8_t>{1, 2, 3});
        CHECK(builder.content_type_header().find("boundary=\"") != std::string::npos);
    }
}

TEST_CASE("validate_instance refuses what is not a UID", "[dicomweb][stow]") {
    CHECK(validate_instance(identified_dataset()));
    for (const auto* bad : {"../../../etc/passwd", "1.2.a", "1..2", ".1", "1.", "1.2 ", "/tmp/x", ".."}) {
        CAPTURE(bad);
        for (const auto tag : {tags::sop_instance_uid, tags::study_instance_uid, tags::series_instance_uid, tags::sop_class_uid}) {
            auto dataset = identified_dataset();
            dataset.set_string(tag, vr_type::UI, bad);
            const auto result = validate_instance(dataset);
            // UI padding is a NUL, so a value that ends in a space is not trimmed away.
            CHECK_FALSE(result);
            CHECK(result.error_code == "INVALID_UID");
        }
    }
    auto long_uid = identified_dataset();
    long_uid.set_string(tags::sop_instance_uid, vr_type::UI, std::string(65, '1'));
    CHECK_FALSE(validate_instance(long_uid));
}

TEST_CASE("build_store_response_json names each failure by PS3.18 I.2.2", "[dicomweb][stow]") {
    store_response response;
    store_instance_result stored;
    stored.sop_class_uid = "1.2";
    stored.sop_instance_uid = "1.2.3";
    stored.retrieve_url = "/dicomweb/studies/1";
    response.referenced_instances.push_back(stored);
    const auto failed_with = [&](const char* code, const char* uid) {
        store_instance_result failed;
        failed.sop_instance_uid = uid;
        failed.error_code = code;
        response.failed_instances.push_back(failed);
    };
    failed_with("DUPLICATE", "1");
    failed_with("INVALID_DATA", "");
    failed_with("INVALID_UID", "quote\"here");
    failed_with("STORAGE_ERROR", "4");

    const auto json = crow::json::load(build_store_response_json(response, "http://host"));
    REQUIRE(json);
    CHECK(json["00081199"]["Value"][0]["00081190"]["Value"][0].s() == "http://host/dicomweb/studies/1");
    const auto& failures = json["00081198"]["Value"];
    REQUIRE(failures.size() == 4);
    CHECK(failures[0]["00081197"]["Value"][0].i() == 0x0111);
    CHECK(failures[1]["00081197"]["Value"][0].i() == 0xC000);
    CHECK_FALSE(failures[1].has("00081155"));
    CHECK(failures[2]["00081197"]["Value"][0].i() == 0xA900);
    CHECK(failures[2]["00081155"]["Value"][0].s() == "quote\"here");
    CHECK(failures[3]["00081197"]["Value"][0].i() == 0xA700);

    const auto xml = build_store_response_xml(response, "http://host");
    CHECK(xml.find("<NativeDicomModel") != std::string::npos);
    CHECK(xml.find("tag=\"00081198\"") != std::string::npos);
    CHECK(xml.find("quote&quot;here") != std::string::npos);
}

TEST_CASE("dataset_to_dicom_json writes PS3.18 Annex F", "[dicomweb][json]") {
    core::dicom_dataset dataset;
    dataset.set_string(tags::image_type, vr_type::CS, "ORIGINAL\\PRIMARY");
    dataset.set_string(core::dicom_tag{0x0020, 0x0032}, vr_type::DS, "1.5\\-2.25\\+3.");
    dataset.set_string(tags::instance_number, vr_type::IS, " 7 ");
    dataset.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN=X^Y=Z^W\\ROE^JANE");
    dataset.set_string(core::dicom_tag{0x0008, 0x0081}, vr_type::ST, "one\\two");
    dataset.set_string(tags::study_description, vr_type::LO, "say \"hi\"");
    dataset.set_string(tags::accession_number, vr_type::SH, "");
    const std::vector<uint16_t> values{1, 2};
    dataset.insert(core::dicom_element::from_numeric_list<uint16_t>(core::dicom_tag{0x0028, 0x0010}, vr_type::US, values));
    const std::vector<uint8_t> at{0x10, 0x00, 0x20, 0x00};
    dataset.insert(core::dicom_element(core::dicom_tag{0x0020, 0x5000}, vr_type::AT, at));
    const std::vector<uint8_t> bytes{0xFF, 0x00, 0x7F};
    dataset.insert(core::dicom_element(core::dicom_tag{0x0009, 0x1001}, vr_type::OB, bytes));
    dataset.insert(core::dicom_element(tags::pixel_data, vr_type::OB, bytes));

    core::dicom_dataset item;
    item.set_string(core::dicom_tag{0x0008, 0x0100}, vr_type::SH, "113100");
    item.insert(core::dicom_element(tags::pixel_data, vr_type::OB, bytes));
    core::dicom_element sequence(core::dicom_tag{0x0088, 0x0200}, vr_type::SQ);
    sequence.sequence_items().push_back(item);
    dataset.insert(std::move(sequence));
    dataset.insert(core::dicom_element(core::dicom_tag{0x0040, 0xA730}, vr_type::SQ));

    const auto text = dataset_to_dicom_json(dataset, false, "http://host/bulk/");
    const auto json = crow::json::load(text);
    REQUIRE(json);

    SECTION("tags are uppercase, values are split, numbers are numbers") {
        CHECK(json["00080008"]["Value"].size() == 2);
        CHECK(json["00080008"]["Value"][1].s() == "PRIMARY");
        CHECK(json["00200032"]["Value"].size() == 3);
        CHECK(json["00200032"]["Value"][1].d() == -2.25);
        CHECK(json["00200032"]["Value"][2].d() == 3.0);
        CHECK(json["00200013"]["Value"][0].i() == 7);
        CHECK(json["00280010"]["Value"][1].i() == 2);
        CHECK(text.find("7FE00010") != std::string::npos);
    }

    SECTION("a person name has its groups, per value") {
        const auto& names = json["00100010"]["Value"];
        REQUIRE(names.size() == 2);
        CHECK(names[0]["Alphabetic"].s() == "DOE^JOHN");
        CHECK(names[0]["Ideographic"].s() == "X^Y");
        CHECK(names[0]["Phonetic"].s() == "Z^W");
        CHECK(names[1]["Alphabetic"].s() == "ROE^JANE");
        CHECK_FALSE(names[1].has("Ideographic"));
    }

    SECTION("text that holds one value is not split") {
        REQUIRE(json["00080081"]["Value"].size() == 1);
        CHECK(json["00080081"]["Value"][0].s() == "one\\two");
    }

    SECTION("an empty attribute is present and has no Value") {
        CHECK(json["00080050"]["vr"].s() == "SH");
        CHECK_FALSE(json["00080050"].has("Value"));
    }

    SECTION("attribute tags are hexadecimal strings") {
        CHECK(json["00205000"]["Value"][0].s() == "00100020");
    }

    SECTION("binary values are base64 unless they are Bulk Data") {
        CHECK(json["00091001"]["InlineBinary"].s() == "/wB/");
        CHECK(json["7FE00010"]["BulkDataURI"].s() == "http://host/bulk/7FE00010");
    }

    SECTION("a nested value has no URL of its own, so it stays inline") {
        const auto& nested = json["00880200"]["Value"][0];
        CHECK(nested["00080100"]["Value"][0].s() == "113100");
        CHECK(nested["7FE00010"]["InlineBinary"].s() == "/wB/");
    }

    SECTION("an empty sequence is present") {
        CHECK(json["0040A730"]["vr"].s() == "SQ");
        CHECK_FALSE(json["0040A730"].has("Value"));
    }
}

TEST_CASE("dataset_to_dicom_json keeps what is not a number as text", "[dicomweb][json]") {
    core::dicom_dataset dataset;
    dataset.set_string(core::dicom_tag{0x0018, 0x0060}, vr_type::DS, "abc");
    const auto json = crow::json::load(dataset_to_dicom_json(dataset));
    REQUIRE(json);
    CHECK(json["00180060"]["Value"][0].s() == "abc");
}

TEST_CASE("dataset_to_dicom_json decodes to UTF-8", "[dicomweb][json]") {
    core::dicom_dataset dataset;
    dataset.set_string(tags::specific_character_set, vr_type::CS, "ISO_IR 100");
    dataset.set_string(tags::patient_name, vr_type::PN, "M\xFCller");
    const auto json = crow::json::load(dataset_to_dicom_json(dataset));
    REQUIRE(json);
    CHECK(json["00100010"]["Value"][0]["Alphabetic"].s() == "M\xC3\xBCller");
    CHECK(json["00080005"]["Value"][0].s() == "ISO_IR 192");
}

TEST_CASE("bytes no character set explains still make valid text", "[dicomweb][json]") {
    core::dicom_dataset dataset;
    dataset.set_string(tags::patient_name, vr_type::PN, "M\xFCller");
    dataset.set_string(tags::study_description, vr_type::LO, "ok \xC3\xBC \xED\xA0\x80");
    const auto text = dataset_to_dicom_json(dataset);
    CHECK(text.find("M\xEF\xBF\xBDller") != std::string::npos);
    CHECK(text.find("ok \xC3\xBC") != std::string::npos);
    CHECK(text.find("\xED\xA0") == std::string::npos);
    CHECK(crow::json::load(text));
    const auto xml = dataset_to_native_xml(dataset);
    CHECK(xml.find("M\xEF\xBF\xBDller") != std::string::npos);
    CHECK(xml.find("\xFC") == std::string::npos);
}

TEST_CASE("dataset_to_native_xml writes PS3.19 Annex A", "[dicomweb][xml]") {
    core::dicom_dataset dataset;
    dataset.set_string(tags::image_type, vr_type::CS, "A\\B");
    dataset.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    dataset.set_string(tags::study_description, vr_type::LO, "a<b&c");
    const std::vector<uint8_t> bytes{0xFF, 0x00, 0x7F};
    dataset.insert(core::dicom_element(tags::pixel_data, vr_type::OB, bytes));
    core::dicom_dataset item;
    item.set_string(core::dicom_tag{0x0008, 0x0100}, vr_type::SH, "X");
    core::dicom_element sequence(core::dicom_tag{0x0008, 0x1032}, vr_type::SQ);
    sequence.sequence_items().push_back(item);
    dataset.insert(std::move(sequence));

    const auto xml = dataset_to_native_xml(dataset, false, "/bulk/");
    CHECK(xml.find("xmlns=\"http://dicom.nema.org/PS3.19/models/NativeDICOM\"") != std::string::npos);
    CHECK(xml.find("tag=\"00080008\" vr=\"CS\" keyword=\"ImageType\"><Value number=\"1\">A</Value><Value number=\"2\">B</Value>") != std::string::npos);
    CHECK(xml.find("<PersonName number=\"1\"><Alphabetic><FamilyName>DOE</FamilyName><GivenName>JOHN</GivenName></Alphabetic></PersonName>") != std::string::npos);
    CHECK(xml.find("a&lt;b&amp;c") != std::string::npos);
    CHECK(xml.find("<BulkData uri=\"/bulk/7FE00010\"/>") != std::string::npos);
    CHECK(xml.find("<Item number=\"1\"><DicomAttribute tag=\"00080100\"") != std::string::npos);
}

TEST_CASE("apply_window_level is the linear function of PS3.3 C.11.2.1.2", "[dicomweb][rendered]") {
    const std::vector<uint8_t> pixels{0, 49, 50, 100, 149, 150, 200, 255};
    const auto output = apply_window_level(pixels, 8, 1, 8, false, 100.0, 101.0);
    REQUIRE(output.size() == 8);
    CHECK(output[0] == 0);
    CHECK(output[1] == 0);    // 49 <= c - 0.5 - (w-1)/2
    CHECK(output[2] == 1);    // ((50 - 99.5) / 100 + 0.5) * 255
    CHECK(output[3] == 129);  // ((100 - 99.5) / 100 + 0.5) * 255
    CHECK(output[4] == 254);
    CHECK(output[5] == 255);  // 150 > c - 0.5 + (w-1)/2
    CHECK(output[7] == 255);

    const std::vector<uint8_t> identity{0, 127, 128, 255};
    CHECK(apply_window_level(identity, 4, 1, 8, false, 128.0, 256.0) == identity);

    SECTION("a width of one is a threshold") {
        const std::vector<uint8_t> edge{99, 100, 101};
        const std::vector<uint8_t> expected{0, 255, 255};
        CHECK(apply_window_level(edge, 3, 1, 8, false, 100.0, 1.0) == expected);
    }
}

TEST_CASE("encode_png writes what a PNG reader expects", "[dicomweb][rendered]") {
    const std::vector<uint8_t> pixels{1, 2, 3, 4, 5, 6};
    const auto png = encode_png(pixels, 3, 2, 1);
    REQUIRE(png.size() > 8);
    CHECK(std::vector<uint8_t>(png.begin(), png.begin() + 8) == std::vector<uint8_t>{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'});
    CHECK(png_samples(png, 3, 2) == pixels);
    CHECK(encode_png(pixels, 4, 2, 1).empty());
    CHECK_FALSE(encode_png(pixels, 2, 1, 3).empty());
    CHECK(encode_png(pixels, 2, 2, 3).empty());
    CHECK(encode_png(pixels, 3, 2, 4).empty());

    SECTION("more data than one stored block holds") {
        std::vector<uint8_t> large(70000);
        std::iota(large.begin(), large.end(), uint8_t{0});
        const auto big = encode_png(large, 700, 100, 1);
        CHECK(big.size() > large.size());
    }
}

TEST_CASE("render_dicom_image renders the frame it was asked for", "[dicomweb][rendered]") {
    temp_directory directory;
    const auto path = write_frames(directory).string();
    rendered_params params;
    params.format = rendered_format::png;

    const auto samples_of_frame = [&](uint32_t frame) {
        params.frame = frame;
        const auto result = render_dicom_image(path, params);
        REQUIRE(result.success);
        CHECK(result.content_type == media_type::png);
        return png_samples(result.data, 2, 2);
    };

    SECTION("each frame is itself, on its own range") {
        CHECK(samples_of_frame(1) == std::vector<uint8_t>{0, 1, 2, 255});
        CHECK(samples_of_frame(2) == std::vector<uint8_t>{0, 11, 12, 255});
        CHECK(samples_of_frame(3) == std::vector<uint8_t>{0, 21, 22, 255});
    }

    SECTION("a frame past the end is not found, and not the first frame") {
        params.frame = 4;
        const auto result = render_dicom_image(path, params);
        CHECK_FALSE(result.success);
        CHECK(result.status == 404);
    }

    SECTION("MONOCHROME1 shows its lowest value as white") {
        const auto inverted = write_frames(directory, "MONOCHROME1").string();
        params.frame = 1;
        const auto result = render_dicom_image(inverted, params);
        REQUIRE(result.success);
        CHECK(png_samples(result.data, 2, 2) == std::vector<uint8_t>{255, 254, 253, 0});
    }

    SECTION("a viewport scales the image to fit, keeping its proportions") {
        params.frame = 1;
        params.viewport_width = 4;
        params.viewport_height = 4;
        const auto doubled = render_dicom_image(path, params);
        REQUIRE(doubled.success);
        CHECK(png_samples(doubled.data, 4, 4) == std::vector<uint8_t>{0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 255, 255, 2, 2, 255, 255});
        params.viewport_height = 40;
        const auto width_decides = render_dicom_image(path, params);
        REQUIRE(width_decides.success);
        CHECK(width_decides.data == doubled.data);
    }

    SECTION("JPEG is the default of the JPEG branch") {
        params.format = rendered_format::jpeg;
        params.frame = 2;
        const auto result = render_dicom_image(path, params);
        REQUIRE(result.success);
        CHECK(result.content_type == media_type::jpeg);
        CHECK(result.data[0] == 0xFF);
        CHECK(result.data[1] == 0xD8);
    }

    SECTION("a viewport nobody could need is refused before it is allocated") {
        params.frame = 1;
        params.viewport_width = 60000;
        params.viewport_height = 60000;
        CHECK_FALSE(render_dicom_image(path, params).success);
    }

    SECTION("pixels shorter than the header says are refused, not read past") {
        auto dataset = identified_dataset();
        dataset.set_numeric<uint16_t>(tags::rows, vr_type::US, 200);
        dataset.set_numeric<uint16_t>(tags::columns, vr_type::US, 200);
        dataset.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, 16);
        dataset.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, 12);
        dataset.set_numeric<uint16_t>(tags::high_bit, vr_type::US, 11);
        dataset.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, 1);
        dataset.set_string(tags::photometric_interpretation, vr_type::CS, "MONOCHROME2");
        const std::vector<uint8_t> few(16, 1);
        dataset.insert(core::dicom_element(tags::pixel_data, vr_type::OW, few));
        const auto short_file = directory.path() / "short.dcm";
        REQUIRE(core::dicom_file::create(dataset, transfer_syntax::explicit_vr_little_endian).save(short_file).is_ok());
        params.frame = 1;
        CHECK_FALSE(render_dicom_image(short_file.string(), params).success);
    }

    SECTION("an object with no pixels") {
        const auto no_pixels = (directory.path() / "text.dcm");
        REQUIRE(core::dicom_file::create(identified_dataset(), transfer_syntax::explicit_vr_little_endian).save(no_pixels).is_ok());
        CHECK_FALSE(render_dicom_image(no_pixels.string(), params).success);
    }
}

TEST_CASE("parse_rendered_params reads the window of PS3.18 8.3.5.1.4", "[dicomweb][rendered]") {
    const auto good = parse_rendered_params("/x?window=100,50.5,linear", "");
    CHECK(good.window_center == 100.0);
    CHECK(good.window_width == 50.5);
    CHECK(good.invalid_parameter.empty());
    CHECK(parse_rendered_params("window=1,0.5,sigmoid", "").window_function == "sigmoid");
    CHECK(parse_rendered_params("window=1,0.5,linear-exact", "").invalid_parameter.empty());
    for (const auto* bad : {"window=100,50", "window=a,b,linear", "window=100,0,linear", "window=100,0.5,linear", "window=100,0,sigmoid", "window=100,50,cubic"}) {
        CAPTURE(bad);
        CHECK_FALSE(parse_rendered_params(bad, "").invalid_parameter.empty());
    }
    temp_directory directory;
    const auto path = write_frames(directory).string();
    CHECK_FALSE(render_dicom_image(path, parse_rendered_params("window=1,0,linear", "")).success);
    CHECK_FALSE(parse_rendered_params("viewport=4,4,0,0,2,2", "").invalid_parameter.empty());
    CHECK(parse_rendered_params("viewport=4,4", "").invalid_parameter.empty());
}
