/**
 * @file metadata_service.cpp
 * @brief Selective metadata retrieval and navigation service implementation
 *
 * @see Issue #544 - Implement Selective Metadata & Navigation APIs
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include "pacs/web/metadata_service.hpp"

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_element.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/storage/index_database.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace pacs::web {

// =============================================================================
// Preset and Sort Order String Conversion
// =============================================================================

std::string_view preset_to_string(metadata_preset preset) {
    switch (preset) {
        case metadata_preset::image_display:
            return "image_display";
        case metadata_preset::window_level:
            return "window_level";
        case metadata_preset::patient_info:
            return "patient_info";
        case metadata_preset::acquisition:
            return "acquisition";
        case metadata_preset::positioning:
            return "positioning";
        case metadata_preset::multiframe:
            return "multiframe";
    }
    return "unknown";
}

std::optional<metadata_preset> preset_from_string(std::string_view str) {
    if (str == "image_display") {
        return metadata_preset::image_display;
    }
    if (str == "window_level") {
        return metadata_preset::window_level;
    }
    if (str == "patient_info") {
        return metadata_preset::patient_info;
    }
    if (str == "acquisition") {
        return metadata_preset::acquisition;
    }
    if (str == "positioning") {
        return metadata_preset::positioning;
    }
    if (str == "multiframe") {
        return metadata_preset::multiframe;
    }
    return std::nullopt;
}

std::string_view sort_order_to_string(sort_order order) {
    switch (order) {
        case sort_order::position:
            return "position";
        case sort_order::instance_number:
            return "instance_number";
        case sort_order::acquisition_time:
            return "acquisition_time";
    }
    return "unknown";
}

std::optional<sort_order> sort_order_from_string(std::string_view str) {
    if (str == "position") {
        return sort_order::position;
    }
    if (str == "instance_number") {
        return sort_order::instance_number;
    }
    if (str == "acquisition_time") {
        return sort_order::acquisition_time;
    }
    return std::nullopt;
}

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/**
 * @brief Convert DICOM tag to hex string (without parentheses)
 */
std::string tag_to_hex(pacs::core::dicom_tag tag) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(4)
        << tag.group() << std::setw(4) << tag.element();
    return oss.str();
}

/**
 * @brief Parse hex string to DICOM tag
 */
std::optional<pacs::core::dicom_tag> hex_to_tag(std::string_view hex) {
    if (hex.size() != 8) {
        return std::nullopt;
    }

    try {
        uint16_t group =
            static_cast<uint16_t>(std::stoul(std::string(hex.substr(0, 4)), nullptr, 16));
        uint16_t element =
            static_cast<uint16_t>(std::stoul(std::string(hex.substr(4, 4)), nullptr, 16));
        return pacs::core::dicom_tag{group, element};
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * @brief Parse multi-valued numeric string
 */
std::vector<double> parse_numeric_list(std::string_view str) {
    std::vector<double> result;
    std::string s(str);
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, '\\')) {
        try {
            result.push_back(std::stod(token));
        } catch (...) {
            // Skip invalid values
        }
    }
    return result;
}

/**
 * @brief Parse multi-valued string
 */
std::vector<std::string> parse_string_list(std::string_view str) {
    std::vector<std::string> result;
    std::string s(str);
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, '\\')) {
        result.push_back(token);
    }
    return result;
}

}  // namespace

// =============================================================================
// Construction / Destruction
// =============================================================================

metadata_service::metadata_service(
    std::shared_ptr<storage::index_database> database)
    : database_(std::move(database)) {}

metadata_service::~metadata_service() = default;

// =============================================================================
// Preset Tag Definitions
// =============================================================================

