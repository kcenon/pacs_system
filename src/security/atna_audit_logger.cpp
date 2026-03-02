/**
 * @file atna_audit_logger.cpp
 * @brief Implementation of the ATNA audit message generator
 */

#include "pacs/security/atna_audit_logger.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace pacs::security {

// =============================================================================
// XML Generation
// =============================================================================

std::string atna_audit_logger::to_xml(const atna_audit_message& msg) {
    std::ostringstream xml;

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<AuditMessage>\n";

    // -- EventIdentification --
    xml << "  <EventIdentification"
        << " EventActionCode=\""
        << static_cast<char>(msg.event_action) << "\""
        << " EventDateTime=\"" << format_datetime(msg.event_date_time) << "\""
        << " EventOutcomeIndicator=\""
        << static_cast<int>(msg.event_outcome) << "\">\n";

    xml << "    <EventID " << format_coded_value_attrs(msg.event_id) << "/>\n";

    for (const auto& etc : msg.event_type_codes) {
        xml << "    <EventTypeCode " << format_coded_value_attrs(etc)
            << "/>\n";
    }

    xml << "  </EventIdentification>\n";

    // -- ActiveParticipant(s) --
    for (const auto& ap : msg.active_participants) {
        xml << "  <ActiveParticipant"
            << " UserID=\"" << xml_escape(ap.user_id) << "\"";

        if (!ap.alternative_user_id.empty()) {
            xml << " AlternativeUserID=\""
                << xml_escape(ap.alternative_user_id) << "\"";
        }

        if (!ap.user_name.empty()) {
            xml << " UserName=\"" << xml_escape(ap.user_name) << "\"";
        }

        xml << " UserIsRequestor=\""
            << (ap.user_is_requestor ? "true" : "false") << "\"";

        if (!ap.network_access_point_id.empty()) {
            xml << " NetworkAccessPointID=\""
                << xml_escape(ap.network_access_point_id) << "\""
                << " NetworkAccessPointTypeCode=\""
                << static_cast<int>(ap.network_access_point_type) << "\"";
        }

        if (ap.role_id_codes.empty()) {
            xml << "/>\n";
        } else {
            xml << ">\n";
            for (const auto& role : ap.role_id_codes) {
                xml << "    <RoleIDCode "
                    << format_coded_value_attrs(role) << "/>\n";
            }
            xml << "  </ActiveParticipant>\n";
        }
    }

    // -- AuditSourceIdentification --
    xml << "  <AuditSourceIdentification"
        << " AuditSourceID=\""
        << xml_escape(msg.audit_source.audit_source_id) << "\"";

    if (!msg.audit_source.audit_enterprise_site_id.empty()) {
        xml << " AuditEnterpriseSiteID=\""
            << xml_escape(msg.audit_source.audit_enterprise_site_id) << "\"";
    }

    if (msg.audit_source.audit_source_type_codes.empty()) {
        xml << "/>\n";
    } else {
        xml << ">\n";
        for (auto type_code : msg.audit_source.audit_source_type_codes) {
            xml << "    <AuditSourceTypeCode code=\""
                << static_cast<int>(type_code) << "\"/>\n";
        }
        xml << "  </AuditSourceIdentification>\n";
    }

    // -- ParticipantObjectIdentification(s) --
    for (const auto& po : msg.participant_objects) {
        xml << "  <ParticipantObjectIdentification"
            << " ParticipantObjectTypeCode=\""
            << static_cast<int>(po.object_type) << "\""
            << " ParticipantObjectTypeCodeRole=\""
            << static_cast<int>(po.object_role) << "\"";

        if (!po.object_id.empty()) {
            xml << " ParticipantObjectID=\""
                << xml_escape(po.object_id) << "\"";
        }

        if (!po.object_name.empty()) {
            xml << " ParticipantObjectName=\""
                << xml_escape(po.object_name) << "\"";
        }

        bool has_children = !po.object_id_type_code.code.empty() ||
                            !po.object_query.empty() ||
                            !po.object_details.empty();

        if (!has_children) {
            xml << "/>\n";
        } else {
            xml << ">\n";

            if (!po.object_id_type_code.code.empty()) {
                xml << "    <ParticipantObjectIDTypeCode "
                    << format_coded_value_attrs(po.object_id_type_code)
                    << "/>\n";
            }

            if (!po.object_query.empty()) {
                xml << "    <ParticipantObjectQuery>"
                    << xml_escape(po.object_query)
                    << "</ParticipantObjectQuery>\n";
            }

            for (const auto& detail : po.object_details) {
                xml << "    <ParticipantObjectDetail"
                    << " type=\"" << xml_escape(detail.type) << "\""
                    << " value=\"" << xml_escape(detail.value) << "\"/>\n";
            }

            xml << "  </ParticipantObjectIdentification>\n";
        }
    }

    xml << "</AuditMessage>\n";

    return xml.str();
}

