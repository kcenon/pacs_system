/**
 * @file dicom_dataset_test.cpp
 * @brief Unit tests for dicom_dataset class
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>

#include <algorithm>
#include <vector>

using namespace pacs::core;
using namespace pacs::encoding;

// ============================================================================
// Construction Tests
// ============================================================================

TEST_CASE("dicom_dataset construction", "[core][dicom_dataset]") {
    SECTION("default construction creates empty dataset") {
        dicom_dataset ds;

        CHECK(ds.empty());
        CHECK(ds.size() == 0);
    }

    SECTION("copy construction") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        original.set_string(tags::patient_id, vr_type::LO, "12345");

        dicom_dataset copy{original};

        CHECK(copy.size() == 2);
        CHECK(copy.get_string(tags::patient_name) == "DOE^JOHN");
        CHECK(copy.get_string(tags::patient_id) == "12345");
    }

    SECTION("move construction") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_dataset moved{std::move(original)};

        CHECK(moved.size() == 1);
        CHECK(moved.get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("copy assignment") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_dataset copy;
        copy = original;

        CHECK(copy.get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("move assignment") {
        dicom_dataset original;
        original.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_dataset moved;
        moved = std::move(original);

        CHECK(moved.get_string(tags::patient_name) == "DOE^JOHN");
    }
}

// ============================================================================
// Element Access Tests
// ============================================================================

TEST_CASE("dicom_dataset element access", "[core][dicom_dataset]") {
    dicom_dataset ds;
    ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

    SECTION("contains returns true for existing element") {
        CHECK(ds.contains(tags::patient_name));
    }

    SECTION("contains returns false for non-existing element") {
        CHECK_FALSE(ds.contains(tags::patient_id));
    }

    SECTION("get returns pointer for existing element") {
        auto* elem = ds.get(tags::patient_name);

        REQUIRE(elem != nullptr);
        CHECK(elem->as_string() == "DOE^JOHN");
    }

    SECTION("get returns nullptr for non-existing element") {
        auto* elem = ds.get(tags::patient_id);

        CHECK(elem == nullptr);
    }

    SECTION("const get works correctly") {
        const auto& const_ds = ds;
        const auto* elem = const_ds.get(tags::patient_name);

        REQUIRE(elem != nullptr);
        CHECK(elem->as_string() == "DOE^JOHN");
    }
}

// ============================================================================
// Convenience Accessor Tests
// ============================================================================

TEST_CASE("dicom_dataset convenience accessors", "[core][dicom_dataset]") {
    dicom_dataset ds;
    ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 256);

    SECTION("get_string returns value for existing element") {
        CHECK(ds.get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("get_string returns default for non-existing element") {
        CHECK(ds.get_string(tags::patient_id) == "");
        CHECK(ds.get_string(tags::patient_id, "UNKNOWN") == "UNKNOWN");
    }

    SECTION("get_numeric returns value for existing element") {
        auto rows = ds.get_numeric<uint16_t>(tags::rows);

        REQUIRE(rows.has_value());
        CHECK(*rows == 512);
    }

    SECTION("get_numeric returns nullopt for non-existing element") {
        auto bits = ds.get_numeric<uint16_t>(tags::bits_allocated);

        CHECK_FALSE(bits.has_value());
    }

    SECTION("get_numeric with wrong type returns nullopt") {
        // Try to read 2-byte value as 4-byte
        auto value = ds.get_numeric<uint32_t>(tags::rows);

        CHECK_FALSE(value.has_value());
    }
}

// ============================================================================
// Modification Tests
// ============================================================================

TEST_CASE("dicom_dataset modification", "[core][dicom_dataset]") {
    SECTION("insert adds new element") {
        dicom_dataset ds;
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        ds.insert(elem);

        CHECK(ds.size() == 1);
        CHECK(ds.get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("insert replaces existing element") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "OLD^NAME");

        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "NEW^NAME");
        ds.insert(elem);

        CHECK(ds.size() == 1);
        CHECK(ds.get_string(tags::patient_name) == "NEW^NAME");
    }

    SECTION("insert with move") {
        dicom_dataset ds;
        auto elem = dicom_element::from_string(
            tags::patient_name, vr_type::PN, "DOE^JOHN");

        ds.insert(std::move(elem));

        CHECK(ds.size() == 1);
        CHECK(ds.get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("set_string adds new element") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        CHECK(ds.contains(tags::patient_name));
        CHECK(ds.get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("set_string replaces existing element") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "OLD");
        ds.set_string(tags::patient_name, vr_type::PN, "NEW");

        CHECK(ds.size() == 1);
        CHECK(ds.get_string(tags::patient_name) == "NEW");
    }

    SECTION("set_numeric adds numeric element") {
        dicom_dataset ds;
        ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);

        CHECK(ds.get_numeric<uint16_t>(tags::rows) == 512);
    }

    SECTION("remove existing element returns true") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        bool removed = ds.remove(tags::patient_name);

        CHECK(removed);
        CHECK_FALSE(ds.contains(tags::patient_name));
        CHECK(ds.empty());
    }

    SECTION("remove non-existing element returns false") {
        dicom_dataset ds;

        bool removed = ds.remove(tags::patient_name);

        CHECK_FALSE(removed);
    }

    SECTION("clear removes all elements") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        ds.set_string(tags::patient_id, vr_type::LO, "12345");
        ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);

        ds.clear();

        CHECK(ds.empty());
        CHECK(ds.size() == 0);
    }
}

// ============================================================================
// Iteration Tests
// ============================================================================

TEST_CASE("dicom_dataset iteration", "[core][dicom_dataset]") {
    dicom_dataset ds;
    // Insert in non-sorted order
    ds.set_string({0x0020, 0x000D}, vr_type::UI, "1.2.3");  // Study UID
    ds.set_string({0x0010, 0x0010}, vr_type::PN, "DOE^JOHN");  // Patient Name
    ds.set_string({0x0010, 0x0020}, vr_type::LO, "12345");  // Patient ID

    SECTION("elements are iterated in ascending tag order") {
        std::vector<dicom_tag> tags;
        for (const auto& [tag, elem] : ds) {
            tags.push_back(tag);
        }

        REQUIRE(tags.size() == 3);
        // Must be in ascending order
        CHECK(tags[0] == dicom_tag{0x0010, 0x0010});  // Patient Name first
        CHECK(tags[1] == dicom_tag{0x0010, 0x0020});  // Patient ID second
        CHECK(tags[2] == dicom_tag{0x0020, 0x000D});  // Study UID third
    }

    SECTION("begin and end work correctly") {
        auto it = ds.begin();
        auto end = ds.end();

        int count = 0;
        while (it != end) {
            ++count;
            ++it;
        }

        CHECK(count == 3);
    }

    SECTION("const iteration works") {
        const auto& const_ds = ds;
        int count = 0;

        for ([[maybe_unused]] const auto& [tag, elem] : const_ds) {
            ++count;
        }

        CHECK(count == 3);
    }

    SECTION("cbegin and cend work correctly") {
        auto it = ds.cbegin();
        auto end = ds.cend();

        CHECK(it != end);
        CHECK(std::distance(it, end) == 3);
    }

    SECTION("range-based for with modification") {
        for (auto& [tag, elem] : ds) {
            if (tag == dicom_tag{0x0010, 0x0010}) {
                elem.set_string("SMITH^JANE");
            }
        }

        CHECK(ds.get_string({0x0010, 0x0010}) == "SMITH^JANE");
    }
}

// ============================================================================
// Size Operations Tests
// ============================================================================

TEST_CASE("dicom_dataset size operations", "[core][dicom_dataset]") {
    SECTION("empty dataset") {
        dicom_dataset ds;

        CHECK(ds.empty());
        CHECK(ds.size() == 0);
    }

    SECTION("non-empty dataset") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        CHECK_FALSE(ds.empty());
        CHECK(ds.size() == 1);
    }

    SECTION("size increases with inserts") {
        dicom_dataset ds;

        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        CHECK(ds.size() == 1);

        ds.set_string(tags::patient_id, vr_type::LO, "12345");
        CHECK(ds.size() == 2);

        // Same tag should not increase size
        ds.set_string(tags::patient_name, vr_type::PN, "NEW^NAME");
        CHECK(ds.size() == 2);
    }

    SECTION("size decreases with removes") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
        ds.set_string(tags::patient_id, vr_type::LO, "12345");

        ds.remove(tags::patient_name);
        CHECK(ds.size() == 1);

        ds.remove(tags::patient_id);
        CHECK(ds.size() == 0);
        CHECK(ds.empty());
    }
}

// ============================================================================
// Utility Operations Tests
// ============================================================================

TEST_CASE("dicom_dataset copy_with_tags", "[core][dicom_dataset]") {
    dicom_dataset ds;
    ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");
    ds.set_string(tags::patient_id, vr_type::LO, "12345");
    ds.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4");
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);

    SECTION("copy with initializer list") {
        auto copy = ds.copy_with_tags({tags::patient_name, tags::patient_id});

        CHECK(copy.size() == 2);
        CHECK(copy.contains(tags::patient_name));
        CHECK(copy.contains(tags::patient_id));
        CHECK_FALSE(copy.contains(tags::study_instance_uid));
        CHECK_FALSE(copy.contains(tags::rows));
    }

    SECTION("copy with span") {
        std::vector<dicom_tag> tags_to_copy = {
            tags::patient_name, tags::study_instance_uid};
        auto copy = ds.copy_with_tags(tags_to_copy);

        CHECK(copy.size() == 2);
        CHECK(copy.get_string(tags::patient_name) == "DOE^JOHN");
        CHECK(copy.get_string(tags::study_instance_uid) == "1.2.3.4");
    }

    SECTION("copy with non-existing tags") {
        auto copy = ds.copy_with_tags({tags::patient_name, tags::modality});

        CHECK(copy.size() == 1);  // Only patient_name exists
        CHECK(copy.contains(tags::patient_name));
        CHECK_FALSE(copy.contains(tags::modality));
    }

    SECTION("copy with empty tag list") {
        auto copy = ds.copy_with_tags({});

        CHECK(copy.empty());
    }

    SECTION("copied elements are independent") {
        auto copy = ds.copy_with_tags({tags::patient_name});

        // Modify original
        ds.set_string(tags::patient_name, vr_type::PN, "MODIFIED");

        // Copy should be unchanged
        CHECK(copy.get_string(tags::patient_name) == "DOE^JOHN");
    }
}

TEST_CASE("dicom_dataset merge", "[core][dicom_dataset]") {
    SECTION("merge adds new elements") {
        dicom_dataset ds1;
        ds1.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_dataset ds2;
        ds2.set_string(tags::patient_id, vr_type::LO, "12345");

        ds1.merge(ds2);

        CHECK(ds1.size() == 2);
        CHECK(ds1.contains(tags::patient_name));
        CHECK(ds1.contains(tags::patient_id));
    }

    SECTION("merge overwrites existing elements") {
        dicom_dataset ds1;
        ds1.set_string(tags::patient_name, vr_type::PN, "OLD^NAME");

        dicom_dataset ds2;
        ds2.set_string(tags::patient_name, vr_type::PN, "NEW^NAME");

        ds1.merge(ds2);

        CHECK(ds1.size() == 1);
        CHECK(ds1.get_string(tags::patient_name) == "NEW^NAME");
    }

    SECTION("merge with move") {
        dicom_dataset ds1;
        ds1.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_dataset ds2;
        ds2.set_string(tags::patient_id, vr_type::LO, "12345");
        ds2.set_numeric<uint16_t>(tags::rows, vr_type::US, 512);

        ds1.merge(std::move(ds2));

        CHECK(ds1.size() == 3);
        CHECK(ds1.contains(tags::patient_name));
        CHECK(ds1.contains(tags::patient_id));
        CHECK(ds1.contains(tags::rows));
        CHECK(ds2.empty());  // Source should be cleared after move
    }

    SECTION("merge empty dataset has no effect") {
        dicom_dataset ds1;
        ds1.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        dicom_dataset ds2;

        ds1.merge(ds2);

        CHECK(ds1.size() == 1);
    }

    SECTION("merge into empty dataset") {
        dicom_dataset ds1;

        dicom_dataset ds2;
        ds2.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        ds1.merge(ds2);

        CHECK(ds1.size() == 1);
        CHECK(ds1.get_string(tags::patient_name) == "DOE^JOHN");
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("dicom_dataset edge cases", "[core][dicom_dataset]") {
    SECTION("large number of elements") {
        dicom_dataset ds;

        // Insert 100 elements
        for (uint16_t i = 0; i < 100; ++i) {
            dicom_tag tag{0x0010, static_cast<uint16_t>(0x1000 + i)};
            ds.set_string(tag, vr_type::LO, std::to_string(i));
        }

        CHECK(ds.size() == 100);

        // Verify ordering is maintained
        uint16_t expected_elem = 0x1000;
        for (const auto& [tag, elem] : ds) {
            CHECK(tag.group() == 0x0010);
            CHECK(tag.element() == expected_elem);
            ++expected_elem;
        }
    }

    SECTION("elements across different groups maintain order") {
        dicom_dataset ds;
        ds.set_string({0x0020, 0x0010}, vr_type::SH, "StudyID");
        ds.set_string({0x0008, 0x0050}, vr_type::SH, "AccNum");
        ds.set_string({0x0010, 0x0020}, vr_type::LO, "PatID");

        std::vector<uint16_t> groups;
        for (const auto& [tag, elem] : ds) {
            groups.push_back(tag.group());
        }

        // Groups should be in ascending order
        REQUIRE(groups.size() == 3);
        CHECK(groups[0] == 0x0008);
        CHECK(groups[1] == 0x0010);
        CHECK(groups[2] == 0x0020);
    }

    SECTION("self-assignment is safe") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        // Test self-assignment (suppress warning for this intentional test)
        auto& self_ref = ds;
        ds = self_ref;

        CHECK(ds.size() == 1);
        CHECK(ds.get_string(tags::patient_name) == "DOE^JOHN");
    }

    SECTION("mutable element access allows modification") {
        dicom_dataset ds;
        ds.set_string(tags::patient_name, vr_type::PN, "DOE^JOHN");

        auto* elem = ds.get(tags::patient_name);
        REQUIRE(elem != nullptr);
        elem->set_string("MODIFIED^NAME");

        CHECK(ds.get_string(tags::patient_name) == "MODIFIED^NAME");
    }
}
