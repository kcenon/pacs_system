// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file dimse_framing.h
 * @brief Splitting DIMSE messages into P-DATA-TF PDUs and reassembling them
 *
 * The association TCP client and v2::dicom_association_handler both carry
 * DIMSE messages over P-DATA-TF PDUs. Encoding and decoding of the PDUs
 * themselves stays in pdu_encoder and pdu_decoder; this file only cuts a
 * message into fragments that fit the peer's maximum PDU length and puts
 * received fragments back together, checking the PS3.8 Annex E rules on
 * the way.
 *
 * @see Issue #1240 - Real TCP client path in association::connect()
 * @see DICOM PS3.8 Section 9.3.5 and Annex E - P-DATA-TF and message fragments
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_NETWORK_DETAIL_DIMSE_FRAMING_HPP
#define PACS_NETWORK_DETAIL_DIMSE_FRAMING_HPP

#include "kcenon/pacs/core/result.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/network/pdu_types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace kcenon::pacs::network::detail {

/// Bytes a PDV adds to its value inside a P-DATA-TF PDU (item length,
/// presentation context ID, message control header)
inline constexpr uint32_t pdv_overhead = 6;

/// Largest PDU length field accepted from a peer whatever was negotiated;
/// pdu_decoder rejects longer PDUs too
inline constexpr uint32_t max_accepted_pdu_length = 16 * 1024 * 1024;

/// PDV value size used when the peer announced no maximum PDU length (0)
inline constexpr uint32_t unlimited_peer_fragment_size = 1024 * 1024;

/// Largest command set accepted from a peer; real command sets are a few
/// hundred bytes
inline constexpr std::size_t max_command_set_length = 64 * 1024;

/**
 * @brief Maximum PDU length both ends can work with.
 *
 * Zero means "no limit" in A-ASSOCIATE (PS3.8 D.1), so it never wins.
 *
 * @param ours The value this end announced
 * @param theirs The value the peer announced
 * @return The smaller non-zero value, or 0 when both are 0
 */
[[nodiscard]] constexpr uint32_t negotiated_max_pdu_length(
    uint32_t ours, uint32_t theirs) noexcept {
    if (ours == 0) {
        return theirs;
    }
    if (theirs == 0) {
        return ours;
    }
    return ours < theirs ? ours : theirs;
}

/**
 * @brief Largest PDU length field to accept for a received P-DATA-TF PDU.
 * @param announced The maximum PDU length this end announced (0 = none)
 * @return @p announced, capped at max_accepted_pdu_length
 */
[[nodiscard]] constexpr uint32_t receive_pdu_limit(uint32_t announced) noexcept {
    return (announced == 0 || announced > max_accepted_pdu_length)
        ? max_accepted_pdu_length
        : announced;
}

/**
 * @brief Encode a DIMSE message as P-DATA-TF PDUs.
 *
 * The command set goes first in command fragments, then the data set (if
 * any) in data fragments; the last fragment of each carries the "last"
 * bit. Every PDU holds one PDV and its length field stays within
 * @p peer_max_pdu_length.
 *
 * @param context_id Accepted presentation context to send on
 * @param msg The message
 * @param transfer_syntax Transfer syntax of the context (for the data set)
 * @param peer_max_pdu_length Maximum PDU length the peer announced (0 = none)
 * @return The PDUs back to back, ready to write, or an error
 */
[[nodiscard]] kcenon::pacs::Result<std::vector<uint8_t>> encode_dimse_pdus(
    uint8_t context_id,
    const dimse::dimse_message& msg,
    const encoding::transfer_syntax& transfer_syntax,
    uint32_t peer_max_pdu_length);

/**
 * @brief Puts PDVs received in P-DATA-TF PDUs back into DIMSE messages.
 *
 * Feed it every PDV in arrival order. It returns a message once the command
 * set and, when the command announces one, the data set are complete. Any
 * PDV that breaks PS3.8 Annex E (unknown or switched presentation context,
 * command fragment after the command set ended, data fragment before it
 * ended or for a command without a data set, oversized command set, or a
 * message that does not decode) is an error, and the caller aborts the
 * association.
 */
class dimse_assembler {
public:
    /// A completed message and the presentation context it arrived on
    using message = std::pair<uint8_t, dimse::dimse_message>;

    /// Transfer syntax of an accepted presentation context, or
    /// std::nullopt when the context was not accepted
    using transfer_syntax_lookup =
        std::function<std::optional<encoding::transfer_syntax>(uint8_t context_id)>;

    /**
     * @brief Construct an assembler.
     * @param lookup Resolves the transfer syntax of a presentation context
     */
    explicit dimse_assembler(transfer_syntax_lookup lookup);

    /**
     * @brief Add the next PDV.
     * @param pdv The PDV as decoded from a P-DATA-TF PDU
     * @return A message when @p pdv completes one, std::nullopt when more
     *         fragments are needed, or an error
     */
    [[nodiscard]] kcenon::pacs::Result<std::optional<message>> add(
        presentation_data_value pdv);

    /**
     * @brief Whether part of a message has arrived.
     */
    [[nodiscard]] bool in_progress() const noexcept;

private:
    /// Forget the message in progress
    void reset() noexcept;

    transfer_syntax_lookup lookup_;
    std::optional<uint8_t> context_id_;
    std::optional<encoding::transfer_syntax> transfer_syntax_;
    std::vector<uint8_t> command_;
    std::vector<uint8_t> dataset_;
    std::optional<dimse::dimse_message> command_only_;
};

}  // namespace kcenon::pacs::network::detail

#endif  // PACS_NETWORK_DETAIL_DIMSE_FRAMING_HPP
