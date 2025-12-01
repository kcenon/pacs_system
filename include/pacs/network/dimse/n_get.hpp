/**
 * @file n_get.hpp
 * @brief N-GET DIMSE service
 *
 * N-GET is used to retrieve attribute values from a managed SOP Instance.
 * It supports selective retrieval via the Attribute Identifier List.
 *
 * @see DICOM PS3.7 Section 10.1.2 - N-GET Service
 *
 * ## Usage Example
 * @code
 * // SCU requests specific attributes
 * std::vector<core::dicom_tag> tags_to_get = {
 *     tags::patient_name,
 *     tags::patient_id,
 *     tags::performed_procedure_step_status
 * };
 * auto rq = make_n_get_rq(3, sop_class_uid, instance_uid, tags_to_get);
 *
 * // SCP responds with requested attributes
 * auto rsp = make_n_get_rsp(3, sop_class_uid, instance_uid, status_success);
 * rsp.set_dataset(requested_attributes);
 * @endcode
 *
 * ## Command Elements
 * | Tag | Keyword | Request | Response |
 * |-----|---------|---------|----------|
 * | (0000,0003) | RequestedSOPClassUID | M | - |
 * | (0000,0002) | AffectedSOPClassUID | - | M |
 * | (0000,0100) | CommandField | M (0x0110/0x8110) | M |
 * | (0000,0110) | MessageID | M | - |
 * | (0000,0120) | MessageIDBeingRespondedTo | - | M |
 * | (0000,0800) | CommandDataSetType | M | M |
 * | (0000,0900) | Status | - | M |
 * | (0000,1001) | RequestedSOPInstanceUID | M | - |
 * | (0000,1000) | AffectedSOPInstanceUID | - | M |
 * | (0000,1005) | AttributeIdentifierList | U | - |
 *
 * M = Mandatory, U = User option (if empty, all attributes returned)
 */

#ifndef PACS_NETWORK_DIMSE_N_GET_HPP
#define PACS_NETWORK_DIMSE_N_GET_HPP

#include "dimse_message.hpp"

namespace pacs::network::dimse {

// N-GET factory functions are declared in dimse_message.hpp:
// - make_n_get_rq()
// - make_n_get_rsp()

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_N_GET_HPP
