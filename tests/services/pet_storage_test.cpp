/**
 * @file pet_storage_test.cpp
 * @brief Unit tests for PET (Positron Emission Tomography) Storage SOP Classes
 */

#include <pacs/services/sop_classes/pet_storage.hpp>
#include <pacs/services/sop_class_registry.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::sop_classes;
using namespace pacs::services;

// ============================================================================
// PET Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("PET Storage SOP Class UIDs are correct", "[services][pet][sop_class]") {
    CHECK(pet_image_storage_uid == "1.2.840.10008.5.1.4.1.1.128");
    CHECK(enhanced_pet_image_storage_uid == "1.2.840.10008.5.1.4.1.1.130");
    CHECK(legacy_converted_enhanced_pet_image_storage_uid == "1.2.840.10008.5.1.4.1.1.128.1");
}

TEST_CASE("is_pet_storage_sop_class identifies PET classes", "[services][pet][sop_class]") {
    SECTION("recognizes primary PET classes") {
        CHECK(is_pet_storage_sop_class(pet_image_storage_uid));
        CHECK(is_pet_storage_sop_class(enhanced_pet_image_storage_uid));
        CHECK(is_pet_storage_sop_class(legacy_converted_enhanced_pet_image_storage_uid));
    }

    SECTION("rejects non-PET classes") {
        CHECK_FALSE(is_pet_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));   // CT
        CHECK_FALSE(is_pet_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));   // MR
        CHECK_FALSE(is_pet_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1")); // US
        CHECK_FALSE(is_pet_storage_sop_class("1.2.840.10008.1.1"));           // Verification
        CHECK_FALSE(is_pet_storage_sop_class(""));
        CHECK_FALSE(is_pet_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_enhanced_pet_sop_class identifies enhanced classes", "[services][pet][sop_class]") {
    CHECK(is_enhanced_pet_sop_class(enhanced_pet_image_storage_uid));
    CHECK(is_enhanced_pet_sop_class(legacy_converted_enhanced_pet_image_storage_uid));
    CHECK_FALSE(is_enhanced_pet_sop_class(pet_image_storage_uid));
}

// ============================================================================
// PET SOP Class Information Tests
// ============================================================================

TEST_CASE("get_pet_sop_class_info returns correct information", "[services][pet][sop_class]") {
    SECTION("PET Image Storage info") {
        const auto* info = get_pet_sop_class_info(pet_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == pet_image_storage_uid);
        CHECK(info->name == "PET Image Storage");
        CHECK_FALSE(info->is_retired);
        CHECK_FALSE(info->supports_multiframe);
        CHECK_FALSE(info->is_enhanced);
    }

    SECTION("Enhanced PET Image Storage info") {
        const auto* info = get_pet_sop_class_info(enhanced_pet_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == enhanced_pet_image_storage_uid);
        CHECK(info->name == "Enhanced PET Image Storage");
        CHECK_FALSE(info->is_retired);
        CHECK(info->supports_multiframe);
        CHECK(info->is_enhanced);
    }

    SECTION("Legacy Converted Enhanced PET info") {
        const auto* info = get_pet_sop_class_info(legacy_converted_enhanced_pet_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->is_enhanced);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_pet_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_pet_storage_sop_classes returns correct list", "[services][pet][sop_class]") {
    auto classes = get_pet_storage_sop_classes(true);
    CHECK(classes.size() == 3);

    // All should be recognizable
    for (const auto& uid : classes) {
        CHECK(is_pet_storage_sop_class(uid));
    }
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_pet_transfer_syntaxes returns valid syntaxes", "[services][pet][transfer]") {
    auto syntaxes = get_pet_transfer_syntaxes();

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

TEST_CASE("pet_photometric_interpretation conversions", "[services][pet][photometric]") {
    SECTION("to_string produces correct DICOM string") {
        CHECK(to_string(pet_photometric_interpretation::monochrome2) == "MONOCHROME2");
    }

    SECTION("parse_pet_photometric_interpretation parses correctly") {
        CHECK(parse_pet_photometric_interpretation("MONOCHROME2") ==
              pet_photometric_interpretation::monochrome2);
        CHECK(parse_pet_photometric_interpretation("UNKNOWN") ==
              pet_photometric_interpretation::monochrome2);
    }
}

TEST_CASE("is_valid_pet_photometric validates correctly", "[services][pet][photometric]") {
    CHECK(is_valid_pet_photometric("MONOCHROME2"));
    CHECK_FALSE(is_valid_pet_photometric("MONOCHROME1"));  // Not standard for PET
    CHECK_FALSE(is_valid_pet_photometric("RGB"));
    CHECK_FALSE(is_valid_pet_photometric(""));
}

// ============================================================================
// PET Reconstruction Type Tests
// ============================================================================

TEST_CASE("pet_reconstruction_type conversions", "[services][pet][reconstruction]") {
    SECTION("to_string produces correct strings") {
        CHECK(to_string(pet_reconstruction_type::fbp) == "FBP");
        CHECK(to_string(pet_reconstruction_type::osem) == "OSEM");
        CHECK(to_string(pet_reconstruction_type::mlem) == "MLEM");
        CHECK(to_string(pet_reconstruction_type::tof_osem) == "TOF-OSEM");
        CHECK(to_string(pet_reconstruction_type::psf_osem) == "PSF-OSEM");
        CHECK(to_string(pet_reconstruction_type::other) == "OTHER");
    }

    SECTION("parse_pet_reconstruction_type parses correctly") {
        CHECK(parse_pet_reconstruction_type("FBP") == pet_reconstruction_type::fbp);
        CHECK(parse_pet_reconstruction_type("FILTERED BACK PROJECTION") == pet_reconstruction_type::fbp);
        CHECK(parse_pet_reconstruction_type("OSEM") == pet_reconstruction_type::osem);
        CHECK(parse_pet_reconstruction_type("3D-OSEM") == pet_reconstruction_type::osem);
        CHECK(parse_pet_reconstruction_type("MLEM") == pet_reconstruction_type::mlem);
        CHECK(parse_pet_reconstruction_type("TOF-OSEM") == pet_reconstruction_type::tof_osem);
        CHECK(parse_pet_reconstruction_type("PSF-OSEM") == pet_reconstruction_type::psf_osem);
        CHECK(parse_pet_reconstruction_type("UNKNOWN") == pet_reconstruction_type::other);
    }
}

// ============================================================================
// PET Units Tests
// ============================================================================

TEST_CASE("pet_units conversions", "[services][pet][units]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(pet_units::cnts) == "CNTS");
        CHECK(to_string(pet_units::bqml) == "BQML");
        CHECK(to_string(pet_units::gml) == "GML");
        CHECK(to_string(pet_units::suv_bw) == "SUV");
        CHECK(to_string(pet_units::suv_lbm) == "SUL");
        CHECK(to_string(pet_units::suv_bsa) == "SUV_BSA");
        CHECK(to_string(pet_units::percent_id_gram) == "%ID/G");
    }

    SECTION("parse_pet_units parses correctly") {
        CHECK(parse_pet_units("CNTS") == pet_units::cnts);
        CHECK(parse_pet_units("BQML") == pet_units::bqml);
        CHECK(parse_pet_units("BQ/ML") == pet_units::bqml);
        CHECK(parse_pet_units("GML") == pet_units::gml);
        CHECK(parse_pet_units("G/ML") == pet_units::gml);
        CHECK(parse_pet_units("SUV") == pet_units::suv_bw);
        CHECK(parse_pet_units("SUV_BW") == pet_units::suv_bw);
        CHECK(parse_pet_units("SUL") == pet_units::suv_lbm);
        CHECK(parse_pet_units("SUV_LBM") == pet_units::suv_lbm);
        CHECK(parse_pet_units("UNKNOWN") == pet_units::other);
    }
}

// ============================================================================
// PET Radiotracer Tests
// ============================================================================

TEST_CASE("pet_radiotracer string conversion", "[services][pet][radiotracer]") {
    CHECK(to_string(pet_radiotracer::fdg) == "18F-FDG");
    CHECK(to_string(pet_radiotracer::naf) == "18F-NaF");
    CHECK(to_string(pet_radiotracer::flt) == "18F-FLT");
    CHECK(to_string(pet_radiotracer::fdopa) == "18F-FDOPA");
    CHECK(to_string(pet_radiotracer::ammonia) == "13N-Ammonia");
    CHECK(to_string(pet_radiotracer::rubidium) == "82Rb");
    CHECK(to_string(pet_radiotracer::gallium_dotatate) == "68Ga-DOTATATE");
    CHECK(to_string(pet_radiotracer::psma) == "PSMA");
    CHECK(to_string(pet_radiotracer::other) == "Other");
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("sop_class_registry contains PET classes", "[services][pet][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("supports PET Image Storage") {
        CHECK(registry.is_supported(pet_image_storage_uid));
        const auto* info = registry.get_info(pet_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::pet);
    }

    SECTION("supports Enhanced PET Image Storage") {
        CHECK(registry.is_supported(enhanced_pet_image_storage_uid));
        const auto* info = registry.get_info(enhanced_pet_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->supports_multiframe);
    }

    SECTION("get_by_modality returns PET classes") {
        auto pet_classes = registry.get_by_modality(modality_type::pet, true);
        CHECK(pet_classes.size() >= 3);

        // Verify all returned classes are PET
        for (const auto& uid : pet_classes) {
            const auto* info = registry.get_info(uid);
            REQUIRE(info != nullptr);
            CHECK(info->modality == modality_type::pet);
        }
    }
}

TEST_CASE("sop_class_registry modality conversion for PET", "[services][pet][registry]") {
    CHECK(sop_class_registry::modality_to_string(modality_type::pet) == "PT");
    CHECK(sop_class_registry::parse_modality("PT") == modality_type::pet);
    CHECK(sop_class_registry::parse_modality("PET") == modality_type::pet);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST_CASE("is_storage_sop_class for PET", "[services][pet][registry]") {
    CHECK(is_storage_sop_class(pet_image_storage_uid));
    CHECK(is_storage_sop_class(enhanced_pet_image_storage_uid));
}

TEST_CASE("get_storage_modality for PET", "[services][pet][registry]") {
    CHECK(get_storage_modality(pet_image_storage_uid) == modality_type::pet);
    CHECK(get_storage_modality(enhanced_pet_image_storage_uid) == modality_type::pet);
}

TEST_CASE("get_sop_class_name for PET", "[services][pet][registry]") {
    CHECK(get_sop_class_name(pet_image_storage_uid) == "PET Image Storage");
    CHECK(get_sop_class_name(enhanced_pet_image_storage_uid) == "Enhanced PET Image Storage");
}
