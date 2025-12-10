/**
 * @file rt_storage_test.cpp
 * @brief Unit tests for Radiation Therapy (RT) Storage SOP Classes
 */

#include <pacs/services/sop_classes/rt_storage.hpp>
#include <pacs/services/sop_class_registry.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace pacs::services::sop_classes;
using namespace pacs::services;

// ============================================================================
// RT Storage SOP Class UID Tests
// ============================================================================

TEST_CASE("RT Storage SOP Class UIDs are correct", "[services][rt][sop_class]") {
    CHECK(rt_plan_storage_uid == "1.2.840.10008.5.1.4.1.1.481.5");
    CHECK(rt_dose_storage_uid == "1.2.840.10008.5.1.4.1.1.481.2");
    CHECK(rt_structure_set_storage_uid == "1.2.840.10008.5.1.4.1.1.481.3");
    CHECK(rt_image_storage_uid == "1.2.840.10008.5.1.4.1.1.481.1");
    CHECK(rt_beams_treatment_record_storage_uid == "1.2.840.10008.5.1.4.1.1.481.4");
    CHECK(rt_brachy_treatment_record_storage_uid == "1.2.840.10008.5.1.4.1.1.481.6");
    CHECK(rt_treatment_summary_record_storage_uid == "1.2.840.10008.5.1.4.1.1.481.7");
    CHECK(rt_ion_plan_storage_uid == "1.2.840.10008.5.1.4.1.1.481.8");
    CHECK(rt_ion_beams_treatment_record_storage_uid == "1.2.840.10008.5.1.4.1.1.481.9");
}

TEST_CASE("is_rt_storage_sop_class identifies RT classes", "[services][rt][sop_class]") {
    SECTION("recognizes RT Plan") {
        CHECK(is_rt_storage_sop_class(rt_plan_storage_uid));
    }

    SECTION("recognizes RT Dose") {
        CHECK(is_rt_storage_sop_class(rt_dose_storage_uid));
    }

    SECTION("recognizes RT Structure Set") {
        CHECK(is_rt_storage_sop_class(rt_structure_set_storage_uid));
    }

    SECTION("recognizes RT Image") {
        CHECK(is_rt_storage_sop_class(rt_image_storage_uid));
    }

    SECTION("recognizes RT Treatment Records") {
        CHECK(is_rt_storage_sop_class(rt_beams_treatment_record_storage_uid));
        CHECK(is_rt_storage_sop_class(rt_brachy_treatment_record_storage_uid));
        CHECK(is_rt_storage_sop_class(rt_treatment_summary_record_storage_uid));
    }

    SECTION("recognizes RT Ion Plan classes") {
        CHECK(is_rt_storage_sop_class(rt_ion_plan_storage_uid));
        CHECK(is_rt_storage_sop_class(rt_ion_beams_treatment_record_storage_uid));
    }

    SECTION("rejects non-RT classes") {
        CHECK_FALSE(is_rt_storage_sop_class("1.2.840.10008.5.1.4.1.1.2"));    // CT
        CHECK_FALSE(is_rt_storage_sop_class("1.2.840.10008.5.1.4.1.1.4"));    // MR
        CHECK_FALSE(is_rt_storage_sop_class("1.2.840.10008.5.1.4.1.1.6.1"));  // US
        CHECK_FALSE(is_rt_storage_sop_class("1.2.840.10008.5.1.4.1.1.20"));   // NM
        CHECK_FALSE(is_rt_storage_sop_class("1.2.840.10008.1.1"));            // Verification
        CHECK_FALSE(is_rt_storage_sop_class(""));
        CHECK_FALSE(is_rt_storage_sop_class("invalid"));
    }
}

TEST_CASE("is_rt_plan_sop_class identifies plan types", "[services][rt][sop_class]") {
    CHECK(is_rt_plan_sop_class(rt_plan_storage_uid));
    CHECK(is_rt_plan_sop_class(rt_ion_plan_storage_uid));

    CHECK_FALSE(is_rt_plan_sop_class(rt_dose_storage_uid));
    CHECK_FALSE(is_rt_plan_sop_class(rt_structure_set_storage_uid));
    CHECK_FALSE(is_rt_plan_sop_class(rt_image_storage_uid));
}

TEST_CASE("rt_sop_class_has_pixel_data identifies pixel data classes", "[services][rt][sop_class]") {
    // RT Dose and RT Image have pixel data
    CHECK(rt_sop_class_has_pixel_data(rt_dose_storage_uid));
    CHECK(rt_sop_class_has_pixel_data(rt_image_storage_uid));

    // RT Plan, Structure Set, and records do not have pixel data
    CHECK_FALSE(rt_sop_class_has_pixel_data(rt_plan_storage_uid));
    CHECK_FALSE(rt_sop_class_has_pixel_data(rt_structure_set_storage_uid));
    CHECK_FALSE(rt_sop_class_has_pixel_data(rt_beams_treatment_record_storage_uid));
}

