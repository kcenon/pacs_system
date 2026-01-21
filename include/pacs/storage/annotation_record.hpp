/**
 * @file annotation_record.hpp
 * @brief Annotation record data structures for database operations
 *
 * This file provides the annotation_record and annotation_query structures
 * for storing and retrieving user annotations on DICOM images.
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
 * @brief Annotation types supported by the system
 */
enum class annotation_type {
    arrow,      ///< Arrow annotation
    line,       ///< Simple line
    rectangle,  ///< Rectangle shape
    ellipse,    ///< Ellipse/oval shape
    polygon,    ///< Multi-point polygon
    freehand,   ///< Freehand drawing
    text,       ///< Text annotation
    angle,      ///< Angle measurement annotation
    roi         ///< Region of interest
};

/**
 * @brief Convert annotation_type to string
 */
[[nodiscard]] inline auto to_string(annotation_type type) -> std::string {
    switch (type) {
        case annotation_type::arrow: return "arrow";
        case annotation_type::line: return "line";
        case annotation_type::rectangle: return "rectangle";
        case annotation_type::ellipse: return "ellipse";
        case annotation_type::polygon: return "polygon";
        case annotation_type::freehand: return "freehand";
        case annotation_type::text: return "text";
        case annotation_type::angle: return "angle";
        case annotation_type::roi: return "roi";
    }
    return "unknown";
}

/**
 * @brief Parse string to annotation_type
 */
[[nodiscard]] inline auto annotation_type_from_string(std::string_view str)
    -> std::optional<annotation_type> {
    if (str == "arrow") return annotation_type::arrow;
    if (str == "line") return annotation_type::line;
    if (str == "rectangle") return annotation_type::rectangle;
    if (str == "ellipse") return annotation_type::ellipse;
    if (str == "polygon") return annotation_type::polygon;
    if (str == "freehand") return annotation_type::freehand;
    if (str == "text") return annotation_type::text;
    if (str == "angle") return annotation_type::angle;
    if (str == "roi") return annotation_type::roi;
    return std::nullopt;
}

/**
 * @brief Style information for annotations
 */
struct annotation_style {
    std::string color{"#FFFF00"};        ///< Stroke/line color (hex)
    int line_width{2};                   ///< Line width in pixels
    std::string fill_color;              ///< Fill color (hex), empty for no fill
    float fill_opacity{0.0f};            ///< Fill opacity (0.0-1.0)
    std::string font_family{"Arial"};    ///< Font family for text
    int font_size{14};                   ///< Font size in pixels
};

/**
 * @brief Annotation record from the database
 *
 * Represents a single annotation on a DICOM image.
 * Maps directly to the annotations table in the database.
 */
struct annotation_record {
    /// Primary key (auto-generated)
    int64_t pk{0};

    /// Unique annotation identifier (UUID)
    std::string annotation_id;

    /// Study Instance UID - DICOM tag (0020,000D)
    std::string study_uid;

    /// Series Instance UID - DICOM tag (0020,000E)
    std::string series_uid;

    /// SOP Instance UID - DICOM tag (0008,0018)
    std::string sop_instance_uid;

    /// Frame number for multi-frame images (1-based)
    std::optional<int> frame_number;

    /// User who created the annotation
    std::string user_id;

    /// Type of annotation
    annotation_type type{annotation_type::text};

    /// Geometry data as JSON string (type-specific coordinates)
    std::string geometry_json;

    /// Text content for text annotations or labels
    std::string text;

    /// Visual style of the annotation
    annotation_style style;

    /// Record creation timestamp
    std::chrono::system_clock::time_point created_at;

    /// Record last update timestamp
    std::chrono::system_clock::time_point updated_at;

    /**
     * @brief Check if this record has valid data
     *
     * @return true if annotation_id is not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool {
        return !annotation_id.empty() && !study_uid.empty();
    }
};

/**
 * @brief Query parameters for annotation search
 *
 * Supports filtering by study, series, instance, user, and type.
 * Empty fields are not included in the query filter.
 *
 * @example
 * @code
 * annotation_query query;
 * query.study_uid = "1.2.3.4.5";
 * query.user_id = "user123";
 * auto results = repo.search(query);
 * @endcode
 */
struct annotation_query {
    /// Study Instance UID filter
    std::optional<std::string> study_uid;

    /// Series Instance UID filter
    std::optional<std::string> series_uid;

    /// SOP Instance UID filter
    std::optional<std::string> sop_instance_uid;

    /// User ID filter
    std::optional<std::string> user_id;

    /// Annotation type filter
    std::optional<annotation_type> type;

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
        return study_uid.has_value() || series_uid.has_value() ||
               sop_instance_uid.has_value() || user_id.has_value() ||
               type.has_value();
    }
};

}  // namespace pacs::storage
