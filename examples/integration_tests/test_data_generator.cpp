/**
 * @file test_data_generator.cpp
 * @brief Implementation of DICOM test data generators
 *
 * @see Issue #137 - Test Fixtures Extension
 */

#include "test_data_generator.hpp"

#include "pacs/core/dicom_element.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>

namespace pacs::integration_test {

// =============================================================================
// multi_modal_study Implementation
// =============================================================================

std::vector<core::dicom_dataset>
multi_modal_study::get_by_modality(const std::string& modality) const {
    std::vector<core::dicom_dataset> result;
    for (const auto& ds : datasets) {
        if (ds.get_string(core::tags::modality) == modality) {
            result.push_back(ds);
        }
    }
    return result;
}

size_t multi_modal_study::series_count() const {
    std::set<std::string> series_uids;
    for (const auto& ds : datasets) {
        auto uid = ds.get_string(core::tags::series_instance_uid);
        if (!uid.empty()) {
            series_uids.insert(uid);
        }
    }
    return series_uids.size();
}

// =============================================================================
// Utility Functions Implementation
// =============================================================================

std::string test_data_generator::generate_uid(const std::string& root) {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    return root + "." + std::to_string(timestamp) + "." + std::to_string(++counter);
}

std::string test_data_generator::current_date() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y%m%d");
    return oss.str();
}

std::string test_data_generator::current_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
#ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%H%M%S");
    return oss.str();
}

// =============================================================================
// Private Helper Functions
// =============================================================================

void test_data_generator::add_patient_module(
    core::dicom_dataset& ds,
    const std::string& patient_name,
    const std::string& patient_id,
    const std::string& birth_date,
    const std::string& sex) {

    ds.set_string(core::tags::patient_name, encoding::vr_type::PN, patient_name);
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, patient_id);
    ds.set_string(core::tags::patient_birth_date, encoding::vr_type::DA, birth_date);
    ds.set_string(core::tags::patient_sex, encoding::vr_type::CS, sex);
}

void test_data_generator::add_study_module(
    core::dicom_dataset& ds,
    const std::string& study_uid,
    const std::string& accession_number,
    const std::string& study_id,
    const std::string& description) {

    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI,
                  study_uid.empty() ? generate_uid() : study_uid);
    ds.set_string(core::tags::study_date, encoding::vr_type::DA, current_date());
    ds.set_string(core::tags::study_time, encoding::vr_type::TM, current_time());
    ds.set_string(core::tags::accession_number, encoding::vr_type::SH, accession_number);
    ds.set_string(core::tags::study_id, encoding::vr_type::SH, study_id);
    ds.set_string(core::tags::study_description, encoding::vr_type::LO, description);
}

void test_data_generator::add_series_module(
    core::dicom_dataset& ds,
    const std::string& series_uid,
    const std::string& modality,
    const std::string& series_number,
    const std::string& description) {

    ds.set_string(core::tags::series_instance_uid, encoding::vr_type::UI,
                  series_uid.empty() ? generate_uid() : series_uid);
    ds.set_string(core::tags::modality, encoding::vr_type::CS, modality);
    ds.set_string(core::tags::series_number, encoding::vr_type::IS, series_number);
    ds.set_string(core::tags::series_description, encoding::vr_type::LO, description);
}

void test_data_generator::add_image_pixel_module(
    core::dicom_dataset& ds,
    uint16_t rows,
    uint16_t columns,
    uint16_t bits_allocated,
    uint16_t bits_stored,
    uint16_t samples_per_pixel,
    const std::string& photometric) {

    ds.set_numeric<uint16_t>(core::tags::rows, encoding::vr_type::US, rows);
    ds.set_numeric<uint16_t>(core::tags::columns, encoding::vr_type::US, columns);
    ds.set_numeric<uint16_t>(core::tags::bits_allocated, encoding::vr_type::US, bits_allocated);
    ds.set_numeric<uint16_t>(core::tags::bits_stored, encoding::vr_type::US, bits_stored);
    ds.set_numeric<uint16_t>(core::tags::high_bit, encoding::vr_type::US,
                             static_cast<uint16_t>(bits_stored - 1));
    ds.set_numeric<uint16_t>(core::tags::pixel_representation, encoding::vr_type::US, 0);
    ds.set_numeric<uint16_t>(core::tags::samples_per_pixel, encoding::vr_type::US, samples_per_pixel);
    ds.set_string(core::tags::photometric_interpretation, encoding::vr_type::CS, photometric);
}

