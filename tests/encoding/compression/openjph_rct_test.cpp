// BSD 3-Clause License
// Copyright (c) 2026, PACS System Contributors
// See the LICENSE file in the project root for full license information.

#include <catch2/catch_test_macros.hpp>
#include <openjph/ojph_mem.h>
#include "ojph_colour.h"
#include "ojph_colour_local.h"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

template<typename Transformed>
void check_rct(const std::array<ojph::si32, 8>& values) {
    std::array<std::vector<ojph::si32>, 3> input, restored;
    std::array<std::vector<Transformed>, 3> transformed;
    for (const auto r : values) {
        for (const auto g : values) {
            for (const auto b : values) {
                input[0].push_back(r);
                input[1].push_back(g);
                input[2].push_back(b);
            }
        }
    }
    const auto count = input[0].size();
    std::array<ojph::line_buf, 3> in_lines, out_lines, restored_lines;
    for (std::size_t c = 0; c < 3; ++c) {
        transformed[c].resize(count);
        restored[c].resize(count);
        in_lines[c].wrap(input[c].data(), count, 0);
        out_lines[c].wrap(transformed[c].data(), count, 0);
        restored_lines[c].wrap(restored[c].data(), count, 0);
    }

    // Call the generic functions explicitly, including on SIMD-capable hosts.
    ojph::local::gen_rct_forward(&in_lines[0], &in_lines[1], &in_lines[2],
                                &out_lines[0], &out_lines[1], &out_lines[2],
                                static_cast<ojph::ui32>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const std::int64_t r = input[0][i], g = input[1][i], b = input[2][i];
        const auto sum = r + 2 * g + b;
        // Explicit floor division: no signed shift in the independent oracle.
        const auto expected_y = sum / 4 - (sum < 0 && sum % 4 != 0 ? 1 : 0);
        INFO("R=" << r << " G=" << g << " B=" << b);
        CHECK(transformed[0][i] == expected_y);
        CHECK(transformed[1][i] == b - g);
        CHECK(transformed[2][i] == r - g);
    }
    ojph::local::gen_rct_backward(&out_lines[0], &out_lines[1], &out_lines[2],
                                 &restored_lines[0], &restored_lines[1], &restored_lines[2],
                                 static_cast<ojph::ui32>(count));
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            CHECK(restored[c][i] == input[c][i]);
        }
    }
}

} // namespace

TEST_CASE("OpenJPH generic RCT 32-bit bounds and reversibility", "[htj2k][openjph-rct]") {
    // A signed 30-bit input keeps R + 2G + B within int32_t, including every
    // intermediate addition. This contains the PACS 8/12/16-bit sample ranges.
    constexpr ojph::si32 bound = ojph::si32{1} << 29;
    check_rct<ojph::si32>({-bound, -32768, -128, -1, 0, 127, 32767, bound - 1});
}

TEST_CASE("OpenJPH generic RCT 64-bit bounds and reversibility", "[htj2k][openjph-rct]") {
    // OpenJPH's 64-bit transformed lines still consume signed 32-bit samples.
    // The weighted sum is in [-2^33, 2^33-4], so multiplication/addition fit.
    constexpr auto low = std::numeric_limits<ojph::si32>::min();
    constexpr auto high = std::numeric_limits<ojph::si32>::max();
    check_rct<ojph::si64>({low, low + 1, -128, -1, 0, 127, high - 1, high});
}

TEST_CASE("OpenJPH ARM dispatch selects generic RCT", "[htj2k][openjph-rct]") {
    ojph::local::init_colour_transform_functions();
    REQUIRE(ojph::local::rct_forward != nullptr);
#if defined(__aarch64__) || defined(_M_ARM64)
    CHECK(ojph::local::rct_forward == ojph::local::gen_rct_forward);
#endif
}
