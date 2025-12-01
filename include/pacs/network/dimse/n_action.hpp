/**
 * @file n_action.hpp
 * @brief N-ACTION DIMSE service
 *
 * N-ACTION is used to request that an action be performed on a managed
 * SOP Instance. It is primarily used for Storage Commitment to request
 * commitment of images to permanent storage.
 *
 * @see DICOM PS3.7 Section 10.1.4 - N-ACTION Service
 *
 * ## Usage Example
 * @code
 * // SCU requests storage commitment
 * auto rq = make_n_action_rq(
 *     1,
 *     storage_commitment_sop_class_uid,
 *     storage_commitment_sop_instance_uid,
 *     1  // Action Type ID: Request Storage Commitment
 * );
 * rq.set_dataset(action_information);  // Contains list of SOP instances
 *
 * // SCP responds
 * auto rsp = make_n_action_rsp(
 *     1, storage_commitment_sop_class_uid, transaction_uid, 1, status_success);
 * @endcode
 *
 * ## Action Type IDs (Storage Commitment)
 * | ID | Meaning |
 * |----|---------|
 * | 1 | Request Storage Commitment |
 *
 * ## Command Elements
 * | Tag | Keyword | Request | Response |
 * |-----|---------|---------|----------|
 * | (0000,0003) | RequestedSOPClassUID | M | - |
 * | (0000,0002) | AffectedSOPClassUID | - | M |
 * | (0000,0100) | CommandField | M (0x0130/0x8130) | M |
 * | (0000,0110) | MessageID | M | - |
 * | (0000,0120) | MessageIDBeingRespondedTo | - | M |
 * | (0000,0800) | CommandDataSetType | M | M |
 * | (0000,0900) | Status | - | M |
 * | (0000,1001) | RequestedSOPInstanceUID | M | - |
 * | (0000,1000) | AffectedSOPInstanceUID | - | M |
 * | (0000,1008) | ActionTypeID | M | C |
 *
 * M = Mandatory, C = Conditional
 */

#ifndef PACS_NETWORK_DIMSE_N_ACTION_HPP
#define PACS_NETWORK_DIMSE_N_ACTION_HPP

#include "dimse_message.hpp"

namespace pacs::network::dimse {

// N-ACTION factory functions are declared in dimse_message.hpp:
// - make_n_action_rq()
// - make_n_action_rsp()

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_N_ACTION_HPP