void test_data_generator::add_pixel_data(
    core::dicom_dataset& ds,
    uint16_t rows,
    uint16_t columns,
    uint16_t bits_allocated,
    uint16_t samples_per_pixel,
    uint16_t fill_value) {

    size_t pixel_count = static_cast<size_t>(rows) * columns * samples_per_pixel;

    if (bits_allocated == 8) {
        std::vector<uint8_t> pixel_data(pixel_count, static_cast<uint8_t>(fill_value & 0xFF));
        core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OB);
        pixel_elem.set_value(std::span<const uint8_t>(pixel_data.data(), pixel_data.size()));
        ds.insert(std::move(pixel_elem));
    } else {
        std::vector<uint16_t> pixel_data(pixel_count, fill_value);
        core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OW);
        pixel_elem.set_value(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(pixel_data.data()),
            pixel_data.size() * sizeof(uint16_t)));
        ds.insert(std::move(pixel_elem));
    }
}

void test_data_generator::add_multiframe_pixel_data(
    core::dicom_dataset& ds,
    uint16_t rows,
    uint16_t columns,
    uint16_t bits_allocated,
    uint32_t num_frames,
    uint16_t samples_per_pixel) {

    // Add Number of Frames attribute
    constexpr core::dicom_tag number_of_frames{0x0028, 0x0008};
    ds.set_string(number_of_frames, encoding::vr_type::IS, std::to_string(num_frames));

    size_t frame_size = static_cast<size_t>(rows) * columns * samples_per_pixel;
    size_t total_pixels = frame_size * num_frames;

    // Use random generator for more realistic data
    std::mt19937 gen(42);  // Fixed seed for reproducibility

    if (bits_allocated == 8) {
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        std::vector<uint8_t> pixel_data(total_pixels);
        for (size_t i = 0; i < total_pixels; ++i) {
            pixel_data[i] = static_cast<uint8_t>(dist(gen));
        }
        core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OB);
        pixel_elem.set_value(std::span<const uint8_t>(pixel_data.data(), pixel_data.size()));
        ds.insert(std::move(pixel_elem));
    } else {
        std::uniform_int_distribution<uint16_t> dist(0, 4095);
        std::vector<uint16_t> pixel_data(total_pixels);
        for (size_t i = 0; i < total_pixels; ++i) {
            pixel_data[i] = dist(gen);
        }
        core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OW);
        pixel_elem.set_value(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(pixel_data.data()),
            pixel_data.size() * sizeof(uint16_t)));
        ds.insert(std::move(pixel_elem));
    }
}

// =============================================================================
// Single Modality Generators
// =============================================================================

core::dicom_dataset test_data_generator::ct(const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^CT^PATIENT", "TESTCT001", "19800101", "M");
    add_study_module(ds, study_uid, "ACCCT001", "STUDYCT001", "CT Integration Test");
    add_series_module(ds, "", "CT", "1", "CT Test Series");

    // SOP Common
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.2");  // CT Image Storage
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module
    add_image_pixel_module(ds, 64, 64, 16, 12);
    add_pixel_data(ds, 64, 64, 16, 1, 512);

    return ds;
}

core::dicom_dataset test_data_generator::mr(const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^MR^PATIENT", "TESTMR001", "19900215", "F");
    add_study_module(ds, study_uid, "ACCMR001", "STUDYMR001", "MR Integration Test");
    add_series_module(ds, "", "MR", "1", "T1 FLAIR");

    // SOP Common
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.4");  // MR Image Storage
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module
    add_image_pixel_module(ds, 64, 64, 16, 12);
    add_pixel_data(ds, 64, 64, 16, 1, 256);

    return ds;
}

core::dicom_dataset test_data_generator::xa(const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^XA^PATIENT", "TESTXA001", "19750610", "F");
    add_study_module(ds, study_uid, "ACCXA001", "STUDYXA001", "XA Integration Test");
    add_series_module(ds, "", "XA", "1", "Coronary Angio");

    // SOP Common
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  std::string(services::sop_classes::xa_image_storage_uid));
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module (512x512 for XA)
    add_image_pixel_module(ds, 512, 512, 16, 12);

    // XA-specific attributes
    constexpr core::dicom_tag positioner_primary_angle{0x0018, 0x1510};
    constexpr core::dicom_tag positioner_secondary_angle{0x0018, 0x1511};
    constexpr core::dicom_tag kvp{0x0018, 0x0060};
    constexpr core::dicom_tag xray_tube_current{0x0018, 0x1151};
    constexpr core::dicom_tag exposure_time{0x0018, 0x1150};

    ds.set_string(positioner_primary_angle, encoding::vr_type::DS, "0");
    ds.set_string(positioner_secondary_angle, encoding::vr_type::DS, "0");
    ds.set_string(kvp, encoding::vr_type::DS, "80");
    ds.set_string(xray_tube_current, encoding::vr_type::IS, "500");
    ds.set_string(exposure_time, encoding::vr_type::IS, "100");

    add_pixel_data(ds, 512, 512, 16, 1, 128);

    return ds;
}

