/**
 * @file dimse_framing_test.cpp
 * @brief Unit tests for DIMSE fragmentation into P-DATA-TF and reassembly
 *
 * @see Issue #1240 - Real TCP client path in association::connect()
 */

#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_element.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/network/detail/dimse_framing.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/network/pdu_decoder.h"
#include "kcenon/pacs/network/pdu_encoder.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

using namespace kcenon::pacs;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::detail;

namespace {

constexpr const char* CT_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.2";
constexpr const char* EXPLICIT_VR_LE = "1.2.840.10008.1.2.1";
constexpr const char* SOP_INSTANCE = "1.2.826.0.1.3680043.9.9999.1240.1";
constexpr uint8_t CONTEXT_ID = 1;

encoding::transfer_syntax explicit_le() {
    return encoding::transfer_syntax{EXPLICIT_VR_LE};
}

dimse_assembler make_assembler() {
    return dimse_assembler{[](uint8_t context_id) -> std::optional<encoding::transfer_syntax> {
        if (context_id != CONTEXT_ID) {
            return std::nullopt;
        }
        return explicit_le();
    }};
}

/// A C-STORE request whose data set has @p pixel_bytes bytes of pixel data
dimse::dimse_message make_store_rq(std::size_t pixel_bytes) {
    auto msg = dimse::make_c_store_rq(7, CT_IMAGE_STORAGE, SOP_INSTANCE);

    core::dicom_dataset ds;
    ds.set_string(core::tags::sop_class_uid, encoding::vr_type::UI, CT_IMAGE_STORAGE);
    ds.set_string(core::tags::sop_instance_uid, encoding::vr_type::UI, SOP_INSTANCE);
    ds.set_string(core::tags::patient_id, encoding::vr_type::LO, "FRAGMENT001");
    std::vector<uint8_t> pixels(pixel_bytes);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<uint8_t>(i * 31);
    }
    core::dicom_element pixel_element(core::tags::pixel_data, encoding::vr_type::OB);
    pixel_element.set_value(std::span<const uint8_t>(pixels));
    ds.insert(std::move(pixel_element));
    msg.set_dataset(std::move(ds));
    return msg;
}

/// Split a byte stream of back-to-back PDUs
std::vector<std::vector<uint8_t>> split_pdus(const std::vector<uint8_t>& stream) {
    std::vector<std::vector<uint8_t>> pdus;
    std::size_t offset = 0;
    while (offset < stream.size()) {
        auto rest = std::span<const uint8_t>(stream).subspan(offset);
        auto length = pdu_decoder::pdu_length(rest);
        REQUIRE(length.has_value());
        pdus.emplace_back(rest.begin(), rest.begin() + static_cast<std::ptrdiff_t>(*length));
        offset += *length;
    }
    return pdus;
}

/// Feed every PDV of @p pdus to @p assembler; return the messages it completed
std::vector<dimse_assembler::message> reassemble(
    dimse_assembler& assembler, const std::vector<std::vector<uint8_t>>& pdus) {
    std::vector<dimse_assembler::message> messages;
    for (const auto& pdu : pdus) {
        auto decoded = pdu_decoder::decode_p_data_tf(pdu);
        REQUIRE(decoded.is_ok());
        for (auto& pdv : decoded.value().pdvs) {
            auto added = assembler.add(std::move(pdv));
            REQUIRE(added.is_ok());
            if (added.value()) {
                messages.push_back(std::move(*added.value()));
            }
        }
    }
    return messages;
}

/// Encoded command set of @p msg
std::vector<uint8_t> command_bytes(const dimse::dimse_message& msg) {
    auto encoded = dimse::dimse_message::encode(msg, explicit_le());
    REQUIRE(encoded.is_ok());
    return encoded.value().first;
}

}  // namespace

// =============================================================================
// Maximum PDU Length
// =============================================================================