std::unordered_set<std::string> metadata_service::get_preset_tags(
    metadata_preset preset) {
    using namespace pacs::core::tags;

    std::unordered_set<std::string> tags;

    switch (preset) {
        case metadata_preset::image_display:
            // Rows, Columns, BitsAllocated, BitsStored, HighBit,
            // PixelRepresentation, PhotometricInterpretation, SamplesPerPixel
            tags.insert(tag_to_hex(rows));
            tags.insert(tag_to_hex(columns));
            tags.insert(tag_to_hex(bits_allocated));
            tags.insert(tag_to_hex(bits_stored));
            tags.insert(tag_to_hex(high_bit));
            tags.insert(tag_to_hex(pixel_representation));
            tags.insert(tag_to_hex(photometric_interpretation));
            tags.insert(tag_to_hex(samples_per_pixel));
            break;

        case metadata_preset::window_level:
            // WindowCenter, WindowWidth, RescaleSlope, RescaleIntercept
            tags.insert(tag_to_hex(window_center));
            tags.insert(tag_to_hex(window_width));
            tags.insert(tag_to_hex(rescale_slope));
            tags.insert(tag_to_hex(rescale_intercept));
            // Window Explanation (0028,1055)
            tags.insert("00281055");
            // VOI LUT Sequence (0028,3010)
            tags.insert("00283010");
            break;

        case metadata_preset::patient_info:
            // PatientName, PatientID, PatientBirthDate, PatientSex, PatientAge
            tags.insert(tag_to_hex(patient_name));
            tags.insert(tag_to_hex(patient_id));
            tags.insert(tag_to_hex(patient_birth_date));
            tags.insert(tag_to_hex(patient_sex));
            tags.insert(tag_to_hex(patient_age));
            break;

        case metadata_preset::acquisition:
            // KVP (0018,0060), ExposureTime (0018,1150), XRayTubeCurrent
            // (0018,1151) SliceThickness (0018,0050), SpacingBetweenSlices
            // (0018,0088)
            tags.insert("00180060");  // KVP
            tags.insert("00181150");  // ExposureTime
            tags.insert("00181151");  // XRayTubeCurrent
            tags.insert("00180050");  // SliceThickness
            tags.insert("00180088");  // SpacingBetweenSlices
            break;

        case metadata_preset::positioning:
            // ImagePositionPatient, ImageOrientationPatient, SliceLocation,
            // PixelSpacing
            tags.insert(tag_to_hex(image_position_patient));
            tags.insert(tag_to_hex(image_orientation_patient));
            tags.insert(tag_to_hex(slice_location));
            tags.insert(tag_to_hex(pixel_spacing));
            break;

        case metadata_preset::multiframe:
            // NumberOfFrames (0028,0008), FrameIncrementPointer (0028,0009),
            // FrameTime (0018,1063)
            tags.insert("00280008");  // NumberOfFrames
            tags.insert("00280009");  // FrameIncrementPointer
            tags.insert("00181063");  // FrameTime
            break;
    }

    return tags;
}

// =============================================================================
// Selective Metadata Retrieval
// =============================================================================

metadata_response metadata_service::get_metadata(
    std::string_view sop_instance_uid, const metadata_request& request) {
    if (database_ == nullptr) {
        return metadata_response::error("Database not configured");
    }

    // Find instance
    auto instance = database_->find_instance(sop_instance_uid);
    if (!instance) {
        return metadata_response::error("Instance not found");
    }

    // Check file exists
    if (!std::filesystem::exists(instance->file_path)) {
        return metadata_response::error("DICOM file not found");
    }

    // Build set of requested tags
    std::unordered_set<std::string> requested_tags;

    // Add preset tags if specified
    if (request.preset.has_value()) {
        auto preset_tags = get_preset_tags(request.preset.value());
        requested_tags.insert(preset_tags.begin(), preset_tags.end());
    }

    // Add explicitly requested tags
    for (const auto& tag : request.tags) {
        requested_tags.insert(tag);
    }

    // If no tags specified, return error
    if (requested_tags.empty()) {
        return metadata_response::error(
            "No tags specified: provide 'tags' or 'preset' parameter");
    }

    // Read and filter DICOM tags
    auto tag_values =
        read_dicom_tags(instance->file_path, requested_tags, request.include_private);

    return metadata_response::ok(std::move(tag_values));
}

