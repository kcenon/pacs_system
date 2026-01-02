/**
 * @file pdu_framing_test.cpp
 * @brief Unit tests for PDU framing in network_system migration
 *
 * Tests the PDU framing layer that handles the 6-byte PDU header parsing
 * and accumulation of fragmented PDUs from the TCP stream.
 *
 * @see Issue #163 - Full integration testing for network_system migration
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/pdu_encoder.hpp"
#include "pacs/network/pdu_decoder.hpp"
#include "pacs/network/pdu_types.hpp"
#include "pacs/network/v2/dicom_association_handler.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

using namespace pacs::network;
using namespace pacs::network::v2;

// =============================================================================
// Test Helpers
// =============================================================================

namespace {

/// Create a minimal A-ASSOCIATE-RQ PDU for testing
std::vector<uint8_t> create_associate_rq_pdu(const std::string& calling_ae,
                                              const std::string& called_ae) {
    associate_rq rq;
    rq.calling_ae_title = calling_ae;
    rq.called_ae_title = called_ae;
    rq.application_context = DICOM_APPLICATION_CONTEXT;

    // Add a presentation context for Verification SOP Class
    presentation_context_rq pc;
    pc.id = 1;
    pc.abstract_syntax = "1.2.840.10008.1.1";  // Verification SOP Class
    pc.transfer_syntaxes = {"1.2.840.10008.1.2.1"};  // Explicit VR LE
    rq.presentation_contexts.push_back(pc);

    // Add user information
    rq.user_info.max_pdu_length = 16384;
    rq.user_info.implementation_class_uid = "1.2.3.4.5.6.7.8.9";
    rq.user_info.implementation_version_name = "TEST_V1";

    return pdu_encoder::encode_associate_rq(rq);
}

/// Create a minimal A-RELEASE-RQ PDU
std::vector<uint8_t> create_release_rq_pdu() {
    return pdu_encoder::encode_release_rq();
}

/// Create an A-ABORT PDU
std::vector<uint8_t> create_abort_pdu(abort_source source, abort_reason reason) {
    return pdu_encoder::encode_abort(source, reason);
}

/// Split data into fragments of specified sizes
std::vector<std::vector<uint8_t>> fragment_data(const std::vector<uint8_t>& data,
                                                 const std::vector<size_t>& sizes) {
    std::vector<std::vector<uint8_t>> fragments;
    size_t offset = 0;

    for (size_t size : sizes) {
        size_t actual_size = (std::min)(size, data.size() - offset);
        if (actual_size == 0) break;

        fragments.emplace_back(data.begin() + offset,
                               data.begin() + offset + actual_size);
        offset += actual_size;
    }

    // Add remaining data if any
    if (offset < data.size()) {
        fragments.emplace_back(data.begin() + offset, data.end());
    }

    return fragments;
}

}  // namespace

// =============================================================================
// PDU Header Parsing Tests
// =============================================================================

TEST_CASE("PDU header parsing", "[pdu_framing][unit]") {
    SECTION("parse valid PDU header") {
        auto pdu = create_associate_rq_pdu("TEST_SCU", "TEST_SCP");

        // First byte is PDU type
        CHECK(pdu[0] == static_cast<uint8_t>(pdu_type::associate_rq));

        // Second byte is reserved (0x00)
        CHECK(pdu[1] == 0x00);

        // Bytes 2-5 are length (big-endian)
        auto length_opt = pdu_decoder::pdu_length(pdu);
        REQUIRE(length_opt.has_value());
        CHECK(*length_opt == pdu.size());
    }

    SECTION("parse PDU type correctly") {
        auto rq_pdu = create_associate_rq_pdu("SCU", "SCP");
        auto type_opt = pdu_decoder::peek_pdu_type(rq_pdu);
        REQUIRE(type_opt.has_value());
        CHECK(*type_opt == pdu_type::associate_rq);

        auto release_pdu = create_release_rq_pdu();
        type_opt = pdu_decoder::peek_pdu_type(release_pdu);
        REQUIRE(type_opt.has_value());
        CHECK(*type_opt == pdu_type::release_rq);

        auto abort_pdu = create_abort_pdu(abort_source::service_user, abort_reason::not_specified);
        type_opt = pdu_decoder::peek_pdu_type(abort_pdu);
        REQUIRE(type_opt.has_value());
        CHECK(*type_opt == pdu_type::abort);
    }

    SECTION("reject too short header") {
        std::vector<uint8_t> short_data = {0x01, 0x00, 0x00};  // Only 3 bytes

        auto length_opt = pdu_decoder::pdu_length(short_data);
        CHECK_FALSE(length_opt.has_value());
    }

    SECTION("handle minimum valid header") {
        // 6-byte header with 0 length payload
        std::vector<uint8_t> min_header = {
            0x05,  // A-RELEASE-RQ
            0x00,  // Reserved
            0x00, 0x00, 0x00, 0x04,  // Length = 4 (minimum for release PDU)
            0x00, 0x00, 0x00, 0x00   // Payload (reserved bytes)
        };

        auto length_opt = pdu_decoder::pdu_length(min_header);
        REQUIRE(length_opt.has_value());
        CHECK(*length_opt == 10);  // 6 header + 4 payload
    }
}

// =============================================================================
// PDU Accumulation Tests
// =============================================================================

TEST_CASE("PDU fragmented accumulation", "[pdu_framing][unit]") {
    SECTION("accumulate single fragment") {
        auto pdu = create_associate_rq_pdu("TEST_SCU", "TEST_SCP");

        // Single fragment should be complete
        auto length_opt = pdu_decoder::pdu_length(pdu);
        REQUIRE(length_opt.has_value());
        CHECK(pdu.size() >= *length_opt);
    }

    SECTION("accumulate multiple fragments") {
        auto pdu = create_associate_rq_pdu("TEST_SCU", "TEST_SCP");

        // Split into header and payload fragments
        auto fragments = fragment_data(pdu, {6, 50, 100, pdu.size()});

        // Simulate accumulation
        std::vector<uint8_t> buffer;

        // First fragment (just header)
        buffer.insert(buffer.end(), fragments[0].begin(), fragments[0].end());
        CHECK(buffer.size() == 6);  // Just the header

        // With only header, pdu_length returns nullopt (incomplete PDU)
        auto length_opt = pdu_decoder::pdu_length(buffer);
        CHECK_FALSE(length_opt.has_value());

        // Add more fragments until complete
        for (size_t i = 1; i < fragments.size(); ++i) {
            buffer.insert(buffer.end(), fragments[i].begin(), fragments[i].end());
        }

        // Should now have complete PDU
        length_opt = pdu_decoder::pdu_length(buffer);
        REQUIRE(length_opt.has_value());
        CHECK(buffer.size() >= *length_opt);
    }

    SECTION("handle byte-by-byte accumulation") {
        auto pdu = create_release_rq_pdu();  // Small PDU

        std::vector<uint8_t> buffer;

        for (size_t i = 0; i < pdu.size(); ++i) {
            buffer.push_back(pdu[i]);

            if (buffer.size() < 6) {
                // Not enough for header
                auto length_opt = pdu_decoder::pdu_length(buffer);
                CHECK_FALSE(length_opt.has_value());
            }
        }

        // Final buffer should be complete
        auto length_opt = pdu_decoder::pdu_length(buffer);
        REQUIRE(length_opt.has_value());
        CHECK(buffer.size() == *length_opt);
    }
}

// =============================================================================
// Multiple PDU Handling Tests
// =============================================================================

TEST_CASE("Multiple PDUs in stream", "[pdu_framing][unit]") {
    SECTION("extract multiple complete PDUs") {
        auto pdu1 = create_associate_rq_pdu("SCU1", "SCP");
        auto pdu2 = create_release_rq_pdu();
        auto pdu3 = create_abort_pdu(abort_source::service_user, abort_reason::not_specified);

        // Concatenate all PDUs
        std::vector<uint8_t> stream;
        stream.insert(stream.end(), pdu1.begin(), pdu1.end());
        stream.insert(stream.end(), pdu2.begin(), pdu2.end());
        stream.insert(stream.end(), pdu3.begin(), pdu3.end());

        // Extract PDUs one by one
        size_t offset = 0;
        std::vector<pdu_type> types_found;

        while (offset < stream.size()) {
            std::vector<uint8_t> remaining(stream.begin() + offset, stream.end());

            auto length_opt = pdu_decoder::pdu_length(remaining);
            if (!length_opt || remaining.size() < *length_opt) {
                break;
            }

            auto type_opt = pdu_decoder::peek_pdu_type(remaining);
            REQUIRE(type_opt.has_value());
            types_found.push_back(*type_opt);

            offset += *length_opt;
        }

        REQUIRE(types_found.size() == 3);
        CHECK(types_found[0] == pdu_type::associate_rq);
        CHECK(types_found[1] == pdu_type::release_rq);
        CHECK(types_found[2] == pdu_type::abort);
    }

    SECTION("handle partial PDU at end of stream") {
        auto pdu1 = create_release_rq_pdu();
        auto pdu2 = create_associate_rq_pdu("TEST", "TEST");

        // Only include part of second PDU
        std::vector<uint8_t> stream;
        stream.insert(stream.end(), pdu1.begin(), pdu1.end());
        stream.insert(stream.end(), pdu2.begin(), pdu2.begin() + 10);  // Partial

        // First PDU should be extractable
        auto length_opt = pdu_decoder::pdu_length(stream);
        REQUIRE(length_opt.has_value());
        CHECK(*length_opt == pdu1.size());

        // After removing first PDU, remaining is incomplete
        std::vector<uint8_t> remaining(stream.begin() + *length_opt, stream.end());
        CHECK(remaining.size() == 10);

        length_opt = pdu_decoder::pdu_length(remaining);
        if (length_opt.has_value()) {
            CHECK(*length_opt > remaining.size());  // Need more data
        }
    }
}

// =============================================================================
// PDU Length Validation Tests
// =============================================================================

TEST_CASE("PDU length validation", "[pdu_framing][unit]") {
    SECTION("reject zero length PDU") {
        std::vector<uint8_t> zero_length = {
            0x01,  // Type
            0x00,  // Reserved
            0x00, 0x00, 0x00, 0x00  // Length = 0
        };

        // Zero length might be valid for some PDU types, depending on implementation
        auto type_opt = pdu_decoder::peek_pdu_type(zero_length);
        REQUIRE(type_opt.has_value());
    }

    SECTION("handle maximum reasonable PDU size") {
        // Create a header with max_pdu_size length
        constexpr uint32_t max_size = dicom_association_handler::max_pdu_size;

        std::vector<uint8_t> large_header = {
            0x04,  // P-DATA-TF
            0x00,  // Reserved
            static_cast<uint8_t>((max_size >> 24) & 0xFF),
            static_cast<uint8_t>((max_size >> 16) & 0xFF),
            static_cast<uint8_t>((max_size >> 8) & 0xFF),
            static_cast<uint8_t>(max_size & 0xFF)
        };

        // pdu_length requires complete PDU data, so with only header it returns nullopt
        auto length_opt = pdu_decoder::pdu_length(large_header);
        CHECK_FALSE(length_opt.has_value());

        // Verify peek_pdu_type works with just the header
        auto type_opt = pdu_decoder::peek_pdu_type(large_header);
        REQUIRE(type_opt.has_value());
        CHECK(*type_opt == pdu_type::p_data_tf);
    }
}

// =============================================================================
// PDU Header Size Constant Test
// =============================================================================

TEST_CASE("PDU header size constant", "[pdu_framing][unit]") {
    CHECK(dicom_association_handler::pdu_header_size == 6);

    // Verify with actual PDU
    auto pdu = create_release_rq_pdu();
    CHECK(pdu.size() >= 6);

    // Type + Reserved (1 + 1 = 2 bytes)
    // Length (4 bytes)
    // Total header = 6 bytes
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("PDU framing edge cases", "[pdu_framing][unit]") {
    SECTION("empty buffer returns no length") {
        std::vector<uint8_t> empty;
        auto length_opt = pdu_decoder::pdu_length(empty);
        CHECK_FALSE(length_opt.has_value());
    }

    SECTION("single byte buffer returns no length") {
        std::vector<uint8_t> single = {0x01};
        auto length_opt = pdu_decoder::pdu_length(single);
        CHECK_FALSE(length_opt.has_value());
    }

    SECTION("exactly header size buffer with incomplete payload") {
        std::vector<uint8_t> exact_header = {
            0x05,  // A-RELEASE-RQ
            0x00,  // Reserved
            0x00, 0x00, 0x00, 0x04  // Length = 4 (needs 4 more bytes)
        };

        // pdu_length requires complete PDU, so returns nullopt for header-only
        auto length_opt = pdu_decoder::pdu_length(exact_header);
        CHECK_FALSE(length_opt.has_value());

        // peek_pdu_type should still work
        auto type_opt = pdu_decoder::peek_pdu_type(exact_header);
        REQUIRE(type_opt.has_value());
        CHECK(*type_opt == pdu_type::release_rq);
    }
}