// =============================================================================
// Event Factory Methods
// =============================================================================

atna_audit_message atna_audit_logger::build_application_activity(
    const std::string& source_id,
    const std::string& app_name,
    bool is_start,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::application_activity;
    msg.event_type_codes.push_back(
        is_start ? atna_event_types::application_start
                 : atna_event_types::application_stop);
    msg.event_action = atna_event_action::execute;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    atna_active_participant ap;
    ap.user_id = app_name;
    ap.user_is_requestor = false;
    ap.role_id_codes.push_back(atna_role_ids::application);
    msg.active_participants.push_back(std::move(ap));

    msg.audit_source.audit_source_id = source_id;

    return msg;
}

atna_audit_message atna_audit_logger::build_dicom_instances_accessed(
    const std::string& source_id,
    const std::string& user_id,
    const std::string& user_ip,
    const std::string& study_uid,
    const std::string& patient_id,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::dicom_instances_accessed;
    msg.event_action = atna_event_action::read;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    atna_active_participant ap;
    ap.user_id = user_id;
    ap.user_is_requestor = true;
    ap.network_access_point_id = user_ip;
    ap.network_access_point_type = atna_network_access_type::ip_address;
    msg.active_participants.push_back(std::move(ap));

    msg.audit_source.audit_source_id = source_id;

    // Study object
    if (!study_uid.empty()) {
        atna_participant_object obj;
        obj.object_type = atna_object_type::system_object;
        obj.object_role = atna_object_role::report;
        obj.object_id_type_code = atna_object_id_types::study_instance_uid;
        obj.object_id = study_uid;
        msg.participant_objects.push_back(std::move(obj));
    }

    // Patient object
    if (!patient_id.empty()) {
        atna_participant_object patient_obj;
        patient_obj.object_type = atna_object_type::person;
        patient_obj.object_role = atna_object_role::patient;
        patient_obj.object_id_type_code = atna_object_id_types::patient_number;
        patient_obj.object_id = patient_id;
        msg.participant_objects.push_back(std::move(patient_obj));
    }

    return msg;
}

atna_audit_message atna_audit_logger::build_dicom_instances_transferred(
    const std::string& source_id,
    const std::string& source_ae,
    const std::string& source_ip,
    const std::string& dest_ae,
    const std::string& dest_ip,
    const std::string& study_uid,
    const std::string& patient_id,
    bool is_import,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::dicom_instances_transferred;
    msg.event_action = is_import ? atna_event_action::create
                                 : atna_event_action::read;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    // Source participant
    atna_active_participant src;
    src.user_id = source_ae;
    src.user_is_requestor = !is_import;
    src.network_access_point_id = source_ip;
    src.network_access_point_type = atna_network_access_type::ip_address;
    src.role_id_codes.push_back(atna_role_ids::source);
    msg.active_participants.push_back(std::move(src));

    // Destination participant
    atna_active_participant dst;
    dst.user_id = dest_ae;
    dst.user_is_requestor = is_import;
    dst.network_access_point_id = dest_ip;
    dst.network_access_point_type = atna_network_access_type::ip_address;
    dst.role_id_codes.push_back(atna_role_ids::destination);
    msg.active_participants.push_back(std::move(dst));

    msg.audit_source.audit_source_id = source_id;

    // Study object
    if (!study_uid.empty()) {
        atna_participant_object obj;
        obj.object_type = atna_object_type::system_object;
        obj.object_role = atna_object_role::report;
        obj.object_id_type_code = atna_object_id_types::study_instance_uid;
        obj.object_id = study_uid;
        msg.participant_objects.push_back(std::move(obj));
    }

    // Patient object
    if (!patient_id.empty()) {
        atna_participant_object patient_obj;
        patient_obj.object_type = atna_object_type::person;
        patient_obj.object_role = atna_object_role::patient;
        patient_obj.object_id_type_code = atna_object_id_types::patient_number;
        patient_obj.object_id = patient_id;
        msg.participant_objects.push_back(std::move(patient_obj));
    }

    return msg;
}

