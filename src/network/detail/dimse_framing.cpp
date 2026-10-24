// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file dimse_framing.cpp
 * @brief Implementation of DIMSE fragmentation and reassembly over P-DATA-TF
 */

#include "kcenon/pacs/network/detail/dimse_framing.h"
#include "kcenon/pacs/network/pdu_encoder.h"

#include <algorithm>
#include <span>
#include <string>

namespace kcenon::pacs::network::detail {

namespace {

using kcenon::pacs::error_info;
using kcenon::pacs::error_codes::pdu_decoding_error;
using kcenon::pacs::error_codes::pdu_encoding_error;

/// Append @p data to @p out as PDVs of at most @p fragment_size bytes, one
/// P-DATA-TF PDU each; an empty @p data still yields one (empty) last PDV
void append_fragments(std::vector<uint8_t>& out,
                      uint8_t context_id,
                      bool is_command,
                      std::span<const uint8_t> data,
                      std::size_t fragment_size) {
    std::size_t offset = 0;
    do {
        const std::size_t length = (std::min)(fragment_size, data.size() - offset);
        const bool is_last = offset + length == data.size();
        presentation_data_value pdv(
            context_id, is_command, is_last,
            std::vector<uint8_t>(data.begin() + static_cast<std::ptrdiff_t>(offset),
                                 data.begin() + static_cast<std::ptrdiff_t>(offset + length)));
        const auto pdu = pdu_encoder::encode_p_data_tf(pdv);
        out.insert(out.end(), pdu.begin(), pdu.end());
        offset += length;
    } while (offset < data.size());
}

error_info protocol_error(const std::string& message) {
    return error_info{pdu_decoding_error, message, "network"};
}

}  // namespace

// =============================================================================
// Encoding
// =============================================================================

kcenon::pacs::Result<std::vector<uint8_t>> encode_dimse_pdus(
    uint8_t context_id,
    const dimse::dimse_message& msg,
    const encoding::transfer_syntax& transfer_syntax,
    uint32_t peer_max_pdu_length) {

    if (peer_max_pdu_length != 0 && peer_max_pdu_length <= pdv_overhead) {
        return error_info{pdu_encoding_error,
            "Peer maximum PDU length " + std::to_string(peer_max_pdu_length) +
            " leaves no room for PDV data", "network"};
    }

    auto encoded = dimse::dimse_message::encode(msg, transfer_syntax);
    if (encoded.is_err()) {
        return error_info{pdu_encoding_error,
            "Cannot encode DIMSE message: " + encoded.error().message, "network"};
    }
    const auto& [command, dataset] = encoded.value();

    const std::size_t fragment_size = peer_max_pdu_length == 0
        ? unlimited_peer_fragment_size
        : peer_max_pdu_length - pdv_overhead;

    std::vector<uint8_t> out;
    append_fragments(out, context_id, true, command, fragment_size);
    if (msg.has_dataset()) {
        append_fragments(out, context_id, false, dataset, fragment_size);
    }
    return out;
}

// =============================================================================
// Reassembly
// =============================================================================

dimse_assembler::dimse_assembler(transfer_syntax_lookup lookup)
    : lookup_(std::move(lookup)) {}

kcenon::pacs::Result<std::optional<dimse_assembler::message>> dimse_assembler::add(
    presentation_data_value pdv) {

    if (context_id_ && *context_id_ != pdv.context_id) {
        return protocol_error(
            "PDV on presentation context " + std::to_string(pdv.context_id) +
            " inside a message on context " + std::to_string(*context_id_));
    }

    if (!context_id_) {
        std::optional<encoding::transfer_syntax> ts;
        if (lookup_) {
            ts = lookup_(pdv.context_id);
        }
        if (!ts) {
            return protocol_error(
                "PDV on presentation context " + std::to_string(pdv.context_id) +
                ", which was not accepted");
        }
        context_id_ = pdv.context_id;
        transfer_syntax_ = std::move(ts);
    }

    if (pdv.is_command) {
        if (command_only_) {
            return protocol_error("Command fragment after the command set ended");
        }
        if (pdv.data.size() > max_command_set_length - command_.size()) {
            return protocol_error("Command set exceeds " +
                std::to_string(max_command_set_length) + " bytes");
        }
        command_.insert(command_.end(), pdv.data.begin(), pdv.data.end());
        if (!pdv.is_last) {
            return std::optional<message>{};
        }

        auto decoded = dimse::dimse_message::decode(command_, {}, *transfer_syntax_);
        if (decoded.is_err()) {
            return protocol_error("Cannot decode command set: " + decoded.error().message);
        }
        const auto data_set_type = decoded.value().command_set().get_numeric<uint16_t>(
            dimse::tag_command_data_set_type);
        if (!data_set_type || *data_set_type == dimse::command_data_set_type_null) {
            message completed{*context_id_, std::move(decoded.value())};
            reset();
            return std::optional<message>{std::move(completed)};
        }
        // A data set follows on the same presentation context
        command_only_ = std::move(decoded.value());
        return std::optional<message>{};
    }

    if (!command_only_) {
        return protocol_error(command_.empty()
            ? "Data set fragment before any command fragment"
            : "Data set fragment before the command set ended");
    }
    dataset_.insert(dataset_.end(), pdv.data.begin(), pdv.data.end());
    if (!pdv.is_last) {
        return std::optional<message>{};
    }

    auto decoded = dimse::dimse_message::decode(command_, dataset_, *transfer_syntax_);
    if (decoded.is_err()) {
        return protocol_error("Cannot decode DIMSE message: " + decoded.error().message);
    }
    message completed{*context_id_, std::move(decoded.value())};
    reset();
    return std::optional<message>{std::move(completed)};
}

bool dimse_assembler::in_progress() const noexcept {
    return context_id_.has_value();
}

void dimse_assembler::reset() noexcept {
    context_id_.reset();
    transfer_syntax_.reset();
    command_.clear();
    // Release the data set buffer: a large image must not stay allocated
    // for the rest of the association
    std::vector<uint8_t>{}.swap(dataset_);
    command_only_.reset();
}

}  // namespace kcenon::pacs::network::detail
