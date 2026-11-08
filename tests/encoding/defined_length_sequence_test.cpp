// BSD 3-Clause License. See LICENSE.
#include <catch2/catch_test_macros.hpp>
#include "kcenon/pacs/encoding/explicit_vr_codec.h"
#include "kcenon/pacs/encoding/implicit_vr_codec.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"

using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

namespace {
void append16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}
void append32(std::vector<uint8_t>& bytes, uint32_t value) {
    append16(bytes, static_cast<uint16_t>(value));
    append16(bytes, static_cast<uint16_t>(value >> 16));
}
std::vector<uint8_t> defined_sequence(bool explicit_vr, dicom_tag tag,
                                     const std::vector<uint8_t>& item, bool undefined_item) {
    std::vector<uint8_t> value;
    append16(value, 0xfffe);
    append16(value, 0xe000);
    append32(value, undefined_item ? 0xffffffff : static_cast<uint32_t>(item.size()));
    value.insert(value.end(), item.begin(), item.end());
    if (undefined_item) {
        append16(value, 0xfffe);
        append16(value, 0xe00d);
        append32(value, 0);
    }
    std::vector<uint8_t> bytes;
    append16(bytes, tag.group());
    append16(bytes, tag.element());
    if (explicit_vr) {
        bytes.insert(bytes.end(), {'S', 'Q', 0, 0});
    }
    append32(bytes, static_cast<uint32_t>(value.size()));
    bytes.insert(bytes.end(), value.begin(), value.end());
    return bytes;
}
auto encode(bool explicit_vr, const dicom_dataset& ds) {
    return explicit_vr ? explicit_vr_codec::encode(ds) : implicit_vr_codec::encode(ds);
}
auto decode(bool explicit_vr, const std::vector<uint8_t>& bytes) {
    return explicit_vr ? explicit_vr_codec::decode(bytes) : implicit_vr_codec::decode(bytes);
}
}

TEST_CASE("DIMSE codecs decode defined-length sequences with nested items",
          "[encoding][sequence][defined-length]") {
    constexpr dicom_tag image_sequence{0x0008, 0x1140};
    constexpr dicom_tag referenced_instance{0x0008, 0x1155};
    for (bool explicit_vr : {false, true}) {
        for (bool undefined_item : {false, true}) {
            CAPTURE(explicit_vr, undefined_item);
            dicom_dataset image;
            image.set_string(referenced_instance, vr_type::UI, "1.2.3.123");
            auto nested = defined_sequence(explicit_vr, image_sequence,
                                           encode(explicit_vr, image), undefined_item);
            dicom_dataset step;
            step.set_string(tags::modality, vr_type::CS, "CT");
            step.set_string(tags::scheduled_procedure_step_start_time, vr_type::TM, "091500");
            auto item_bytes = encode(explicit_vr, step);
            item_bytes.insert(item_bytes.end(), nested.begin(), nested.end());
            auto bytes = defined_sequence(explicit_vr, tags::scheduled_procedure_step_sequence,
                                          item_bytes, undefined_item);
            // The next root element must not be consumed as part of the SQ.
            dicom_dataset following;
            following.set_string(tags::patient_id, vr_type::LO, "AFTER_SEQUENCE");
            auto tail = encode(explicit_vr, following);
            bytes.insert(bytes.end(), tail.begin(), tail.end());
            auto decoded = decode(explicit_vr, bytes);
            REQUIRE(decoded.is_ok());
            CHECK(decoded.value().get_string(tags::patient_id) == "AFTER_SEQUENCE");
            const auto* steps = decoded.value().get_sequence(tags::scheduled_procedure_step_sequence);
            REQUIRE(steps != nullptr);
            REQUIRE(steps->size() == 1);
            CHECK(steps->front().get_string(tags::modality) == "CT");
            CHECK(steps->front().get_string(tags::scheduled_procedure_step_start_time) == "091500");
            const auto* images = steps->front().get_sequence(image_sequence);
            REQUIRE(images != nullptr);
            REQUIRE(images->size() == 1);
            CHECK(images->front().get_string(referenced_instance) == "1.2.3.123");
        }
    }
}

TEST_CASE("DIMSE codecs reject truncated defined sequences and missing item delimiters",
          "[encoding][sequence][defined-length]") {
    for (bool explicit_vr : {false, true}) {
        CAPTURE(explicit_vr);
        dicom_dataset step;
        step.set_string(tags::modality, vr_type::CS, "CT");
        auto bytes = defined_sequence(explicit_vr, tags::scheduled_procedure_step_sequence,
                                      encode(explicit_vr, step), false);
        bytes.pop_back();
        CHECK(decode(explicit_vr, bytes).is_err());

        bytes = defined_sequence(explicit_vr, tags::scheduled_procedure_step_sequence,
                                  encode(explicit_vr, step), true);
        const std::size_t header_size = explicit_vr ? 12 : 8;
        // Remove the item delimiter but keep the outer length self-consistent.
        bytes.resize(bytes.size() - 8);
        const auto length = static_cast<uint32_t>(bytes.size() - header_size);
        for (int i = 0; i < 4; ++i) {
            bytes[header_size - 4 + i] = static_cast<uint8_t>(length >> (i * 8));
        }
        CHECK(decode(explicit_vr, bytes).is_err());
        // A partial delimiter cannot cause an out-of-bounds span operation.
        append16(bytes, 0xfffe);
        append16(bytes, 0xe00d);
        const auto partial_length = static_cast<uint32_t>(bytes.size() - header_size);
        for (int i = 0; i < 4; ++i) {
            bytes[header_size - 4 + i] = static_cast<uint8_t>(partial_length >> (i * 8));
        }
        CHECK(decode(explicit_vr, bytes).is_err());
    }
}
