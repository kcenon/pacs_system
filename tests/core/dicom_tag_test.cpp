/**
 * @file dicom_tag_test.cpp
 * @brief Unit tests for dicom_tag class
 */

#include <catch2/catch_test_macros.hpp>
#include <unordered_map>
#include <unordered_set>

#include "pacs/core/dicom_tag.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

using namespace pacs::core;

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("dicom_tag default constructor", "[dicom_tag][construction]") {
    const dicom_tag tag;

    CHECK(tag.group() == 0);
    CHECK(tag.element() == 0);
    CHECK(tag.combined() == 0);
}

TEST_CASE("dicom_tag component constructor", "[dicom_tag][construction]") {
    SECTION("standard tag") {
        const dicom_tag tag{0x0010, 0x0020};

        CHECK(tag.group() == 0x0010);
        CHECK(tag.element() == 0x0020);
        CHECK(tag.combined() == 0x00100020);
    }

    SECTION("maximum values") {
        const dicom_tag tag{0xFFFF, 0xFFFF};

        CHECK(tag.group() == 0xFFFF);
        CHECK(tag.element() == 0xFFFF);
        CHECK(tag.combined() == 0xFFFFFFFF);
    }

    SECTION("zero element") {
        const dicom_tag tag{0x0008, 0x0000};

        CHECK(tag.group() == 0x0008);
        CHECK(tag.element() == 0x0000);
    }
}

TEST_CASE("dicom_tag combined constructor", "[dicom_tag][construction]") {
    const dicom_tag tag{static_cast<uint32_t>(0x00100020)};

    CHECK(tag.group() == 0x0010);
    CHECK(tag.element() == 0x0020);
    CHECK(tag.combined() == 0x00100020);
}

TEST_CASE("dicom_tag constexpr construction", "[dicom_tag][construction]") {
    // Verify compile-time construction
    constexpr dicom_tag tag{0x0010, 0x0010};
    static_assert(tag.group() == 0x0010);
    static_assert(tag.element() == 0x0010);
    static_assert(tag.combined() == 0x00100010);
}

// ============================================================================
// String Conversion Tests
// ============================================================================

TEST_CASE("dicom_tag to_string", "[dicom_tag][string]") {
    SECTION("standard format") {
        const dicom_tag tag{0x0010, 0x0020};
        CHECK(tag.to_string() == "(0010,0020)");
    }

    SECTION("zero tag") {
        const dicom_tag tag{0x0000, 0x0000};
        CHECK(tag.to_string() == "(0000,0000)");
    }

    SECTION("maximum values") {
        const dicom_tag tag{0xFFFF, 0xFFFF};
        CHECK(tag.to_string() == "(FFFF,FFFF)");
    }

    SECTION("common tags") {
        CHECK(tags::patient_name.to_string() == "(0010,0010)");
        CHECK(tags::study_instance_uid.to_string() == "(0020,000D)");
        CHECK(tags::pixel_data.to_string() == "(7FE0,0010)");
    }
}

TEST_CASE("dicom_tag from_string with parentheses", "[dicom_tag][string]") {
    SECTION("valid format") {
        const auto tag = dicom_tag::from_string("(0010,0020)");
        REQUIRE(tag.has_value());
        CHECK(tag->group() == 0x0010);
        CHECK(tag->element() == 0x0020);
    }

    SECTION("lowercase hex") {
        const auto tag = dicom_tag::from_string("(00ff,00ab)");
        REQUIRE(tag.has_value());
        CHECK(tag->group() == 0x00FF);
        CHECK(tag->element() == 0x00AB);
    }

    SECTION("mixed case hex") {
        const auto tag = dicom_tag::from_string("(00Ff,00Ab)");
        REQUIRE(tag.has_value());
        CHECK(tag->group() == 0x00FF);
        CHECK(tag->element() == 0x00AB);
    }
}

TEST_CASE("dicom_tag from_string compact format", "[dicom_tag][string]") {
    SECTION("valid format") {
        const auto tag = dicom_tag::from_string("00100020");
        REQUIRE(tag.has_value());
        CHECK(tag->group() == 0x0010);
        CHECK(tag->element() == 0x0020);
    }

    SECTION("pixel data tag") {
        const auto tag = dicom_tag::from_string("7FE00010");
        REQUIRE(tag.has_value());
        CHECK(*tag == tags::pixel_data);
    }
}

TEST_CASE("dicom_tag from_string without parentheses", "[dicom_tag][string]") {
    const auto tag = dicom_tag::from_string("0010,0020");
    REQUIRE(tag.has_value());
    CHECK(tag->group() == 0x0010);
    CHECK(tag->element() == 0x0020);
}