std::unordered_map<std::string, std::string> metadata_service::read_dicom_tags(
    std::string_view file_path,
    const std::unordered_set<std::string>& requested_tags,
    bool include_private) {
    std::unordered_map<std::string, std::string> result;

    // Open DICOM file
    auto file_result = pacs::core::dicom_file::open(std::filesystem::path(file_path));
    if (file_result.is_err()) {
        return result;  // Return empty map on failure
    }

    const auto& dataset = file_result.value().dataset();

    // Convert requested tags to dicom_tag and extract values
    for (const auto& tag_hex : requested_tags) {
        auto tag_opt = hex_to_tag(tag_hex);
        if (!tag_opt.has_value()) {
            continue;
        }

        auto tag = tag_opt.value();

        // Skip private tags if not requested
        if (tag.is_private() && !include_private) {
            continue;
        }

        // Get element value
        const auto* elem = dataset.get(tag);
        if (elem != nullptr) {
            auto str_result = elem->as_string();
            if (str_result.is_ok()) {
                result[tag_hex] = str_result.value();
            }
        }
    }

    return result;
}

// =============================================================================
// Series Navigation
// =============================================================================

std::optional<std::string> metadata_service::get_series_uid(
    std::string_view sop_instance_uid) {
    if (database_ == nullptr) {
        return std::nullopt;
    }

    auto instance = database_->find_instance(sop_instance_uid);
    if (!instance) {
        return std::nullopt;
    }

    // Get series from series_pk
    auto series = database_->find_series_by_pk(instance->series_pk);
    if (!series) {
        return std::nullopt;
    }

    return series->series_uid;
}

sorted_instances_response metadata_service::get_sorted_instances(
    std::string_view series_uid, sort_order order, bool ascending) {
    if (database_ == nullptr) {
        return sorted_instances_response::error("Database not configured");
    }

    // Get all instances in the series
    auto instances_result = database_->list_instances(series_uid);
    if (instances_result.is_err()) {
        return sorted_instances_response::error("Failed to list instances");
    }

    auto& instances = instances_result.value();
    if (instances.empty()) {
        return sorted_instances_response::error("Series not found or empty");
    }

    // Build sorted_instance list with additional DICOM metadata
    std::vector<sorted_instance> sorted;
    sorted.reserve(instances.size());

    for (const auto& inst : instances) {
        sorted_instance si;
        si.sop_instance_uid = inst.sop_uid;
        si.instance_number = inst.instance_number;

        // Read additional positioning data from DICOM file if exists
        if (std::filesystem::exists(inst.file_path)) {
            auto file_result =
                pacs::core::dicom_file::open(std::filesystem::path(inst.file_path));
            if (file_result.is_ok()) {
                const auto& ds = file_result.value().dataset();

                // Slice location
                auto slice_str = ds.get_string(pacs::core::tags::slice_location);
                if (!slice_str.empty()) {
                    try {
                        si.slice_location = std::stod(slice_str);
                    } catch (...) {
                    }
                }

                // Image position patient
                auto pos_str =
                    ds.get_string(pacs::core::tags::image_position_patient);
                if (!pos_str.empty()) {
                    si.image_position_patient = parse_numeric_list(pos_str);
                }

                // Acquisition time
                auto time_str = ds.get_string(pacs::core::tags::acquisition_time);
                if (!time_str.empty()) {
                    si.acquisition_time = time_str;
                }
            }
        }

        sorted.push_back(std::move(si));
    }

    // Sort based on order
    auto compare = [order, ascending](const sorted_instance& a,
                                       const sorted_instance& b) {
        bool result = false;

        switch (order) {
            case sort_order::position: {
                // Sort by slice location or Z position
                double a_pos = 0.0;
                double b_pos = 0.0;

                if (a.slice_location.has_value()) {
                    a_pos = a.slice_location.value();
                } else if (a.image_position_patient.has_value() &&
                           a.image_position_patient->size() >= 3) {
                    a_pos = (*a.image_position_patient)[2];  // Z position
                }

                if (b.slice_location.has_value()) {
                    b_pos = b.slice_location.value();
                } else if (b.image_position_patient.has_value() &&
                           b.image_position_patient->size() >= 3) {
                    b_pos = (*b.image_position_patient)[2];
                }

                result = a_pos < b_pos;
                break;
            }

            case sort_order::instance_number: {
                int a_num = a.instance_number.value_or(0);
                int b_num = b.instance_number.value_or(0);
                result = a_num < b_num;
                break;
            }

            case sort_order::acquisition_time: {
                std::string a_time = a.acquisition_time.value_or("");
                std::string b_time = b.acquisition_time.value_or("");
                result = a_time < b_time;
                break;
            }
        }

        return ascending ? result : !result;
    };

    std::sort(sorted.begin(), sorted.end(), compare);

    return sorted_instances_response::ok(std::move(sorted), instances.size());
}

