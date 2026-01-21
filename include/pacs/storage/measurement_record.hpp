/**
 * @file measurement_record.hpp
 * @brief Measurement record data structures for database operations
 *
 * This file provides the measurement_record and measurement_query structures
 * for storing and retrieving measurements on DICOM images.
 *
 * @see Issue #545 - Implement Annotation & Measurement APIs
 * @see Issue #581 - Part 1: Data Models and Repositories
 */

#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace pacs::storage {

/**
 * @brief Measurement types supported by the system
 */
enum class measurement_type {
    length,         ///< Linear distance measurement
    area,           ///< Area measurement (generic)
    angle,          ///< Angle measurement in degrees
    hounsfield,     ///< CT Hounsfield unit value
    suv,            ///< PET Standard Uptake Value
    ellipse_area,   ///< Ellipse area measurement
    polygon_area    ///< Polygon area measurement
};

/**
 * @brief Convert measurement_type to string
 */
[[nodiscard]] inline auto to_string(measurement_type type) -> std::string {
    switch (type) {
        case measurement_type::length: return "length";
        case measurement_type::area: return "area";
        case measurement_type::angle: return "angle";
        case measurement_type::hounsfield: return "hounsfield";
        case measurement_type::suv: return "suv";
        case measurement_type::ellipse_area: return "ellipse_area";
        case measurement_type::polygon_area: return "polygon_area";
    }
    return "unknown";
}

/**
 * @brief Parse string to measurement_type
 */
[[nodiscard]] inline auto measurement_type_from_string(std::string_view str)
    -> std::optional<measurement_type> {
    if (str == "length") return measurement_type::length;
    if (str == "area") return measurement_type::area;
    if (str == "angle") return measurement_type::angle;
    if (str == "hounsfield") return measurement_type::hounsfield;
    if (str == "suv") return measurement_type::suv;
    if (str == "ellipse_area") return measurement_type::ellipse_area;
    if (str == "polygon_area") return measurement_type::polygon_area;
    return std::nullopt;
}

/**
 * @brief Measurement record from the database
 *
 * Represents a single measurement on a DICOM image.
 * Maps directly to the measurements table in the database.
 */
struct measurement_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Unique measurement identifier (UUID)
    std::string measurement_id;

    /// SOP Instance UID - DICOM tag (0008,0018)
    std::string sop_instance_uid;

    /// Frame number for multi-frame images (1-based)
    std::optional<int> frame_number;

    /// User who created the measurement
    std::string user_id;

    /// Type of measurement
    measurement_type type{measurement_type::length};

    /// Geometry data as JSON string (coordinates)
    std::string geometry_json;

    /// Calculated measurement value
    double value{0.0};

    /// Unit of measurement (mm, cm2, degrees, HU, g/ml, etc.)
    std::string unit;

    /// Optional label/description
    std::string label;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if measurement_id and sop_instance_uid are not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !measurement_id.empty() && !sop_instance_uid.empty();
    }
};

/**
 * @brief Query parameters for measurement search
 *
 * Supports filtering by instance, study (via instance), user, and type.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * measurement_query query;
 * query.sop_instance_uid = "1.2.3.4.5.6";
 * query.type = measurement_type::length;
 * auto results = repo.search(query);
 * @endcode
 */
struct measurement_query {
    /// SOP Instance UID filter
    std::optional<std::string> sop_instance_uid;

    /// Study Instance UID filter (requires join with instances)
    std::optional<std::string> study_uid;

    /// User ID filter
    std::optional<std::string> user_id;

    /// Measurement type filter
    std::optional<measurement_type> type;

    /// Maximum number of results to return (0 = unlimited)
    size_t limit{0};

    /// Offset for pagination
    size_t offset{0};

    /**
     * @brief Check if any filter criteria is set
     *
     * @return true if at least one filter field is set
     */
    [[nodiscard]] auto has_criteria() const noexcept -> bool {
        return sop_instance_uid.has_value() || study_uid.has_value() ||
               user_id.has_value() || type.has_value();
    }
};

}  // namespace pacs::storage
