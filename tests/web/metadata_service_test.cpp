/**
 * @file metadata_service_test.cpp
 * @brief Unit tests for metadata service
 *
 * @see Issue #544 - Implement Selective Metadata & Navigation APIs
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/web/metadata_service.hpp"

using namespace pacs::web;

// =============================================================================
// Preset String Conversion Tests
// =============================================================================

TEST_CASE("preset_to_string conversion", "[web][metadata]") {
    REQUIRE(preset_to_string(metadata_preset::image_display) == "image_display");
    REQUIRE(preset_to_string(metadata_preset::window_level) == "window_level");
    REQUIRE(preset_to_string(metadata_preset::patient_info) == "patient_info");
    REQUIRE(preset_to_string(metadata_preset::acquisition) == "acquisition");
    REQUIRE(preset_to_string(metadata_preset::positioning) == "positioning");
    REQUIRE(preset_to_string(metadata_preset::multiframe) == "multiframe");
}

TEST_CASE("preset_from_string conversion", "[web][metadata]") {
    SECTION("valid presets") {
        auto result = preset_from_string("image_display");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == metadata_preset::image_display);

        result = preset_from_string("window_level");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == metadata_preset::window_level);

        result = preset_from_string("patient_info");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == metadata_preset::patient_info);

        result = preset_from_string("acquisition");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == metadata_preset::acquisition);

        result = preset_from_string("positioning");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == metadata_preset::positioning);

        result = preset_from_string("multiframe");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == metadata_preset::multiframe);
    }

    SECTION("invalid preset") {
        auto result = preset_from_string("invalid");
        REQUIRE_FALSE(result.has_value());

        result = preset_from_string("");
        REQUIRE_FALSE(result.has_value());
    }
}

// =============================================================================
// Sort Order String Conversion Tests
// =============================================================================

TEST_CASE("sort_order_to_string conversion", "[web][metadata]") {
    REQUIRE(sort_order_to_string(sort_order::position) == "position");
    REQUIRE(sort_order_to_string(sort_order::instance_number) == "instance_number");
    REQUIRE(sort_order_to_string(sort_order::acquisition_time) == "acquisition_time");
}

TEST_CASE("sort_order_from_string conversion", "[web][metadata]") {
    SECTION("valid sort orders") {
        auto result = sort_order_from_string("position");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == sort_order::position);

        result = sort_order_from_string("instance_number");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == sort_order::instance_number);

        result = sort_order_from_string("acquisition_time");
        REQUIRE(result.has_value());
        REQUIRE(result.value() == sort_order::acquisition_time);
    }

    SECTION("invalid sort order") {
        auto result = sort_order_from_string("invalid");
        REQUIRE_FALSE(result.has_value());
    }
}

// =============================================================================
// Metadata Request Tests
// =============================================================================

TEST_CASE("metadata_request default values", "[web][metadata]") {
    metadata_request request;

    REQUIRE(request.tags.empty());
    REQUIRE_FALSE(request.preset.has_value());
    REQUIRE(request.include_private == false);
}

TEST_CASE("metadata_request with tags", "[web][metadata]") {
    metadata_request request;
    request.tags = {"00280010", "00280011", "00281050"};

    REQUIRE(request.tags.size() == 3);
    REQUIRE(request.tags[0] == "00280010");
    REQUIRE(request.tags[1] == "00280011");
    REQUIRE(request.tags[2] == "00281050");
}

TEST_CASE("metadata_request with preset", "[web][metadata]") {
    metadata_request request;
    request.preset = metadata_preset::image_display;

    REQUIRE(request.preset.has_value());
    REQUIRE(request.preset.value() == metadata_preset::image_display);
}

// =============================================================================
// Metadata Response Tests
// =============================================================================

TEST_CASE("metadata_response success", "[web][metadata]") {
    std::unordered_map<std::string, std::string> tags;
    tags["00280010"] = "512";
    tags["00280011"] = "512";

    auto result = metadata_response::ok(tags);

    REQUIRE(result.success == true);
    REQUIRE(result.error_message.empty());
    REQUIRE(result.tags.size() == 2);
    REQUIRE(result.tags["00280010"] == "512");
}

TEST_CASE("metadata_response error", "[web][metadata]") {
    auto result = metadata_response::error("Instance not found");

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Instance not found");
    REQUIRE(result.tags.empty());
}

// =============================================================================
// Sorted Instance Tests
// =============================================================================

TEST_CASE("sorted_instance structure", "[web][metadata]") {
    sorted_instance inst;
    inst.sop_instance_uid = "1.2.3.4.5";
    inst.instance_number = 1;
    inst.slice_location = -150.5;
    inst.image_position_patient = {0.0, 0.0, -150.5};
    inst.acquisition_time = "120530";

    REQUIRE(inst.sop_instance_uid == "1.2.3.4.5");
    REQUIRE(inst.instance_number.has_value());
    REQUIRE(inst.instance_number.value() == 1);
    REQUIRE(inst.slice_location.has_value());
    REQUIRE(inst.slice_location.value() == -150.5);
    REQUIRE(inst.image_position_patient.has_value());
    REQUIRE(inst.image_position_patient->size() == 3);
    REQUIRE(inst.acquisition_time.has_value());
}

TEST_CASE("sorted_instances_response success", "[web][metadata]") {
    std::vector<sorted_instance> instances;
    sorted_instance inst1;
    inst1.sop_instance_uid = "1.2.3.1";
    inst1.instance_number = 1;
    instances.push_back(inst1);

    sorted_instance inst2;
    inst2.sop_instance_uid = "1.2.3.2";
    inst2.instance_number = 2;
    instances.push_back(inst2);

    auto result = sorted_instances_response::ok(instances, 2);

    REQUIRE(result.success == true);
    REQUIRE(result.instances.size() == 2);
    REQUIRE(result.total == 2);
}

TEST_CASE("sorted_instances_response error", "[web][metadata]") {
    auto result = sorted_instances_response::error("Series not found");

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Series not found");
    REQUIRE(result.instances.empty());
}

// =============================================================================
// Navigation Info Tests
// =============================================================================

TEST_CASE("navigation_info success", "[web][metadata]") {
    navigation_info nav = navigation_info::ok();
    nav.previous = "1.2.3.49";
    nav.next = "1.2.3.51";
    nav.index = 50;
    nav.total = 120;
    nav.first = "1.2.3.1";
    nav.last = "1.2.3.120";

    REQUIRE(nav.success == true);
    REQUIRE(nav.previous == "1.2.3.49");
    REQUIRE(nav.next == "1.2.3.51");
    REQUIRE(nav.index == 50);
    REQUIRE(nav.total == 120);
    REQUIRE(nav.first == "1.2.3.1");
    REQUIRE(nav.last == "1.2.3.120");
}

TEST_CASE("navigation_info error", "[web][metadata]") {
    auto nav = navigation_info::error("Instance not found");

    REQUIRE(nav.success == false);
    REQUIRE(nav.error_message == "Instance not found");
}

// =============================================================================
// Window/Level Preset Tests
// =============================================================================

TEST_CASE("window_level_preset structure", "[web][metadata]") {
    window_level_preset preset;
    preset.name = "Lung";
    preset.center = -600;
    preset.width = 1500;

    REQUIRE(preset.name == "Lung");
    REQUIRE(preset.center == -600);
    REQUIRE(preset.width == 1500);
}

TEST_CASE("get_window_level_presets CT", "[web][metadata]") {
    auto presets = metadata_service::get_window_level_presets("CT");

    REQUIRE(presets.size() >= 4);  // At least Lung, Bone, Soft Tissue, Brain

    // Check for common CT presets
    bool found_lung = false;
    bool found_bone = false;
    for (const auto& p : presets) {
        if (p.name == "Lung") {
            found_lung = true;
            REQUIRE(p.center == -600);
            REQUIRE(p.width == 1500);
        }
        if (p.name == "Bone") {
            found_bone = true;
            REQUIRE(p.center == 300);
            REQUIRE(p.width == 1500);
        }
    }
    REQUIRE(found_lung);
    REQUIRE(found_bone);
}

TEST_CASE("get_window_level_presets MR", "[web][metadata]") {
    auto presets = metadata_service::get_window_level_presets("MR");

    REQUIRE(presets.size() >= 1);
}

TEST_CASE("get_window_level_presets unknown modality", "[web][metadata]") {
    auto presets = metadata_service::get_window_level_presets("UNKNOWN");

    // Should return generic presets
    REQUIRE(presets.size() >= 1);
}

// =============================================================================
// VOI LUT Info Tests
// =============================================================================

TEST_CASE("voi_lut_info success", "[web][metadata]") {
    voi_lut_info info = voi_lut_info::ok();
    info.window_center = {40, 300};
    info.window_width = {400, 1500};
    info.window_explanations = {"Soft Tissue", "Bone"};
    info.rescale_slope = 1.0;
    info.rescale_intercept = -1024;

    REQUIRE(info.success == true);
    REQUIRE(info.window_center.size() == 2);
    REQUIRE(info.window_width.size() == 2);
    REQUIRE(info.window_explanations.size() == 2);
    REQUIRE(info.rescale_slope == 1.0);
    REQUIRE(info.rescale_intercept == -1024);
}

TEST_CASE("voi_lut_info error", "[web][metadata]") {
    auto info = voi_lut_info::error("Instance not found");

    REQUIRE(info.success == false);
    REQUIRE(info.error_message == "Instance not found");
}

// =============================================================================
// Frame Info Tests
// =============================================================================

TEST_CASE("frame_info success", "[web][metadata]") {
    frame_info info = frame_info::ok();
    info.total_frames = 50;
    info.frame_time = 33.33;
    info.frame_rate = 30.0;
    info.rows = 512;
    info.columns = 512;

    REQUIRE(info.success == true);
    REQUIRE(info.total_frames == 50);
    REQUIRE(info.frame_time.has_value());
    REQUIRE(info.frame_time.value() == 33.33);
    REQUIRE(info.frame_rate.has_value());
    REQUIRE(info.frame_rate.value() == 30.0);
    REQUIRE(info.rows == 512);
    REQUIRE(info.columns == 512);
}

TEST_CASE("frame_info error", "[web][metadata]") {
    auto info = frame_info::error("File not found");

    REQUIRE(info.success == false);
    REQUIRE(info.error_message == "File not found");
}

// =============================================================================
// Preset Tags Tests
// =============================================================================

TEST_CASE("get_preset_tags image_display", "[web][metadata]") {
    auto tags = metadata_service::get_preset_tags(metadata_preset::image_display);

    // Should include Rows, Columns, BitsAllocated, etc.
    REQUIRE(tags.count("00280010") == 1);  // Rows
    REQUIRE(tags.count("00280011") == 1);  // Columns
    REQUIRE(tags.count("00280100") == 1);  // BitsAllocated
    REQUIRE(tags.count("00280101") == 1);  // BitsStored
    REQUIRE(tags.count("00280102") == 1);  // HighBit
    REQUIRE(tags.count("00280103") == 1);  // PixelRepresentation
    REQUIRE(tags.count("00280004") == 1);  // PhotometricInterpretation
    REQUIRE(tags.count("00280002") == 1);  // SamplesPerPixel
}

TEST_CASE("get_preset_tags window_level", "[web][metadata]") {
    auto tags = metadata_service::get_preset_tags(metadata_preset::window_level);

    REQUIRE(tags.count("00281050") == 1);  // WindowCenter
    REQUIRE(tags.count("00281051") == 1);  // WindowWidth
    REQUIRE(tags.count("00281053") == 1);  // RescaleSlope
    REQUIRE(tags.count("00281052") == 1);  // RescaleIntercept
}

TEST_CASE("get_preset_tags patient_info", "[web][metadata]") {
    auto tags = metadata_service::get_preset_tags(metadata_preset::patient_info);

    REQUIRE(tags.count("00100010") == 1);  // PatientName
    REQUIRE(tags.count("00100020") == 1);  // PatientID
    REQUIRE(tags.count("00100030") == 1);  // PatientBirthDate
    REQUIRE(tags.count("00100040") == 1);  // PatientSex
    REQUIRE(tags.count("00101010") == 1);  // PatientAge
}

TEST_CASE("get_preset_tags positioning", "[web][metadata]") {
    auto tags = metadata_service::get_preset_tags(metadata_preset::positioning);

    REQUIRE(tags.count("00200032") == 1);  // ImagePositionPatient
    REQUIRE(tags.count("00200037") == 1);  // ImageOrientationPatient
    REQUIRE(tags.count("00201041") == 1);  // SliceLocation
    REQUIRE(tags.count("00280030") == 1);  // PixelSpacing
}

// =============================================================================
// Service Construction Tests
// =============================================================================

TEST_CASE("metadata_service construction", "[web][metadata]") {
    // Test with nullptr database
    metadata_service service(nullptr);

    // Service should be created but return errors for operations
    metadata_request request;
    request.preset = metadata_preset::image_display;
    auto result = service.get_metadata("1.2.3.4", request);

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Database not configured");
}

TEST_CASE("metadata_service get_sorted_instances without database", "[web][metadata]") {
    metadata_service service(nullptr);

    auto result = service.get_sorted_instances("1.2.3.4");

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Database not configured");
}

TEST_CASE("metadata_service get_navigation without database", "[web][metadata]") {
    metadata_service service(nullptr);

    auto result = service.get_navigation("1.2.3.4");

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Database not configured");
}

TEST_CASE("metadata_service get_voi_lut without database", "[web][metadata]") {
    metadata_service service(nullptr);

    auto result = service.get_voi_lut("1.2.3.4");

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Database not configured");
}

TEST_CASE("metadata_service get_frame_info without database", "[web][metadata]") {
    metadata_service service(nullptr);

    auto result = service.get_frame_info("1.2.3.4");

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Database not configured");
}