core::dicom_dataset test_data_generator::us(const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^US^PATIENT", "TESTUS001", "19850305", "M");
    add_study_module(ds, study_uid, "ACCUS001", "STUDYUS001", "US Integration Test");
    add_series_module(ds, "", "US", "1", "Cardiac Echo");

    // SOP Common
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  std::string(services::sop_classes::us_image_storage_uid));
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module (640x480 typical for US)
    add_image_pixel_module(ds, 480, 640, 8, 8);
    add_pixel_data(ds, 480, 640, 8, 1, 64);

    return ds;
}

// =============================================================================
// Multi-frame Generators
// =============================================================================

core::dicom_dataset test_data_generator::xa_cine(uint32_t frames, const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^XACINE^PATIENT", "TESTXACINE001", "19700815", "M");
    add_study_module(ds, study_uid, "ACCXACINE001", "STUDYXACINE001", "XA Cine Test");
    add_series_module(ds, "", "XA", "1", "Coronary Cine Run");

    // SOP Common - use standard XA (supports multiframe)
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  std::string(services::sop_classes::xa_image_storage_uid));
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module (512x512 for XA)
    add_image_pixel_module(ds, 512, 512, 16, 12);

    // XA-specific attributes
    constexpr core::dicom_tag positioner_primary_angle{0x0018, 0x1510};
    constexpr core::dicom_tag positioner_secondary_angle{0x0018, 0x1511};
    constexpr core::dicom_tag cine_rate{0x0018, 0x0040};
    constexpr core::dicom_tag frame_time{0x0018, 0x1063};

    ds.set_string(positioner_primary_angle, encoding::vr_type::DS, "30");
    ds.set_string(positioner_secondary_angle, encoding::vr_type::DS, "-15");
    ds.set_string(cine_rate, encoding::vr_type::IS, "15");  // 15 fps
    ds.set_string(frame_time, encoding::vr_type::DS, "66.67");  // ~15 fps

    add_multiframe_pixel_data(ds, 512, 512, 16, frames);

    return ds;
}

core::dicom_dataset test_data_generator::us_cine(uint32_t frames, const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^USCINE^PATIENT", "TESTUSCINE001", "19880422", "F");
    add_study_module(ds, study_uid, "ACCUSCINE001", "STUDYUSCINE001", "US Cine Test");
    add_series_module(ds, "", "US", "1", "Cardiac Cine Loop");

    // SOP Common - US Multi-frame Image Storage
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  std::string(services::sop_classes::us_multiframe_image_storage_uid));
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module (640x480 typical for US)
    add_image_pixel_module(ds, 480, 640, 8, 8);

    // US-specific attributes
    constexpr core::dicom_tag frame_time{0x0018, 0x1063};
    ds.set_string(frame_time, encoding::vr_type::DS, "33.33");  // ~30 fps

    add_multiframe_pixel_data(ds, 480, 640, 8, frames);

    return ds;
}

core::dicom_dataset test_data_generator::enhanced_ct(uint32_t frames, const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^ENHCT^PATIENT", "TESTENHCT001", "19650110", "M");
    add_study_module(ds, study_uid, "ACCENHCT001", "STUDYENHCT001", "Enhanced CT Test");
    add_series_module(ds, "", "CT", "1", "Enhanced CT Volume");

    // SOP Common - Enhanced CT Image Storage
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.2.1");  // Enhanced CT
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module (use smaller size for enhanced multi-frame)
    add_image_pixel_module(ds, 128, 128, 16, 12);

    // Enhanced-specific: Image Type
    ds.set_string(core::tags::image_type, encoding::vr_type::CS,
                  "ORIGINAL\\PRIMARY\\VOLUME\\NONE");

    add_multiframe_pixel_data(ds, 128, 128, 16, frames);

    return ds;
}

core::dicom_dataset test_data_generator::enhanced_mr(uint32_t frames, const std::string& study_uid) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^ENHMR^PATIENT", "TESTENHMR001", "19720520", "F");
    add_study_module(ds, study_uid, "ACCENHMR001", "STUDYENHMR001", "Enhanced MR Test");
    add_series_module(ds, "", "MR", "1", "Enhanced MR Volume");

    // SOP Common - Enhanced MR Image Storage
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.4.1");  // Enhanced MR
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Image Pixel Module (use smaller size for enhanced multi-frame)
    add_image_pixel_module(ds, 128, 128, 16, 12);

    // Enhanced-specific: Image Type
    ds.set_string(core::tags::image_type, encoding::vr_type::CS,
                  "ORIGINAL\\PRIMARY\\VOLUME\\NONE");

    add_multiframe_pixel_data(ds, 128, 128, 16, frames);

    return ds;
}

