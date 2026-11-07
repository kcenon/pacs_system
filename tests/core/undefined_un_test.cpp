// BSD 3-Clause License. See LICENSE.
#include <catch2/catch_test_macros.hpp>
#include <kcenon/pacs/core/dicom_file.h>
#include <algorithm>
#include <string_view>

using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;
namespace {
using bytes = std::vector<uint8_t>;
constexpr dicom_tag unknown{0x0011, 0x1010}, nested{0x0008, 0x1115};
constexpr dicom_tag name{0x0010, 0x0010}, rows{0x0028, 0x0010}, after{0x7776, 0x0010};
const bytes fake_delimiter{0xfe, 0xff, 0xdd, 0xe0, 0, 0, 0, 0};
void append(bytes& output, const bytes& value) { output.insert(output.end(), value.begin(), value.end()); }
void number(bytes& output, uint32_t value, bool big, unsigned size) {
    for (unsigned i = 0; i < size; ++i)
        output.push_back(static_cast<uint8_t>(value >> (8 * (big ? size - i - 1 : i))));
}
bytes special(uint16_t element, uint32_t size, bool big = false) {
    bytes result;
    number(result, 0xfffe, big, 2);
    number(result, element, big, 2);
    number(result, size, big, 4);
    return result;
}
bytes implicit(dicom_tag tag, const bytes& value, bool undefined = false) {
    bytes result;
    number(result, tag.group(), false, 2);
    number(result, tag.element(), false, 2);
    number(result, undefined ? 0xffffffff : static_cast<uint32_t>(value.size()), false, 4);
    append(result, value);
    return result;
}
bytes explicit_value(dicom_tag tag, std::string_view vr, const bytes& value,
                     bool big, bool undefined = false) {
    bytes result;
    number(result, tag.group(), big, 2);
    number(result, tag.element(), big, 2);
    result.push_back(static_cast<uint8_t>(vr[0]));
    result.push_back(static_cast<uint8_t>(vr[1]));
    const bool wide = vr == "SQ" || vr == "UN";
    if (wide) number(result, 0, big, 2);
    number(result, undefined ? 0xffffffff : static_cast<uint32_t>(value.size()), big, wide ? 4 : 2);
    append(result, value);
    return result;
}
bytes item(const bytes& value, bool undefined, bool big = false) {
    auto result = special(0xe000, undefined ? 0xffffffff : static_cast<uint32_t>(value.size()), big);
    append(result, value);
    if (undefined) append(result, special(0xe00d, 0, big));
    return result;
}
bytes sequence_value() {
    auto inner = implicit(name, {'N', 'E', 'S', 'T'});
    append(inner, implicit(rows, {0x02, 0x01}));
    auto inner_sequence = item(inner, true);
    append(inner_sequence, special(0xe0dd, 0));
    auto first = implicit(nested, inner_sequence, true);
    append(first, implicit(name, {'O', 'N', 'E', ' '}));
    append(first, implicit({0x7777, 0x1010}, fake_delimiter));
    auto result = item(first, false);
    append(result, item(implicit(name, {'T', 'W', 'O', ' '}), true));
    append(result, item({}, false));
    append(result, special(0xe0dd, 0));
    return result;
}
bytes dataset(bool big, const bytes& value, bool undefined = true) {
    auto result = explicit_value(unknown, "UN", value, big, undefined);
    append(result, explicit_value(after, "LO", {'A', 'F', 'T', 'E', 'R', ' '}, big));
    append(result, explicit_value(rows, "US", big ? bytes{0x03, 0x04} : bytes{0x04, 0x03}, big));
    return result;
}
bytes part10(bool big, const bytes& data) {
    auto result = dicom_file::create({}, big ? transfer_syntax::explicit_vr_big_endian
                                            : transfer_syntax::explicit_vr_little_endian).to_bytes();
    append(result, data);
    return result;
}
void check(const dicom_dataset& ds) {
    CHECK(ds.get_string(after) == "AFTER");
    CHECK(ds.get_numeric<uint16_t>(rows) == 0x0304);
    const auto* value = ds.get(unknown);
    REQUIRE(value);
    CHECK(value->vr() == vr_type::SQ);
    const auto& items = value->sequence_items();
    REQUIRE(items.size() == 3);
    CHECK(items[0].get_string(name) == "ONE");
    CHECK(items[1].get_string(name) == "TWO");
    CHECK(items[2].empty());
    const auto* raw = items[0].get({0x7777, 0x1010});
    REQUIRE(raw);
    CHECK(std::ranges::equal(raw->raw_data(), fake_delimiter));
    auto* inner = items[0].get_sequence(nested);
    REQUIRE(inner);
    REQUIRE(inner->size() == 1);
    CHECK(inner->front().get_string(name) == "NEST");
    CHECK(inner->front().get_numeric<uint16_t>(rows) == 0x0102);
}
}

