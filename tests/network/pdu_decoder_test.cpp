#include <catch2/catch_test_macros.hpp>

#include "pacs/network/pdu_decoder.hpp"
#include "pacs/network/pdu_encoder.hpp"
#include "pacs/network/pdu_types.hpp"

using namespace pacs::network;

// ============================================================================
// pdu_length Tests
// ============================================================================

TEST_CASE("pdu_decoder pdu_length", "[network][pdu_decoder]") {
    SECTION("returns nullopt for empty buffer") {
        std::vector<uint8_t> empty;
        auto length = pdu_decoder::pdu_length(empty);
        CHECK_FALSE(length.has_value());
    }

    SECTION("returns nullopt for incomplete header") {
        std::vector<uint8_t> partial = {0x01, 0x00, 0x00};
        auto length = pdu_decoder::pdu_length(partial);
        CHECK_FALSE(length.has_value());
    }

    SECTION("returns nullopt when PDU data is incomplete") {
        // Header says length is 100, but we only have 10 bytes total
        std::vector<uint8_t> incomplete = {
            0x01, 0x00,              // Type, Reserved
            0x00, 0x00, 0x00, 0x64,  // Length = 100
            0x00, 0x00, 0x00, 0x00   // Only 4 bytes of data
        };
        auto length = pdu_decoder::pdu_length(incomplete);
        CHECK_FALSE(length.has_value());
    }

    SECTION("returns correct length for complete PDU") {
        auto bytes = pdu_encoder::encode_release_rq();
        auto length = pdu_decoder::pdu_length(bytes);
        REQUIRE(length.has_value());
        CHECK(*length == 10);
    }

    SECTION("returns correct length for A-ASSOCIATE-RQ") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3.4.5";

        auto bytes = pdu_encoder::encode_associate_rq(rq);
        auto length = pdu_decoder::pdu_length(bytes);

        REQUIRE(length.has_value());
        CHECK(*length == bytes.size());
    }
}

// ============================================================================
// peek_pdu_type Tests
// ============================================================================

TEST_CASE("pdu_decoder peek_pdu_type", "[network][pdu_decoder]") {
    SECTION("returns nullopt for empty buffer") {
        std::vector<uint8_t> empty;
        auto type = pdu_decoder::peek_pdu_type(empty);
        CHECK_FALSE(type.has_value());
    }

    SECTION("correctly identifies all PDU types") {
        std::vector<uint8_t> rq = {0x01};
        std::vector<uint8_t> ac = {0x02};
        std::vector<uint8_t> rj = {0x03};
        std::vector<uint8_t> pdata = {0x04};
        std::vector<uint8_t> rel_rq = {0x05};
        std::vector<uint8_t> rel_rp = {0x06};
        std::vector<uint8_t> abort_pdu = {0x07};

        CHECK(pdu_decoder::peek_pdu_type(rq) == pdu_type::associate_rq);
        CHECK(pdu_decoder::peek_pdu_type(ac) == pdu_type::associate_ac);
        CHECK(pdu_decoder::peek_pdu_type(rj) == pdu_type::associate_rj);
        CHECK(pdu_decoder::peek_pdu_type(pdata) == pdu_type::p_data_tf);
        CHECK(pdu_decoder::peek_pdu_type(rel_rq) == pdu_type::release_rq);
        CHECK(pdu_decoder::peek_pdu_type(rel_rp) == pdu_type::release_rp);
        CHECK(pdu_decoder::peek_pdu_type(abort_pdu) == pdu_type::abort);
    }

    SECTION("returns nullopt for invalid PDU type") {
        std::vector<uint8_t> invalid0 = {0x00};
        std::vector<uint8_t> invalid8 = {0x08};
        std::vector<uint8_t> invalidFF = {0xFF};

        CHECK_FALSE(pdu_decoder::peek_pdu_type(invalid0).has_value());
        CHECK_FALSE(pdu_decoder::peek_pdu_type(invalid8).has_value());
        CHECK_FALSE(pdu_decoder::peek_pdu_type(invalidFF).has_value());
    }
}

// ============================================================================
// A-RELEASE-RQ Tests
// ============================================================================

