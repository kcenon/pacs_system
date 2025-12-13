/**
 * @file worklist_result_formatter.hpp
 * @brief Worklist Result Formatting Utilities
 *
 * Provides formatting utilities for displaying MWL C-FIND query results
 * in various formats: table, JSON, and CSV.
 *
 * @see Issue #104 - Worklist SCU Sample
 */

#ifndef WORKLIST_SCU_WORKLIST_RESULT_FORMATTER_HPP
#define WORKLIST_SCU_WORKLIST_RESULT_FORMATTER_HPP

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace worklist_scu {

/**
 * @brief Output format enumeration
 */
enum class output_format {
    table,  ///< Human-readable table format (alias: text)
    json,   ///< JSON format for integration
    csv,    ///< CSV format for export
    xml     ///< XML format for integration
};

/**
 * @brief Parse output format from string
 * @param format_str Format string ("table", "text", "json", "csv", "xml")
 * @return Parsed output format, or table if invalid
 */
[[nodiscard]] inline output_format parse_output_format(std::string_view format_str) {
    if (format_str == "json") return output_format::json;
    if (format_str == "csv") return output_format::csv;
    if (format_str == "xml") return output_format::xml;
    if (format_str == "text") return output_format::table;
    return output_format::table;
}

/**
 * @brief Result formatter for worklist query results
 *
 * Formats MWL C-FIND query results for display in different output formats.
 * Handles the complexity of extracting data from the Scheduled Procedure
 * Step Sequence.
 */
class worklist_result_formatter {
public:
    /**
     * @brief Construct formatter with output format
     * @param format The output format to use
     */
    explicit worklist_result_formatter(output_format format)
        : format_(format) {}

    /**
     * @brief Format worklist results
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
            case output_format::xml:
                return format_xml(results);
            case output_format::table:
            default:
                return format_table(results);
        }
    }

private:
    /**
     * @brief Worklist item data extracted for display
     */
    struct worklist_item {
        // Patient info
        std::string patient_name;
        std::string patient_id;
        std::string patient_birth_date;
        std::string patient_sex;

        // Scheduled Procedure Step info
        std::string scheduled_date;
        std::string scheduled_time;
        std::string modality;
        std::string station_ae;
        std::string step_id;
        std::string step_description;

        // Study/Request info
        std::string accession_number;
        std::string study_uid;
        std::string requested_procedure_id;
    };

    /**
     * @brief Extract worklist item data from dataset
     *
     * @note This implementation uses a flat dataset structure without nested
     * sequences. The Scheduled Procedure Step attributes are extracted directly
     * from the main dataset.
     */
    [[nodiscard]] worklist_item extract_item(
        const pacs::core::dicom_dataset& ds) const {
        using namespace pacs::core;

        worklist_item item;

        // Patient demographics
        item.patient_name = ds.get_string(tags::patient_name);
        item.patient_id = ds.get_string(tags::patient_id);
        item.patient_birth_date = ds.get_string(tags::patient_birth_date);
        item.patient_sex = ds.get_string(tags::patient_sex);

        // Study-level attributes
        item.accession_number = ds.get_string(tags::accession_number);
        item.study_uid = ds.get_string(tags::study_instance_uid);
        item.requested_procedure_id = ds.get_string(tags::requested_procedure_id);

        // Scheduled Procedure Step attributes (flat structure)
        item.scheduled_date = ds.get_string(tags::scheduled_procedure_step_start_date);
        item.scheduled_time = ds.get_string(tags::scheduled_procedure_step_start_time);
        item.modality = ds.get_string(tags::modality);
        item.station_ae = ds.get_string(tags::scheduled_station_ae_title);
        item.step_id = ds.get_string(tags::scheduled_procedure_step_id);
        item.step_description = ds.get_string(tags::scheduled_procedure_step_description);

        return item;
    }

    /**
     * @brief Format results as a human-readable table
     */
    [[nodiscard]] std::string format_table(
        const std::vector<pacs::core::dicom_dataset>& results) const {
        std::ostringstream oss;

        if (results.empty()) {
            oss << "No worklist items found.\n";
            return oss.str();
        }

        // Extract all items
        std::vector<worklist_item> items;
        items.reserve(results.size());
        for (const auto& r : results) {
            items.push_back(extract_item(r));
        }

        // Define column widths
        size_t w_name = 20, w_id = 12, w_date = 10, w_time = 8;
        size_t w_mod = 6, w_station = 16, w_accession = 12, w_step = 12;

        // Update widths based on data
        for (const auto& item : items) {
            w_name = std::min(size_t(30), std::max(w_name, item.patient_name.length()));
            w_id = std::min(size_t(20), std::max(w_id, item.patient_id.length()));
            w_station = std::min(size_t(20), std::max(w_station, item.station_ae.length()));
            w_accession = std::min(size_t(20), std::max(w_accession, item.accession_number.length()));
            w_step = std::min(size_t(20), std::max(w_step, item.step_id.length()));
        }

        // Print header
        oss << "\n=== Worklist Results (" << results.size() << " scheduled procedure(s)) ===\n\n";

        // Print column headers
        oss << std::left
            << std::setw(static_cast<int>(w_name + 2)) << "Patient Name"
            << std::setw(static_cast<int>(w_id + 2)) << "Patient ID"
            << std::setw(static_cast<int>(w_date + 2)) << "Sched Date"
            << std::setw(static_cast<int>(w_time + 2)) << "Time"
            << std::setw(static_cast<int>(w_mod + 2)) << "Mod"
            << std::setw(static_cast<int>(w_station + 2)) << "Station AE"
            << std::setw(static_cast<int>(w_accession + 2)) << "Accession#"
            << std::setw(static_cast<int>(w_step + 2)) << "Step ID"
            << "\n";

        // Print separator
        oss << std::string(w_name, '-') << "  "
            << std::string(w_id, '-') << "  "
            << std::string(w_date, '-') << "  "
            << std::string(w_time, '-') << "  "
            << std::string(w_mod, '-') << "  "
            << std::string(w_station, '-') << "  "
            << std::string(w_accession, '-') << "  "
            << std::string(w_step, '-') << "  "
            << "\n";

        // Print data rows
        for (const auto& item : items) {
            oss << std::left
                << std::setw(static_cast<int>(w_name + 2)) << truncate(item.patient_name, w_name)
                << std::setw(static_cast<int>(w_id + 2)) << truncate(item.patient_id, w_id)
                << std::setw(static_cast<int>(w_date + 2)) << format_date(item.scheduled_date)
                << std::setw(static_cast<int>(w_time + 2)) << format_time(item.scheduled_time)
                << std::setw(static_cast<int>(w_mod + 2)) << item.modality
                << std::setw(static_cast<int>(w_station + 2)) << truncate(item.station_ae, w_station)
                << std::setw(static_cast<int>(w_accession + 2)) << truncate(item.accession_number, w_accession)
                << std::setw(static_cast<int>(w_step + 2)) << truncate(item.step_id, w_step)
                << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Format results as JSON
     */
    [[nodiscard]] std::string format_json(
        const std::vector<pacs::core::dicom_dataset>& results) const {
        std::ostringstream oss;

        oss << "{\n";
        oss << "  \"resultCount\": " << results.size() << ",\n";
        oss << "  \"worklistItems\": [\n";

        for (size_t i = 0; i < results.size(); ++i) {
            auto item = extract_item(results[i]);

            oss << "    {\n";
            oss << "      \"patient\": {\n";
            oss << "        \"name\": \"" << escape_json(item.patient_name) << "\",\n";
            oss << "        \"id\": \"" << escape_json(item.patient_id) << "\",\n";
            oss << "        \"birthDate\": \"" << item.patient_birth_date << "\",\n";
            oss << "        \"sex\": \"" << item.patient_sex << "\"\n";
            oss << "      },\n";
            oss << "      \"scheduledProcedureStep\": {\n";
            oss << "        \"startDate\": \"" << item.scheduled_date << "\",\n";
            oss << "        \"startTime\": \"" << item.scheduled_time << "\",\n";
            oss << "        \"modality\": \"" << item.modality << "\",\n";
            oss << "        \"stationAETitle\": \"" << escape_json(item.station_ae) << "\",\n";
            oss << "        \"stepId\": \"" << escape_json(item.step_id) << "\",\n";
            oss << "        \"description\": \"" << escape_json(item.step_description) << "\"\n";
            oss << "      },\n";
            oss << "      \"accessionNumber\": \"" << escape_json(item.accession_number) << "\",\n";
            oss << "      \"studyInstanceUid\": \"" << item.study_uid << "\",\n";
            oss << "      \"requestedProcedureId\": \"" << escape_json(item.requested_procedure_id) << "\"\n";
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
        std::ostringstream oss;

        // Header row
        oss << "PatientName,PatientID,BirthDate,Sex,"
            << "ScheduledDate,ScheduledTime,Modality,StationAE,"
            << "StepID,StepDescription,AccessionNumber,StudyUID,RequestedProcedureID\n";

        // Data rows
        for (const auto& r : results) {
            auto item = extract_item(r);

            oss << escape_csv(item.patient_name) << ","
                << escape_csv(item.patient_id) << ","
                << item.patient_birth_date << ","
                << item.patient_sex << ","
                << item.scheduled_date << ","
                << item.scheduled_time << ","
                << item.modality << ","
                << escape_csv(item.station_ae) << ","
                << escape_csv(item.step_id) << ","
                << escape_csv(item.step_description) << ","
                << escape_csv(item.accession_number) << ","
                << item.study_uid << ","
                << escape_csv(item.requested_procedure_id) << "\n";
        }

        return oss.str();
    }

    /**
     * @brief Format results as XML
     */
    [[nodiscard]] std::string format_xml(
        const std::vector<pacs::core::dicom_dataset>& results) const {
        std::ostringstream oss;

        oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        oss << "<WorklistQueryResult>\n";
        oss << "  <ResultCount>" << results.size() << "</ResultCount>\n";
        oss << "  <WorklistItems>\n";

        for (size_t i = 0; i < results.size(); ++i) {
            auto item = extract_item(results[i]);

            oss << "    <WorklistItem index=\"" << (i + 1) << "\">\n";

            // Patient information
            oss << "      <Patient>\n";
            oss << "        <Name>" << escape_xml(item.patient_name) << "</Name>\n";
            oss << "        <ID>" << escape_xml(item.patient_id) << "</ID>\n";
            oss << "        <BirthDate>" << item.patient_birth_date << "</BirthDate>\n";
            oss << "        <Sex>" << item.patient_sex << "</Sex>\n";
            oss << "      </Patient>\n";

            // Scheduled Procedure Step
            oss << "      <ScheduledProcedureStep>\n";
            oss << "        <StartDate>" << item.scheduled_date << "</StartDate>\n";
            oss << "        <StartTime>" << item.scheduled_time << "</StartTime>\n";
            oss << "        <Modality>" << item.modality << "</Modality>\n";
            oss << "        <StationAETitle>" << escape_xml(item.station_ae) << "</StationAETitle>\n";
            oss << "        <StepID>" << escape_xml(item.step_id) << "</StepID>\n";
            oss << "        <Description>" << escape_xml(item.step_description) << "</Description>\n";
            oss << "      </ScheduledProcedureStep>\n";

            // Study information
            oss << "      <AccessionNumber>" << escape_xml(item.accession_number) << "</AccessionNumber>\n";
            oss << "      <StudyInstanceUID>" << item.study_uid << "</StudyInstanceUID>\n";
            oss << "      <RequestedProcedureID>" << escape_xml(item.requested_procedure_id) << "</RequestedProcedureID>\n";

            oss << "    </WorklistItem>\n";
        }

        oss << "  </WorklistItems>\n";
        oss << "</WorklistQueryResult>\n";

        return oss.str();
    }

    /**
     * @brief Truncate string to max length
     */
    [[nodiscard]] static std::string truncate(const std::string& s, size_t max_len) {
        if (s.length() <= max_len) {
            return s;
        }
        return s.substr(0, max_len - 3) + "...";
    }

    /**
     * @brief Format DICOM date (YYYYMMDD) for display
     */
    [[nodiscard]] static std::string format_date(const std::string& date) {
        if (date.length() == 8) {
            return date.substr(0, 4) + "-" + date.substr(4, 2) + "-" + date.substr(6, 2);
        }
        return date;
    }

    /**
     * @brief Format DICOM time (HHMMSS) for display
     */
    [[nodiscard]] static std::string format_time(const std::string& time) {
        if (time.length() >= 4) {
            return time.substr(0, 2) + ":" + time.substr(2, 2);
        }
        return time;
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

    /**
     * @brief Escape string for XML output
     */
    [[nodiscard]] static std::string escape_xml(const std::string& s) {
        std::string result;
        result.reserve(s.length());
        for (char c : s) {
            switch (c) {
                case '&': result += "&amp;"; break;
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result += c;
            }
        }
        return result;
    }

    output_format format_;
};

}  // namespace worklist_scu

#endif  // WORKLIST_SCU_WORKLIST_RESULT_FORMATTER_HPP
