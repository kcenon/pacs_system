/**
 * @file n_create.hpp
 * @brief N-CREATE DIMSE service
 *
 * N-CREATE is used to create a new managed SOP Instance. It is primarily used
 * for normalized SOP Classes such as MPPS (Modality Performed Procedure Step).
 *
 * @see DICOM PS3.7 Section 10.1.5 - N-CREATE Service
 *
 * ## Usage Example
 * @code
 * // SCU creates an MPPS instance
 * auto rq = make_n_create_rq(1, mpps_sop_class_uid, instance_uid);
 * rq.set_dataset(initial_attributes);
 *
 * // SCP responds with created instance
 * auto rsp = make_n_create_rsp(1, mpps_sop_class_uid, instance_uid, status_success);
 * rsp.set_dataset(returned_attributes);
 * @endcode
 *
 * ## Command Elements
 * | Tag | Keyword | Request | Response |
 * |-----|---------|---------|----------|
 * | (0000,0002) | AffectedSOPClassUID | M | M |
 * | (0000,0100) | CommandField | M (0x0140/0x8140) | M |
 * | (0000,0110) | MessageID | M | - |
 * | (0000,0120) | MessageIDBeingRespondedTo | - | M |
 * | (0000,0800) | CommandDataSetType | M | M |
 * | (0000,0900) | Status | - | M |
 * | (0000,1000) | AffectedSOPInstanceUID | U | C |
 *
 * M = Mandatory, U = User option, C = Conditional
 */

#ifndef PACS_NETWORK_DIMSE_N_CREATE_HPP
#define PACS_NETWORK_DIMSE_N_CREATE_HPP

#include "dimse_message.hpp"

namespace pacs::network::dimse {

// N-CREATE factory functions are declared in dimse_message.hpp:
// - make_n_create_rq()
// - make_n_create_rsp()

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_N_CREATE_HPP
