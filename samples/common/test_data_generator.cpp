/**
 * @file test_data_generator.cpp
 * @brief Implementation of test data generation utilities
 */

#include "test_data_generator.hpp"

#include <pacs/core/dicom_file.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/encoding/transfer_syntax.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace pacs::samples {

// ============================================================================
// Static Member Definitions
// ============================================================================

std::atomic<uint64_t> test_data_generator::uid_counter_{0};

// ============================================================================
// SOP Class UIDs
// ============================================================================

namespace sop_class {
constexpr std::string_view ct_image_storage = "1.2.840.10008.5.1.4.1.1.2";
constexpr std::string_view mr_image_storage = "1.2.840.10008.5.1.4.1.1.4";
constexpr std::string_view cr_image_storage = "1.2.840.10008.5.1.4.1.1.1";
constexpr std::string_view dx_image_storage = "1.2.840.10008.5.1.4.1.1.1.1";
}  // namespace sop_class

// ============================================================================
// UID Generation
// ============================================================================

auto test_data_generator::generate_uid(std::string_view root) -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();

    auto counter = uid_counter_.fetch_add(1, std::memory_order_relaxed);

    std::ostringstream oss;
    oss << root << "." << time << "." << counter;
    return oss.str();
}

// ============================================================================
// Date/Time Utilities
// ============================================================================

auto test_data_generator::current_date() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm.tm_year + 1900)
        << std::setw(2) << (tm.tm_mon + 1)
        << std::setw(2) << tm.tm_mday;
    return oss.str();
}

auto test_data_generator::current_time() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count() % 1000000;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(2) << tm.tm_hour
        << std::setw(2) << tm.tm_min
        << std::setw(2) << tm.tm_sec
        << "." << std::setw(6) << ms;
    return oss.str();
}

// ============================================================================
// Base Dataset Creation
// ============================================================================

auto test_data_generator::create_base_dataset(
    const patient_info& patient,
    const image_params& params) -> core::dicom_dataset {

    using namespace core::tags;
    using vr = encoding::vr_type;

    core::dicom_dataset ds;

    // Patient Module
    ds.set_string(patient_name, vr::PN, patient.patient_name);
    ds.set_string(patient_id, vr::LO, patient.patient_id);
    ds.set_string(patient_birth_date, vr::DA, patient.birth_date);
    ds.set_string(patient_sex, vr::CS, patient.sex);

    // General Study Module
    auto study_uid = generate_uid();
    ds.set_string(study_instance_uid, vr::UI, study_uid);
    ds.set_string(study_date, vr::DA, current_date());
    ds.set_string(study_time, vr::TM, current_time());
    ds.set_string(accession_number, vr::SH, "ACC" + patient.patient_id);
    ds.set_string(study_id, vr::SH, "1");
    ds.set_string(study_description, vr::LO, "Test Study");
    ds.set_string(referring_physician_name, vr::PN, "REFERRING^PHYSICIAN");

    // General Series Module
    auto series_uid = generate_uid();
    ds.set_string(series_instance_uid, vr::UI, series_uid);
    ds.set_string(modality, vr::CS, params.modality);
    ds.set_numeric<int32_t>(series_number, vr::IS, 1);
    ds.set_string(series_date, vr::DA, current_date());
    ds.set_string(series_time, vr::TM, current_time());
    ds.set_string(series_description, vr::LO, params.modality + " Series");

    // Frame of Reference Module
    ds.set_string(frame_of_reference_uid, vr::UI, generate_uid());

    // General Equipment Module
    ds.set_string(manufacturer, vr::LO, "PACS System");
    ds.set_string(station_name, vr::SH, "SAMPLE_STATION");
    ds.set_string(manufacturers_model_name, vr::LO, "Sample Generator");

    // General Image Module
    auto instance_uid = generate_uid();
    ds.set_string(sop_instance_uid, vr::UI, instance_uid);
    ds.set_numeric<int32_t>(instance_number, vr::IS, 1);
    ds.set_string(content_date, vr::DA, current_date());
    ds.set_string(content_time, vr::TM, current_time());
    ds.set_string(image_type, vr::CS, "ORIGINAL\\PRIMARY\\AXIAL");

    // Image Pixel Module
    ds.set_numeric<uint16_t>(samples_per_pixel, vr::US, 1);
    ds.set_string(photometric_interpretation, vr::CS, params.photometric);
    ds.set_numeric<uint16_t>(rows, vr::US, params.rows);
    ds.set_numeric<uint16_t>(columns, vr::US, params.columns);
    ds.set_numeric<uint16_t>(bits_allocated, vr::US, params.bits_allocated);
    ds.set_numeric<uint16_t>(bits_stored, vr::US, params.bits_stored);
    ds.set_numeric<uint16_t>(high_bit, vr::US, params.high_bit);
    ds.set_numeric<uint16_t>(pixel_representation, vr::US, params.pixel_representation);

    // VOI LUT Module
    ds.set_string(window_center, vr::DS, std::to_string(params.window_center));
    ds.set_string(window_width, vr::DS, std::to_string(params.window_width));

    return ds;
}

