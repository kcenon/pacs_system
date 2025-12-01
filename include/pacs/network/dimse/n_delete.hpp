/**
 * @file n_delete.hpp
 * @brief N-DELETE DIMSE service
 *
 * N-DELETE is used to delete a managed SOP Instance. It is primarily used
 * for Print Management to delete print jobs and related objects.
 *
 * @see DICOM PS3.7 Section 10.1.6 - N-DELETE Service
 *
 * ## Usage Example
 * @code
 * // SCU requests deletion of a print job
 * auto rq = make_n_delete_rq(1, print_job_sop_class_uid, job_instance_uid);
 *
 * // SCP responds
 * auto rsp = make_n_delete_rsp(1, print_job_sop_class_uid, job_instance_uid, status_success);
 * @endcode
 *
 * ## Command Elements
 * | Tag | Keyword | Request | Response |
 * |-----|---------|---------|----------|
 * | (0000,0003) | RequestedSOPClassUID | M | - |
 * | (0000,0002) | AffectedSOPClassUID | - | M |
 * | (0000,0100) | CommandField | M (0x0150/0x8150) | M |
 * | (0000,0110) | MessageID | M | - |
 * | (0000,0120) | MessageIDBeingRespondedTo | - | M |
 * | (0000,0800) | CommandDataSetType | M (0x0101) | M (0x0101) |
 * | (0000,0900) | Status | - | M |
 * | (0000,1001) | RequestedSOPInstanceUID | M | - |
 * | (0000,1000) | AffectedSOPInstanceUID | - | M |
 *
 * M = Mandatory
 *
 * Note: N-DELETE has no data set (CommandDataSetType = 0x0101)
 */

#ifndef PACS_NETWORK_DIMSE_N_DELETE_HPP
#define PACS_NETWORK_DIMSE_N_DELETE_HPP

#include "dimse_message.hpp"

namespace pacs::network::dimse {

// N-DELETE factory functions are declared in dimse_message.hpp:
// - make_n_delete_rq()
// - make_n_delete_rsp()

}  // namespace pacs::network::dimse

#endif  // PACS_NETWORK_DIMSE_N_DELETE_HPP