TEST_CASE("pdu_decoder A-RELEASE-RQ", "[network][pdu_decoder]") {
    SECTION("decodes encoded A-RELEASE-RQ") {
        auto bytes = pdu_encoder::encode_release_rq();
        auto result = pdu_decoder::decode_release_rq(bytes);

        REQUIRE(result.is_ok());
    }

    SECTION("decode returns release_rq_pdu variant") {
        auto bytes = pdu_encoder::encode_release_rq();
        auto result = pdu_decoder::decode(bytes);

        REQUIRE(result.is_ok());
        CHECK(std::holds_alternative<release_rq_pdu>(result.value()));
    }

    SECTION("fails on incomplete data") {
        std::vector<uint8_t> incomplete = {0x05, 0x00, 0x00};
        auto result = pdu_decoder::decode_release_rq(incomplete);
        CHECK(result.is_err());
    }
}

// ============================================================================
// A-RELEASE-RP Tests
// ============================================================================

TEST_CASE("pdu_decoder A-RELEASE-RP", "[network][pdu_decoder]") {
    SECTION("decodes encoded A-RELEASE-RP") {
        auto bytes = pdu_encoder::encode_release_rp();
        auto result = pdu_decoder::decode_release_rp(bytes);

        REQUIRE(result.is_ok());
    }

    SECTION("decode returns release_rp_pdu variant") {
        auto bytes = pdu_encoder::encode_release_rp();
        auto result = pdu_decoder::decode(bytes);

        REQUIRE(result.is_ok());
        CHECK(std::holds_alternative<release_rp_pdu>(result.value()));
    }
}

// ============================================================================
// A-ABORT Tests
// ============================================================================

TEST_CASE("pdu_decoder A-ABORT", "[network][pdu_decoder]") {
    SECTION("decodes A-ABORT from service user") {
        auto bytes = pdu_encoder::encode_abort(
            abort_source::service_user, abort_reason::not_specified);
        auto result = pdu_decoder::decode_abort(bytes);

        REQUIRE(result.is_ok());
        CHECK(result.value().source == abort_source::service_user);
        CHECK(result.value().reason == abort_reason::not_specified);
    }

    SECTION("decodes A-ABORT from service provider") {
        auto bytes = pdu_encoder::encode_abort(
            abort_source::service_provider, abort_reason::unexpected_pdu);
        auto result = pdu_decoder::decode_abort(bytes);

        REQUIRE(result.is_ok());
        CHECK(result.value().source == abort_source::service_provider);
        CHECK(result.value().reason == abort_reason::unexpected_pdu);
    }

    SECTION("decode returns abort_pdu variant") {
        auto bytes = pdu_encoder::encode_abort(2, 1);
        auto result = pdu_decoder::decode(bytes);

        REQUIRE(result.is_ok());
        CHECK(std::holds_alternative<abort_pdu>(result.value()));
    }
}

// ============================================================================
// A-ASSOCIATE-RJ Tests
// ============================================================================

TEST_CASE("pdu_decoder A-ASSOCIATE-RJ", "[network][pdu_decoder]") {
    SECTION("decodes A-ASSOCIATE-RJ with permanent rejection") {
        associate_rj rj;
        rj.result = reject_result::rejected_permanent;
        rj.source = static_cast<uint8_t>(reject_source::service_user);
        rj.reason = static_cast<uint8_t>(reject_reason_user::called_ae_not_recognized);

        auto bytes = pdu_encoder::encode_associate_rj(rj);
        auto result = pdu_decoder::decode_associate_rj(bytes);

        REQUIRE(result.is_ok());
        CHECK(result.value().result == reject_result::rejected_permanent);
        CHECK(result.value().source == static_cast<uint8_t>(reject_source::service_user));
        CHECK(result.value().reason == static_cast<uint8_t>(reject_reason_user::called_ae_not_recognized));
    }

    SECTION("decodes A-ASSOCIATE-RJ with transient rejection") {
        associate_rj rj(reject_result::rejected_transient, 3, 1);

        auto bytes = pdu_encoder::encode_associate_rj(rj);
        auto result = pdu_decoder::decode_associate_rj(bytes);

        REQUIRE(result.is_ok());
        CHECK(result.value().result == reject_result::rejected_transient);
        CHECK(result.value().source == 3);
        CHECK(result.value().reason == 1);
    }

    SECTION("decode returns associate_rj variant") {
        associate_rj rj(reject_result::rejected_permanent, 1, 7);
        auto bytes = pdu_encoder::encode_associate_rj(rj);
        auto result = pdu_decoder::decode(bytes);

        REQUIRE(result.is_ok());
        CHECK(std::holds_alternative<associate_rj>(result.value()));
    }
}