// =============================================================================
// Clinical Workflow Generators
// =============================================================================

multi_modal_study test_data_generator::patient_journey(
    const std::string& patient_id,
    const std::vector<std::string>& modalities) {

    multi_modal_study study;
    study.patient_id = patient_id.empty() ? "PATJOURNEY001" : patient_id;
    study.patient_name = "TEST^MULTIMODAL^PATIENT";
    study.study_uid = generate_uid();

    for (const auto& modality : modalities) {
        core::dicom_dataset ds;

        add_patient_module(ds, study.patient_name, study.patient_id, "19750101", "M");
        add_study_module(ds, study.study_uid, "ACCMULTI001", "STUDYMULTI001",
                         "Multi-Modal Patient Journey");

        std::string series_desc = modality + " Series";
        add_series_module(ds, "", modality, std::to_string(study.datasets.size() + 1), series_desc);

        // Set SOP Class based on modality
        std::string sop_class_uid;
        uint16_t rows = 64, columns = 64, bits = 16;

        if (modality == "CT") {
            sop_class_uid = "1.2.840.10008.5.1.4.1.1.2";
        } else if (modality == "MR") {
            sop_class_uid = "1.2.840.10008.5.1.4.1.1.4";
        } else if (modality == "XA") {
            sop_class_uid = std::string(services::sop_classes::xa_image_storage_uid);
            rows = 512;
            columns = 512;
        } else if (modality == "US") {
            sop_class_uid = std::string(services::sop_classes::us_image_storage_uid);
            rows = 480;
            columns = 640;
            bits = 8;
        } else {
            // Default to Secondary Capture for unknown modalities
            sop_class_uid = "1.2.840.10008.5.1.4.1.1.7";
        }

        ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, sop_class_uid);
        ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

        add_image_pixel_module(ds, rows, columns, bits, static_cast<uint16_t>(bits - 4));
        add_pixel_data(ds, rows, columns, bits);

        study.datasets.push_back(std::move(ds));
    }

    return study;
}

core::dicom_dataset test_data_generator::worklist(
    const std::string& patient_id,
    const std::string& modality) {

    core::dicom_dataset ds;

    std::string pid = patient_id.empty() ? "TESTWL001" : patient_id;
    add_patient_module(ds, "WORKLIST^TEST^PATIENT", pid, "19850520", "M");

    // Scheduled Procedure Step
    ds.set_string(core::tags::scheduled_procedure_step_start_date, encoding::vr_type::DA,
                  current_date());
    ds.set_string(core::tags::scheduled_procedure_step_start_time, encoding::vr_type::TM,
                  "090000");
    ds.set_string(core::tags::modality, encoding::vr_type::CS, modality);
    ds.set_string(core::tags::scheduled_station_ae_title, encoding::vr_type::AE,
                  modality + "_SCANNER");
    ds.set_string(core::tags::scheduled_procedure_step_description, encoding::vr_type::LO,
                  modality + " Examination");

    // Requested Procedure
    ds.set_string(core::tags::requested_procedure_id, encoding::vr_type::SH, "RP001");
    ds.set_string(core::tags::accession_number, encoding::vr_type::SH, "WLACC001");
    ds.set_string(core::tags::study_instance_uid, encoding::vr_type::UI, generate_uid());

    return ds;
}

// =============================================================================
// Edge Case Generators
// =============================================================================

core::dicom_dataset test_data_generator::large(size_t target_size_mb) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^LARGE^DATASET", "TESTLARGE001", "19700101", "M");
    add_study_module(ds, "", "ACCLARGE001", "STUDYLARGE001", "Large Dataset Test");
    add_series_module(ds, "", "OT", "1", "Large Test Series");

    // SOP Common - Secondary Capture
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.7");
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Calculate dimensions to achieve target size
    // target_size_mb * 1024 * 1024 = rows * columns * 2 (16-bit)
    size_t total_bytes = target_size_mb * 1024 * 1024;
    size_t total_pixels = total_bytes / 2;

    // Use square dimensions
    uint16_t dimension = static_cast<uint16_t>(std::sqrt(static_cast<double>(total_pixels)));
    dimension = std::min(dimension, static_cast<uint16_t>(4096));  // Cap at 4096x4096

    add_image_pixel_module(ds, dimension, dimension, 16, 12);
    add_pixel_data(ds, dimension, dimension, 16, 1, 1024);

    return ds;
}

