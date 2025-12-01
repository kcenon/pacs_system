/**
 * @file n_set.hpp
 * @brief N-SET DIMSE service
 *
 * N-SET is used to modify attributes of a managed SOP Instance. It is primarily
 * used for normalized SOP Classes such as MPPS to update procedure step status.
 *
 * @see DICOM PS3.7 Section 10.1.3 - N-SET Service
 *
 * ## Usage Example
 * @code
 * // SCU updates MPPS status to COMPLETED
 * auto rq = make_n_set_rq(2, mpps_sop_class_uid, instance_uid);
 * core::dicom_dataset modification;
 * modification.set_string(tags::performed_procedure_step_status, vr_type::CS, "COMPLETED");
 * rq.set_dataset(std::move(modification));
 *
 * // SCP responds
 * auto rsp = make_n_set_rsp(2, mpps_sop_class_uid, instance_uid, status_success);
 * @endcode
 *
 * ## Command Elements
 * | Tag | Keyword | Request | Response |
 * |-----|---------|---------|----------|
 * | (0000,0003) | RequestedSOPClassUID | M | - |
 * | (0000,0002) | AffectedSOPClassUID | - | M |
 * | (0000,0100) | CommandField | M (0x0120/0x8120) | M |
 * | (0000,0110) | MessageID | M | - |
 * | (0000,0120) | MessageIDBeingRespondedTo | - | M |
 * | (0000,0800) | CommandDataSetType | M | M |
 * | (0000,0900) | Status | - | M |
 * | (0000,1001) | RequestedSOPInstanceUID | M | - |
 * | (0000,1000) | AffectedSOPInstanceUID | - | M |
 *
 * M = Mandatory
 */

#ifndef PACS_NETWORK_DIMSE_N_SET_HPP
#define PACS_NETWORK_DIMSE_N_SET_HPP

#include "dimse_message.hpp"

namespace pacs::network::dimse {

// N-SET factory functions are declared in dimse_message.hpp:
// - make_n_set_rq()
// - make_n_set_rsp()

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_N_SET_HPP