atna_audit_message atna_audit_logger::build_study_deleted(
    const std::string& source_id,
    const std::string& user_id,
    const std::string& user_ip,
    const std::string& study_uid,
    const std::string& patient_id,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::dicom_study_deleted;
    msg.event_action = atna_event_action::delete_action;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    atna_active_participant ap;
    ap.user_id = user_id;
    ap.user_is_requestor = true;
    ap.network_access_point_id = user_ip;
    ap.network_access_point_type = atna_network_access_type::ip_address;
    msg.active_participants.push_back(std::move(ap));

    msg.audit_source.audit_source_id = source_id;

    // Study object
    atna_participant_object obj;
    obj.object_type = atna_object_type::system_object;
    obj.object_role = atna_object_role::report;
    obj.object_id_type_code = atna_object_id_types::study_instance_uid;
    obj.object_id = study_uid;
    msg.participant_objects.push_back(std::move(obj));

    // Patient object
    if (!patient_id.empty()) {
        atna_participant_object patient_obj;
        patient_obj.object_type = atna_object_type::person;
        patient_obj.object_role = atna_object_role::patient;
        patient_obj.object_id_type_code = atna_object_id_types::patient_number;
        patient_obj.object_id = patient_id;
        msg.participant_objects.push_back(std::move(patient_obj));
    }

    return msg;
}

atna_audit_message atna_audit_logger::build_security_alert(
    const std::string& source_id,
    const std::string& user_id,
    const std::string& user_ip,
    const std::string& alert_description,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::security_alert;
    msg.event_action = atna_event_action::execute;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    atna_active_participant ap;
    ap.user_id = user_id;
    ap.user_is_requestor = true;
    ap.network_access_point_id = user_ip;
    ap.network_access_point_type = atna_network_access_type::ip_address;
    msg.active_participants.push_back(std::move(ap));

    msg.audit_source.audit_source_id = source_id;

    // Alert description as participant object
    atna_participant_object obj;
    obj.object_type = atna_object_type::system_object;
    obj.object_role = atna_object_role::security_resource;
    obj.object_id = source_id;
    obj.object_name = alert_description;
    msg.participant_objects.push_back(std::move(obj));

    return msg;
}

atna_audit_message atna_audit_logger::build_user_authentication(
    const std::string& source_id,
    const std::string& user_id,
    const std::string& user_ip,
    bool is_login,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::user_authentication;
    msg.event_type_codes.push_back(
        is_login ? atna_event_types::login : atna_event_types::logout);
    msg.event_action = atna_event_action::execute;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    atna_active_participant ap;
    ap.user_id = user_id;
    ap.user_is_requestor = true;
    ap.network_access_point_id = user_ip;
    ap.network_access_point_type = atna_network_access_type::ip_address;
    msg.active_participants.push_back(std::move(ap));

    msg.audit_source.audit_source_id = source_id;

    return msg;
}

