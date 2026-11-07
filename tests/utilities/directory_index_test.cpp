// BSD 3-Clause License. See LICENSE.
#include <catch2/catch_test_macros.hpp>
#include "../../tools/dcm_dir/directory_index.h"

using bytes = std::vector<uint8_t>;
using pacs::tools::directory::item_offsets;
namespace {
void u32(bytes& data, uint32_t n) {
    for (unsigned shift : {0, 8, 16, 24}) data.push_back(static_cast<uint8_t>(n >> shift));
}
bytes file(bytes payload) {
    bytes data(128);
    data.insert(data.end(), {'D', 'I', 'C', 'M'});
    data.insert(data.end(), payload.begin(), payload.end());
    return data;
}
bytes sequence(bool undefined_sequence, bool undefined_item) {
    bytes item{0xfe, 0xff, 0x00, 0xe0};
    u32(item, undefined_item ? 0xffffffff : 10);
    item.insert(item.end(), {0x04, 0x00, 0x30, 0x14, 'C', 'S', 0x02, 0x00, 'A', 'B'});
    if (undefined_item) item.insert(item.end(), {0xfe, 0xff, 0x0d, 0xe0, 0, 0, 0, 0});
    bytes data{0x04, 0x00, 0x20, 0x12, 'S', 'Q', 0, 0};
    u32(data, undefined_sequence ? 0xffffffff : static_cast<uint32_t>(item.size()));
    data.insert(data.end(), item.begin(), item.end());
    if (undefined_sequence) data.insert(data.end(), {0xfe, 0xff, 0xdd, 0xe0, 0, 0, 0, 0});
    return data;
}
}

TEST_CASE("DICOMDIR scanner indexes mixed defined and undefined Items", "[utilities][dicomdir][1301]") {
    for (bool sq : {false, true}) for (bool item : {false, true}) {
        auto bytes = file(sequence(sq, item));
        auto result = item_offsets(bytes);
        REQUIRE(result.is_ok());
        CHECK(result.value() == std::vector<uint32_t>{144});
        // Every strict prefix of this single-sequence file is incomplete.
        for (size_t length = 0; length < bytes.size(); ++length)
            CHECK(item_offsets(std::span(bytes).first(length)).is_err());
    }
}

TEST_CASE("DICOMDIR scanner ignores Item-like bytes inside values", "[utilities][dicomdir][1301]") {
    bytes payload{0x02, 0x00, 0x02, 0x10, 'O', 'B', 0, 0, 8, 0, 0, 0,
                  0xfe, 0xff, 0x00, 0xe0, 0, 0, 0, 0};
    auto sq = sequence(false, false);
    payload.insert(payload.end(), sq.begin(), sq.end());
    auto result = item_offsets(file(payload));
    REQUIRE(result.is_ok());
    CHECK(result.value() == std::vector<uint32_t>{164});
}

TEST_CASE("DICOMDIR scanner rejects malformed lengths delimiters and duplicate sequences", "[utilities][dicomdir][1301]") {
    auto base = file(sequence(true, true));
    for (auto [offset, value] : std::vector<std::pair<size_t, uint8_t>>{
             {138, 1}, {140, 0xfd}, {146, 1}, {150, 0xfe}, {164, 0xdd}, {166, 1}, {174, 1}}) {
        auto bad = base;
        REQUIRE(offset < bad.size());
        bad[offset] = value;
        CHECK(item_offsets(bad).is_err());
    }
    auto duplicate = sequence(false, false);
    base.insert(base.end(), duplicate.begin(), duplicate.end());
    CHECK(item_offsets(base).is_err());
}

TEST_CASE("DICOMDIR scanner bounds nested sequences", "[utilities][dicomdir][1301]") {
    bytes nested;
    for (size_t i = 0; i < 34; ++i) {
        bytes data{0x04, 0x00, 0x20, 0x12, 'S', 'Q', 0, 0};
        u32(data, static_cast<uint32_t>(nested.size() + 8));
        data.insert(data.end(), {0xfe, 0xff, 0x00, 0xe0});
        u32(data, static_cast<uint32_t>(nested.size()));
        data.insert(data.end(), nested.begin(), nested.end());
        nested = std::move(data);
    }
    CHECK(item_offsets(file(nested)).is_err());
}