TEST_CASE("Undefined UN uses implicit LE items in either explicit byte order", "[core][dicom_file][1302]") {
    for (bool big : {false, true}) {
        CAPTURE(big);
        auto decoded = dicom_file::from_bytes(part10(big, dataset(big, sequence_value())));
        REQUIRE(decoded.is_ok());
        check(decoded.value().dataset());
        // Direct to_bytes is required: a CLI same-syntax copy cannot establish it.
        auto saved = dicom_file::from_bytes(decoded.value().to_bytes());
        REQUIRE(saved.is_ok());
        check(saved.value().dataset());
        for (auto ts : {transfer_syntax::explicit_vr_little_endian, transfer_syntax::explicit_vr_big_endian}) {
            auto converted = dicom_file::from_bytes(dicom_file::create(decoded.value().dataset(), ts).to_bytes());
            REQUIRE(converted.is_ok());
            check(converted.value().dataset());
        }
        auto empty = dicom_file::from_bytes(part10(big, dataset(big, special(0xe0dd, 0))));
        REQUIRE(empty.is_ok());
        REQUIRE(empty.value().dataset().get_sequence(unknown));
        CHECK(empty.value().dataset().get_sequence(unknown)->empty());
        CHECK(empty.value().dataset().get_string(after) == "AFTER");
    }
}

TEST_CASE("Defined UN remains an opaque byte stream", "[core][dicom_file][1302]") {
    for (bool big : {false, true}) {
        auto decoded = dicom_file::from_bytes(part10(big, dataset(big, fake_delimiter, false)));
        REQUIRE(decoded.is_ok());
        const auto* value = decoded.value().dataset().get(unknown);
        REQUIRE(value);
        CHECK(value->vr() == vr_type::UN);
        CHECK(std::ranges::equal(value->raw_data(), fake_delimiter));
        CHECK(decoded.value().dataset().get_string(after) == "AFTER");
    }
}

TEST_CASE("Undefined UN rejects incomplete item streams without partial success", "[core][dicom_file][1302]") {
    auto valid = sequence_value();
    for (bool big : {false, true}) {
        for (size_t length = 0; length < valid.size(); ++length) {
            CAPTURE(big, length);
            bytes prefix(valid.begin(), valid.begin() + static_cast<std::ptrdiff_t>(length));
            CHECK(dicom_file::from_bytes(part10(big, dataset(big, prefix))).is_err());
        }
        auto invalid = valid;
        invalid.back() = 1; // Sequence Delimitation Item length must be zero.
        CHECK(dicom_file::from_bytes(part10(big, dataset(big, invalid))).is_err());
    }
}

TEST_CASE("Undefined UN nested under explicit SQ propagates items and errors", "[core][dicom_file][1302]") {
    for (bool big : {false, true}) for (bool undefined : {false, true}) {
        for (bool malformed : {false, true}) {
            auto value = sequence_value();
            if (malformed) value.pop_back();
            auto wrapped = item(dataset(big, value), undefined, big);
            if (undefined) append(wrapped, special(0xe0dd, 0, big));
            auto decoded = dicom_file::from_bytes(part10(big,
                explicit_value(nested, "SQ", wrapped, big, undefined)));
            if (malformed) {
                CHECK(decoded.is_err());
            } else {
                REQUIRE(decoded.is_ok());
                auto* sequence = decoded.value().dataset().get_sequence(nested);
                REQUIRE(sequence);
                REQUIRE(sequence->size() == 1);
                check(sequence->front());
            }
        }
    }
}

TEST_CASE("Undefined UN bounds recursive implicit sequence nesting", "[core][dicom_file][1302]") {
    bytes content;
    for (unsigned i = 0; i < 70; ++i) {
        auto value = item(content, false);
        append(value, special(0xe0dd, 0));
        content = implicit(nested, value, true);
    }
    auto value = item(content, false);
    append(value, special(0xe0dd, 0));
    for (bool big : {false, true}) {
        auto decoded = dicom_file::from_bytes(part10(big, dataset(big, value)));
        REQUIRE(decoded.is_err());
        CHECK(decoded.error().message.find("nesting limit") != std::string::npos);
    }
}