atna_audit_message atna_audit_logger::build_query(
    const std::string& source_id,
    const std::string& user_id,
    const std::string& user_ip,
    const std::string& query_data,
    const std::string& patient_id,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::query;
    msg.event_action = atna_event_action::execute;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    atna_active_participant ap;
    ap.user_id = user_id;
    ap.user_is_requestor = true;
    ap.network_access_point_id = user_ip;
    ap.network_access_point_type = atna_network_access_type::ip_address;
    ap.role_id_codes.push_back(atna_role_ids::source);
    msg.active_participants.push_back(std::move(ap));

    msg.audit_source.audit_source_id = source_id;

    // Query object
    atna_participant_object query_obj;
    query_obj.object_type = atna_object_type::system_object;
    query_obj.object_role = atna_object_role::query;
    query_obj.object_id_type_code = atna_object_id_types::sop_class_uid;
    query_obj.object_query = query_data;
    msg.participant_objects.push_back(std::move(query_obj));

    // Patient object
    if (!patient_id.empty()) {
        atna_participant_object patient_obj;
        patient_obj.object_type = atna_object_type::person;
        patient_obj.object_role = atna_object_role::patient;
        patient_obj.object_id_type_code = atna_object_id_types::patient_number;
        patient_obj.object_id = patient_id;
        msg.participant_objects.push_back(std::move(patient_obj));
    }

    return msg;
}

atna_audit_message atna_audit_logger::build_export(
    const std::string& source_id,
    const std::string& user_id,
    const std::string& user_ip,
    const std::string& dest_id,
    const std::string& study_uid,
    const std::string& patient_id,
    atna_event_outcome outcome) {

    atna_audit_message msg;
    msg.event_id = atna_event_ids::data_export;
    msg.event_action = atna_event_action::read;
    msg.event_date_time = std::chrono::system_clock::now();
    msg.event_outcome = outcome;

    // Exporter
    atna_active_participant exporter;
    exporter.user_id = user_id;
    exporter.user_is_requestor = true;
    exporter.network_access_point_id = user_ip;
    exporter.network_access_point_type = atna_network_access_type::ip_address;
    exporter.role_id_codes.push_back(atna_role_ids::source);
    msg.active_participants.push_back(std::move(exporter));

    // Destination
    atna_active_participant dest;
    dest.user_id = dest_id;
    dest.user_is_requestor = false;
    dest.role_id_codes.push_back(atna_role_ids::destination);
    msg.active_participants.push_back(std::move(dest));

    msg.audit_source.audit_source_id = source_id;

    // Study object
    if (!study_uid.empty()) {
        atna_participant_object obj;
        obj.object_type = atna_object_type::system_object;
        obj.object_role = atna_object_role::report;
        obj.object_id_type_code = atna_object_id_types::study_instance_uid;
        obj.object_id = study_uid;
        msg.participant_objects.push_back(std::move(obj));
    }

    // Patient object
    if (!patient_id.empty()) {
        atna_participant_object patient_obj;
        patient_obj.object_type = atna_object_type::person;
        patient_obj.object_role = atna_object_role::patient;
        patient_obj.object_id_type_code = atna_object_id_types::patient_number;
        patient_obj.object_id = patient_id;
        msg.participant_objects.push_back(std::move(patient_obj));
    }

    return msg;
}

// =============================================================================
// Private XML Helpers
// =============================================================================

std::string atna_audit_logger::xml_escape(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;";  break;
            case '<':  result += "&lt;";   break;
            case '>':  result += "&gt;";   break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c;        break;
        }
    }

    return result;
}

std::string atna_audit_logger::format_datetime(
    std::chrono::system_clock::time_point tp) {

    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;

    std::tm tm_val{};
#if defined(_WIN32)
    gmtime_s(&tm_val, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm_val);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count()
        << "Z";

    return oss.str();
}

std::string atna_audit_logger::format_coded_value_attrs(
    const atna_coded_value& cv) {

    std::ostringstream oss;
    oss << "csd-code=\"" << xml_escape(cv.code) << "\"";

    if (!cv.code_system_name.empty()) {
        oss << " codeSystemName=\"" << xml_escape(cv.code_system_name) << "\"";
    }

    if (!cv.display_name.empty()) {
        oss << " originalText=\"" << xml_escape(cv.display_name) << "\"";
    }

    return oss.str();
}

}  // namespace pacs::security