// ============================================================================
// Pixel Data Generation
// ============================================================================

auto test_data_generator::generate_pixel_data(
    uint16_t row_count,
    uint16_t col_count,
    uint16_t bits_alloc) -> std::vector<uint8_t> {

    size_t bytes_per_pixel = bits_alloc / 8;
    size_t total_pixels = static_cast<size_t>(row_count) * col_count;
    std::vector<uint8_t> data(total_pixels * bytes_per_pixel);

    // Generate a simple gradient pattern
    uint16_t max_value = static_cast<uint16_t>((1 << (bits_alloc > 16 ? 16 : bits_alloc)) - 1);

    for (uint16_t y = 0; y < row_count; ++y) {
        for (uint16_t x = 0; x < col_count; ++x) {
            size_t idx = (static_cast<size_t>(y) * col_count + x) * bytes_per_pixel;

            // Create a circular gradient pattern
            double cx = col_count / 2.0;
            double cy = row_count / 2.0;
            double dx = x - cx;
            double dy = y - cy;
            double dist = std::sqrt(dx * dx + dy * dy);
            double max_dist = std::sqrt(cx * cx + cy * cy);
            double normalized = 1.0 - (dist / max_dist);

            uint16_t value = static_cast<uint16_t>(normalized * max_value * 0.8);

            // Store as little-endian
            data[idx] = static_cast<uint8_t>(value & 0xFF);
            if (bytes_per_pixel > 1) {
                data[idx + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
            }
        }
    }

    return data;
}

// ============================================================================
// CT Dataset Creation
// ============================================================================

auto test_data_generator::create_ct_dataset(
    const patient_info& patient,
    const image_params& params) -> core::dicom_dataset {

    using namespace core::tags;
    using vr = encoding::vr_type;

    image_params ct_params = params;
    ct_params.modality = "CT";
    ct_params.photometric = "MONOCHROME2";
    ct_params.window_center = 40.0;
    ct_params.window_width = 400.0;

    auto ds = create_base_dataset(patient, ct_params);

    // SOP Class
    ds.set_string(sop_class_uid, vr::UI, std::string{sop_class::ct_image_storage});

    // CT-specific attributes
    ds.set_string(rescale_intercept, vr::DS, "-1024");
    ds.set_string(rescale_slope, vr::DS, "1");
    ds.set_string(rescale_type, vr::LO, "HU");
    ds.set_string(pixel_spacing, vr::DS, "0.5\\0.5");
    ds.set_string(slice_location, vr::DS, "0.0");
    ds.set_string(image_position_patient, vr::DS, "0.0\\0.0\\0.0");
    ds.set_string(image_orientation_patient, vr::DS, "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");

    // Add pixel data
    auto pixel_bytes = generate_pixel_data(ct_params.rows, ct_params.columns,
                                           ct_params.bits_allocated);
    ds.insert(core::dicom_element{pixel_data, vr::OW, pixel_bytes});

    return ds;
}

// ============================================================================
// MR Dataset Creation
// ============================================================================

auto test_data_generator::create_mr_dataset(
    const patient_info& patient,
    image_params params) -> core::dicom_dataset {

    using namespace core::tags;
    using vr = encoding::vr_type;

    params.modality = "MR";
    params.photometric = "MONOCHROME2";
    params.window_center = 500.0;
    params.window_width = 1000.0;

    auto ds = create_base_dataset(patient, params);

    // SOP Class
    ds.set_string(sop_class_uid, vr::UI, std::string{sop_class::mr_image_storage});

    // MR-specific attributes
    ds.set_string(pixel_spacing, vr::DS, "1.0\\1.0");
    ds.set_string(slice_location, vr::DS, "0.0");
    ds.set_string(image_position_patient, vr::DS, "0.0\\0.0\\0.0");
    ds.set_string(image_orientation_patient, vr::DS, "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");

    // Add pixel data
    auto pixel_bytes = generate_pixel_data(params.rows, params.columns,
                                           params.bits_allocated);
    ds.insert(core::dicom_element{pixel_data, vr::OW, pixel_bytes});

    return ds;
}

// ============================================================================
// CR Dataset Creation
// ============================================================================

auto test_data_generator::create_cr_dataset(
    const patient_info& patient,
    image_params params) -> core::dicom_dataset {

    using namespace core::tags;
    using vr = encoding::vr_type;

    params.modality = "CR";
    params.photometric = "MONOCHROME2";
    params.rows = 2048;
    params.columns = 2048;
    params.bits_allocated = 16;
    params.bits_stored = 14;
    params.high_bit = 13;
    params.window_center = 8192.0;
    params.window_width = 16384.0;

    auto ds = create_base_dataset(patient, params);

    // SOP Class
    ds.set_string(sop_class_uid, vr::UI, std::string{sop_class::cr_image_storage});

    // Add pixel data
    auto pixel_bytes = generate_pixel_data(params.rows, params.columns,
                                           params.bits_allocated);
    ds.insert(core::dicom_element{pixel_data, vr::OW, pixel_bytes});

    return ds;
}

// ============================================================================
// DX Dataset Creation
// ============================================================================

auto test_data_generator::create_dx_dataset(
    const patient_info& patient,
    image_params params) -> core::dicom_dataset {

    using namespace core::tags;
    using vr = encoding::vr_type;

    params.modality = "DX";
    params.photometric = "MONOCHROME2";
    params.rows = 3000;
    params.columns = 3000;
    params.bits_allocated = 16;
    params.bits_stored = 14;
    params.high_bit = 13;
    params.window_center = 8192.0;
    params.window_width = 16384.0;

    auto ds = create_base_dataset(patient, params);

    // SOP Class
    ds.set_string(sop_class_uid, vr::UI, std::string{sop_class::dx_image_storage});

    // DX-specific attributes
    ds.set_string(pixel_spacing, vr::DS, "0.15\\0.15");
    ds.set_string(image_orientation_patient, vr::DS, "1.0\\0.0\\0.0\\0.0\\1.0\\0.0");

    // Add pixel data
    auto pixel_bytes = generate_pixel_data(params.rows, params.columns,
                                           params.bits_allocated);
    ds.insert(core::dicom_element{pixel_data, vr::OW, pixel_bytes});

    return ds;
}

// ============================================================================
// Study Generation
// ============================================================================

auto test_data_generator::create_study(
    const patient_info& patient,
    const study_info& study,
    int series_count,
    int instances_per_series) -> std::vector<core::dicom_dataset> {

    using namespace core::tags;
    using vr = encoding::vr_type;

    std::vector<core::dicom_dataset> datasets;
    datasets.reserve(static_cast<size_t>(series_count * instances_per_series));

    auto study_uid = study.study_uid.empty() ? generate_uid() : study.study_uid;
    auto study_dt = study.study_date.empty() ? current_date() : study.study_date;
    auto study_tm = study.study_time.empty() ? current_time() : study.study_time;

    for (int series_idx = 0; series_idx < series_count; ++series_idx) {
        auto series_uid = generate_uid();
        auto frame_uid = generate_uid();

        for (int instance_idx = 0; instance_idx < instances_per_series; ++instance_idx) {
            auto ds = create_ct_dataset(patient);

            // Override with study-level info
            ds.set_string(study_instance_uid, vr::UI, study_uid);
            ds.set_string(study_date, vr::DA, study_dt);
            ds.set_string(study_time, vr::TM, study_tm);

            if (!study.accession_number.empty()) {
                ds.set_string(accession_number, vr::SH, study.accession_number);
            }
            if (!study.description.empty()) {
                ds.set_string(study_description, vr::LO, study.description);
            }
            if (!study.referring_physician.empty()) {
                ds.set_string(referring_physician_name, vr::PN, study.referring_physician);
            }

            // Series-level info
            ds.set_string(series_instance_uid, vr::UI, series_uid);
            ds.set_numeric<int32_t>(series_number, vr::IS, series_idx + 1);
            ds.set_string(frame_of_reference_uid, vr::UI, frame_uid);

            // Instance-level info
            ds.set_string(sop_instance_uid, vr::UI, generate_uid());
            ds.set_numeric<int32_t>(instance_number, vr::IS, instance_idx + 1);

            // Slice location for 3D datasets
            double slice_loc = static_cast<double>(instance_idx) * 5.0;
            ds.set_string(slice_location, vr::DS, std::to_string(slice_loc));
            ds.set_string(image_position_patient, vr::DS,
                "0.0\\0.0\\" + std::to_string(slice_loc));

            datasets.push_back(std::move(ds));
        }
    }

    return datasets;
}

// ============================================================================
// File Operations
// ============================================================================

void test_data_generator::save_to_directory(
    const std::vector<core::dicom_dataset>& datasets,
    const std::filesystem::path& directory) {

    using namespace core::tags;
    namespace fs = std::filesystem;

    fs::create_directories(directory);

    for (const auto& ds : datasets) {
        auto pat_id = ds.get_string(patient_id, "UNKNOWN");
        auto study_uid_str = ds.get_string(study_instance_uid, "UNKNOWN");
        auto series_uid_str = ds.get_string(series_instance_uid, "UNKNOWN");
        auto instance_uid_str = ds.get_string(sop_instance_uid, "UNKNOWN");

        // Create hierarchical path
        auto path = directory / pat_id / study_uid_str / series_uid_str;
        fs::create_directories(path);

        auto file_path = path / (instance_uid_str + ".dcm");

        // Create DICOM file and save
        auto file = core::dicom_file::create(
            ds, encoding::transfer_syntax::explicit_vr_little_endian);
        auto result = file.save(file_path);

        if (!result.is_ok()) {
            throw std::runtime_error("Failed to save DICOM file: " +
                                     file_path.string());
        }
    }
}

}  // namespace pacs::samples