TEST_CASE("negotiated_max_pdu_length treats 0 as no limit", "[network][framing]") {
    CHECK(negotiated_max_pdu_length(16384, 32768) == 16384);
    CHECK(negotiated_max_pdu_length(65536, 4096) == 4096);
    CHECK(negotiated_max_pdu_length(16384, 0) == 16384);
    CHECK(negotiated_max_pdu_length(0, 32768) == 32768);
    CHECK(negotiated_max_pdu_length(0, 0) == 0);
}

TEST_CASE("receive_pdu_limit caps what a peer may send", "[network][framing]") {
    CHECK(receive_pdu_limit(16384) == 16384);
    CHECK(receive_pdu_limit(0) == max_accepted_pdu_length);
    CHECK(receive_pdu_limit(max_accepted_pdu_length + 1) == max_accepted_pdu_length);
}

// =============================================================================
// Fragmentation
// =============================================================================

TEST_CASE("encode_dimse_pdus puts a small command in one PDU", "[network][framing]") {
    auto echo = dimse::make_c_echo_rq(1);
    auto encoded = encode_dimse_pdus(CONTEXT_ID, echo, explicit_le(), 16384);
    REQUIRE(encoded.is_ok());

    auto pdus = split_pdus(encoded.value());
    REQUIRE(pdus.size() == 1);
    auto decoded = pdu_decoder::decode_p_data_tf(pdus[0]);
    REQUIRE(decoded.is_ok());
    REQUIRE(decoded.value().pdvs.size() == 1);
    CHECK(decoded.value().pdvs[0].context_id == CONTEXT_ID);
    CHECK(decoded.value().pdvs[0].is_command);
    CHECK(decoded.value().pdvs[0].is_last);
}

TEST_CASE("encode_dimse_pdus keeps every PDU within the peer maximum", "[network][framing]") {
    constexpr uint32_t max_pdu = 512;
    auto store = make_store_rq(5000);
    auto encoded = encode_dimse_pdus(CONTEXT_ID, store, explicit_le(), max_pdu);
    REQUIRE(encoded.is_ok());

    auto pdus = split_pdus(encoded.value());
    REQUIRE(pdus.size() > 10);

    bool seen_data = false;
    std::size_t command_last = 0;
    std::size_t data_last = 0;
    for (const auto& pdu : pdus) {
        // PDU length field (PS3.8 9.3.5) never exceeds the peer's maximum
        CHECK(pdu.size() - 6 <= max_pdu);

        auto decoded = pdu_decoder::decode_p_data_tf(pdu);
        REQUIRE(decoded.is_ok());
        REQUIRE(decoded.value().pdvs.size() == 1);
        const auto& pdv = decoded.value().pdvs[0];
        if (pdv.is_command) {
            // Command fragments all come before the data fragments
            CHECK_FALSE(seen_data);
            command_last += pdv.is_last ? 1 : 0;
        } else {
            seen_data = true;
            data_last += pdv.is_last ? 1 : 0;
        }
    }
    CHECK(seen_data);
    CHECK(command_last == 1);
    CHECK(data_last == 1);
}

TEST_CASE("encode_dimse_pdus refuses a peer maximum with no room for data",
          "[network][framing]") {
    auto echo = dimse::make_c_echo_rq(1);
    CHECK(encode_dimse_pdus(CONTEXT_ID, echo, explicit_le(), 6).is_err());
    CHECK(encode_dimse_pdus(CONTEXT_ID, echo, explicit_le(), 7).is_ok());
}

// =============================================================================
// Reassembly
// =============================================================================

