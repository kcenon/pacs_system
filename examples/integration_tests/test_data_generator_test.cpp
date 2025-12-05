/**
 * @file test_data_generator_test.cpp
 * @brief Unit tests for the test_data_generator class
 *
 * Validates that all DICOM test data generators produce valid datasets
 * with correct attributes and structures.
 *
 * @see Issue #137 - Test Fixtures Extension
 */

#include "test_data_generator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>  // for std::all_of

namespace pacs::integration_test {

// =============================================================================
// Single Modality Generator Tests
// =============================================================================

TEST_CASE("test_data_generator::ct generates valid CT dataset", "[data_generator][ct]") {
    auto ds = test_data_generator::ct();

    SECTION("has required patient module attributes") {
        REQUIRE(ds.contains(core::tags::patient_name));
        REQUIRE(ds.contains(core::tags::patient_id));
        CHECK(ds.get_string(core::tags::patient_name) == "TEST^CT^PATIENT");
    }

    SECTION("has required study module attributes") {
        REQUIRE(ds.contains(core::tags::study_instance_uid));
        REQUIRE(ds.contains(core::tags::study_date));
    }

    SECTION("has required series module attributes") {
        REQUIRE(ds.contains(core::tags::series_instance_uid));
        REQUIRE(ds.contains(core::tags::modality));
        CHECK(ds.get_string(core::tags::modality) == "CT");
    }

    SECTION("has required SOP common attributes") {
        REQUIRE(ds.contains(core::tags::sop_class_uid));
        REQUIRE(ds.contains(core::tags::sop_instance_uid));
        CHECK(ds.get_string(core::tags::sop_class_uid) == "1.2.840.10008.5.1.4.1.1.2");
    }

    SECTION("has pixel data") {
        REQUIRE(ds.contains(core::tags::pixel_data));
    }

    SECTION("respects provided study UID") {
        std::string custom_study_uid = "1.2.3.4.5.6.7.8.9";
        auto ds2 = test_data_generator::ct(custom_study_uid);
        CHECK(ds2.get_string(core::tags::study_instance_uid) == custom_study_uid);
    }
}

TEST_CASE("test_data_generator::mr generates valid MR dataset", "[data_generator][mr]") {
    auto ds = test_data_generator::mr();

    REQUIRE(ds.contains(core::tags::modality));
    CHECK(ds.get_string(core::tags::modality) == "MR");
    CHECK(ds.get_string(core::tags::sop_class_uid) == "1.2.840.10008.5.1.4.1.1.4");
    REQUIRE(ds.contains(core::tags::pixel_data));
}

TEST_CASE("test_data_generator::xa generates valid XA dataset", "[data_generator][xa]") {
    auto ds = test_data_generator::xa();

    REQUIRE(ds.contains(core::tags::modality));
    CHECK(ds.get_string(core::tags::modality) == "XA");

    SECTION("has XA SOP Class UID") {
        CHECK(ds.get_string(core::tags::sop_class_uid) ==
              std::string(services::sop_classes::xa_image_storage_uid));
    }

    SECTION("has XA-specific attributes") {
        constexpr core::dicom_tag positioner_primary_angle{0x0018, 0x1510};
        constexpr core::dicom_tag kvp{0x0018, 0x0060};

        REQUIRE(ds.contains(positioner_primary_angle));
        REQUIRE(ds.contains(kvp));
    }

    SECTION("has larger image dimensions than CT/MR") {
        REQUIRE(ds.contains(core::tags::rows));
        REQUIRE(ds.contains(core::tags::columns));

        auto rows = ds.get_numeric<uint16_t>(core::tags::rows);
        auto cols = ds.get_numeric<uint16_t>(core::tags::columns);

        REQUIRE(rows.has_value());
        REQUIRE(cols.has_value());
        CHECK(rows.value() == 512);
        CHECK(cols.value() == 512);
    }
}

TEST_CASE("test_data_generator::us generates valid US dataset", "[data_generator][us]") {
    auto ds = test_data_generator::us();

    REQUIRE(ds.contains(core::tags::modality));
    CHECK(ds.get_string(core::tags::modality) == "US");

    SECTION("has US SOP Class UID") {
        CHECK(ds.get_string(core::tags::sop_class_uid) ==
              std::string(services::sop_classes::us_image_storage_uid));
    }

    SECTION("has 8-bit pixel data") {
        auto bits_allocated = ds.get_numeric<uint16_t>(core::tags::bits_allocated);
        REQUIRE(bits_allocated.has_value());
        CHECK(bits_allocated.value() == 8);
    }
}

// =============================================================================
// Multi-frame Generator Tests
// =============================================================================

TEST_CASE("test_data_generator::xa_cine generates valid multi-frame XA dataset", "[data_generator][xa][multiframe]") {
    constexpr uint32_t num_frames = 15;
    auto ds = test_data_generator::xa_cine(num_frames);

    SECTION("has Number of Frames attribute") {
        constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};
        REQUIRE(ds.contains(number_of_frames));
        CHECK(ds.get_string(number_of_frames) == std::to_string(num_frames));
    }