TEST_CASE("dicom_tag from_string with whitespace", "[dicom_tag][string]") {
    SECTION("leading whitespace") {
        const auto tag = dicom_tag::from_string("  (0010,0020)");
        REQUIRE(tag.has_value());
        CHECK(tag->combined() == 0x00100020);
    }

    SECTION("trailing whitespace") {
        const auto tag = dicom_tag::from_string("(0010,0020)  ");
        REQUIRE(tag.has_value());
        CHECK(tag->combined() == 0x00100020);
    }

    SECTION("both sides whitespace") {
        const auto tag = dicom_tag::from_string("  (0010,0020)  ");
        REQUIRE(tag.has_value());
        CHECK(tag->combined() == 0x00100020);
    }
}

TEST_CASE("dicom_tag from_string invalid input", "[dicom_tag][string]") {
    SECTION("empty string") {
        CHECK(!dicom_tag::from_string("").has_value());
    }

    SECTION("only whitespace") {
        CHECK(!dicom_tag::from_string("   ").has_value());
    }

    SECTION("invalid characters") {
        CHECK(!dicom_tag::from_string("(GGGG,HHHH)").has_value());
    }

    SECTION("wrong length") {
        CHECK(!dicom_tag::from_string("(0010,002)").has_value());
        CHECK(!dicom_tag::from_string("0010002").has_value());
    }

    SECTION("missing comma") {
        CHECK(!dicom_tag::from_string("(00100020)").has_value());
    }

    SECTION("missing parentheses") {
        CHECK(!dicom_tag::from_string("0010,0020)").has_value());
        CHECK(!dicom_tag::from_string("(0010,0020").has_value());
    }

    SECTION("random text") {
        CHECK(!dicom_tag::from_string("invalid").has_value());
        CHECK(!dicom_tag::from_string("patient_name").has_value());
    }
}

TEST_CASE("dicom_tag roundtrip conversion", "[dicom_tag][string]") {
    const std::vector<dicom_tag> test_tags = {
        {0x0000, 0x0000},
        {0x0010, 0x0020},
        {0x7FE0, 0x0010},
        {0xFFFF, 0xFFFF},
        tags::patient_name,
        tags::study_instance_uid,
        tags::pixel_data,
    };

    for (const auto& original : test_tags) {
        const auto str = original.to_string();
        const auto parsed = dicom_tag::from_string(str);
        REQUIRE(parsed.has_value());
        CHECK(*parsed == original);
    }
}

// ============================================================================
// Private Tag Detection Tests
// ============================================================================

TEST_CASE("dicom_tag is_private", "[dicom_tag][private]") {
    SECTION("standard tags are not private") {
        CHECK(!tags::patient_name.is_private());
        CHECK(!tags::patient_id.is_private());
        CHECK(!tags::study_instance_uid.is_private());
        CHECK(!tags::pixel_data.is_private());
    }

    SECTION("odd group > 0x0008 is private") {
        CHECK(dicom_tag{0x0009, 0x0010}.is_private());
        CHECK(dicom_tag{0x0011, 0x0020}.is_private());
        CHECK(dicom_tag{0x00FF, 0x0001}.is_private());
        CHECK(dicom_tag{0x7FE1, 0x0010}.is_private());
    }

    SECTION("group 0x0001 is not private") {
        CHECK(!dicom_tag{0x0001, 0x0000}.is_private());
    }

    SECTION("group 0x0003 is not private (reserved)") {
        CHECK(!dicom_tag{0x0003, 0x0000}.is_private());
    }

    SECTION("group 0x0005 is not private (reserved)") {
        CHECK(!dicom_tag{0x0005, 0x0000}.is_private());
    }

    SECTION("group 0x0007 is not private (reserved)") {
        CHECK(!dicom_tag{0x0007, 0x0000}.is_private());
    }
}

TEST_CASE("dicom_tag is_group_length", "[dicom_tag][classification]") {
    CHECK(dicom_tag{0x0008, 0x0000}.is_group_length());
    CHECK(dicom_tag{0x0010, 0x0000}.is_group_length());
    CHECK(!dicom_tag{0x0010, 0x0010}.is_group_length());
    CHECK(!tags::patient_name.is_group_length());
}

TEST_CASE("dicom_tag is_private_creator", "[dicom_tag][private]") {
    SECTION("private creator range") {
        CHECK(dicom_tag{0x0009, 0x0010}.is_private_creator());
        CHECK(dicom_tag{0x0009, 0x00FF}.is_private_creator());
        CHECK(dicom_tag{0x0011, 0x0050}.is_private_creator());
    }

    SECTION("not private creator") {
        // Element below 0x0010
        CHECK(!dicom_tag{0x0009, 0x0000}.is_private_creator());
        CHECK(!dicom_tag{0x0009, 0x000F}.is_private_creator());

        // Element above 0x00FF
        CHECK(!dicom_tag{0x0009, 0x0100}.is_private_creator());
        CHECK(!dicom_tag{0x0009, 0x1010}.is_private_creator());

        // Standard group (not private)
        CHECK(!dicom_tag{0x0010, 0x0010}.is_private_creator());
    }
}

// ============================================================================
// Comparison Tests
// ============================================================================