TEST_CASE("dimse_assembler restores a fragmented message", "[network][framing]") {
    auto store = make_store_rq(20000);
    auto encoded = encode_dimse_pdus(CONTEXT_ID, store, explicit_le(), 1024);
    REQUIRE(encoded.is_ok());

    auto assembler = make_assembler();
    auto messages = reassemble(assembler, split_pdus(encoded.value()));
    REQUIRE(messages.size() == 1);
    CHECK_FALSE(assembler.in_progress());

    const auto& [context_id, msg] = messages[0];
    CHECK(context_id == CONTEXT_ID);
    CHECK(msg.command() == dimse::command_field::c_store_rq);
    CHECK(msg.message_id() == 7);
    CHECK(msg.affected_sop_instance_uid() == SOP_INSTANCE);
    REQUIRE(msg.has_dataset());

    auto original = store.dataset();
    auto restored = msg.dataset();
    REQUIRE(original.is_ok());
    REQUIRE(restored.is_ok());
    const auto* original_pixels = original.value().get().get(core::tags::pixel_data);
    const auto* restored_pixels = restored.value().get().get(core::tags::pixel_data);
    REQUIRE(original_pixels != nullptr);
    REQUIRE(restored_pixels != nullptr);
    REQUIRE(restored_pixels->length() == original_pixels->length());
    const auto original_bytes = original_pixels->raw_data();
    const auto restored_bytes = restored_pixels->raw_data();
    CHECK(std::equal(original_bytes.begin(), original_bytes.end(), restored_bytes.begin()));
    CHECK(restored.value().get().get_string(core::tags::patient_id) == "FRAGMENT001");
}

TEST_CASE("dimse_assembler returns consecutive messages", "[network][framing]") {
    auto first = encode_dimse_pdus(CONTEXT_ID, dimse::make_c_echo_rq(1), explicit_le(), 64);
    auto second = encode_dimse_pdus(CONTEXT_ID, dimse::make_c_echo_rq(2), explicit_le(), 64);
    REQUIRE(first.is_ok());
    REQUIRE(second.is_ok());
    auto stream = first.value();
    stream.insert(stream.end(), second.value().begin(), second.value().end());

    auto assembler = make_assembler();
    auto messages = reassemble(assembler, split_pdus(stream));
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].second.message_id() == 1);
    CHECK(messages[1].second.message_id() == 2);
}

TEST_CASE("dimse_assembler rejects PDVs that break PS3.8 Annex E", "[network][framing]") {
    const auto echo_command = command_bytes(dimse::make_c_echo_rq(1));
    const auto store_command = command_bytes(make_store_rq(16));

    SECTION("a presentation context that was not accepted") {
        auto assembler = make_assembler();
        CHECK(assembler.add({3, true, true, echo_command}).is_err());
    }

    SECTION("a data fragment before any command fragment") {
        auto assembler = make_assembler();
        CHECK(assembler.add({CONTEXT_ID, false, true, {0x00, 0x01}}).is_err());
    }

    SECTION("a data fragment for a command without a data set") {
        auto assembler = make_assembler();
        auto added = assembler.add({CONTEXT_ID, true, true, echo_command});
        REQUIRE(added.is_ok());
        REQUIRE(added.value().has_value());
        CHECK(assembler.add({CONTEXT_ID, false, true, {0x00, 0x01}}).is_err());
    }

    SECTION("a command fragment after the command set ended") {
        auto assembler = make_assembler();
        auto added = assembler.add({CONTEXT_ID, true, true, store_command});
        REQUIRE(added.is_ok());
        CHECK_FALSE(added.value().has_value());  // waits for the data set
        CHECK(assembler.add({CONTEXT_ID, true, true, store_command}).is_err());
    }

    SECTION("a switch of presentation context inside a message") {
        auto assembler = [] {
            return dimse_assembler{[](uint8_t) -> std::optional<encoding::transfer_syntax> {
                return explicit_le();
            }};
        }();
        std::vector<uint8_t> half(store_command.begin(),
                                  store_command.begin() + 10);
        REQUIRE(assembler.add({CONTEXT_ID, true, false, half}).is_ok());
        CHECK(assembler.add({3, true, true, half}).is_err());
    }

    SECTION("a command set larger than the limit") {
        auto assembler = make_assembler();
        std::vector<uint8_t> chunk(max_command_set_length / 2 + 1, 0);
        REQUIRE(assembler.add({CONTEXT_ID, true, false, chunk}).is_ok());
        CHECK(assembler.add({CONTEXT_ID, true, false, chunk}).is_err());
    }

    SECTION("a command set that does not decode") {
        auto assembler = make_assembler();
        CHECK(assembler.add({CONTEXT_ID, true, true, {0x01, 0x02, 0x03}}).is_err());
    }
}
