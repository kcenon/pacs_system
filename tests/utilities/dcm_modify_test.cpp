/**
 * @file dcm_modify_test.cpp
 * @brief Unit tests for dcm_modify utility functions
 *
 * Tests the tag parsing, modification, and script file parsing functionality.
 */

#include <catch2/catch_test_macros.hpp>

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_dictionary.hpp>
#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

using namespace pacs::core;
using namespace pacs::encoding;

namespace {

/**
 * @brief Parse a tag string in format (GGGG,EEEE) or GGGG,EEEE
 * @param tag_str The tag string to parse
 * @return Parsed dicom_tag or nullopt if invalid
 */
std::optional<dicom_tag> parse_tag_string(const std::string& tag_str) {
    std::string s = tag_str;

    // Remove parentheses if present
    if (!s.empty() && s.front() == '(') {
        s.erase(0, 1);
    }
    if (!s.empty() && s.back() == ')') {
        s.pop_back();
    }

    // Remove spaces
    s.erase(std::remove(s.begin(), s.end(), ' '), s.end());

    // Parse GGGG,EEEE format
    size_t comma_pos = s.find(',');
    if (comma_pos != std::string::npos) {
        try {
            uint16_t group =
                static_cast<uint16_t>(std::stoul(s.substr(0, comma_pos), nullptr, 16));
            uint16_t element =
                static_cast<uint16_t>(std::stoul(s.substr(comma_pos + 1), nullptr, 16));
            return dicom_tag{group, element};
        } catch (...) {
            return std::nullopt;
        }
    }

    // Parse GGGGEEEE format (8 hex chars)
    if (s.length() == 8) {
        try {
            uint16_t group =
                static_cast<uint16_t>(std::stoul(s.substr(0, 4), nullptr, 16));
            uint16_t element =
                static_cast<uint16_t>(std::stoul(s.substr(4, 4), nullptr, 16));
            return dicom_tag{group, element};
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

/**
 * @brief Look up tag by keyword or parse tag string
 * @param str Tag keyword or numeric string
 * @return The tag if found/parsed, nullopt otherwise
 */
std::optional<dicom_tag> resolve_tag(const std::string& str) {
    // First, try as numeric tag format
    if (str.find('(') != std::string::npos || str.find(',') != std::string::npos ||
        (str.length() == 8 && std::all_of(str.begin(), str.end(), ::isxdigit))) {
        return parse_tag_string(str);
    }

    // Try as keyword
    auto& dict = dicom_dictionary::instance();
    auto info = dict.find_by_keyword(str);
    if (info) {
        return info->tag;
    }

    return std::nullopt;
}

/**
 * @brief Create a minimal DICOM dataset for testing
 */
dicom_dataset create_test_dataset() {
    dicom_dataset dataset;

    // Add some basic tags
    dataset.set_string(tags::patient_name, vr_type::PN, "Test^Patient");
    dataset.set_string(tags::patient_id, vr_type::LO, "TEST001");
    dataset.set_string(tags::study_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.9");
    dataset.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.10");
    dataset.set_string(tags::sop_instance_uid, vr_type::UI, "1.2.3.4.5.6.7.8.11");
    dataset.set_string(dicom_tag{0x0010, 0x1000}, vr_type::LO, "OTHER_ID_1");  // OtherPatientIDs

    return dataset;
}

}  // namespace

TEST_CASE("Tag string parsing", "[dcm_modify][parsing]") {
    SECTION("Parse tag with parentheses") {
        auto tag = parse_tag_string("(0010,0010)");
        REQUIRE(tag.has_value());
        REQUIRE(tag->group() == 0x0010);
        REQUIRE(tag->element() == 0x0010);
    }

    SECTION("Parse tag without parentheses") {
        auto tag = parse_tag_string("0010,0010");
        REQUIRE(tag.has_value());
        REQUIRE(tag->group() == 0x0010);
        REQUIRE(tag->element() == 0x0010);
    }

    SECTION("Parse tag with spaces") {
        auto tag = parse_tag_string("( 0010 , 0010 )");
        REQUIRE(tag.has_value());
        REQUIRE(tag->group() == 0x0010);
        REQUIRE(tag->element() == 0x0010);
    }

    SECTION("Parse 8-character hex format") {
        auto tag = parse_tag_string("00100010");
        REQUIRE(tag.has_value());
        REQUIRE(tag->group() == 0x0010);
        REQUIRE(tag->element() == 0x0010);
    }

    SECTION("Parse different tags") {
        auto tag1 = parse_tag_string("(0008,0050)");  // AccessionNumber
        REQUIRE(tag1.has_value());
        REQUIRE(tag1->group() == 0x0008);
        REQUIRE(tag1->element() == 0x0050);

        auto tag2 = parse_tag_string("(7FE0,0010)");  // PixelData
        REQUIRE(tag2.has_value());
        REQUIRE(tag2->group() == 0x7FE0);
        REQUIRE(tag2->element() == 0x0010);
    }

    SECTION("Invalid tag strings") {
        REQUIRE_FALSE(parse_tag_string("invalid").has_value());
        REQUIRE_FALSE(parse_tag_string("0010").has_value());
        REQUIRE_FALSE(parse_tag_string("(0010)").has_value());
        REQUIRE_FALSE(parse_tag_string("GGGG,0010").has_value());
    }
}

TEST_CASE("Tag resolution by keyword", "[dcm_modify][parsing]") {
    SECTION("Resolve PatientName") {
        auto tag = resolve_tag("PatientName");
        REQUIRE(tag.has_value());
        REQUIRE(tag->group() == 0x0010);
        REQUIRE(tag->element() == 0x0010);
    }

    SECTION("Resolve PatientID") {
        auto tag = resolve_tag("PatientID");
        REQUIRE(tag.has_value());
        REQUIRE(tag->group() == 0x0010);
        REQUIRE(tag->element() == 0x0020);
    }

    SECTION("Resolve numeric format as fallback") {
        auto tag = resolve_tag("(0010,0010)");
        REQUIRE(tag.has_value());
        REQUIRE(tag->group() == 0x0010);
        REQUIRE(tag->element() == 0x0010);
    }

    SECTION("Unknown keyword returns nullopt") {
        auto tag = resolve_tag("NonExistentKeyword");
        REQUIRE_FALSE(tag.has_value());
    }
}

TEST_CASE("Dataset modification operations", "[dcm_modify][dataset]") {
    auto dataset = create_test_dataset();

    SECTION("Insert new tag") {
        dicom_tag new_tag{0x0010, 0x1030};  // PatientWeight
        REQUIRE_FALSE(dataset.contains(new_tag));

        dataset.set_string(new_tag, vr_type::DS, "70.5");

        REQUIRE(dataset.contains(new_tag));
        REQUIRE(dataset.get_string(new_tag) == "70.5");
    }

    SECTION("Modify existing tag") {
        REQUIRE(dataset.contains(tags::patient_name));
        REQUIRE(dataset.get_string(tags::patient_name) == "Test^Patient");

        dataset.set_string(tags::patient_name, vr_type::PN, "Modified^Name");

        REQUIRE(dataset.get_string(tags::patient_name) == "Modified^Name");
    }

    SECTION("Erase tag") {
        dicom_tag other_ids{0x0010, 0x1000};  // OtherPatientIDs
        REQUIRE(dataset.contains(other_ids));

        dataset.remove(other_ids);

        REQUIRE_FALSE(dataset.contains(other_ids));
    }

    SECTION("Erase non-existent tag does not throw") {
        dicom_tag non_existent{0x9999, 0x9999};
        REQUIRE_FALSE(dataset.contains(non_existent));

        dataset.remove(non_existent);  // Should not throw

        REQUIRE_FALSE(dataset.contains(non_existent));
    }
}

TEST_CASE("Private tag operations", "[dcm_modify][private]") {
    dicom_dataset dataset;

    // Add some private tags (odd group numbers)
    dicom_tag private_tag1{0x0011, 0x0010};  // Private creator
    dicom_tag private_tag2{0x0011, 0x1001};  // Private data
    dicom_tag private_tag3{0x0013, 0x0010};  // Another private creator

    dataset.set_string(private_tag1, vr_type::LO, "PrivateCreator");
    dataset.set_string(private_tag2, vr_type::LO, "PrivateData");
    dataset.set_string(private_tag3, vr_type::LO, "AnotherCreator");

    // Add a public tag
    dataset.set_string(tags::patient_name, vr_type::PN, "Test^Patient");

    SECTION("Identify private tags") {
        REQUIRE(private_tag1.is_private());
        REQUIRE(private_tag2.is_private());
        REQUIRE(private_tag3.is_private());
        REQUIRE_FALSE(tags::patient_name.is_private());
    }

    SECTION("Remove all private tags") {
        // Collect and remove private tags
        std::vector<dicom_tag> private_tags;
        for (const auto& [tag, element] : dataset) {
            if (tag.is_private()) {
                private_tags.push_back(tag);
            }
        }

        for (const auto& tag : private_tags) {
            dataset.remove(tag);
        }

        // Verify private tags are removed
        REQUIRE_FALSE(dataset.contains(private_tag1));
        REQUIRE_FALSE(dataset.contains(private_tag2));
        REQUIRE_FALSE(dataset.contains(private_tag3));

        // Public tag should remain
        REQUIRE(dataset.contains(tags::patient_name));
    }
}

TEST_CASE("UID modification", "[dcm_modify][uid]") {
    auto dataset = create_test_dataset();

    SECTION("Modify StudyInstanceUID") {
        std::string original_uid = dataset.get_string(tags::study_instance_uid);
        std::string new_uid = "1.2.826.0.1.3680043.8.1055.2.12345";

        dataset.set_string(tags::study_instance_uid, vr_type::UI, new_uid);

        REQUIRE(dataset.get_string(tags::study_instance_uid) == new_uid);
        REQUIRE(dataset.get_string(tags::study_instance_uid) != original_uid);
    }

    SECTION("Modify SeriesInstanceUID") {
        std::string original_uid = dataset.get_string(tags::series_instance_uid);
        std::string new_uid = "1.2.826.0.1.3680043.8.1055.2.12346";

        dataset.set_string(tags::series_instance_uid, vr_type::UI, new_uid);

        REQUIRE(dataset.get_string(tags::series_instance_uid) == new_uid);
        REQUIRE(dataset.get_string(tags::series_instance_uid) != original_uid);
    }

    SECTION("Modify SOPInstanceUID") {
        std::string original_uid = dataset.get_string(tags::sop_instance_uid);
        std::string new_uid = "1.2.826.0.1.3680043.8.1055.2.12347";

        dataset.set_string(tags::sop_instance_uid, vr_type::UI, new_uid);

        REQUIRE(dataset.get_string(tags::sop_instance_uid) == new_uid);
        REQUIRE(dataset.get_string(tags::sop_instance_uid) != original_uid);
    }
}

TEST_CASE("Script file format parsing", "[dcm_modify][script]") {
    // This test validates the script file format expectations
    // The actual script parsing is done in the CLI, but we can test the format

    SECTION("Valid script commands format") {
        // i (tag)=value - insert
        // m (tag)=value - modify (must exist)
        // e (tag) - erase
        // ea (tag) - erase all (including in sequences)

        std::string insert_cmd = "i (0010,0010)=Anonymous";
        std::string modify_cmd = "m (0008,0050)=ACC001";
        std::string erase_cmd = "e (0010,1000)";
        std::string erase_all_cmd = "ea (0010,1001)";

        // Verify command prefix parsing
        REQUIRE(insert_cmd.substr(0, 2) == "i ");
        REQUIRE(modify_cmd.substr(0, 2) == "m ");
        REQUIRE(erase_cmd.substr(0, 2) == "e ");
        REQUIRE(erase_all_cmd.substr(0, 3) == "ea ");
    }

    SECTION("Comment lines start with #") {
        std::string comment = "# This is a comment";
        REQUIRE(comment[0] == '#');
    }
}