// ============================================================================
// P-DATA-TF Tests
// ============================================================================

TEST_CASE("pdu_decoder P-DATA-TF", "[network][pdu_decoder]") {
    SECTION("decodes P-DATA-TF with single PDV") {
        presentation_data_value pdv;
        pdv.context_id = 1;
        pdv.is_command = false;
        pdv.is_last = true;
        pdv.data = {0x00, 0x01, 0x02, 0x03};

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);
        auto result = pdu_decoder::decode_p_data_tf(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().pdvs.size() == 1);

        const auto& decoded_pdv = result.value().pdvs[0];
        CHECK(decoded_pdv.context_id == 1);
        CHECK_FALSE(decoded_pdv.is_command);
        CHECK(decoded_pdv.is_last);
        CHECK(decoded_pdv.data == std::vector<uint8_t>{0x00, 0x01, 0x02, 0x03});
    }

    SECTION("decodes P-DATA-TF with command fragment") {
        presentation_data_value pdv;
        pdv.context_id = 3;
        pdv.is_command = true;
        pdv.is_last = false;
        pdv.data = {0xAA, 0xBB};

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);
        auto result = pdu_decoder::decode_p_data_tf(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().pdvs.size() == 1);

        const auto& decoded_pdv = result.value().pdvs[0];
        CHECK(decoded_pdv.context_id == 3);
        CHECK(decoded_pdv.is_command);
        CHECK_FALSE(decoded_pdv.is_last);
    }

    SECTION("decodes P-DATA-TF with last command fragment") {
        presentation_data_value pdv;
        pdv.context_id = 5;
        pdv.is_command = true;
        pdv.is_last = true;
        pdv.data = {0xFF};

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);
        auto result = pdu_decoder::decode_p_data_tf(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().pdvs.size() == 1);

        const auto& decoded_pdv = result.value().pdvs[0];
        CHECK(decoded_pdv.is_command);
        CHECK(decoded_pdv.is_last);
    }

    SECTION("decodes P-DATA-TF with multiple PDVs") {
        std::vector<presentation_data_value> pdvs;
        pdvs.emplace_back(1, true, false, std::vector<uint8_t>{0x01, 0x02});
        pdvs.emplace_back(1, true, true, std::vector<uint8_t>{0x03, 0x04});
        pdvs.emplace_back(1, false, true, std::vector<uint8_t>{0x05, 0x06, 0x07});

        auto bytes = pdu_encoder::encode_p_data_tf(pdvs);
        auto result = pdu_decoder::decode_p_data_tf(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().pdvs.size() == 3);

        CHECK(result.value().pdvs[0].is_command);
        CHECK_FALSE(result.value().pdvs[0].is_last);
        CHECK(result.value().pdvs[1].is_command);
        CHECK(result.value().pdvs[1].is_last);
        CHECK_FALSE(result.value().pdvs[2].is_command);
        CHECK(result.value().pdvs[2].is_last);
    }

    SECTION("decodes P-DATA-TF with empty data") {
        presentation_data_value pdv;
        pdv.context_id = 1;
        pdv.is_command = true;
        pdv.is_last = true;

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);
        auto result = pdu_decoder::decode_p_data_tf(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().pdvs.size() == 1);
        CHECK(result.value().pdvs[0].data.empty());
    }

    SECTION("decode returns p_data_tf_pdu variant") {
        presentation_data_value pdv(1, false, true, {0x00});
        auto bytes = pdu_encoder::encode_p_data_tf(pdv);
        auto result = pdu_decoder::decode(bytes);

        REQUIRE(result.is_ok());
        CHECK(std::holds_alternative<p_data_tf_pdu>(result.value()));
    }
}

// ============================================================================
// A-ASSOCIATE-RQ Tests
// ============================================================================