    SECTION("has XA-specific cine attributes") {
        constexpr core::dicom_tag cine_rate{0x0018, 0x0040};
        constexpr core::dicom_tag frame_time{0x0018, 0x1063};

        REQUIRE(ds.contains(cine_rate));
        REQUIRE(ds.contains(frame_time));
    }

    SECTION("has appropriately sized pixel data") {
        auto* pixel_elem = ds.get(core::tags::pixel_data);
        REQUIRE(pixel_elem != nullptr);

        // 512x512 * 2 bytes * 15 frames = 7,864,320 bytes
        size_t expected_size = 512 * 512 * 2 * num_frames;
        CHECK(pixel_elem->length() == expected_size);
    }
}

TEST_CASE("test_data_generator::us_cine generates valid multi-frame US dataset", "[data_generator][us][multiframe]") {
    constexpr uint32_t num_frames = 30;
    auto ds = test_data_generator::us_cine(num_frames);

    SECTION("has US Multi-frame SOP Class") {
        CHECK(ds.get_string(core::tags::sop_class_uid) ==
              std::string(services::sop_classes::us_multiframe_image_storage_uid));
    }

    SECTION("has Number of Frames attribute") {
        constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};
        REQUIRE(ds.contains(number_of_frames));
        CHECK(ds.get_string(number_of_frames) == std::to_string(num_frames));
    }

    SECTION("has 8-bit pixel data with multiple frames") {
        auto* pixel_elem = ds.get(core::tags::pixel_data);
        REQUIRE(pixel_elem != nullptr);

        // 640x480 * 1 byte * 30 frames = 9,216,000 bytes
        size_t expected_size = 640 * 480 * 1 * num_frames;
        CHECK(pixel_elem->length() == expected_size);
    }
}

TEST_CASE("test_data_generator::enhanced_ct generates valid Enhanced CT dataset", "[data_generator][ct][enhanced]") {
    constexpr uint32_t num_frames = 50;
    auto ds = test_data_generator::enhanced_ct(num_frames);

    SECTION("has Enhanced CT SOP Class") {
        CHECK(ds.get_string(core::tags::sop_class_uid) == "1.2.840.10008.5.1.4.1.1.2.1");
    }

    SECTION("has Image Type attribute") {
        REQUIRE(ds.contains(core::tags::image_type));
    }

    SECTION("has Number of Frames") {
        constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};
        CHECK(ds.get_string(number_of_frames) == std::to_string(num_frames));
    }
}

TEST_CASE("test_data_generator::enhanced_mr generates valid Enhanced MR dataset", "[data_generator][mr][enhanced]") {
    constexpr uint32_t num_frames = 25;
    auto ds = test_data_generator::enhanced_mr(num_frames);

    SECTION("has Enhanced MR SOP Class") {
        CHECK(ds.get_string(core::tags::sop_class_uid) == "1.2.840.10008.5.1.4.1.1.4.1");
    }

    SECTION("has Number of Frames") {
        constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};
        CHECK(ds.get_string(number_of_frames) == std::to_string(num_frames));
    }
}

// =============================================================================
// Clinical Workflow Tests
// =============================================================================

TEST_CASE("test_data_generator::patient_journey creates multi-modal study", "[data_generator][workflow]") {
    auto study = test_data_generator::patient_journey("PATIENT001", {"CT", "MR", "XA"});

    SECTION("has consistent patient information") {
        CHECK(study.patient_id == "PATIENT001");
        CHECK_FALSE(study.study_uid.empty());

        for (const auto& ds : study.datasets) {
            CHECK(ds.get_string(core::tags::patient_id) == "PATIENT001");
            CHECK(ds.get_string(core::tags::study_instance_uid) == study.study_uid);
        }
    }

    SECTION("contains all requested modalities") {
        CHECK(study.datasets.size() == 3);

        auto ct_datasets = study.get_by_modality("CT");
        auto mr_datasets = study.get_by_modality("MR");
        auto xa_datasets = study.get_by_modality("XA");

        CHECK(ct_datasets.size() == 1);
        CHECK(mr_datasets.size() == 1);
        CHECK(xa_datasets.size() == 1);
    }

    SECTION("each modality has unique series UID") {
        CHECK(study.series_count() == 3);
    }
}

TEST_CASE("test_data_generator::worklist generates valid worklist item", "[data_generator][worklist]") {
    auto ds = test_data_generator::worklist("WL001", "MR");

    SECTION("has patient attributes") {
        REQUIRE(ds.contains(core::tags::patient_name));
        CHECK(ds.get_string(core::tags::patient_id) == "WL001");
    }

    SECTION("has scheduled procedure step attributes") {
        REQUIRE(ds.contains(core::tags::scheduled_procedure_step_start_date));
        REQUIRE(ds.contains(core::tags::scheduled_station_ae_title));
        CHECK(ds.get_string(core::tags::modality) == "MR");
    }

    SECTION("has requested procedure attributes") {
        REQUIRE(ds.contains(core::tags::requested_procedure_id));
        REQUIRE(ds.contains(core::tags::accession_number));
        REQUIRE(ds.contains(core::tags::study_instance_uid));
    }
}

