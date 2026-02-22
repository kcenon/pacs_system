// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
 * @author kcenon
 * @since 1.0.0
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