TEST_CASE("pdu_decoder A-ASSOCIATE-RQ", "[network][pdu_decoder]") {
    SECTION("decodes minimal A-ASSOCIATE-RQ") {
        associate_rq rq;
        rq.called_ae_title = "PACS_SCP";
        rq.calling_ae_title = "MY_SCU";
        rq.application_context = DICOM_APPLICATION_CONTEXT;
        rq.user_info.max_pdu_length = DEFAULT_MAX_PDU_LENGTH;
        rq.user_info.implementation_class_uid = "1.2.3.4.5";

        auto bytes = pdu_encoder::encode_associate_rq(rq);
        auto result = pdu_decoder::decode_associate_rq(bytes);

        REQUIRE(result.is_ok());
        CHECK(result.value().called_ae_title == "PACS_SCP");
        CHECK(result.value().calling_ae_title == "MY_SCU");
        CHECK(result.value().application_context == DICOM_APPLICATION_CONTEXT);
        CHECK(result.value().user_info.max_pdu_length == DEFAULT_MAX_PDU_LENGTH);
        CHECK(result.value().user_info.implementation_class_uid == "1.2.3.4.5");
    }

    SECTION("decodes A-ASSOCIATE-RQ with presentation contexts") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3.4";

        presentation_context_rq pc;
        pc.id = 1;
        pc.abstract_syntax = "1.2.840.10008.5.1.4.1.1.2";
        pc.transfer_syntaxes.push_back("1.2.840.10008.1.2");
        pc.transfer_syntaxes.push_back("1.2.840.10008.1.2.1");
        rq.presentation_contexts.push_back(pc);

        auto bytes = pdu_encoder::encode_associate_rq(rq);
        auto result = pdu_decoder::decode_associate_rq(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().presentation_contexts.size() == 1);

        const auto& decoded_pc = result.value().presentation_contexts[0];
        CHECK(decoded_pc.id == 1);
        CHECK(decoded_pc.abstract_syntax == "1.2.840.10008.5.1.4.1.1.2");
        REQUIRE(decoded_pc.transfer_syntaxes.size() == 2);
        CHECK(decoded_pc.transfer_syntaxes[0] == "1.2.840.10008.1.2");
        CHECK(decoded_pc.transfer_syntaxes[1] == "1.2.840.10008.1.2.1");
    }

    SECTION("decodes A-ASSOCIATE-RQ with implementation version name") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3.4.5";
        rq.user_info.implementation_version_name = "PACS_V1.0";

        auto bytes = pdu_encoder::encode_associate_rq(rq);
        auto result = pdu_decoder::decode_associate_rq(bytes);

        REQUIRE(result.is_ok());
        CHECK(result.value().user_info.implementation_version_name == "PACS_V1.0");
    }

    SECTION("decodes A-ASSOCIATE-RQ with SCP/SCU role selection") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3";

        scp_scu_role_selection role;
        role.sop_class_uid = "1.2.840.10008.5.1.4.1.1.2";
        role.scu_role = true;
        role.scp_role = false;
        rq.user_info.role_selections.push_back(role);

        auto bytes = pdu_encoder::encode_associate_rq(rq);
        auto result = pdu_decoder::decode_associate_rq(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().user_info.role_selections.size() == 1);

        const auto& decoded_role = result.value().user_info.role_selections[0];
        CHECK(decoded_role.sop_class_uid == "1.2.840.10008.5.1.4.1.1.2");
        CHECK(decoded_role.scu_role);
        CHECK_FALSE(decoded_role.scp_role);
    }

    SECTION("decode returns associate_rq variant") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3";

        auto bytes = pdu_encoder::encode_associate_rq(rq);
        auto result = pdu_decoder::decode(bytes);

        REQUIRE(result.is_ok());
        CHECK(std::holds_alternative<associate_rq>(result.value()));
    }
}

// ============================================================================
// A-ASSOCIATE-AC Tests
// ============================================================================