// ============================================================================
// RT SOP Class Information Tests
// ============================================================================

TEST_CASE("get_rt_sop_class_info returns correct information", "[services][rt][sop_class]") {
    SECTION("RT Plan Storage info") {
        const auto* info = get_rt_sop_class_info(rt_plan_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == rt_plan_storage_uid);
        CHECK(info->name == "RT Plan Storage");
        CHECK_FALSE(info->is_retired);
        CHECK_FALSE(info->has_pixel_data);
    }

    SECTION("RT Dose Storage info") {
        const auto* info = get_rt_sop_class_info(rt_dose_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == rt_dose_storage_uid);
        CHECK(info->name == "RT Dose Storage");
        CHECK_FALSE(info->is_retired);
        CHECK(info->has_pixel_data);  // Dose grid has pixel data
    }

    SECTION("RT Structure Set Storage info") {
        const auto* info = get_rt_sop_class_info(rt_structure_set_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->uid == rt_structure_set_storage_uid);
        CHECK(info->name == "RT Structure Set Storage");
        CHECK_FALSE(info->is_retired);
        CHECK_FALSE(info->has_pixel_data);
    }

    SECTION("returns nullptr for unknown UID") {
        CHECK(get_rt_sop_class_info("1.2.3.4.5.6.7") == nullptr);
    }
}

TEST_CASE("get_rt_storage_sop_classes returns correct list", "[services][rt][sop_class]") {
    auto classes = get_rt_storage_sop_classes(true);
    CHECK(classes.size() == 9);  // All 9 RT SOP classes

    // Verify all known RT classes are included
    CHECK(std::find(classes.begin(), classes.end(),
                   std::string(rt_plan_storage_uid)) != classes.end());
    CHECK(std::find(classes.begin(), classes.end(),
                   std::string(rt_dose_storage_uid)) != classes.end());
    CHECK(std::find(classes.begin(), classes.end(),
                   std::string(rt_structure_set_storage_uid)) != classes.end());
}

// ============================================================================
// Transfer Syntax Tests
// ============================================================================

TEST_CASE("get_rt_transfer_syntaxes returns valid syntaxes", "[services][rt][transfer]") {
    auto syntaxes = get_rt_transfer_syntaxes();

    CHECK(syntaxes.size() > 0);

    // Should include Explicit VR Little Endian (most preferred)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.1") != syntaxes.end());

    // Should include Implicit VR Little Endian (universal baseline)
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2") != syntaxes.end());

    // Should include lossless compression for dose grids
    CHECK(std::find(syntaxes.begin(), syntaxes.end(),
                   "1.2.840.10008.1.2.4.70") != syntaxes.end());
}

// ============================================================================
// RT Plan Intent Tests
// ============================================================================

TEST_CASE("rt_plan_intent conversions", "[services][rt][plan_intent]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_plan_intent::curative) == "CURATIVE");
        CHECK(to_string(rt_plan_intent::palliative) == "PALLIATIVE");
        CHECK(to_string(rt_plan_intent::prophylactic) == "PROPHYLACTIC");
        CHECK(to_string(rt_plan_intent::verification) == "VERIFICATION");
        CHECK(to_string(rt_plan_intent::machine_qa) == "MACHINE_QA");
        CHECK(to_string(rt_plan_intent::research) == "RESEARCH");
        CHECK(to_string(rt_plan_intent::service) == "SERVICE");
    }

    SECTION("parse_rt_plan_intent parses correctly") {
        CHECK(parse_rt_plan_intent("CURATIVE") == rt_plan_intent::curative);
        CHECK(parse_rt_plan_intent("PALLIATIVE") == rt_plan_intent::palliative);
        CHECK(parse_rt_plan_intent("VERIFICATION") == rt_plan_intent::verification);
        CHECK(parse_rt_plan_intent("UNKNOWN") == rt_plan_intent::curative);  // Default
    }
}

// ============================================================================
// RT Plan Geometry Tests
// ============================================================================

TEST_CASE("rt_plan_geometry conversions", "[services][rt][plan_geometry]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_plan_geometry::patient) == "PATIENT");
        CHECK(to_string(rt_plan_geometry::treatment_device) == "TREATMENT_DEVICE");
    }

    SECTION("parse_rt_plan_geometry parses correctly") {
        CHECK(parse_rt_plan_geometry("PATIENT") == rt_plan_geometry::patient);
        CHECK(parse_rt_plan_geometry("TREATMENT_DEVICE") == rt_plan_geometry::treatment_device);
        CHECK(parse_rt_plan_geometry("UNKNOWN") == rt_plan_geometry::patient);  // Default
    }
}