core::dicom_dataset test_data_generator::unicode() {
    core::dicom_dataset ds;

    // Set specific character set for Unicode
    ds.set_string(core::tags::specific_character_set, encoding::vr_type::CS, "ISO 2022 IR 149");

    // Unicode patient names (Korean, Japanese, Chinese examples)
    add_patient_module(ds, "홍^길동", "TESTUNICODE001", "19801225", "M");
    add_study_module(ds, "", "ACCUNICODE001", "STUDYUNICODE001",
                     "Unicode Test Study - 유니코드 테스트");
    add_series_module(ds, "", "OT", "1", "Unicode Series - 한글 시리즈");

    // SOP Common - Secondary Capture
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.7");
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    add_image_pixel_module(ds, 64, 64, 8, 8);
    add_pixel_data(ds, 64, 64, 8);

    return ds;
}

core::dicom_dataset test_data_generator::with_private_tags(const std::string& creator_id) {
    core::dicom_dataset ds;

    add_patient_module(ds, "TEST^PRIVATE^TAGS", "TESTPRIVATE001", "19851231", "F");
    add_study_module(ds, "", "ACCPRIVATE001", "STUDYPRIVATE001", "Private Tags Test");
    add_series_module(ds, "", "OT", "1", "Private Tags Series");

    // SOP Common - Secondary Capture
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.7");
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    // Add private creator identification
    constexpr core::dicom_tag private_creator_tag{0x0011, 0x0010};
    ds.set_string(private_creator_tag, encoding::vr_type::LO, creator_id);

    // Add private data elements
    constexpr core::dicom_tag private_data_tag_1{0x0011, 0x1001};
    constexpr core::dicom_tag private_data_tag_2{0x0011, 0x1002};
    constexpr core::dicom_tag private_data_tag_3{0x0011, 0x1003};

    ds.set_string(private_data_tag_1, encoding::vr_type::LO, "Private String Value");
    ds.set_string(private_data_tag_2, encoding::vr_type::DS, "123.456");
    ds.set_string(private_data_tag_3, encoding::vr_type::IS, "42");

    add_image_pixel_module(ds, 64, 64, 8, 8);
    add_pixel_data(ds, 64, 64, 8);

    return ds;
}

core::dicom_dataset test_data_generator::invalid(invalid_dataset_type type) {
    core::dicom_dataset ds;

    // Start with a valid base dataset
    add_patient_module(ds, "TEST^INVALID^DATASET", "TESTINVALID001", "19901010", "M");
    add_study_module(ds, "", "ACCINVALID001", "STUDYINVALID001", "Invalid Dataset Test");
    add_series_module(ds, "", "OT", "1", "Invalid Series");

    // Set SOP Common (will be modified based on type)
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.7");
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, generate_uid());

    add_image_pixel_module(ds, 64, 64, 8, 8);
    add_pixel_data(ds, 64, 64, 8);

    // Now introduce the specific invalidity
    switch (type) {
        case invalid_dataset_type::missing_sop_class_uid:
            ds.remove(core::tags::sop_class_uid);
            break;

        case invalid_dataset_type::missing_sop_instance_uid:
            ds.remove(core::tags::sop_instance_uid);
            break;

        case invalid_dataset_type::missing_patient_id:
            ds.remove(core::tags::patient_id);
            break;

        case invalid_dataset_type::missing_study_instance_uid:
            ds.remove(core::tags::study_instance_uid);
            break;

        case invalid_dataset_type::invalid_vr:
            // Set a numeric value where UI is expected
            ds.set_numeric<uint16_t>(core::tags::sop_class_uid, encoding::vr_type::US, 12345);
            break;

        case invalid_dataset_type::corrupted_pixel_data: {
            // Remove proper pixel data and add truncated data
            ds.remove(core::tags::pixel_data);
            std::vector<uint8_t> corrupted(100, 0xFF);  // Way too small
            core::dicom_element pixel_elem(core::tags::pixel_data, encoding::vr_type::OB);
            pixel_elem.set_value(std::span<const uint8_t>(corrupted.data(), corrupted.size()));
            ds.insert(std::move(pixel_elem));
            break;
        }

        case invalid_dataset_type::oversized_value:
            // SH VR has max 16 characters - exceed it
            ds.set_string(core::tags::accession_number, encoding::vr_type::SH,
                          "THIS_ACCESSION_NUMBER_IS_WAY_TOO_LONG_FOR_SH_VR");
            break;
    }

    return ds;
}

}  // namespace pacs::integration_test