TEST_CASE("pdu_decoder A-ASSOCIATE-AC", "[network][pdu_decoder]") {
    SECTION("decodes minimal A-ASSOCIATE-AC") {
        associate_ac ac;
        ac.called_ae_title = "PACS_SCP";
        ac.calling_ae_title = "MY_SCU";
        ac.application_context = DICOM_APPLICATION_CONTEXT;
        ac.user_info.max_pdu_length = DEFAULT_MAX_PDU_LENGTH;
        ac.user_info.implementation_class_uid = "1.2.3.4.5";

        auto bytes = pdu_encoder::encode_associate_ac(ac);
        auto result = pdu_decoder::decode_associate_ac(bytes);

        REQUIRE(result.is_ok());
        CHECK(result.value().called_ae_title == "PACS_SCP");
        CHECK(result.value().calling_ae_title == "MY_SCU");
        CHECK(result.value().user_info.max_pdu_length == DEFAULT_MAX_PDU_LENGTH);
    }

    SECTION("decodes A-ASSOCIATE-AC with accepted presentation context") {
        associate_ac ac;
        ac.called_ae_title = "SERVER";
        ac.calling_ae_title = "CLIENT";
        ac.user_info.max_pdu_length = 16384;
        ac.user_info.implementation_class_uid = "1.2.3";

        presentation_context_ac pc;
        pc.id = 1;
        pc.result = presentation_context_result::acceptance;
        pc.transfer_syntax = "1.2.840.10008.1.2";
        ac.presentation_contexts.push_back(pc);

        auto bytes = pdu_encoder::encode_associate_ac(ac);
        auto result = pdu_decoder::decode_associate_ac(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().presentation_contexts.size() == 1);

        const auto& decoded_pc = result.value().presentation_contexts[0];
        CHECK(decoded_pc.id == 1);
        CHECK(decoded_pc.result == presentation_context_result::acceptance);
        CHECK(decoded_pc.transfer_syntax == "1.2.840.10008.1.2");
    }

    SECTION("decodes A-ASSOCIATE-AC with rejected presentation context") {
        associate_ac ac;
        ac.called_ae_title = "SERVER";
        ac.calling_ae_title = "CLIENT";
        ac.user_info.max_pdu_length = 16384;
        ac.user_info.implementation_class_uid = "1.2.3";

        presentation_context_ac pc;
        pc.id = 1;
        pc.result = presentation_context_result::abstract_syntax_not_supported;
        ac.presentation_contexts.push_back(pc);

        auto bytes = pdu_encoder::encode_associate_ac(ac);
        auto result = pdu_decoder::decode_associate_ac(bytes);

        REQUIRE(result.is_ok());
        REQUIRE(result.value().presentation_contexts.size() == 1);
        CHECK(result.value().presentation_contexts[0].result ==
              presentation_context_result::abstract_syntax_not_supported);
    }

    SECTION("decode returns associate_ac variant") {
        associate_ac ac;
        ac.called_ae_title = "SERVER";
        ac.calling_ae_title = "CLIENT";
        ac.user_info.max_pdu_length = 16384;
        ac.user_info.implementation_class_uid = "1.2.3";

        auto bytes = pdu_encoder::encode_associate_ac(ac);
        auto result = pdu_decoder::decode(bytes);

        REQUIRE(result.is_ok());
        CHECK(std::holds_alternative<associate_ac>(result.value()));
    }
}

// ============================================================================
// Round-trip Tests
// ============================================================================

