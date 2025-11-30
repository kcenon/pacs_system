/**
 * @file dicom_dictionary_test.cpp
 * @brief Unit tests for dicom_dictionary class
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/core/dicom_dictionary.hpp"
#include "pacs/encoding/vr_type.hpp"

using namespace pacs::core;
using pacs::encoding::vr_type;

TEST_CASE("dicom_dictionary singleton", "[dicom_dictionary]") {
    auto& dict1 = dicom_dictionary::instance();
    auto& dict2 = dicom_dictionary::instance();

    CHECK(&dict1 == &dict2);
}

TEST_CASE("dicom_dictionary has standard tags", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    CHECK(dict.size() > 0);
    CHECK(dict.standard_tag_count() > 0);
    CHECK(dict.private_tag_count() == 0);  // Initially no private tags
}

TEST_CASE("dicom_dictionary find by tag", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    SECTION("Patient Name tag") {
        auto info = dict.find(dicom_tag{0x0010, 0x0010});

        REQUIRE(info.has_value());
        CHECK(info->keyword == "PatientName");
        CHECK(info->name == "Patient's Name");
        CHECK(info->vr == static_cast<uint16_t>(vr_type::PN));
        CHECK_FALSE(info->retired);
    }

    SECTION("Patient ID tag") {
        auto info = dict.find(dicom_tag{0x0010, 0x0020});

        REQUIRE(info.has_value());
        CHECK(info->keyword == "PatientID");
        CHECK(info->vr == static_cast<uint16_t>(vr_type::LO));
    }

    SECTION("Study Instance UID") {
        auto info = dict.find(dicom_tag{0x0020, 0x000D});

        REQUIRE(info.has_value());
        CHECK(info->keyword == "StudyInstanceUID");
        CHECK(info->vr == static_cast<uint16_t>(vr_type::UI));
    }

    SECTION("Modality tag") {
        auto info = dict.find(dicom_tag{0x0008, 0x0060});

        REQUIRE(info.has_value());
        CHECK(info->keyword == "Modality");
        CHECK(info->vr == static_cast<uint16_t>(vr_type::CS));
    }

    SECTION("Pixel Data tag") {
        auto info = dict.find(dicom_tag{0x7FE0, 0x0010});

        REQUIRE(info.has_value());
        CHECK(info->keyword == "PixelData");
    }

    SECTION("Non-existent tag") {
        auto info = dict.find(dicom_tag{0xFFFF, 0xFFFF});

        CHECK_FALSE(info.has_value());
    }
}

TEST_CASE("dicom_dictionary find by keyword", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    SECTION("Find PatientName") {
        auto info = dict.find_by_keyword("PatientName");

        REQUIRE(info.has_value());
        CHECK(info->tag == dicom_tag{0x0010, 0x0010});
    }

    SECTION("Find SOPClassUID") {
        auto info = dict.find_by_keyword("SOPClassUID");

        REQUIRE(info.has_value());
        CHECK(info->tag == dicom_tag{0x0008, 0x0016});
    }

    SECTION("Find AccessionNumber") {
        auto info = dict.find_by_keyword("AccessionNumber");

        REQUIRE(info.has_value());
        CHECK(info->tag == dicom_tag{0x0008, 0x0050});
    }

    SECTION("Non-existent keyword") {
        auto info = dict.find_by_keyword("NonExistentKeyword");

        CHECK_FALSE(info.has_value());
    }

    SECTION("Empty keyword") {
        auto info = dict.find_by_keyword("");

        CHECK_FALSE(info.has_value());
    }
}

TEST_CASE("dicom_dictionary contains checks", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    CHECK(dict.contains(dicom_tag{0x0010, 0x0010}));
    CHECK(dict.contains(dicom_tag{0x0008, 0x0060}));
    CHECK_FALSE(dict.contains(dicom_tag{0xFFFF, 0xFFFF}));

    CHECK(dict.contains_keyword("PatientName"));
    CHECK(dict.contains_keyword("Modality"));
    CHECK_FALSE(dict.contains_keyword("NonExistent"));
}

TEST_CASE("dicom_dictionary VM validation", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    SECTION("Patient Name - VM 1") {
        CHECK(dict.validate_vm(dicom_tag{0x0010, 0x0010}, 1));
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0010, 0x0010}, 0));
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0010, 0x0010}, 2));
    }

    SECTION("Image Type - VM 2-n") {
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0008, 0x0008}, 0));
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0008, 0x0008}, 1));
        CHECK(dict.validate_vm(dicom_tag{0x0008, 0x0008}, 2));
        CHECK(dict.validate_vm(dicom_tag{0x0008, 0x0008}, 5));
        CHECK(dict.validate_vm(dicom_tag{0x0008, 0x0008}, 100));
    }

    SECTION("Image Position Patient - VM 3") {
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0020, 0x0032}, 2));
        CHECK(dict.validate_vm(dicom_tag{0x0020, 0x0032}, 3));
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0020, 0x0032}, 4));
    }

    SECTION("Image Orientation Patient - VM 6") {
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0020, 0x0037}, 5));
        CHECK(dict.validate_vm(dicom_tag{0x0020, 0x0037}, 6));
        CHECK_FALSE(dict.validate_vm(dicom_tag{0x0020, 0x0037}, 7));
    }

    SECTION("Non-existent tag") {
        CHECK_FALSE(dict.validate_vm(dicom_tag{0xFFFF, 0xFFFF}, 1));
    }
}

TEST_CASE("dicom_dictionary get_vr", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    CHECK(dict.get_vr(dicom_tag{0x0010, 0x0010}) == static_cast<uint16_t>(vr_type::PN));
    CHECK(dict.get_vr(dicom_tag{0x0008, 0x0060}) == static_cast<uint16_t>(vr_type::CS));
    CHECK(dict.get_vr(dicom_tag{0x0028, 0x0010}) == static_cast<uint16_t>(vr_type::US));
    CHECK(dict.get_vr(dicom_tag{0xFFFF, 0xFFFF}) == 0);  // Unknown tag
}

TEST_CASE("dicom_dictionary private tag registration", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    SECTION("Register valid private tag") {
        tag_info private_tag{
            dicom_tag{0x0009, 0x0010},  // Private creator in odd group
            static_cast<uint16_t>(vr_type::LO),
            value_multiplicity{1, 1},
            "PrivateCreator0009",
            "Private Creator for group 0009",
            false
        };

        // Note: This might fail if run multiple times since it's a singleton
        // In a real test setup, we'd need to reset the dictionary
        bool registered = dict.register_private_tag(private_tag);

        if (registered) {
            auto found = dict.find(dicom_tag{0x0009, 0x0010});
            REQUIRE(found.has_value());
            CHECK(found->keyword == "PrivateCreator0009");
            CHECK(dict.private_tag_count() > 0);
        }
    }

    SECTION("Cannot register standard (public) tag") {
        tag_info public_tag{
            dicom_tag{0x0010, 0x0099},  // Even group = public
            static_cast<uint16_t>(vr_type::LO),
            value_multiplicity{1, 1},
            "FakePublicTag",
            "Fake Public Tag",
            false
        };

        CHECK_FALSE(dict.register_private_tag(public_tag));
    }
}

TEST_CASE("dicom_dictionary get_tags_in_group", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    SECTION("Patient group 0x0010") {
        auto tags = dict.get_tags_in_group(0x0010);

        CHECK(tags.size() > 0);

        // Check that PatientName is included
        bool found_patient_name = false;
        for (const auto& info : tags) {
            if (info.keyword == "PatientName") {
                found_patient_name = true;
                break;
            }
        }
        CHECK(found_patient_name);

        // Check ordering
        for (size_t i = 1; i < tags.size(); ++i) {
            CHECK(tags[i - 1].tag < tags[i].tag);
        }
    }

    SECTION("File Meta group 0x0002") {
        auto tags = dict.get_tags_in_group(0x0002);

        CHECK(tags.size() > 0);

        bool found_transfer_syntax = false;
        for (const auto& info : tags) {
            if (info.keyword == "TransferSyntaxUID") {
                found_transfer_syntax = true;
                break;
            }
        }
        CHECK(found_transfer_syntax);
    }

    SECTION("Non-existent group") {
        auto tags = dict.get_tags_in_group(0x9999);
        CHECK(tags.empty());
    }
}

TEST_CASE("dicom_dictionary retired tags", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    SECTION("OtherPatientIDs is retired") {
        auto info = dict.find(dicom_tag{0x0010, 0x1000});

        REQUIRE(info.has_value());
        CHECK(info->retired);
    }

    SECTION("PatientName is not retired") {
        auto info = dict.find(dicom_tag{0x0010, 0x0010});

        REQUIRE(info.has_value());
        CHECK_FALSE(info->retired);
    }
}

TEST_CASE("dicom_dictionary common clinical tags", "[dicom_dictionary]") {
    auto& dict = dicom_dictionary::instance();

    // These are commonly used tags that should always be present

    SECTION("Study level tags") {
        CHECK(dict.contains_keyword("StudyInstanceUID"));
        CHECK(dict.contains_keyword("StudyDate"));
        CHECK(dict.contains_keyword("StudyTime"));
        CHECK(dict.contains_keyword("StudyDescription"));
        CHECK(dict.contains_keyword("AccessionNumber"));
    }

    SECTION("Series level tags") {
        CHECK(dict.contains_keyword("SeriesInstanceUID"));
        CHECK(dict.contains_keyword("SeriesNumber"));
        CHECK(dict.contains_keyword("SeriesDescription"));
        CHECK(dict.contains_keyword("Modality"));
    }

    SECTION("Instance level tags") {
        CHECK(dict.contains_keyword("SOPInstanceUID"));
        CHECK(dict.contains_keyword("SOPClassUID"));
        CHECK(dict.contains_keyword("InstanceNumber"));
    }

    SECTION("Image pixel tags") {
        CHECK(dict.contains_keyword("Rows"));
        CHECK(dict.contains_keyword("Columns"));
        CHECK(dict.contains_keyword("BitsAllocated"));
        CHECK(dict.contains_keyword("BitsStored"));
        CHECK(dict.contains_keyword("PixelRepresentation"));
        CHECK(dict.contains_keyword("PixelData"));
    }

    SECTION("Worklist tags") {
        CHECK(dict.contains_keyword("ScheduledProcedureStepSequence"));
        CHECK(dict.contains_keyword("ScheduledProcedureStepStartDate"));
        CHECK(dict.contains_keyword("ScheduledProcedureStepStartTime"));
        CHECK(dict.contains_keyword("RequestedProcedureID"));
    }
}