// ============================================================================
// RT Dose Type Tests
// ============================================================================

TEST_CASE("rt_dose_type conversions", "[services][rt][dose_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_dose_type::physical) == "PHYSICAL");
        CHECK(to_string(rt_dose_type::effective) == "EFFECTIVE");
        CHECK(to_string(rt_dose_type::error) == "ERROR");
    }

    SECTION("parse_rt_dose_type parses correctly") {
        CHECK(parse_rt_dose_type("PHYSICAL") == rt_dose_type::physical);
        CHECK(parse_rt_dose_type("EFFECTIVE") == rt_dose_type::effective);
        CHECK(parse_rt_dose_type("ERROR") == rt_dose_type::error);
        CHECK(parse_rt_dose_type("UNKNOWN") == rt_dose_type::physical);  // Default
    }
}

// ============================================================================
// RT Dose Summation Type Tests
// ============================================================================

TEST_CASE("rt_dose_summation_type conversions", "[services][rt][summation_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_dose_summation_type::plan) == "PLAN");
        CHECK(to_string(rt_dose_summation_type::multi_plan) == "MULTI_PLAN");
        CHECK(to_string(rt_dose_summation_type::fraction) == "FRACTION");
        CHECK(to_string(rt_dose_summation_type::beam) == "BEAM");
        CHECK(to_string(rt_dose_summation_type::brachy) == "BRACHY");
    }

    SECTION("parse_rt_dose_summation_type parses correctly") {
        CHECK(parse_rt_dose_summation_type("PLAN") == rt_dose_summation_type::plan);
        CHECK(parse_rt_dose_summation_type("FRACTION") == rt_dose_summation_type::fraction);
        CHECK(parse_rt_dose_summation_type("BEAM") == rt_dose_summation_type::beam);
        CHECK(parse_rt_dose_summation_type("UNKNOWN") == rt_dose_summation_type::plan);  // Default
    }
}

// ============================================================================
// RT Dose Units Tests
// ============================================================================

TEST_CASE("rt_dose_units conversions", "[services][rt][dose_units]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_dose_units::gy) == "GY");
        CHECK(to_string(rt_dose_units::relative) == "RELATIVE");
    }

    SECTION("parse_rt_dose_units parses correctly") {
        CHECK(parse_rt_dose_units("GY") == rt_dose_units::gy);
        CHECK(parse_rt_dose_units("RELATIVE") == rt_dose_units::relative);
        CHECK(parse_rt_dose_units("UNKNOWN") == rt_dose_units::gy);  // Default
    }
}

// ============================================================================
// RT ROI Interpreted Type Tests
// ============================================================================

TEST_CASE("rt_roi_interpreted_type conversions", "[services][rt][roi_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_roi_interpreted_type::external) == "EXTERNAL");
        CHECK(to_string(rt_roi_interpreted_type::ptv) == "PTV");
        CHECK(to_string(rt_roi_interpreted_type::ctv) == "CTV");
        CHECK(to_string(rt_roi_interpreted_type::gtv) == "GTV");
        CHECK(to_string(rt_roi_interpreted_type::organ) == "ORGAN");
        CHECK(to_string(rt_roi_interpreted_type::avoidance) == "AVOIDANCE");
        CHECK(to_string(rt_roi_interpreted_type::bolus) == "BOLUS");
    }

    SECTION("parse_rt_roi_interpreted_type parses correctly") {
        CHECK(parse_rt_roi_interpreted_type("EXTERNAL") == rt_roi_interpreted_type::external);
        CHECK(parse_rt_roi_interpreted_type("PTV") == rt_roi_interpreted_type::ptv);
        CHECK(parse_rt_roi_interpreted_type("CTV") == rt_roi_interpreted_type::ctv);
        CHECK(parse_rt_roi_interpreted_type("GTV") == rt_roi_interpreted_type::gtv);
        CHECK(parse_rt_roi_interpreted_type("ORGAN") == rt_roi_interpreted_type::organ);
        CHECK(parse_rt_roi_interpreted_type("UNKNOWN") == rt_roi_interpreted_type::organ);  // Default
    }
}

// ============================================================================
// RT ROI Generation Algorithm Tests
// ============================================================================

TEST_CASE("rt_roi_generation_algorithm conversions", "[services][rt][roi_algorithm]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_roi_generation_algorithm::automatic) == "AUTOMATIC");
        CHECK(to_string(rt_roi_generation_algorithm::semiautomatic) == "SEMIAUTOMATIC");
        CHECK(to_string(rt_roi_generation_algorithm::manual) == "MANUAL");
    }

    SECTION("parse_rt_roi_generation_algorithm parses correctly") {
        CHECK(parse_rt_roi_generation_algorithm("AUTOMATIC") == rt_roi_generation_algorithm::automatic);
        CHECK(parse_rt_roi_generation_algorithm("SEMIAUTOMATIC") == rt_roi_generation_algorithm::semiautomatic);
        CHECK(parse_rt_roi_generation_algorithm("MANUAL") == rt_roi_generation_algorithm::manual);
        CHECK(parse_rt_roi_generation_algorithm("UNKNOWN") == rt_roi_generation_algorithm::manual);  // Default
    }
}

