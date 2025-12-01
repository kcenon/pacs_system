/**
 * @file byte_swap_test.cpp
 * @brief Unit tests for byte swapping utilities
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/encoding/byte_swap.hpp>

#include <cstring>

using namespace pacs::encoding;

// ============================================================================
// Single Value Byte Swapping Tests
// ============================================================================

TEST_CASE("byte_swap16 swaps bytes correctly", "[encoding][byte_swap]") {
    SECTION("basic swap") {
        CHECK(byte_swap16(0x1234) == 0x3412);
        CHECK(byte_swap16(0xABCD) == 0xCDAB);
    }

    SECTION("edge cases") {
        CHECK(byte_swap16(0x0000) == 0x0000);
        CHECK(byte_swap16(0xFFFF) == 0xFFFF);
        CHECK(byte_swap16(0x00FF) == 0xFF00);
        CHECK(byte_swap16(0xFF00) == 0x00FF);
    }

    SECTION("double swap returns original") {
        CHECK(byte_swap16(byte_swap16(0x1234)) == 0x1234);
        CHECK(byte_swap16(byte_swap16(0xABCD)) == 0xABCD);
    }
}

TEST_CASE("byte_swap32 swaps bytes correctly", "[encoding][byte_swap]") {
    SECTION("basic swap") {
        CHECK(byte_swap32(0x12345678) == 0x78563412);
        CHECK(byte_swap32(0xAABBCCDD) == 0xDDCCBBAA);
    }

    SECTION("edge cases") {
        CHECK(byte_swap32(0x00000000) == 0x00000000);
        CHECK(byte_swap32(0xFFFFFFFF) == 0xFFFFFFFF);
        CHECK(byte_swap32(0x000000FF) == 0xFF000000);
        CHECK(byte_swap32(0xFF000000) == 0x000000FF);
    }

    SECTION("double swap returns original") {
        CHECK(byte_swap32(byte_swap32(0x12345678)) == 0x12345678);
        CHECK(byte_swap32(byte_swap32(0xAABBCCDD)) == 0xAABBCCDD);
    }
}

TEST_CASE("byte_swap64 swaps bytes correctly", "[encoding][byte_swap]") {
    SECTION("basic swap") {
        CHECK(byte_swap64(0x123456789ABCDEF0ULL) == 0xF0DEBC9A78563412ULL);
        CHECK(byte_swap64(0xAABBCCDDEEFF0011ULL) == 0x1100FFEEDDCCBBAAULL);
    }

    SECTION("edge cases") {
        CHECK(byte_swap64(0x0000000000000000ULL) == 0x0000000000000000ULL);
        CHECK(byte_swap64(0xFFFFFFFFFFFFFFFFULL) == 0xFFFFFFFFFFFFFFFFULL);
        CHECK(byte_swap64(0x00000000000000FFULL) == 0xFF00000000000000ULL);
    }

    SECTION("double swap returns original") {
        CHECK(byte_swap64(byte_swap64(0x123456789ABCDEF0ULL)) == 0x123456789ABCDEF0ULL);
    }
}

// ============================================================================
// Big Endian Read Tests
// ============================================================================

TEST_CASE("read_be16 reads big-endian correctly", "[encoding][byte_swap]") {
    SECTION("basic read") {
        uint8_t data[] = {0x12, 0x34};
        CHECK(read_be16(data) == 0x1234);
    }

    SECTION("high byte first") {
        uint8_t data[] = {0xAB, 0xCD};
        CHECK(read_be16(data) == 0xABCD);
    }

    SECTION("zero values") {
        uint8_t data[] = {0x00, 0x00};
        CHECK(read_be16(data) == 0x0000);
    }
}

TEST_CASE("read_be32 reads big-endian correctly", "[encoding][byte_swap]") {
    SECTION("basic read") {
        uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
        CHECK(read_be32(data) == 0x12345678);
    }

    SECTION("all bytes different") {
        uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
        CHECK(read_be32(data) == 0xAABBCCDD);
    }
}

TEST_CASE("read_be64 reads big-endian correctly", "[encoding][byte_swap]") {
    SECTION("basic read") {
        uint8_t data[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
        CHECK(read_be64(data) == 0x123456789ABCDEF0ULL);
    }
}

// ============================================================================
// Big Endian Write Tests
// ============================================================================

TEST_CASE("write_be16 writes big-endian correctly", "[encoding][byte_swap]") {
    SECTION("basic write") {
        std::vector<uint8_t> buffer;
        write_be16(buffer, 0x1234);
        REQUIRE(buffer.size() == 2);
        CHECK(buffer[0] == 0x12);
        CHECK(buffer[1] == 0x34);
    }

    SECTION("appends to existing buffer") {
        std::vector<uint8_t> buffer = {0xFF};
        write_be16(buffer, 0xABCD);
        REQUIRE(buffer.size() == 3);
        CHECK(buffer[0] == 0xFF);
        CHECK(buffer[1] == 0xAB);
        CHECK(buffer[2] == 0xCD);
    }
}

TEST_CASE("write_be32 writes big-endian correctly", "[encoding][byte_swap]") {
    SECTION("basic write") {
        std::vector<uint8_t> buffer;
        write_be32(buffer, 0x12345678);
        REQUIRE(buffer.size() == 4);
        CHECK(buffer[0] == 0x12);
        CHECK(buffer[1] == 0x34);
        CHECK(buffer[2] == 0x56);
        CHECK(buffer[3] == 0x78);
    }
}

TEST_CASE("write_be64 writes big-endian correctly", "[encoding][byte_swap]") {
    SECTION("basic write") {
        std::vector<uint8_t> buffer;
        write_be64(buffer, 0x123456789ABCDEF0ULL);
        REQUIRE(buffer.size() == 8);
        CHECK(buffer[0] == 0x12);
        CHECK(buffer[1] == 0x34);
        CHECK(buffer[2] == 0x56);
        CHECK(buffer[3] == 0x78);
        CHECK(buffer[4] == 0x9A);
        CHECK(buffer[5] == 0xBC);
        CHECK(buffer[6] == 0xDE);
        CHECK(buffer[7] == 0xF0);
    }
}

// ============================================================================
// Bulk Byte Swapping Tests
// ============================================================================

TEST_CASE("swap_ow_bytes swaps 16-bit words", "[encoding][byte_swap]") {
    SECTION("single word") {
        std::vector<uint8_t> data = {0x12, 0x34};
        auto result = swap_ow_bytes(data);
        REQUIRE(result.size() == 2);
        CHECK(result[0] == 0x34);
        CHECK(result[1] == 0x12);
    }

    SECTION("multiple words") {
        std::vector<uint8_t> data = {0x12, 0x34, 0xAB, 0xCD};
        auto result = swap_ow_bytes(data);
        REQUIRE(result.size() == 4);
        CHECK(result[0] == 0x34);
        CHECK(result[1] == 0x12);
        CHECK(result[2] == 0xCD);
        CHECK(result[3] == 0xAB);
    }

    SECTION("empty data") {
        std::vector<uint8_t> data;
        auto result = swap_ow_bytes(data);
        CHECK(result.empty());
    }

    SECTION("double swap returns original") {
        std::vector<uint8_t> original = {0x12, 0x34, 0xAB, 0xCD};
        auto swapped = swap_ow_bytes(original);
        auto restored = swap_ow_bytes(swapped);
        CHECK(restored == original);
    }
}

TEST_CASE("swap_ol_bytes swaps 32-bit values", "[encoding][byte_swap]") {
    SECTION("single value") {
        std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};
        auto result = swap_ol_bytes(data);
        REQUIRE(result.size() == 4);
        CHECK(result[0] == 0x78);
        CHECK(result[1] == 0x56);
        CHECK(result[2] == 0x34);
        CHECK(result[3] == 0x12);
    }

    SECTION("multiple values") {
        std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78, 0xAA, 0xBB, 0xCC, 0xDD};
        auto result = swap_ol_bytes(data);
        REQUIRE(result.size() == 8);
        CHECK(result[0] == 0x78);
        CHECK(result[1] == 0x56);
        CHECK(result[2] == 0x34);
        CHECK(result[3] == 0x12);
        CHECK(result[4] == 0xDD);
        CHECK(result[5] == 0xCC);
        CHECK(result[6] == 0xBB);
        CHECK(result[7] == 0xAA);
    }

    SECTION("double swap returns original") {
        std::vector<uint8_t> original = {0x12, 0x34, 0x56, 0x78};
        auto swapped = swap_ol_bytes(original);
        auto restored = swap_ol_bytes(swapped);
        CHECK(restored == original);
    }
}

TEST_CASE("swap_od_bytes swaps 64-bit values", "[encoding][byte_swap]") {
    SECTION("single value") {
        std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
        auto result = swap_od_bytes(data);
        REQUIRE(result.size() == 8);
        CHECK(result[0] == 0xF0);
        CHECK(result[1] == 0xDE);
        CHECK(result[2] == 0xBC);
        CHECK(result[3] == 0x9A);
        CHECK(result[4] == 0x78);
        CHECK(result[5] == 0x56);
        CHECK(result[6] == 0x34);
        CHECK(result[7] == 0x12);
    }

    SECTION("double swap returns original") {
        std::vector<uint8_t> original = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0};
        auto swapped = swap_od_bytes(original);
        auto restored = swap_od_bytes(swapped);
        CHECK(restored == original);
    }
}

// ============================================================================
// Constexpr Tests
// ============================================================================

TEST_CASE("byte swap functions are constexpr", "[encoding][byte_swap]") {
    SECTION("compile-time evaluation") {
        static_assert(byte_swap16(0x1234) == 0x3412);
        static_assert(byte_swap32(0x12345678) == 0x78563412);
        static_assert(byte_swap64(0x123456789ABCDEF0ULL) == 0xF0DEBC9A78563412ULL);
        // Note: read_be16 constexpr test requires constant array, not reinterpret_cast
        constexpr uint8_t test_data[] = {0x12, 0x34};
        static_assert(read_be16(test_data) == 0x1234);
    }
}

// ============================================================================
// Round-Trip with Read/Write Tests
// ============================================================================

TEST_CASE("read/write big-endian round-trip", "[encoding][byte_swap]") {
    SECTION("16-bit round-trip") {
        std::vector<uint8_t> buffer;
        write_be16(buffer, 0xABCD);
        CHECK(read_be16(buffer.data()) == 0xABCD);
    }

    SECTION("32-bit round-trip") {
        std::vector<uint8_t> buffer;
        write_be32(buffer, 0x12345678);
        CHECK(read_be32(buffer.data()) == 0x12345678);
    }

    SECTION("64-bit round-trip") {
        std::vector<uint8_t> buffer;
        write_be64(buffer, 0x123456789ABCDEF0ULL);
        CHECK(read_be64(buffer.data()) == 0x123456789ABCDEF0ULL);
    }
}

// ============================================================================
// DICOM-Specific VR Swap Tests
// ============================================================================

TEST_CASE("swap_at_bytes swaps attribute tag correctly", "[encoding][byte_swap]") {
    SECTION("AT is two 16-bit values") {
        // AT (0010,0020) stored as little-endian: 10 00 20 00
        // After swap for big-endian: 00 10 00 20
        std::vector<uint8_t> le_data = {0x10, 0x00, 0x20, 0x00};
        auto be_data = swap_at_bytes(le_data);
        REQUIRE(be_data.size() == 4);
        CHECK(be_data[0] == 0x00);
        CHECK(be_data[1] == 0x10);
        CHECK(be_data[2] == 0x00);
        CHECK(be_data[3] == 0x20);
    }
}

TEST_CASE("swap_us_bytes swaps unsigned short correctly", "[encoding][byte_swap]") {
    // US value 512 = 0x0200 stored LE: 00 02
    std::vector<uint8_t> le_data = {0x00, 0x02};
    auto be_data = swap_us_bytes(le_data);
    CHECK(be_data[0] == 0x02);
    CHECK(be_data[1] == 0x00);
}

TEST_CASE("swap_ul_bytes swaps unsigned long correctly", "[encoding][byte_swap]") {
    // UL value 0x12345678 stored LE: 78 56 34 12
    std::vector<uint8_t> le_data = {0x78, 0x56, 0x34, 0x12};
    auto be_data = swap_ul_bytes(le_data);
    CHECK(be_data[0] == 0x12);
    CHECK(be_data[1] == 0x34);
    CHECK(be_data[2] == 0x56);
    CHECK(be_data[3] == 0x78);
}

TEST_CASE("swap_fl_bytes swaps float correctly", "[encoding][byte_swap]") {
    // Same byte layout as UL
    std::vector<uint8_t> le_data = {0x00, 0x00, 0x80, 0x3F};  // 1.0f in LE
    auto be_data = swap_fl_bytes(le_data);
    CHECK(be_data[0] == 0x3F);
    CHECK(be_data[1] == 0x80);
    CHECK(be_data[2] == 0x00);
    CHECK(be_data[3] == 0x00);
}

TEST_CASE("swap_fd_bytes swaps double correctly", "[encoding][byte_swap]") {
    // 1.0 as double in LE: 00 00 00 00 00 00 F0 3F
    std::vector<uint8_t> le_data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x3F};
    auto be_data = swap_fd_bytes(le_data);
    CHECK(be_data[0] == 0x3F);
    CHECK(be_data[1] == 0xF0);
    CHECK(be_data[2] == 0x00);
    CHECK(be_data[3] == 0x00);
    CHECK(be_data[4] == 0x00);
    CHECK(be_data[5] == 0x00);
    CHECK(be_data[6] == 0x00);
    CHECK(be_data[7] == 0x00);
}