navigation_info metadata_service::get_navigation(
    std::string_view sop_instance_uid) {
    if (database_ == nullptr) {
        return navigation_info::error("Database not configured");
    }

    // Get series UID for this instance
    auto series_uid_opt = get_series_uid(sop_instance_uid);
    if (!series_uid_opt.has_value()) {
        return navigation_info::error("Instance not found");
    }

    // Get sorted instances
    auto sorted_result =
        get_sorted_instances(series_uid_opt.value(), sort_order::position, true);
    if (!sorted_result.success) {
        return navigation_info::error(sorted_result.error_message);
    }

    const auto& instances = sorted_result.instances;
    if (instances.empty()) {
        return navigation_info::error("Series is empty");
    }

    // Find current instance index
    size_t current_index = 0;
    bool found = false;
    for (size_t i = 0; i < instances.size(); ++i) {
        if (instances[i].sop_instance_uid == sop_instance_uid) {
            current_index = i;
            found = true;
            break;
        }
    }

    if (!found) {
        return navigation_info::error("Instance not found in series");
    }

    // Build navigation info
    navigation_info nav = navigation_info::ok();
    nav.index = current_index;
    nav.total = instances.size();
    nav.first = instances.front().sop_instance_uid;
    nav.last = instances.back().sop_instance_uid;

    if (current_index > 0) {
        nav.previous = instances[current_index - 1].sop_instance_uid;
    }

    if (current_index < instances.size() - 1) {
        nav.next = instances[current_index + 1].sop_instance_uid;
    }

    return nav;
}

// =============================================================================
// Window/Level Presets
// =============================================================================

std::vector<window_level_preset> metadata_service::get_window_level_presets(
    std::string_view modality) {
    std::vector<window_level_preset> presets;

    if (modality == "CT") {
        presets.push_back({"Lung", -600, 1500});
        presets.push_back({"Bone", 300, 1500});
        presets.push_back({"Soft Tissue", 40, 400});
        presets.push_back({"Brain", 40, 80});
        presets.push_back({"Liver", 60, 150});
        presets.push_back({"Mediastinum", 50, 350});
    } else if (modality == "MR") {
        presets.push_back({"T1 Brain", 600, 1200});
        presets.push_back({"T2 Brain", 700, 1400});
        presets.push_back({"Spine", 500, 1000});
    } else if (modality == "CR" || modality == "DX") {
        presets.push_back({"Default", 2048, 4096});
        presets.push_back({"Bone", 1500, 3000});
        presets.push_back({"Soft Tissue", 1800, 3600});
    } else if (modality == "US") {
        presets.push_back({"Default", 128, 256});
    } else {
        // Generic presets
        presets.push_back({"Default", 128, 256});
    }

    return presets;
}

