/**
 * @file result_formatter.hpp
 * @brief Query Result Formatting Utilities
 *
 * Provides formatting utilities for displaying C-FIND query results
 * in various formats: table, JSON, and CSV.
 *
 * @see Issue #102 - Query SCU Sample
 */

#ifndef QUERY_SCU_RESULT_FORMATTER_HPP
#define QUERY_SCU_RESULT_FORMATTER_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/services/query_scp.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace query_scu {

/**
 * @brief Output format enumeration
 */
enum class output_format {
    table,  ///< Human-readable table format
    json,   ///< JSON format for integration
    csv     ///< CSV format for export
};

/**
 * @brief Parse output format from string
 * @param format_str Format string ("table", "json", "csv")
 * @return Parsed output format, or table if invalid
 */
[[nodiscard]] inline output_format parse_output_format(std::string_view format_str) {
    if (format_str == "json") return output_format::json;
    if (format_str == "csv") return output_format::csv;
    return output_format::table;
}

/**
 * @brief Result formatter for query results
 *
 * Formats C-FIND query results for display in different output formats.
 */
class result_formatter {
public:
    using query_level = pacs::services::query_level;

    /**
     * @brief Construct formatter with output format
     * @param format The output format to use
     * @param level The query level for proper column selection
     */
    explicit result_formatter(output_format format, query_level level)
        : format_(format), level_(level) {}

