/**
 * @file nm_storage_test.cpp
 * @brief Unit tests for Nuclear Medicine (NM) Storage SOP Classes
 */

#include <pacs/services/sop_classes/nm_storage.hpp>
#include <pacs/services/sop_class_registry.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::sop_classes;
using namespace pacs::services;

// ============================================================================
// NM Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("NM Storage SOP Class UIDs are correct", "[services][nm][sop_class]") {
    CHECK(nm_image_storage_uid == "1.2.840.10008.5.1.4.1.1.20");
    CHECK(nm_image_storage_retired_uid == "1.2.840.10008.5.1.4.1.1.5");
}

TEST_CASE("is_nm_storage_sop_class identifies NM classes", "[services][nm][sop_class]") {
    SECTION("recognizes primary NM classes") {
        CHECK(is_nm_storage_sop_class(nm_image_storage_uid));
    }

    SECTION("recognizes retired NM classes") {
        CHECK(is_nm_storage_sop_class(nm_image_storage_retired_uid));
    }

    SECTION("rejects non-NM classes") {
        CHECK_FALSE(is_nm_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));    // CT
        CHECK_FALSE(is_nm_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));    // MR
        CHECK_FALSE(is_nm_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
        CHECK_FALSE(is_nm_storage_sop_class("1.2.840.10008.5.1.4.1.1.128")); // PET
        CHECK_FALSE(is_nm_storage_sop_class("1.2.840.10008.1.1"));            // Verification
        CHECK_FALSE(is_nm_storage_sop_class(""));
        CHECK_FALSE(is_nm_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_nm_multiframe_sop_class identifies multiframe classes", "[services][nm][sop_class]") {
    // NM images are typically multiframe (SPECT, dynamic, gated)
    CHECK(is_nm_multiframe_sop_class(nm_image_storage_uid));
    CHECK(is_nm_multiframe_sop_class(nm_image_storage_retired_uid));
}

// ============================================================================
// NM SOP Class Information Tests
// ============================================================================

TEST_CASE("get_nm_sop_class_info returns correct information", "[services][nm][sop_class]") {
    SECTION("NM Image Storage info") {
        const auto* info = get_nm_sop_class_info(nm_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == nm_image_storage_uid);
        CHECK(info->name == "NM Image Storage");
        CHECK_FALSE(info->is_retired);
        CHECK(info->supports_multiframe);
    }

    SECTION("NM Image Storage (Retired) info") {
        const auto* info = get_nm_sop_class_info(nm_image_storage_retired_uid);
        REQUIRE(info != nullptr);
        CHECK(info->is_retired);
        CHECK(info->supports_multiframe);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_nm_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_nm_storage_sop_classes returns correct list", "[services][nm][sop_class]") {
    SECTION("with retired classes") {
        auto classes = get_nm_storage_sop_classes(true);
        CHECK(classes.size() == 2);
    }

    SECTION("without retired classes") {
        auto classes = get_nm_storage_sop_classes(false);
        CHECK(classes.size() == 1);
        // Verify only current classes are returned
        for (const auto& uid : classes) {
            const auto* info = get_nm_sop_class_info(uid);
            REQUIRE(info != nullptr);
            CHECK_FALSE(info->is_retired);
        }
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_nm_transfer_syntaxes returns valid syntaxes", "[services][nm][transfer]") {
    auto syntaxes = get_nm_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());

    // Should include lossless JPEG for quantitative data preservation
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.4.70") != syntaxes.end());
}

// ============================================================================
// Photometric Interpretation Tests
// ============================================================================

TEST_CASE("nm_photometric_interpretation conversions", "[services][nm][photometric]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(nm_photometric_interpretation::monochrome2) == "MONOCHROME2");
        CHECK(to_string(nm_photometric_interpretation::palette_color) == "PALETTE COLOR");
    }

    SECTION("parse_nm_photometric_interpretation parses correctly") {
        CHECK(parse_nm_photometric_interpretation("MONOCHROME2") ==
              nm_photometric_interpretation::monochrome2);
        CHECK(parse_nm_photometric_interpretation("PALETTE COLOR") ==
              nm_photometric_interpretation::palette_color);
        CHECK(parse_nm_photometric_interpretation("UNKNOWN") ==
              nm_photometric_interpretation::monochrome2);
    }
}

TEST_CASE("is_valid_nm_photometric validates correctly", "[services][nm][photometric]") {
    CHECK(is_valid_nm_photometric("MONOCHROME2"));
    CHECK(is_valid_nm_photometric("PALETTE COLOR"));
    CHECK_FALSE(is_valid_nm_photometric("MONOCHROME1"));
    CHECK_FALSE(is_valid_nm_photometric("RGB"));
    CHECK_FALSE(is_valid_nm_photometric(""));
}

// ============================================================================
// NM Type of Data Tests
// ============================================================================

TEST_CASE("nm_type_of_data conversions", "[services][nm][type_of_data]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(nm_type_of_data::static_image) == "STATIC");
        CHECK(to_string(nm_type_of_data::dynamic) == "DYNAMIC");
        CHECK(to_string(nm_type_of_data::gated) == "GATED");
        CHECK(to_string(nm_type_of_data::whole_body) == "WHOLE BODY");
        CHECK(to_string(nm_type_of_data::recon_tomo) == "RECON TOMO");
        CHECK(to_string(nm_type_of_data::recon_gated_tomo) == "RECON GATED TOMO");
        CHECK(to_string(nm_type_of_data::tomo) == "TOMO");
        CHECK(to_string(nm_type_of_data::gated_tomo) == "GATED TOMO");
    }

    SECTION("parse_nm_type_of_data parses correctly") {
        CHECK(parse_nm_type_of_data("STATIC") == nm_type_of_data::static_image);
        CHECK(parse_nm_type_of_data("DYNAMIC") == nm_type_of_data::dynamic);
        CHECK(parse_nm_type_of_data("GATED") == nm_type_of_data::gated);
        CHECK(parse_nm_type_of_data("WHOLE BODY") == nm_type_of_data::whole_body);
        CHECK(parse_nm_type_of_data("RECON TOMO") == nm_type_of_data::recon_tomo);
        CHECK(parse_nm_type_of_data("RECON GATED TOMO") == nm_type_of_data::recon_gated_tomo);
        CHECK(parse_nm_type_of_data("TOMO") == nm_type_of_data::tomo);
        CHECK(parse_nm_type_of_data("GATED TOMO") == nm_type_of_data::gated_tomo);
        CHECK(parse_nm_type_of_data("UNKNOWN") == nm_type_of_data::static_image);
    }
}

