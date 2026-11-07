// BSD 3-Clause License. See LICENSE.
// Byte oracles are literal wire values, independent of the production swap table.
#include <catch2/catch_test_macros.hpp>
#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/encoding/explicit_vr_big_endian_codec.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

using namespace kcenon::pacs;
using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

namespace {
using bytes = std::vector<uint8_t>;
struct fixture {
    vr_type vr;
    bool wide;
    std::string_view little;
    std::string_view big;
};
const std::array fixtures{
    fixture{vr_type::US, false, "02010403", "01020304"},
    fixture{vr_type::SS, false, "feff0080", "fffe8000"},
    fixture{vr_type::UL, false, "0403020188776655", "0102030455667788"},
    fixture{vr_type::SL, false, "feffffff00000080", "fffffffe80000000"},
    fixture{vr_type::FL, false, "0000803f000000c0", "3f800000c0000000"},
    fixture{vr_type::FD, false, "000000000000f03f00000000000000c0", "3ff0000000000000c000000000000000"},
    fixture{vr_type::SV, true, "feffffffffffffff0807060504030201", "fffffffffffffffe0102030405060708"},
    fixture{vr_type::UV, true, "08070605040302011817161514131211", "01020304050607081112131415161718"},
    fixture{vr_type::AT, false, "1800631028001000", "0018106300280010"},
    fixture{vr_type::OW, true, "02010403", "01020304"},
    fixture{vr_type::OL, true, "0403020188776655", "0102030455667788"},
    fixture{vr_type::OF, true, "0000803f000000c0", "3f800000c0000000"},
    fixture{vr_type::OD, true, "000000000000f03f00000000000000c0", "3ff0000000000000c000000000000000"},
    fixture{vr_type::OV, true, "08070605040302011817161514131211", "01020304050607081112131415161718"},
    fixture{vr_type::OB, true, "01020304", "01020304"},
    fixture{vr_type::PN, false, "41424344", "41424344"},
};

bytes hex(std::string_view text) {
    bytes result;
    auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (size_t i = 0; i < text.size(); i += 2)
        result.push_back(static_cast<uint8_t>((digit(text[i]) << 4) | digit(text[i + 1])));
    return result;
}

void length(bytes& value, uint32_t size) {
    for (int shift : {24, 16, 8, 0}) value.push_back(static_cast<uint8_t>(size >> shift));
}

bytes wire(const fixture& value, size_t index, const bytes& payload) {
    bytes result{0x77, 0x76, 0x10, static_cast<uint8_t>(index)};
    auto vr = to_string(value.vr);
    result.push_back(static_cast<uint8_t>(vr[0]));
    result.push_back(static_cast<uint8_t>(vr[1]));
    if (value.wide) {
        result.insert(result.end(), {0, 0});
        length(result, static_cast<uint32_t>(payload.size()));
    } else {
        result.push_back(static_cast<uint8_t>(payload.size() >> 8));
        result.push_back(static_cast<uint8_t>(payload.size()));
    }
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

dicom_tag tag(size_t index) { return {0x7776, static_cast<uint16_t>(0x1000 + index)}; }

bytes part10(const bytes& value) {
    // Only the unrelated File Meta Information comes from PACS; the tested
    // dataset is assembled independently from literal Big Endian values.
    auto file = dicom_file::create({}, transfer_syntax::explicit_vr_big_endian).to_bytes();
    file.insert(file.end(), value.begin(), value.end());
    return file;
}

bytes sequence(const bytes& content, bool undefined) {
    bytes item{0xff, 0xfe, 0xe0, 0x00};
    item.reserve(content.size() + 16);
    length(item, undefined ? 0xffffffffu : static_cast<uint32_t>(content.size()));
    item.insert(item.end(), content.begin(), content.end());
    if (undefined) item.insert(item.end(), {0xff, 0xfe, 0xe0, 0x0d, 0, 0, 0, 0});
    bytes result{0x00, 0x08, 0x11, 0x15, 'S', 'Q', 0, 0};
    result.reserve(item.size() + 20);
    length(result, undefined ? 0xffffffffu : static_cast<uint32_t>(item.size()));
    result.insert(result.end(), item.begin(), item.end());
    if (undefined) result.insert(result.end(), {0xff, 0xfe, 0xe0, 0xdd, 0, 0, 0, 0});
    return result;
}
} // namespace

TEST_CASE("Big Endian values match independent intermediate bytes", "[encoding][big_endian][1293]") {
    for (size_t index = 0; index < fixtures.size(); ++index) {
        const auto& value = fixtures[index];
        CAPTURE(to_string(value.vr));
        const auto little = hex(value.little);
        const auto expected = wire(value, index, hex(value.big));
        dicom_element element{tag(index), value.vr, little};
        CHECK(explicit_vr_big_endian_codec::encode_element(element) == expected);
        dicom_dataset dataset;
        dataset.insert(element);
        auto encoded = dicom_file::create(dataset, transfer_syntax::explicit_vr_big_endian).to_bytes();
        REQUIRE(encoded.size() >= expected.size());
        CHECK(bytes(encoded.end() - static_cast<std::ptrdiff_t>(expected.size()), encoded.end()) == expected);

        auto from_file = dicom_file::from_bytes(part10(expected));
        REQUIRE(from_file.is_ok());
        REQUIRE(from_file.value().dataset().get(tag(index)) != nullptr);
        CHECK(std::ranges::equal(from_file.value().dataset().get(tag(index))->raw_data(), little));
        auto from_codec = explicit_vr_big_endian_codec::decode(expected);
        REQUIRE(from_codec.is_ok());
        CHECK(std::ranges::equal(from_codec.value().get(tag(index))->raw_data(), little));
        // Asymmetric oracle above remains even though round-trip coverage follows.
        auto restored = dicom_file::create(from_file.value().dataset(), transfer_syntax::explicit_vr_little_endian);
        auto round_trip = dicom_file::from_bytes(restored.to_bytes());
        REQUIRE(round_trip.is_ok());
        CHECK(std::ranges::equal(round_trip.value().dataset().get(tag(index))->raw_data(), little));
    }
}

TEST_CASE("Big Endian nested sequence values use the shared policy", "[encoding][big_endian][1293]") {
    for (bool undefined : {false, true}) {
        bytes content;
        for (size_t index = 0; index < fixtures.size(); ++index) {
            auto element = wire(fixtures[index], index, hex(fixtures[index].big));
            content.insert(content.end(), element.begin(), element.end());
        }
        auto nested = sequence(sequence(content, !undefined), undefined);
        auto file = dicom_file::from_bytes(part10(nested));
        auto codec = explicit_vr_big_endian_codec::decode(nested);
        REQUIRE(file.is_ok());
        REQUIRE(codec.is_ok());
        for (const auto* dataset : {&file.value().dataset(), &codec.value()}) {
            auto outer = dataset->get({0x0008, 0x1115});
            REQUIRE(outer != nullptr);
            REQUIRE(outer->sequence_items().size() == 1);
            auto inner = outer->sequence_items()[0].get({0x0008, 0x1115});
            REQUIRE(inner != nullptr);
            REQUIRE(inner->sequence_items().size() == 1);
            for (size_t index = 0; index < fixtures.size(); ++index) {
                auto value = inner->sequence_items()[0].get(tag(index));
                REQUIRE(value != nullptr);
                auto expected = hex(fixtures[index].little);
                CHECK(std::ranges::equal(value->raw_data(), expected));
            }
        }
        for (auto encoded : {file.value().to_bytes(), explicit_vr_big_endian_codec::encode(codec.value())}) {
            for (size_t index = 0; index < fixtures.size(); ++index) {
                auto expected = wire(fixtures[index], index, hex(fixtures[index].big));
                CHECK(std::search(encoded.begin(), encoded.end(), expected.begin(), expected.end()) != encoded.end());
            }
        }
    }
}

TEST_CASE("Big Endian rejects incomplete binary values at every depth", "[encoding][big_endian][1293]") {
    for (size_t index = 0; index < 14; ++index) {
        const auto& value = fixtures[index];
        CAPTURE(to_string(value.vr));
        auto malformed = hex(value.big);
        malformed.pop_back();
        if (value.vr == vr_type::AT) malformed.pop_back(); // whole 16-bit component, incomplete tag pair
        auto encoded = wire(value, index, malformed);
        for (auto input : {encoded, sequence(encoded, false), sequence(encoded, true)}) {
            auto file = dicom_file::from_bytes(part10(input));
            auto codec = explicit_vr_big_endian_codec::decode(input);
            REQUIRE(file.is_err());
            REQUIRE(codec.is_err());
            CHECK(file.error().code == error_codes::invalid_length_encoding);
            CHECK(codec.error().code == error_codes::invalid_length_encoding);
        }
        dicom_dataset dataset;
        dataset.insert(dicom_element{tag(index), value.vr, malformed});
        CHECK(dicom_file::create(dataset, transfer_syntax::explicit_vr_big_endian).to_bytes().empty());
        CHECK_THROWS_AS(explicit_vr_big_endian_codec::encode(dataset), std::invalid_argument);
        CHECK_THROWS_AS(explicit_vr_big_endian_codec::to_big_endian(value.vr, malformed), std::invalid_argument);
        CHECK_THROWS_AS(explicit_vr_big_endian_codec::from_big_endian(value.vr, malformed), std::invalid_argument);
        dicom_element item{{0x0008, 0x1115}, vr_type::SQ};
        item.add_sequence_item(dataset);
        dicom_dataset nested;
        nested.insert(std::move(item));
        CHECK(dicom_file::create(nested, transfer_syntax::explicit_vr_big_endian).to_bytes().empty());
        CHECK_THROWS_AS(explicit_vr_big_endian_codec::encode(nested), std::invalid_argument);

        auto empty = wire(value, index, {});
        CHECK(explicit_vr_big_endian_codec::to_big_endian(value.vr, {}).empty());
        CHECK(explicit_vr_big_endian_codec::decode(empty).is_ok());
        CHECK(dicom_file::from_bytes(part10(empty)).is_ok());
    }
}

TEST_CASE("OW remains word ordered with eight-bit allocation while OB is unchanged", "[encoding][big_endian][1293]") {
    const bytes little{0x02, 0x01, 0x04, 0x03};
    for (auto vr : {vr_type::OW, vr_type::OB}) {
        dicom_dataset dataset;
        dataset.set_numeric<uint16_t>({0x0028, 0x0100}, vr_type::US, 8);
        dataset.insert(dicom_element{{0x7fe0, 0x0010}, vr, little});
        const auto encoded = dicom_file::create(dataset, transfer_syntax::explicit_vr_big_endian).to_bytes();
        REQUIRE(encoded.size() >= 4);
        const bytes expected = vr == vr_type::OW ? bytes{0x01, 0x02, 0x03, 0x04} : little;
        CHECK(bytes(encoded.end() - 4, encoded.end()) == expected);
    }
}