TEST_CASE("pdu_decoder round-trip", "[network][pdu_decoder]") {
    SECTION("A-ASSOCIATE-RQ round-trip") {
        associate_rq original;
        original.called_ae_title = "TEST_SCP";
        original.calling_ae_title = "TEST_SCU";
        original.user_info.max_pdu_length = 32768;
        original.user_info.implementation_class_uid = "1.2.3.4.5.6.7.8.9";
        original.user_info.implementation_version_name = "TestVersion";

        presentation_context_rq pc;
        pc.id = 1;
        pc.abstract_syntax = "1.2.840.10008.5.1.4.1.1.2";
        pc.transfer_syntaxes = {"1.2.840.10008.1.2", "1.2.840.10008.1.2.1"};
        original.presentation_contexts.push_back(pc);

        auto encoded = pdu_encoder::encode_associate_rq(original);
        auto result = pdu_decoder::decode_associate_rq(encoded);

        REQUIRE(result.is_ok());
        const auto& decoded = result.value();

        CHECK(decoded.called_ae_title == "TEST_SCP");
        CHECK(decoded.calling_ae_title == "TEST_SCU");
        CHECK(decoded.user_info.max_pdu_length == 32768);
        CHECK(decoded.user_info.implementation_class_uid == "1.2.3.4.5.6.7.8.9");
        CHECK(decoded.user_info.implementation_version_name == "TestVersion");
        REQUIRE(decoded.presentation_contexts.size() == 1);
        CHECK(decoded.presentation_contexts[0].id == 1);
    }

    SECTION("A-ASSOCIATE-AC round-trip") {
        associate_ac original;
        original.called_ae_title = "TEST_SCP";
        original.calling_ae_title = "TEST_SCU";
        original.user_info.max_pdu_length = 65536;
        original.user_info.implementation_class_uid = "9.8.7.6.5.4.3.2.1";

        presentation_context_ac pc;
        pc.id = 1;
        pc.result = presentation_context_result::acceptance;
        pc.transfer_syntax = "1.2.840.10008.1.2.1";
        original.presentation_contexts.push_back(pc);

        auto encoded = pdu_encoder::encode_associate_ac(original);
        auto result = pdu_decoder::decode_associate_ac(encoded);

        REQUIRE(result.is_ok());
        const auto& decoded = result.value();

        CHECK(decoded.called_ae_title == "TEST_SCP");
        CHECK(decoded.user_info.max_pdu_length == 65536);
        REQUIRE(decoded.presentation_contexts.size() == 1);
        CHECK(decoded.presentation_contexts[0].transfer_syntax == "1.2.840.10008.1.2.1");
    }

    SECTION("P-DATA-TF round-trip") {
        std::vector<presentation_data_value> original_pdvs;
        original_pdvs.emplace_back(1, true, true,
            std::vector<uint8_t>{0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x01, 0x01});
        original_pdvs.emplace_back(1, false, false,
            std::vector<uint8_t>(1000, 0xAB));  // 1000 bytes of data

        auto encoded = pdu_encoder::encode_p_data_tf(original_pdvs);
        auto result = pdu_decoder::decode_p_data_tf(encoded);

        REQUIRE(result.is_ok());
        const auto& decoded = result.value();

        REQUIRE(decoded.pdvs.size() == 2);
        CHECK(decoded.pdvs[0].context_id == 1);
        CHECK(decoded.pdvs[0].is_command);
        CHECK(decoded.pdvs[0].is_last);
        CHECK(decoded.pdvs[0].data.size() == 8);

        CHECK(decoded.pdvs[1].context_id == 1);
        CHECK_FALSE(decoded.pdvs[1].is_command);
        CHECK_FALSE(decoded.pdvs[1].is_last);
        CHECK(decoded.pdvs[1].data.size() == 1000);
    }
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("pdu_decoder error handling", "[network][pdu_decoder]") {
    SECTION("decode fails on empty buffer") {
        std::vector<uint8_t> empty;
        auto result = pdu_decoder::decode(empty);
        CHECK(result.is_err());
    }

    SECTION("decode fails on invalid PDU type") {
        std::vector<uint8_t> invalid = {
            0x00, 0x00,              // Invalid type
            0x00, 0x00, 0x00, 0x04,  // Length = 4
            0x00, 0x00, 0x00, 0x00   // Data
        };
        auto result = pdu_decoder::decode(invalid);
        CHECK(result.is_err());
    }

    SECTION("decode fails on truncated PDU") {
        // Create valid header but truncate data
        std::vector<uint8_t> truncated = {
            0x01, 0x00,              // Type: A-ASSOCIATE-RQ
            0x00, 0x00, 0x00, 0x64   // Length = 100 (but no data follows)
        };
        auto result = pdu_decoder::decode(truncated);
        CHECK(result.is_err());
    }

    SECTION("decode_p_data_tf fails on malformed PDV") {
        // Create P-DATA-TF with invalid PDV item length
        std::vector<uint8_t> malformed = {
            0x04, 0x00,              // Type: P-DATA-TF
            0x00, 0x00, 0x00, 0x08,  // PDU Length = 8
            0x00, 0x00, 0x00, 0x01,  // PDV item length = 1 (too small, need at least 2)
            0x01,                     // Context ID only, missing control byte
            0x00, 0x00, 0x00          // Padding
        };
        auto result = pdu_decoder::decode_p_data_tf(malformed);
        CHECK(result.is_err());
    }
}

// ============================================================================
// pdu_decode_error to_string Tests
// ============================================================================

TEST_CASE("pdu_decode_error to_string", "[network][pdu_decoder]") {
    CHECK(std::string(to_string(pdu_decode_error::success)) == "Success");
    CHECK(std::string(to_string(pdu_decode_error::incomplete_header)) == "Incomplete PDU header");
    CHECK(std::string(to_string(pdu_decode_error::incomplete_pdu)) == "Incomplete PDU data");
    CHECK(std::string(to_string(pdu_decode_error::invalid_pdu_type)) == "Invalid PDU type");
    CHECK(std::string(to_string(pdu_decode_error::invalid_protocol_version)) == "Invalid protocol version");
    CHECK(std::string(to_string(pdu_decode_error::invalid_item_type)) == "Invalid item type");
    CHECK(std::string(to_string(pdu_decode_error::malformed_pdu)) == "Malformed PDU");
    CHECK(std::string(to_string(pdu_decode_error::buffer_overflow)) == "Buffer overflow");
}