// ============================================================================
// NM Collimator Type Tests
// ============================================================================

TEST_CASE("nm_collimator_type conversions", "[services][nm][collimator]") {
    SECTION("to_string produces correct DICOM codes") {
        CHECK(to_string(nm_collimator_type::parallel) == "PARA");
        CHECK(to_string(nm_collimator_type::fan_beam) == "FANB");
        CHECK(to_string(nm_collimator_type::cone_beam) == "CONE");
        CHECK(to_string(nm_collimator_type::pinhole) == "PINH");
        CHECK(to_string(nm_collimator_type::diverging) == "DIVG");
        CHECK(to_string(nm_collimator_type::converging) == "CVGB");
        CHECK(to_string(nm_collimator_type::none) == "NONE");
    }

    SECTION("parse_nm_collimator_type parses correctly") {
        CHECK(parse_nm_collimator_type("PARA") == nm_collimator_type::parallel);
        CHECK(parse_nm_collimator_type("PARALLEL") == nm_collimator_type::parallel);
        CHECK(parse_nm_collimator_type("FANB") == nm_collimator_type::fan_beam);
        CHECK(parse_nm_collimator_type("FAN BEAM") == nm_collimator_type::fan_beam);
        CHECK(parse_nm_collimator_type("CONE") == nm_collimator_type::cone_beam);
        CHECK(parse_nm_collimator_type("PINH") == nm_collimator_type::pinhole);
        CHECK(parse_nm_collimator_type("PINHOLE") == nm_collimator_type::pinhole);
        CHECK(parse_nm_collimator_type("NONE") == nm_collimator_type::none);
        CHECK(parse_nm_collimator_type("UNKNOWN") == nm_collimator_type::parallel);
    }
}

