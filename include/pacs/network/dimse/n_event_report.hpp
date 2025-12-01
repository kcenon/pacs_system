/**
 * @file n_event_report.hpp
 * @brief N-EVENT-REPORT DIMSE service
 *
 * N-EVENT-REPORT is used to report an event that has occurred on a managed
 * SOP Instance. It is primarily used for Storage Commitment to report the
 * success or failure of committing images to permanent storage.
 *
 * @see DICOM PS3.7 Section 10.1.1 - N-EVENT-REPORT Service
 *
 * ## Usage Example
 * @code
 * // SCP reports storage commitment result
 * auto rq = make_n_event_report_rq(
 *     1,
 *     storage_commitment_sop_class_uid,
 *     transaction_uid,
 *     1  // Event Type ID: Storage Commitment Request Successful
 * );
 * rq.set_dataset(event_information);
 *
 * // SCU acknowledges
 * auto rsp = make_n_event_report_rsp(
 *     1, storage_commitment_sop_class_uid, transaction_uid, 1, status_success);
 * @endcode
 *
 * ## Event Type IDs (Storage Commitment)
 * | ID | Meaning |
 * |----|---------|
 * | 1 | Storage Commitment Request Successful |
 * | 2 | Storage Commitment Request Complete - Failures Exist |
 *
 * ## Command Elements
 * | Tag | Keyword | Request | Response |
 * |-----|---------|---------|----------|
 * | (0000,0002) | AffectedSOPClassUID | M | M |
 * | (0000,0100) | CommandField | M (0x0100/0x8100) | M |
 * | (0000,0110) | MessageID | M | - |
 * | (0000,0120) | MessageIDBeingRespondedTo | - | M |
 * | (0000,0800) | CommandDataSetType | M | M |
 * | (0000,0900) | Status | - | M |
 * | (0000,1000) | AffectedSOPInstanceUID | M | M |
 * | (0000,1002) | EventTypeID | M | C |
 *
 * M = Mandatory, C = Conditional
 */

#ifndef PACS_NETWORK_DIMSE_N_EVENT_REPORT_HPP
#define PACS_NETWORK_DIMSE_N_EVENT_REPORT_HPP

#include "dimse_message.hpp"

namespace pacs::network::dimse {

// N-EVENT-REPORT factory functions are declared in dimse_message.hpp:
// - make_n_event_report_rq()
// - make_n_event_report_rsp()

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_N_EVENT_REPORT_HPP
