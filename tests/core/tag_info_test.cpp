/**
 * @file tag_info_test.cpp
 * @brief Unit tests for tag_info and value_multiplicity
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/core/tag_info.hpp"

using namespace pacs::core;

TEST_CASE("value_multiplicity default construction", "[tag_info][vm]") {
    value_multiplicity vm;

    CHECK(vm.min == 1);
    CHECK(vm.max.has_value());
    CHECK(vm.max.value() == 1);
    CHECK(vm.multiplier == 1);
}

TEST_CASE("value_multiplicity validation", "[tag_info][vm]") {
    SECTION("VM 1 - single value") {
        value_multiplicity vm{1, 1};

        CHECK(vm.is_valid(1));
        CHECK_FALSE(vm.is_valid(0));
        CHECK_FALSE(vm.is_valid(2));
        CHECK_FALSE(vm.allows_multiple());
        CHECK_FALSE(vm.is_unbounded());
    }

    SECTION("VM 1-2 - one or two values") {
        value_multiplicity vm{1, 2};

        CHECK_FALSE(vm.is_valid(0));
        CHECK(vm.is_valid(1));
        CHECK(vm.is_valid(2));
        CHECK_FALSE(vm.is_valid(3));
        CHECK(vm.allows_multiple());
        CHECK_FALSE(vm.is_unbounded());
    }

    SECTION("VM 2 - exactly two values") {
        value_multiplicity vm{2, 2};

        CHECK_FALSE(vm.is_valid(0));
        CHECK_FALSE(vm.is_valid(1));
        CHECK(vm.is_valid(2));
        CHECK_FALSE(vm.is_valid(3));
        CHECK(vm.allows_multiple());  // 2 values > 1, so it allows multiple
        CHECK_FALSE(vm.is_unbounded());
    }

    SECTION("VM 1-n - unbounded") {
        value_multiplicity vm{1, std::nullopt};

        CHECK_FALSE(vm.is_valid(0));
        CHECK(vm.is_valid(1));
        CHECK(vm.is_valid(100));
        CHECK(vm.is_valid(10000));
        CHECK(vm.allows_multiple());
        CHECK(vm.is_unbounded());
    }

    SECTION("VM 3 - exactly three values") {
        value_multiplicity vm{3, 3};

        CHECK_FALSE(vm.is_valid(2));
        CHECK(vm.is_valid(3));
        CHECK_FALSE(vm.is_valid(4));
    }

    SECTION("VM 6 - exactly six values (orientation)") {
        value_multiplicity vm{6, 6};

        CHECK_FALSE(vm.is_valid(5));
        CHECK(vm.is_valid(6));
        CHECK_FALSE(vm.is_valid(7));
    }

    SECTION("VM 2-2n - pairs only") {
        value_multiplicity vm{2, std::nullopt, 2};

        CHECK_FALSE(vm.is_valid(1));
        CHECK(vm.is_valid(2));
        CHECK_FALSE(vm.is_valid(3));  // Not a multiple of 2
        CHECK(vm.is_valid(4));
        CHECK_FALSE(vm.is_valid(5));
        CHECK(vm.is_valid(6));
        CHECK(vm.is_unbounded());
    }

    SECTION("VM 3-3n - triples only") {
        value_multiplicity vm{3, std::nullopt, 3};

        CHECK_FALSE(vm.is_valid(2));
        CHECK(vm.is_valid(3));
        CHECK_FALSE(vm.is_valid(4));
        CHECK_FALSE(vm.is_valid(5));
        CHECK(vm.is_valid(6));
        CHECK_FALSE(vm.is_valid(7));
        CHECK_FALSE(vm.is_valid(8));
        CHECK(vm.is_valid(9));
    }
}

TEST_CASE("value_multiplicity from_string parsing", "[tag_info][vm]") {
    SECTION("Single value") {
        auto vm = value_multiplicity::from_string("1");
        REQUIRE(vm.has_value());
        CHECK(vm->min == 1);
        CHECK(vm->max.value() == 1);
    }

    SECTION("Range") {
        auto vm = value_multiplicity::from_string("1-2");
        REQUIRE(vm.has_value());
        CHECK(vm->min == 1);
        CHECK(vm->max.value() == 2);
    }

    SECTION("Unbounded") {
        auto vm = value_multiplicity::from_string("1-n");
        REQUIRE(vm.has_value());
        CHECK(vm->min == 1);
        CHECK_FALSE(vm->max.has_value());
        CHECK(vm->multiplier == 1);
    }

    SECTION("Multiplier pattern") {
        auto vm = value_multiplicity::from_string("2-2n");
        REQUIRE(vm.has_value());
        CHECK(vm->min == 2);
        CHECK_FALSE(vm->max.has_value());
        CHECK(vm->multiplier == 2);
    }

    SECTION("Larger values") {
        auto vm = value_multiplicity::from_string("3-3n");
        REQUIRE(vm.has_value());
        CHECK(vm->min == 3);
        CHECK_FALSE(vm->max.has_value());
        CHECK(vm->multiplier == 3);
    }

    SECTION("Invalid strings") {
        CHECK_FALSE(value_multiplicity::from_string("").has_value());
        CHECK_FALSE(value_multiplicity::from_string("-").has_value());
        CHECK_FALSE(value_multiplicity::from_string("abc").has_value());
        CHECK_FALSE(value_multiplicity::from_string("-1").has_value());
        CHECK_FALSE(value_multiplicity::from_string("1-").has_value());
    }
}

TEST_CASE("value_multiplicity to_string conversion", "[tag_info][vm]") {
    SECTION("Single value") {
        value_multiplicity vm{1, 1};
        CHECK(vm.to_string() == "1");
    }

    SECTION("Range") {
        value_multiplicity vm{1, 3};
        CHECK(vm.to_string() == "1-3");
    }

    SECTION("Unbounded") {
        value_multiplicity vm{1, std::nullopt};
        CHECK(vm.to_string() == "1-n");
    }

    SECTION("Multiplier pattern") {
        value_multiplicity vm{2, std::nullopt, 2};
        CHECK(vm.to_string() == "2-2n");
    }
}

TEST_CASE("tag_info structure", "[tag_info]") {
    SECTION("Default tag_info is invalid") {
        tag_info info;
        CHECK_FALSE(info.is_valid());
    }

    SECTION("Constructed tag_info is valid") {
        tag_info info{
            dicom_tag{0x0010, 0x0010},
            0x504E,  // PN
            value_multiplicity{1, 1},
            "PatientName",
            "Patient's Name",
            false
        };

        CHECK(info.is_valid());
        CHECK(info.tag == dicom_tag{0x0010, 0x0010});
        CHECK(info.keyword == "PatientName");
        CHECK(info.name == "Patient's Name");
        CHECK_FALSE(info.retired);
    }

    SECTION("Retired tag") {
        tag_info info{
            dicom_tag{0x0010, 0x1000},
            0x4C4F,  // LO
            value_multiplicity{1, std::nullopt},
            "OtherPatientIDs",
            "Other Patient IDs",
            true
        };

        CHECK(info.retired);
    }

    SECTION("Comparison by tag") {
        tag_info info1{dicom_tag{0x0010, 0x0010}, 0, {}, "A", "", false};
        tag_info info2{dicom_tag{0x0010, 0x0010}, 0, {}, "B", "", false};
        tag_info info3{dicom_tag{0x0010, 0x0020}, 0, {}, "C", "", false};

        CHECK(info1 == info2);  // Same tag
        CHECK_FALSE(info1 == info3);  // Different tag
        CHECK(info1 < info3);  // Ordering by tag
    }
}