// ============================================================================
// RT Beam Type Tests
// ============================================================================

TEST_CASE("rt_beam_type conversions", "[services][rt][beam_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_beam_type::static_beam) == "STATIC");
        CHECK(to_string(rt_beam_type::dynamic) == "DYNAMIC");
    }

    SECTION("parse_rt_beam_type parses correctly") {
        CHECK(parse_rt_beam_type("STATIC") == rt_beam_type::static_beam);
        CHECK(parse_rt_beam_type("DYNAMIC") == rt_beam_type::dynamic);
        CHECK(parse_rt_beam_type("UNKNOWN") == rt_beam_type::static_beam);  // Default
    }
}

// ============================================================================
// RT Radiation Type Tests
// ============================================================================

TEST_CASE("rt_radiation_type conversions", "[services][rt][radiation_type]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_radiation_type::photon) == "PHOTON");
        CHECK(to_string(rt_radiation_type::electron) == "ELECTRON");
        CHECK(to_string(rt_radiation_type::neutron) == "NEUTRON");
        CHECK(to_string(rt_radiation_type::proton) == "PROTON");
        CHECK(to_string(rt_radiation_type::ion) == "ION");
    }

    SECTION("parse_rt_radiation_type parses correctly") {
        CHECK(parse_rt_radiation_type("PHOTON") == rt_radiation_type::photon);
        CHECK(parse_rt_radiation_type("ELECTRON") == rt_radiation_type::electron);
        CHECK(parse_rt_radiation_type("PROTON") == rt_radiation_type::proton);
        CHECK(parse_rt_radiation_type("ION") == rt_radiation_type::ion);
        CHECK(parse_rt_radiation_type("UNKNOWN") == rt_radiation_type::photon);  // Default
    }
}

// ============================================================================
// RT Image Plane Tests
// ============================================================================

TEST_CASE("rt_image_plane conversions", "[services][rt][image_plane]") {
    SECTION("to_string produces correct DICOM strings") {
        CHECK(to_string(rt_image_plane::axial) == "AXIAL");
        CHECK(to_string(rt_image_plane::localizer) == "LOCALIZER");
        CHECK(to_string(rt_image_plane::drr) == "DRR");
        CHECK(to_string(rt_image_plane::portal) == "PORTAL");
        CHECK(to_string(rt_image_plane::fluence) == "FLUENCE");
    }

    SECTION("parse_rt_image_plane parses correctly") {
        CHECK(parse_rt_image_plane("AXIAL") == rt_image_plane::axial);
        CHECK(parse_rt_image_plane("DRR") == rt_image_plane::drr);
        CHECK(parse_rt_image_plane("PORTAL") == rt_image_plane::portal);
        CHECK(parse_rt_image_plane("UNKNOWN") == rt_image_plane::portal);  // Default
    }
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("RT SOP classes are registered in central registry", "[services][rt][registry]") {
    auto& registry = sop_class_registry::instance();

    SECTION("RT Plan is registered") {
        CHECK(registry.is_supported(rt_plan_storage_uid));
        const auto* info = registry.get_info(rt_plan_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::rt);
    }

    SECTION("RT Dose is registered") {
        CHECK(registry.is_supported(rt_dose_storage_uid));
        const auto* info = registry.get_info(rt_dose_storage_uid);
        REQUIRE(info != nullptr);
        CHECK(info->category == sop_class_category::storage);
        CHECK(info->modality == modality_type::rt);
    }

    SECTION("RT Structure Set is registered") {
        CHECK(registry.is_supported(rt_structure_set_storage_uid));
    }

    SECTION("RT classes are returned by modality query") {
        auto rt_classes = registry.get_by_modality(modality_type::rt, true);
        CHECK(rt_classes.size() == 9);  // All 9 RT SOP classes
    }
}

TEST_CASE("RT modality parsing works correctly", "[services][rt][registry]") {
    CHECK(sop_class_registry::parse_modality("RT") == modality_type::rt);
    CHECK(sop_class_registry::parse_modality("RTPLAN") == modality_type::rt);
    CHECK(sop_class_registry::parse_modality("RTDOSE") == modality_type::rt);
    CHECK(sop_class_registry::parse_modality("RTSTRUCT") == modality_type::rt);
    CHECK(sop_class_registry::parse_modality("RTIMAGE") == modality_type::rt);
    CHECK(sop_class_registry::parse_modality("RTRECORD") == modality_type::rt);
}
