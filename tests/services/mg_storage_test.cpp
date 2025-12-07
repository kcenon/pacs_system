/**
 * @file mg_storage_test.cpp
 * @brief Unit tests for Digital Mammography Storage SOP Classes
 */

#include <pacs/services/sop_classes/mg_storage.hpp>
#include <pacs/services/sop_class_registry.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::sop_classes;
using namespace pacs::services;

// ============================================================================
// MG Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("MG Storage SOP Class UIDs are correct", "[services][mg][sop_class]") {
    CHECK(mg_image_storage_for_presentation_uid == "1.2.840.10008.5.1.4.1.1.1.2");
    CHECK(mg_image_storage_for_processing_uid == "1.2.840.10008.5.1.4.1.1.1.2.1");
    CHECK(breast_tomosynthesis_image_storage_uid == "1.2.840.10008.5.1.4.1.1.13.1.3");
    CHECK(breast_projection_image_storage_for_presentation_uid == "1.2.840.10008.5.1.4.1.1.13.1.4");
    CHECK(breast_projection_image_storage_for_processing_uid == "1.2.840.10008.5.1.4.1.1.13.1.5");
}

TEST_CASE("is_mg_storage_sop_class identifies MG classes", "[services][mg][sop_class]") {
    SECTION("recognizes standard mammography classes") {
        CHECK(is_mg_storage_sop_class(mg_image_storage_for_presentation_uid));
        CHECK(is_mg_storage_sop_class(mg_image_storage_for_processing_uid));
    }

    SECTION("recognizes tomosynthesis classes") {
        CHECK(is_mg_storage_sop_class(breast_tomosynthesis_image_storage_uid));
        CHECK(is_mg_storage_sop_class(breast_projection_image_storage_for_presentation_uid));
        CHECK(is_mg_storage_sop_class(breast_projection_image_storage_for_processing_uid));
    }

    SECTION("rejects non-MG classes") {
        CHECK_FALSE(is_mg_storage_sop_class("1.2.840.10008.5.1.4.1.1.1.1"));  // DX
        CHECK_FALSE(is_mg_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));    // CT
        CHECK_FALSE(is_mg_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));    // MR
        CHECK_FALSE(is_mg_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
        CHECK_FALSE(is_mg_storage_sop_class(""));
        CHECK_FALSE(is_mg_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_mg_for_processing_sop_class identifies For Processing classes", "[services][mg][sop_class]") {
    CHECK(is_mg_for_processing_sop_class(mg_image_storage_for_processing_uid));
    CHECK(is_mg_for_processing_sop_class(breast_projection_image_storage_for_processing_uid));

    CHECK_FALSE(is_mg_for_processing_sop_class(mg_image_storage_for_presentation_uid));
    CHECK_FALSE(is_mg_for_processing_sop_class(breast_tomosynthesis_image_storage_uid));
}

TEST_CASE("is_mg_for_presentation_sop_class identifies For Presentation classes", "[services][mg][sop_class]") {
    CHECK(is_mg_for_presentation_sop_class(mg_image_storage_for_presentation_uid));
    CHECK(is_mg_for_presentation_sop_class(breast_tomosynthesis_image_storage_uid));
    CHECK(is_mg_for_presentation_sop_class(breast_projection_image_storage_for_presentation_uid));

    CHECK_FALSE(is_mg_for_presentation_sop_class(mg_image_storage_for_processing_uid));
    CHECK_FALSE(is_mg_for_presentation_sop_class(breast_projection_image_storage_for_processing_uid));
}

TEST_CASE("is_breast_tomosynthesis_sop_class identifies tomosynthesis classes", "[services][mg][sop_class]") {
    CHECK(is_breast_tomosynthesis_sop_class(breast_tomosynthesis_image_storage_uid));
    CHECK(is_breast_tomosynthesis_sop_class(breast_projection_image_storage_for_presentation_uid));
    CHECK(is_breast_tomosynthesis_sop_class(breast_projection_image_storage_for_processing_uid));

    CHECK_FALSE(is_breast_tomosynthesis_sop_class(mg_image_storage_for_presentation_uid));
    CHECK_FALSE(is_breast_tomosynthesis_sop_class(mg_image_storage_for_processing_uid));
}

// ============================================================================
// MG SOP Class Information Tests
// ============================================================================

TEST_CASE("get_mg_sop_class_info returns correct information", "[services][mg][sop_class]") {
    SECTION("MG For Presentation info") {
        const auto* info = get_mg_sop_class_info(mg_image_storage_for_presentation_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == mg_image_storage_for_presentation_uid);
        CHECK(info->image_type == mg_image_type::for_presentation);
        CHECK_FALSE(info->is_tomosynthesis);
        CHECK_FALSE(info->supports_multiframe);
    }

    SECTION("MG For Processing info") {
        const auto* info = get_mg_sop_class_info(mg_image_storage_for_processing_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == mg_image_storage_for_processing_uid);
        CHECK(info->image_type == mg_image_type::for_processing);
        CHECK_FALSE(info->is_tomosynthesis);
    }

    SECTION("Breast Tomosynthesis info") {
        const auto* info = get_mg_sop_class_info(breast_tomosynthesis_image_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->is_tomosynthesis);
        CHECK(info->supports_multiframe);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_mg_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_mg_storage_sop_classes returns correct list", "[services][mg][sop_class]") {
    SECTION("all MG classes") {
        auto classes = get_mg_storage_sop_classes(true);
        CHECK(classes.size() == 5);
    }

    SECTION("without tomosynthesis") {
        auto classes = get_mg_storage_sop_classes(false);
        CHECK(classes.size() == 2);
        for (const auto& uid : classes) {
            CHECK_FALSE(is_breast_tomosynthesis_sop_class(uid));
        }
    }
}

// ============================================================================
// Breast Laterality Tests
// ============================================================================

TEST_CASE("breast_laterality conversion", "[services][mg][laterality]") {
    SECTION("enum to string") {
        CHECK(to_string(breast_laterality::left) == "L");
        CHECK(to_string(breast_laterality::right) == "R");
        CHECK(to_string(breast_laterality::bilateral) == "B");
        CHECK(to_string(breast_laterality::unknown) == "");
    }

    SECTION("string to enum - single character") {
        CHECK(parse_breast_laterality("L") == breast_laterality::left);
        CHECK(parse_breast_laterality("R") == breast_laterality::right);
        CHECK(parse_breast_laterality("B") == breast_laterality::bilateral);
        CHECK(parse_breast_laterality("l") == breast_laterality::left);  // lowercase
        CHECK(parse_breast_laterality("r") == breast_laterality::right);
    }

    SECTION("string to enum - full words") {
        CHECK(parse_breast_laterality("LEFT") == breast_laterality::left);
        CHECK(parse_breast_laterality("RIGHT") == breast_laterality::right);
        CHECK(parse_breast_laterality("BILATERAL") == breast_laterality::bilateral);
        CHECK(parse_breast_laterality("left") == breast_laterality::left);
        CHECK(parse_breast_laterality("BOTH") == breast_laterality::bilateral);
    }

    SECTION("unknown values") {
        CHECK(parse_breast_laterality("") == breast_laterality::unknown);
        CHECK(parse_breast_laterality("X") == breast_laterality::unknown);
        CHECK(parse_breast_laterality("INVALID") == breast_laterality::unknown);
    }

    SECTION("validation") {
        CHECK(is_valid_breast_laterality("L"));
        CHECK(is_valid_breast_laterality("R"));
        CHECK(is_valid_breast_laterality("B"));
        CHECK_FALSE(is_valid_breast_laterality(""));
        CHECK_FALSE(is_valid_breast_laterality("l"));  // Case sensitive for DICOM
        CHECK_FALSE(is_valid_breast_laterality("LEFT"));
        CHECK_FALSE(is_valid_breast_laterality("X"));
    }
}

// ============================================================================
// Mammography View Position Tests
// ============================================================================

TEST_CASE("mg_view_position conversion", "[services][mg][view_position]") {
    SECTION("enum to string - standard views") {
        CHECK(to_string(mg_view_position::cc) == "CC");
        CHECK(to_string(mg_view_position::mlo) == "MLO");
        CHECK(to_string(mg_view_position::ml) == "ML");
        CHECK(to_string(mg_view_position::lm) == "LM");
    }

    SECTION("enum to string - extended views") {
        CHECK(to_string(mg_view_position::xccl) == "XCCL");
        CHECK(to_string(mg_view_position::xccm) == "XCCM");
        CHECK(to_string(mg_view_position::fb) == "FB");
        CHECK(to_string(mg_view_position::sio) == "SIO");
        CHECK(to_string(mg_view_position::cv) == "CV");
        CHECK(to_string(mg_view_position::at) == "AT");
    }

    SECTION("enum to string - spot/mag views") {
        CHECK(to_string(mg_view_position::spot) == "SPOT");
        CHECK(to_string(mg_view_position::mag) == "MAG");
        CHECK(to_string(mg_view_position::spot_mag) == "SPOT MAG");
    }

    SECTION("enum to string - rolled views") {
        CHECK(to_string(mg_view_position::rl) == "RL");
        CHECK(to_string(mg_view_position::rm) == "RM");
        CHECK(to_string(mg_view_position::rs) == "RS");
        CHECK(to_string(mg_view_position::ri) == "RI");
    }

    SECTION("enum to string - specialized views") {
        CHECK(to_string(mg_view_position::tangen) == "TAN");
        CHECK(to_string(mg_view_position::implant) == "ID");
    }

    SECTION("string to enum") {
        CHECK(parse_mg_view_position("CC") == mg_view_position::cc);
        CHECK(parse_mg_view_position("MLO") == mg_view_position::mlo);
        CHECK(parse_mg_view_position("ML") == mg_view_position::ml);
        CHECK(parse_mg_view_position("LM") == mg_view_position::lm);
        CHECK(parse_mg_view_position("XCCL") == mg_view_position::xccl);
        CHECK(parse_mg_view_position("XCCM") == mg_view_position::xccm);
        CHECK(parse_mg_view_position("SPOT") == mg_view_position::spot);
        CHECK(parse_mg_view_position("MAG") == mg_view_position::mag);
        CHECK(parse_mg_view_position("SPOT MAG") == mg_view_position::spot_mag);
        CHECK(parse_mg_view_position("SPOTMAG") == mg_view_position::spot_mag);
        CHECK(parse_mg_view_position("TAN") == mg_view_position::tangen);
        CHECK(parse_mg_view_position("TANGENTIAL") == mg_view_position::tangen);
        CHECK(parse_mg_view_position("ID") == mg_view_position::implant);
        CHECK(parse_mg_view_position("IMPLANT DISPLACED") == mg_view_position::implant);
    }

    SECTION("case insensitive parsing") {
        CHECK(parse_mg_view_position("cc") == mg_view_position::cc);
        CHECK(parse_mg_view_position("Cc") == mg_view_position::cc);
        CHECK(parse_mg_view_position("mlo") == mg_view_position::mlo);
        CHECK(parse_mg_view_position("Mlo") == mg_view_position::mlo);
    }

    SECTION("unknown values") {
        CHECK(parse_mg_view_position("") == mg_view_position::other);
        CHECK(parse_mg_view_position("INVALID") == mg_view_position::other);
        CHECK(parse_mg_view_position("AP") == mg_view_position::other);  // DX view, not MG
    }
}

TEST_CASE("mg_view_position classification functions", "[services][mg][view_position]") {
    SECTION("is_screening_view") {
        CHECK(is_screening_view(mg_view_position::cc));
        CHECK(is_screening_view(mg_view_position::mlo));

        CHECK_FALSE(is_screening_view(mg_view_position::ml));
        CHECK_FALSE(is_screening_view(mg_view_position::lm));
        CHECK_FALSE(is_screening_view(mg_view_position::spot));
        CHECK_FALSE(is_screening_view(mg_view_position::mag));
    }

    SECTION("is_magnification_view") {
        CHECK(is_magnification_view(mg_view_position::mag));
        CHECK(is_magnification_view(mg_view_position::spot_mag));

        CHECK_FALSE(is_magnification_view(mg_view_position::cc));
        CHECK_FALSE(is_magnification_view(mg_view_position::mlo));
        CHECK_FALSE(is_magnification_view(mg_view_position::spot));
    }

    SECTION("is_spot_compression_view") {
        CHECK(is_spot_compression_view(mg_view_position::spot));
        CHECK(is_spot_compression_view(mg_view_position::spot_mag));

        CHECK_FALSE(is_spot_compression_view(mg_view_position::cc));
        CHECK_FALSE(is_spot_compression_view(mg_view_position::mlo));
        CHECK_FALSE(is_spot_compression_view(mg_view_position::mag));
    }
}

TEST_CASE("get_valid_mg_view_positions returns all valid codes", "[services][mg][view_position]") {
    auto views = get_valid_mg_view_positions();

    CHECK_FALSE(views.empty());
    CHECK(views.size() >= 15);

    // Should contain standard views
    bool has_cc = false;
    bool has_mlo = false;
    for (const auto& v : views) {
        if (v == "CC") has_cc = true;
        if (v == "MLO") has_mlo = true;
    }
    CHECK(has_cc);
    CHECK(has_mlo);
}

// ============================================================================
// Compression Force Tests
// ============================================================================

TEST_CASE("compression force validation", "[services][mg][compression]") {
    SECTION("valid forces") {
        CHECK(is_valid_compression_force(50.0));
        CHECK(is_valid_compression_force(100.0));
        CHECK(is_valid_compression_force(150.0));
        CHECK(is_valid_compression_force(200.0));
        CHECK(is_valid_compression_force(20.0));   // Lower bound
        CHECK(is_valid_compression_force(300.0));  // Upper bound
    }

    SECTION("invalid forces") {
        CHECK_FALSE(is_valid_compression_force(0.0));
        CHECK_FALSE(is_valid_compression_force(10.0));
        CHECK_FALSE(is_valid_compression_force(350.0));
        CHECK_FALSE(is_valid_compression_force(-50.0));
        CHECK_FALSE(is_valid_compression_force(500.0));
    }

    SECTION("typical range") {
        auto [min_typical, max_typical] = get_typical_compression_force_range();
        CHECK(min_typical == 50.0);
        CHECK(max_typical == 200.0);
    }
}

TEST_CASE("compressed breast thickness validation", "[services][mg][compression]") {
    SECTION("valid thickness") {
        CHECK(is_valid_compressed_breast_thickness(30.0));
        CHECK(is_valid_compressed_breast_thickness(50.0));
        CHECK(is_valid_compressed_breast_thickness(70.0));
        CHECK(is_valid_compressed_breast_thickness(10.0));   // Lower bound
        CHECK(is_valid_compressed_breast_thickness(150.0));  // Upper bound
    }

    SECTION("invalid thickness") {
        CHECK_FALSE(is_valid_compressed_breast_thickness(0.0));
        CHECK_FALSE(is_valid_compressed_breast_thickness(5.0));
        CHECK_FALSE(is_valid_compressed_breast_thickness(200.0));
        CHECK_FALSE(is_valid_compressed_breast_thickness(-10.0));
    }
}

// ============================================================================
// MG Image Type Tests
// ============================================================================

TEST_CASE("mg_image_type conversion", "[services][mg][image_type]") {
    CHECK(to_string(mg_image_type::for_presentation) == "FOR PRESENTATION");
    CHECK(to_string(mg_image_type::for_processing) == "FOR PROCESSING");
}

// ============================================================================
// CAD Processing Status Tests
// ============================================================================

TEST_CASE("cad_processing_status conversion", "[services][mg][cad]") {
    CHECK(to_string(cad_processing_status::not_processed) == "NOT PROCESSED");
    CHECK(to_string(cad_processing_status::processed_no_findings) == "PROCESSED - NO FINDINGS");
    CHECK(to_string(cad_processing_status::processed_findings) == "PROCESSED - FINDINGS");
    CHECK(to_string(cad_processing_status::processing_failed) == "PROCESSING FAILED");
    CHECK(to_string(cad_processing_status::pending) == "PENDING");
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_mg_transfer_syntaxes returns valid syntaxes", "[services][mg][transfer_syntax]") {
    auto syntaxes = get_mg_transfer_syntaxes();

    CHECK_FALSE(syntaxes.empty());

    // Should include Explicit VR Little Endian
    auto it = std::find(syntaxes.begin(), syntaxes.end(), "1.2.840.10008.1.2.1");
    CHECK(it != syntaxes.end());

    // Should include Implicit VR Little Endian
    it = std::find(syntaxes.begin(), syntaxes.end(), "1.2.840.10008.1.2");
    CHECK(it != syntaxes.end());

    // Should include JPEG Lossless for diagnostic quality
    it = std::find(syntaxes.begin(), syntaxes.end(), "1.2.840.10008.1.2.4.70");
    CHECK(it != syntaxes.end());

    // Should include JPEG 2000 Lossless
    it = std::find(syntaxes.begin(), syntaxes.end(), "1.2.840.10008.1.2.4.90");
    CHECK(it != syntaxes.end());
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_CASE("is_valid_laterality_view_combination validates combinations", "[services][mg][utility]") {
    SECTION("left/right with any view is valid") {
        CHECK(is_valid_laterality_view_combination(breast_laterality::left, mg_view_position::cc));
        CHECK(is_valid_laterality_view_combination(breast_laterality::right, mg_view_position::cc));
        CHECK(is_valid_laterality_view_combination(breast_laterality::left, mg_view_position::mlo));
        CHECK(is_valid_laterality_view_combination(breast_laterality::right, mg_view_position::mlo));
        CHECK(is_valid_laterality_view_combination(breast_laterality::left, mg_view_position::spot));
    }

    SECTION("bilateral with cleavage view is valid") {
        CHECK(is_valid_laterality_view_combination(breast_laterality::bilateral, mg_view_position::cv));
    }

    SECTION("bilateral with most views is invalid") {
        CHECK_FALSE(is_valid_laterality_view_combination(breast_laterality::bilateral, mg_view_position::cc));
        CHECK_FALSE(is_valid_laterality_view_combination(breast_laterality::bilateral, mg_view_position::mlo));
    }

    SECTION("unknown laterality is problematic") {
        CHECK_FALSE(is_valid_laterality_view_combination(breast_laterality::unknown, mg_view_position::cc));
        // Unknown with other view is allowed
        CHECK(is_valid_laterality_view_combination(breast_laterality::unknown, mg_view_position::other));
    }
}

TEST_CASE("get_standard_screening_views returns four views", "[services][mg][utility]") {
    auto views = get_standard_screening_views();

    CHECK(views.size() == 4);

    // Should have R CC, L CC, R MLO, L MLO
    bool has_rcc = false, has_lcc = false, has_rmlo = false, has_lmlo = false;
    for (const auto& [lat, view] : views) {
        if (lat == breast_laterality::right && view == mg_view_position::cc) has_rcc = true;
        if (lat == breast_laterality::left && view == mg_view_position::cc) has_lcc = true;
        if (lat == breast_laterality::right && view == mg_view_position::mlo) has_rmlo = true;
        if (lat == breast_laterality::left && view == mg_view_position::mlo) has_lmlo = true;
    }
    CHECK(has_rcc);
    CHECK(has_lcc);
    CHECK(has_rmlo);
    CHECK(has_lmlo);
}

TEST_CASE("create_mg_image_type creates proper format", "[services][mg][utility]") {
    SECTION("original primary for presentation") {
        auto result = create_mg_image_type(true, true, mg_image_type::for_presentation);
        CHECK(result.find("ORIGINAL") != std::string::npos);
        CHECK(result.find("PRIMARY") != std::string::npos);
    }

    SECTION("derived secondary for processing") {
        auto result = create_mg_image_type(false, false, mg_image_type::for_processing);
        CHECK(result.find("DERIVED") != std::string::npos);
        CHECK(result.find("SECONDARY") != std::string::npos);
    }
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("MG classes are registered in SOP Class Registry", "[services][mg][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("MG For Presentation is registered") {
        CHECK(registry.is_supported(mg_image_storage_for_presentation_uid));
        const auto* info = registry.get_info(mg_image_storage_for_presentation_uid);
        REQUIRE(info != nullptr);
        CHECK(info->modality == modality_type::mg);
        CHECK(info->category == sop_class_category::storage);
    }

    SECTION("MG For Processing is registered") {
        CHECK(registry.is_supported(mg_image_storage_for_processing_uid));
        const auto* info = registry.get_info(mg_image_storage_for_processing_uid);
        REQUIRE(info != nullptr);
        CHECK(info->modality == modality_type::mg);
    }

    SECTION("get_by_modality returns MG classes") {
        auto mg_classes = registry.get_by_modality(modality_type::mg);
        CHECK(mg_classes.size() >= 2);

        // Should contain the main MG classes
        auto it = std::find(mg_classes.begin(), mg_classes.end(),
                           std::string(mg_image_storage_for_presentation_uid));
        CHECK(it != mg_classes.end());
    }
}