    /**
     * @brief Format query results
     * @param results Vector of result datasets
     * @return Formatted string output
     */
    [[nodiscard]] std::string format(
        const std::vector<pacs::core::dicom_dataset>& results) const {
        switch (format_) {
            case output_format::json:
                return format_json(results);
            case output_format::csv:
                return format_csv(results);
            case output_format::table:
            default:
                return format_table(results);
        }
    }

private:
    /**
     * @brief Format results as a human-readable table
     */
    [[nodiscard]] std::string format_table(
        const std::vector<pacs::core::dicom_dataset>& results) const {
        using namespace pacs::core;
        std::ostringstream oss;

        if (results.empty()) {
            oss << "No results found.\n";
            return oss.str();
        }

        // Define columns based on query level
        auto columns = get_columns_for_level();

        // Calculate column widths
        std::vector<size_t> widths;
        widths.reserve(columns.size());
        for (const auto& col : columns) {
            widths.push_back(col.header.length());
        }

        // Update widths based on data
        for (const auto& result : results) {
            for (size_t i = 0; i < columns.size(); ++i) {
                auto value = get_tag_value(result, columns[i].tag);
                widths[i] = std::max(widths[i], value.length());
            }
        }

        // Cap widths at reasonable maximum
        for (auto& w : widths) {
            w = std::min(w, size_t(40));
        }

        // Print header
        oss << "\n=== Query Results (" << results.size() << " "
            << pacs::services::to_string(level_) << "(s)) ===\n\n";

        // Print column headers
        for (size_t i = 0; i < columns.size(); ++i) {
            oss << std::left << std::setw(static_cast<int>(widths[i] + 2))
                << columns[i].header;
        }
        oss << "\n";

        // Print separator
        for (size_t i = 0; i < columns.size(); ++i) {
            oss << std::string(widths[i], '-') << "  ";
        }
        oss << "\n";

        // Print data rows
        for (const auto& result : results) {
            for (size_t i = 0; i < columns.size(); ++i) {
                auto value = get_tag_value(result, columns[i].tag);
                if (value.length() > widths[i]) {
                    value = value.substr(0, widths[i] - 3) + "...";
                }
                oss << std::left << std::setw(static_cast<int>(widths[i] + 2))
                    << value;
            }
            oss << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Format results as JSON
     */
    [[nodiscard]] std::string format_json(
        const std::vector<pacs::core::dicom_dataset>& results) const {
        using namespace pacs::core;
        std::ostringstream oss;

        oss << "{\n";
        oss << "  \"queryLevel\": \"" << pacs::services::to_string(level_) << "\",\n";
        oss << "  \"resultCount\": " << results.size() << ",\n";
        oss << "  \"results\": [\n";

        auto columns = get_columns_for_level();

        for (size_t i = 0; i < results.size(); ++i) {
            const auto& result = results[i];
            oss << "    {\n";

            for (size_t j = 0; j < columns.size(); ++j) {
                auto value = get_tag_value(result, columns[j].tag);
                oss << "      \"" << columns[j].json_key << "\": \""
                    << escape_json(value) << "\"";
                if (j < columns.size() - 1) {
                    oss << ",";
                }
                oss << "\n";
            }

            oss << "    }";
            if (i < results.size() - 1) {
                oss << ",";
            }
            oss << "\n";
        }

        oss << "  ]\n";
        oss << "}\n";

        return oss.str();
    }

    /**
     * @brief Format results as CSV
     */
    [[nodiscard]] std::string format_csv(
        const std::vector<pacs::core::dicom_dataset>& results) const {
        using namespace pacs::core;
        std::ostringstream oss;

        auto columns = get_columns_for_level();

        // Print header row
        for (size_t i = 0; i < columns.size(); ++i) {
            oss << columns[i].header;
            if (i < columns.size() - 1) {
                oss << ",";
            }
        }
        oss << "\n";

        // Print data rows
        for (const auto& result : results) {
            for (size_t i = 0; i < columns.size(); ++i) {
                auto value = get_tag_value(result, columns[i].tag);
                oss << escape_csv(value);
                if (i < columns.size() - 1) {
                    oss << ",";
                }
            }
            oss << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Column definition for formatting
     */
    struct column_def {
        std::string header;
        pacs::core::dicom_tag tag;
        std::string json_key;
    };

    /**
     * @brief Get columns appropriate for the query level
     */
    [[nodiscard]] std::vector<column_def> get_columns_for_level() const {
        using namespace pacs::core;
        std::vector<column_def> columns;

        // Patient level columns (always included)
        columns.push_back({"Patient Name", tags::patient_name, "patientName"});
        columns.push_back({"Patient ID", tags::patient_id, "patientId"});

        if (level_ == query_level::patient) {
            columns.push_back({"Birth Date", tags::patient_birth_date, "birthDate"});
            columns.push_back({"Sex", tags::patient_sex, "sex"});
            return columns;
        }

        // Study level columns
        columns.push_back({"Study Date", tags::study_date, "studyDate"});
        columns.push_back({"Accession #", tags::accession_number, "accessionNumber"});
        columns.push_back({"Description", tags::study_description, "studyDescription"});

        if (level_ == query_level::study) {
            columns.push_back({"Modalities", tags::modalities_in_study, "modalities"});
            columns.push_back({"Study UID", tags::study_instance_uid, "studyInstanceUid"});
            return columns;
        }

        // Series level columns
        columns.push_back({"Modality", tags::modality, "modality"});
        columns.push_back({"Series #", tags::series_number, "seriesNumber"});
        columns.push_back({"Series Desc", tags::series_description, "seriesDescription"});

        if (level_ == query_level::series) {
            columns.push_back({"Series UID", tags::series_instance_uid, "seriesInstanceUid"});
            return columns;
        }

        // Instance level columns
        columns.push_back({"Instance #", tags::instance_number, "instanceNumber"});
        columns.push_back({"SOP Class", tags::sop_class_uid, "sopClassUid"});
        columns.push_back({"SOP Instance UID", tags::sop_instance_uid, "sopInstanceUid"});

        return columns;
    }

    /**
     * @brief Get tag value from dataset as string
     */
    [[nodiscard]] static std::string get_tag_value(
        const pacs::core::dicom_dataset& ds,
        pacs::core::dicom_tag tag) {
        return ds.get_string(tag);
    }

    /**
     * @brief Escape string for JSON output
     */
    [[nodiscard]] static std::string escape_json(const std::string& s) {
        std::string result;
        result.reserve(s.length());
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        result += "\\u";
                        char buf[5];
                        std::snprintf(buf, sizeof(buf), "%04X",
                                      static_cast<unsigned char>(c));
                        result += buf;
                    } else {
                        result += c;
                    }
            }
        }
        return result;
    }

    /**
     * @brief Escape string for CSV output
     */
    [[nodiscard]] static std::string escape_csv(const std::string& s) {
        if (s.find_first_of(",\"\n\r") == std::string::npos) {
            return s;
        }

        std::string result = "\"";
        for (char c : s) {
            if (c == '"') {
                result += "\"\"";
            } else {
                result += c;
            }
        }
        result += "\"";
        return result;
    }

    output_format format_;
    query_level level_;
};

}  // namespace query_scu

#endif  // QUERY_SCU_RESULT_FORMATTER_HPP