TEST_CASE("dicom_tag equality comparison", "[dicom_tag][comparison]") {
    const dicom_tag a{0x0010, 0x0010};
    const dicom_tag b{0x0010, 0x0010};
    const dicom_tag c{0x0010, 0x0020};

    CHECK(a == b);
    CHECK(!(a == c));
    CHECK(a != c);
    CHECK(!(a != b));
}

TEST_CASE("dicom_tag ordering comparison", "[dicom_tag][comparison]") {
    const dicom_tag a{0x0010, 0x0010};
    const dicom_tag b{0x0010, 0x0020};
    const dicom_tag c{0x0020, 0x0010};

    SECTION("less than") {
        CHECK(a < b);
        CHECK(b < c);
        CHECK(a < c);
    }

    SECTION("greater than") {
        CHECK(b > a);
        CHECK(c > b);
        CHECK(c > a);
    }

    SECTION("less than or equal") {
        CHECK(a <= a);
        CHECK(a <= b);
    }

    SECTION("greater than or equal") {
        CHECK(b >= b);
        CHECK(b >= a);
    }

    SECTION("spaceship operator") {
        CHECK((a <=> a) == std::strong_ordering::equal);
        CHECK((a <=> b) == std::strong_ordering::less);
        CHECK((b <=> a) == std::strong_ordering::greater);
    }
}

// ============================================================================
// Hash Tests
// ============================================================================

TEST_CASE("dicom_tag hash function", "[dicom_tag][hash]") {
    const std::hash<dicom_tag> hasher;

    SECTION("equal tags have equal hashes") {
        const dicom_tag a{0x0010, 0x0010};
        const dicom_tag b{0x0010, 0x0010};
        CHECK(hasher(a) == hasher(b));
    }

    SECTION("different tags likely have different hashes") {
        const dicom_tag a{0x0010, 0x0010};
        const dicom_tag b{0x0010, 0x0020};
        // Not guaranteed, but highly likely
        CHECK(hasher(a) != hasher(b));
    }
}

TEST_CASE("dicom_tag in unordered_set", "[dicom_tag][hash]") {
    std::unordered_set<dicom_tag> tag_set;

    tag_set.insert(tags::patient_name);
    tag_set.insert(tags::patient_id);
    tag_set.insert(tags::study_instance_uid);

    CHECK(tag_set.size() == 3);
    CHECK(tag_set.contains(tags::patient_name));
    CHECK(tag_set.contains(tags::patient_id));
    CHECK(!tag_set.contains(tags::series_instance_uid));

    // Insert duplicate
    tag_set.insert(tags::patient_name);
    CHECK(tag_set.size() == 3);
}

TEST_CASE("dicom_tag in unordered_map", "[dicom_tag][hash]") {
    std::unordered_map<dicom_tag, std::string> tag_map;

    tag_map[tags::patient_name] = "John Doe";
    tag_map[tags::patient_id] = "12345";

    CHECK(tag_map.size() == 2);
    CHECK(tag_map[tags::patient_name] == "John Doe");
    CHECK(tag_map[tags::patient_id] == "12345");
}

// ============================================================================
// Tag Constants Tests
// ============================================================================

TEST_CASE("dicom_tag constants are correct", "[dicom_tag][constants]") {
    // File Meta Information
    CHECK(tags::transfer_syntax_uid == dicom_tag{0x0002, 0x0010});

    // Patient Module
    CHECK(tags::patient_name == dicom_tag{0x0010, 0x0010});
    CHECK(tags::patient_id == dicom_tag{0x0010, 0x0020});
    CHECK(tags::patient_birth_date == dicom_tag{0x0010, 0x0030});
    CHECK(tags::patient_sex == dicom_tag{0x0010, 0x0040});

    // Study/Series Identification
    CHECK(tags::study_instance_uid == dicom_tag{0x0020, 0x000D});
    CHECK(tags::series_instance_uid == dicom_tag{0x0020, 0x000E});
    CHECK(tags::study_id == dicom_tag{0x0020, 0x0010});

    // SOP Common
    CHECK(tags::sop_class_uid == dicom_tag{0x0008, 0x0016});
    CHECK(tags::sop_instance_uid == dicom_tag{0x0008, 0x0018});
    CHECK(tags::modality == dicom_tag{0x0008, 0x0060});

    // Pixel Data
    CHECK(tags::pixel_data == dicom_tag{0x7FE0, 0x0010});

    // Sequence delimiters
    CHECK(tags::item == dicom_tag{0xFFFE, 0xE000});
    CHECK(tags::item_delimitation_item == dicom_tag{0xFFFE, 0xE00D});
    CHECK(tags::sequence_delimitation_item == dicom_tag{0xFFFE, 0xE0DD});
}

TEST_CASE("dicom_tag constants are constexpr", "[dicom_tag][constants]") {
    // Verify compile-time usage
    constexpr auto patient_name_group = tags::patient_name.group();
    static_assert(patient_name_group == 0x0010);

    constexpr auto pixel_data_combined = tags::pixel_data.combined();
    static_assert(pixel_data_combined == 0x7FE00010);
}