// ============================================================================
// NM Radioisotope Tests
// ============================================================================

TEST_CASE("nm_radioisotope string conversion", "[services][nm][radioisotope]") {
    CHECK(to_string(nm_radioisotope::tc99m) == "Tc-99m");
    CHECK(to_string(nm_radioisotope::i131) == "I-131");
    CHECK(to_string(nm_radioisotope::i123) == "I-123");
    CHECK(to_string(nm_radioisotope::tl201) == "Tl-201");
    CHECK(to_string(nm_radioisotope::ga67) == "Ga-67");
    CHECK(to_string(nm_radioisotope::in111) == "In-111");
    CHECK(to_string(nm_radioisotope::f18) == "F-18");
    CHECK(to_string(nm_radioisotope::other) == "Other");
}

TEST_CASE("nm_radioisotope primary energy values", "[services][nm][radioisotope]") {
    CHECK(get_primary_energy_kev(nm_radioisotope::tc99m) == 140.0);
    CHECK(get_primary_energy_kev(nm_radioisotope::i131) == 364.0);
    CHECK(get_primary_energy_kev(nm_radioisotope::i123) == 159.0);
    CHECK(get_primary_energy_kev(nm_radioisotope::tl201) == 71.0);
    CHECK(get_primary_energy_kev(nm_radioisotope::ga67) == 93.0);
    CHECK(get_primary_energy_kev(nm_radioisotope::in111) == 171.0);
    CHECK(get_primary_energy_kev(nm_radioisotope::f18) == 511.0);
    CHECK(get_primary_energy_kev(nm_radioisotope::other) == 0.0);
}

// ============================================================================
// NM Whole Body Technique Tests
// ============================================================================

TEST_CASE("nm_whole_body_technique conversions", "[services][nm][whole_body]") {
    CHECK(to_string(nm_whole_body_technique::single_pass) == "1PASS");
    CHECK(to_string(nm_whole_body_technique::multi_pass) == "2PASS");
    CHECK(to_string(nm_whole_body_technique::stepping) == "STEP");
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("sop_class_registry contains NM classes", "[services][nm][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("supports NM Image Storage") {
        CHECK(registry.is_supported(nm_image_storage_uid));
        const auto* info = registry.get_info(nm_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::nm);
    }

    SECTION("supports NM Image Storage (Retired)") {
        CHECK(registry.is_supported(nm_image_storage_retired_uid));
        const auto* info = registry.get_info(nm_image_storage_retired_uid);
        REQUIRE(info != nullptr);
        CHECK(info->is_retired);
    }

    SECTION("get_by_modality returns NM classes") {
        auto nm_classes = registry.get_by_modality(modality_type::nm, true);
        CHECK(nm_classes.size() >= 2);

        // Verify all returned classes are NM
        for (const auto& uid : nm_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK(info->modality == modality_type::nm);
        }
    }

    SECTION("get_by_modality filters retired NM classes") {
        auto current_classes = registry.get_by_modality(modality_type::nm, false);
        CHECK(current_classes.size() == 1);

        for (const auto& uid : current_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK_FALSE(info->is_retired);
        }
    }
}

TEST_CASE("sop_class_registry modality conversion for NM", "[services][nm][registry]") {
    CHECK(sop_class_registry::modality_to_string(modality_type::nm) == "NM");
    CHECK(sop_class_registry::parse_modality("NM") == modality_type::nm);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("is_storage_sop_class for NM", "[services][nm][registry]") {
    CHECK(is_storage_sop_class(nm_image_storage_uid));
    CHECK(is_storage_sop_class(nm_image_storage_retired_uid));
}

TEST_CASE("get_storage_modality for NM", "[services][nm][registry]") {
    CHECK(get_storage_modality(nm_image_storage_uid) == modality_type::nm);
    CHECK(get_storage_modality(nm_image_storage_retired_uid) == modality_type::nm);
}

TEST_CASE("get_sop_class_name for NM", "[services][nm][registry]") {
    CHECK(get_sop_class_name(nm_image_storage_uid) == "NM Image Storage");
    CHECK(get_sop_class_name(nm_image_storage_retired_uid) == "NM Image Storage (Retired)");
}