voi_lut_info metadata_service::get_voi_lut(std::string_view sop_instance_uid) {
    if (database_ == nullptr) {
        return voi_lut_info::error("Database not configured");
    }

    auto instance = database_->find_instance(sop_instance_uid);
    if (!instance) {
        return voi_lut_info::error("Instance not found");
    }

    if (!std::filesystem::exists(instance->file_path)) {
        return voi_lut_info::error("DICOM file not found");
    }

    auto file_result =
        pacs::core::dicom_file::open(std::filesystem::path(instance->file_path));
    if (file_result.is_err()) {
        return voi_lut_info::error("Failed to open DICOM file");
    }

    const auto& ds = file_result.value().dataset();

    voi_lut_info info = voi_lut_info::ok();

    // Window Center
    auto wc_str = ds.get_string(pacs::core::tags::window_center);
    if (!wc_str.empty()) {
        info.window_center = parse_numeric_list(wc_str);
    }

    // Window Width
    auto ww_str = ds.get_string(pacs::core::tags::window_width);
    if (!ww_str.empty()) {
        info.window_width = parse_numeric_list(ww_str);
    }

    // Window Explanation (0028,1055)
    const auto* we_elem =
        ds.get(pacs::core::dicom_tag{0x0028, 0x1055});
    if (we_elem != nullptr) {
        auto we_result = we_elem->as_string();
        if (we_result.is_ok()) {
            info.window_explanations = parse_string_list(we_result.value());
        }
    }

    // Rescale Slope
    auto rs_str = ds.get_string(pacs::core::tags::rescale_slope);
    if (!rs_str.empty()) {
        try {
            info.rescale_slope = std::stod(rs_str);
        } catch (...) {
        }
    }

    // Rescale Intercept
    auto ri_str = ds.get_string(pacs::core::tags::rescale_intercept);
    if (!ri_str.empty()) {
        try {
            info.rescale_intercept = std::stod(ri_str);
        } catch (...) {
        }
    }

    return info;
}

// =============================================================================
// Multi-frame Support
// =============================================================================

frame_info metadata_service::get_frame_info(std::string_view sop_instance_uid) {
    if (database_ == nullptr) {
        return frame_info::error("Database not configured");
    }

    auto instance = database_->find_instance(sop_instance_uid);
    if (!instance) {
        return frame_info::error("Instance not found");
    }

    if (!std::filesystem::exists(instance->file_path)) {
        return frame_info::error("DICOM file not found");
    }

    auto file_result =
        pacs::core::dicom_file::open(std::filesystem::path(instance->file_path));
    if (file_result.is_err()) {
        return frame_info::error("Failed to open DICOM file");
    }

    const auto& ds = file_result.value().dataset();

    frame_info info = frame_info::ok();

    // Number of Frames (0028,0008)
    const auto* nf_elem = ds.get(pacs::core::dicom_tag{0x0028, 0x0008});
    if (nf_elem != nullptr) {
        auto nf_result = nf_elem->as_string();
        if (nf_result.is_ok()) {
            try {
                info.total_frames = static_cast<uint32_t>(std::stoul(nf_result.value()));
            } catch (...) {
                info.total_frames = 1;
            }
        }
    }

    // Frame Time (0018,1063) - in milliseconds
    const auto* ft_elem = ds.get(pacs::core::dicom_tag{0x0018, 0x1063});
    if (ft_elem != nullptr) {
        auto ft_result = ft_elem->as_string();
        if (ft_result.is_ok()) {
            try {
                info.frame_time = std::stod(ft_result.value());
                if (info.frame_time.has_value() && info.frame_time.value() > 0) {
                    info.frame_rate = 1000.0 / info.frame_time.value();
                }
            } catch (...) {
            }
        }
    }

    // Rows
    auto rows_opt = ds.get_numeric<uint16_t>(pacs::core::tags::rows);
    if (rows_opt.has_value()) {
        info.rows = rows_opt.value();
    }

    // Columns
    auto cols_opt = ds.get_numeric<uint16_t>(pacs::core::tags::columns);
    if (cols_opt.has_value()) {
        info.columns = cols_opt.value();
    }

    return info;
}

}  // namespace pacs::web
