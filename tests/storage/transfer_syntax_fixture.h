// BSD 3-Clause License
// Copyright (c) 2026, kcenon
#pragma once

#include <kcenon/pacs/storage/storage_interface.h>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

namespace storage_syntax_test {
using namespace kcenon::pacs;

// Structural fixture only. Independent DCMTK tests verify actual codec pixels.
inline auto fragments(bool empty_bot = false) -> std::vector<uint8_t> {
    std::vector<uint8_t> bytes;
    const auto word = [&](uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            bytes.push_back(static_cast<uint8_t>(value >> shift));
    };
    word(0xE000FFFE); word(empty_bot ? 0 : 8);
    if (!empty_bot) { word(0); word(24); }
    for (uint32_t payload : {0x04030201U, 0x08070605U, 0x0C0B0A09U}) {
        word(0xE000FFFE); word(4); word(payload);
    }
    word(0xE0DDFFFE); word(0);
    return bytes;
}

inline void check_representation(storage::storage_interface& backend,
                                 core::dicom_dataset dataset) {
    const auto uid = dataset.get_string(core::tags::sop_instance_uid);
    auto expected = fragments();
    SECTION("nonempty BOT and multiple fragments") {}
    SECTION("empty BOT remains empty") { expected = fragments(true); }
    const auto pixel = core::dicom_element::from_encapsulated_pixel_data(expected);
    REQUIRE(pixel.is_ok());
    dataset.insert(pixel.value());

    // Both the legacy API and an explicitly incompatible syntax must fail
    // before creating an instance.
    REQUIRE(backend.store(dataset).is_err());
    REQUIRE(backend.store(dataset, encoding::transfer_syntax::explicit_vr_little_endian).is_err());
    CHECK_FALSE(backend.exists(uid));
    REQUIRE(backend.store(dataset, encoding::transfer_syntax::rle_lossless).is_ok());
    auto result = backend.retrieve_file(uid);
    REQUIRE(result.is_ok());
    CHECK(result.value().transfer_syntax() == encoding::transfer_syntax::rle_lossless);
    const auto* stored = result.value().dataset().get(core::tags::pixel_data);
    REQUIRE(stored != nullptr);
    CHECK(stored->is_encapsulated_pixel_data());
    CHECK(std::ranges::equal(stored->raw_data(), expected));
    CHECK(result.value().dataset().get_string(core::tags::patient_name)
          == dataset.get_string(core::tags::patient_name));
    REQUIRE(backend.remove(uid).is_ok());

    const std::vector<uint8_t> native{1,2,3,4};
    dataset.insert(core::dicom_element(core::tags::pixel_data, encoding::vr_type::OB, native));
    REQUIRE(backend.store(dataset, encoding::transfer_syntax::rle_lossless).is_err());
    CHECK_FALSE(backend.exists(uid));
    REQUIRE(backend.store(dataset).is_ok());
    auto native_result = backend.retrieve_file(uid);
    REQUIRE(native_result.is_ok());
    CHECK(native_result.value().transfer_syntax() == encoding::transfer_syntax::explicit_vr_little_endian);
    REQUIRE(native_result.value().dataset().get(core::tags::pixel_data));
    CHECK(std::ranges::equal(native_result.value().dataset().get(core::tags::pixel_data)->raw_data(), native));
}
} // namespace storage_syntax_test
