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
 * @author kcenon
 * @since 1.0.0
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