// =============================================================================
// Edge Case Generator Tests
// =============================================================================

TEST_CASE("test_data_generator::large creates appropriately sized dataset", "[data_generator][edge_case]") {
    constexpr size_t target_mb = 2;
    auto ds = test_data_generator::large(target_mb);

    auto* pixel_elem = ds.get(core::tags::pixel_data);
    REQUIRE(pixel_elem != nullptr);

    // Check that pixel data size is approximately correct
    // (may not be exact due to square dimension rounding)
    size_t target_bytes = target_mb * 1024 * 1024;
    size_t actual_size = pixel_elem->length();

    // Allow for some variance due to dimension rounding
    CHECK(actual_size >= target_bytes / 2);
    CHECK(actual_size <= target_bytes * 2);
}

TEST_CASE("test_data_generator::unicode creates dataset with Unicode characters", "[data_generator][edge_case][unicode]") {
    auto ds = test_data_generator::unicode();

    SECTION("has specific character set") {
        REQUIRE(ds.contains(core::tags::specific_character_set));
    }

    SECTION("has patient name with Korean characters") {
        REQUIRE(ds.contains(core::tags::patient_name));
        // The patient name contains Korean characters
        auto patient_name = ds.get_string(core::tags::patient_name);
        CHECK_FALSE(patient_name.empty());
    }
}

TEST_CASE("test_data_generator::with_private_tags includes private tags", "[data_generator][edge_case][private]") {
    auto ds = test_data_generator::with_private_tags("MY_PRIVATE_CREATOR");

    SECTION("has private creator tag") {
        constexpr core::dicom_tag private_creator_tag{0x0011, 0x0010};
        REQUIRE(ds.contains(private_creator_tag));
        CHECK(ds.get_string(private_creator_tag) == "MY_PRIVATE_CREATOR");
    }

    SECTION("has private data tags") {
        constexpr core::dicom_tag private_data_1{0x0011, 0x1001};
        constexpr core::dicom_tag private_data_2{0x0011, 0x1002};

        REQUIRE(ds.contains(private_data_1));
        REQUIRE(ds.contains(private_data_2));
    }
}

TEST_CASE("test_data_generator::invalid creates datasets with specific errors", "[data_generator][edge_case][invalid]") {
    SECTION("missing_sop_class_uid") {
        auto ds = test_data_generator::invalid(invalid_dataset_type::missing_sop_class_uid);
        CHECK_FALSE(ds.contains(core::tags::sop_class_uid));
    }

    SECTION("missing_sop_instance_uid") {
        auto ds = test_data_generator::invalid(invalid_dataset_type::missing_sop_instance_uid);
        CHECK_FALSE(ds.contains(core::tags::sop_instance_uid));
    }

    SECTION("missing_patient_id") {
        auto ds = test_data_generator::invalid(invalid_dataset_type::missing_patient_id);
        CHECK_FALSE(ds.contains(core::tags::patient_id));
    }

    SECTION("missing_study_instance_uid") {
        auto ds = test_data_generator::invalid(invalid_dataset_type::missing_study_instance_uid);
        CHECK_FALSE(ds.contains(core::tags::study_instance_uid));
    }

    SECTION("corrupted_pixel_data") {
        auto ds = test_data_generator::invalid(invalid_dataset_type::corrupted_pixel_data);
        auto* pixel_elem = ds.get(core::tags::pixel_data);
        REQUIRE(pixel_elem != nullptr);
        // Pixel data should be much smaller than expected
        CHECK(pixel_elem->length() < 1000);
    }
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST_CASE("test_data_generator::generate_uid creates unique UIDs", "[data_generator][utility]") {
    std::string uid1 = test_data_generator::generate_uid();
    std::string uid2 = test_data_generator::generate_uid();
    std::string uid3 = test_data_generator::generate_uid();

    CHECK(uid1 != uid2);
    CHECK(uid2 != uid3);
    CHECK(uid1 != uid3);

    // All should start with the default root
    CHECK(uid1.find("1.2.826.0.1.3680043.9.9999") == 0);
}

TEST_CASE("test_data_generator::current_date returns valid DICOM date", "[data_generator][utility]") {
    std::string date = test_data_generator::current_date();

    // DICOM DA format: YYYYMMDD (8 characters)
    CHECK(date.length() == 8);

    // Should be all digits
    CHECK(std::all_of(date.begin(), date.end(), ::isdigit));
}

TEST_CASE("test_data_generator::current_time returns valid DICOM time", "[data_generator][utility]") {
    std::string time = test_data_generator::current_time();

    // DICOM TM format: HHMMSS (6 characters minimum)
    CHECK(time.length() >= 6);

    // Should be all digits
    CHECK(std::all_of(time.begin(), time.end(), ::isdigit));
}

}  // namespace pacs::integration_test
