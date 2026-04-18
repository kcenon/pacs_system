#include <catch2/catch_test_macros.hpp>

#include "kcenon/pacs/network/pdu_decoder.h"
#include "kcenon/pacs/network/pdu_encoder.h"
#include "kcenon/pacs/network/pdu_types.h"

using namespace kcenon::pacs::network;

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
// Regression Tests: Bounds Checks on Length-Prefixed Reads
// ============================================================================
//
// These tests cover defensive bounds checks added in response to issue #1098.
// Each test constructs a synthetic PDU whose declared length field is
// inconsistent with the bytes that follow (e.g., an item whose declared
// length is smaller than the fixed header the item is supposed to contain,
// or a nested TLV whose stated size would require reading past the outer
// buffer). The decoder must return an error cleanly instead of reading
// out of bounds.
//
// Byte sequences below are constructed by hand from the public DICOM PS3.8
// Upper Layer Protocol format; no fuzzer artifact is embedded.

namespace {

// Helper: build a minimum-sized A-ASSOCIATE-RQ fixed header followed by
// caller-supplied variable-items bytes. The outer PDU length field is set
// to reflect the full body.
std::vector<uint8_t> make_associate_rq_with_items(
    const std::vector<uint8_t>& variable_items) {
    std::vector<uint8_t> buf;
    // Reserve space: 6 (PDU header) + 68 (ASSOCIATE header) + items
    const uint32_t body_len = 68u + static_cast<uint32_t>(variable_items.size());
    buf.reserve(6 + body_len);

    // PDU header: type 0x01, reserved 0x00, 4-byte big-endian length
    buf.push_back(0x01);
    buf.push_back(0x00);
    buf.push_back(static_cast<uint8_t>((body_len >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((body_len >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((body_len >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(body_len & 0xFF));

    // ASSOCIATE fixed header (68 bytes):
    //   2 bytes protocol version (0x0001)
    //   2 bytes reserved
    //   16 bytes called AE title (spaces)
    //   16 bytes calling AE title (spaces)
    //   32 bytes reserved
    buf.push_back(0x00);
    buf.push_back(0x01);        // protocol version = 1
    buf.push_back(0x00);
    buf.push_back(0x00);        // reserved
    for (int i = 0; i < 16; ++i) buf.push_back(' ');   // called AE
    for (int i = 0; i < 16; ++i) buf.push_back(' ');   // calling AE
    for (int i = 0; i < 32; ++i) buf.push_back(0x00);  // reserved

    // Variable items
    buf.insert(buf.end(), variable_items.begin(), variable_items.end());
    return buf;
}

// Same for A-ASSOCIATE-AC (PDU type 0x02).
std::vector<uint8_t> make_associate_ac_with_items(
    const std::vector<uint8_t>& variable_items) {
    auto buf = make_associate_rq_with_items(variable_items);
    buf[0] = 0x02;  // flip type to AC
    return buf;
}

}  // namespace

TEST_CASE("pdu_decoder regression: length-prefixed bounds checks",
          "[network][pdu_decoder][security]") {

    SECTION("presentation context RQ with item_length=0 is rejected cleanly") {
        // Item header declares a presentation-context-RQ (type 0x20) whose
        // length field is 0. Before the fix, the decoder would still read
        // data[pos] as the context id, crossing outside the item body.
        std::vector<uint8_t> items = {
            0x20, 0x00,       // type = PC-RQ, reserved
            0x00, 0x00        // item length = 0 (no body)
        };
        auto bytes = make_associate_rq_with_items(items);
        auto result = pdu_decoder::decode_associate_rq(bytes);
        // The decoder must either return an error or succeed with zero
        // presentation contexts. It must never read past the declared
        // item body. Because decode_variable_items swallows errors inside
        // decode_associate_rq, we accept either outcome here and rely on
        // ASAN/UBSan in CI to flag the invalid read if the bounds check
        // regresses.
        if (result.is_ok()) {
            CHECK(result.value().presentation_contexts.empty());
        } else {
            CHECK(result.is_err());
        }
    }

    SECTION("presentation context RQ with item_length=1 is rejected cleanly") {
        // item_length = 1 leaves no room for the reserved+sub-items portion
        // after the context_id. Decoder must treat the item as malformed
        // rather than indexing past its body.
        std::vector<uint8_t> items = {
            0x20, 0x00,
            0x00, 0x01,
            0x05              // stray byte inside the item body
        };
        auto bytes = make_associate_rq_with_items(items);
        auto result = pdu_decoder::decode_associate_rq(bytes);
        if (result.is_ok()) {
            CHECK(result.value().presentation_contexts.empty());
        } else {
            CHECK(result.is_err());
        }
    }

    SECTION("presentation context AC with item_length=2 is rejected cleanly") {
        // AC body needs 4 bytes (id + reserved + result + reserved) before
        // any sub-items. item_length = 2 leaves data[pos+2] out of the
        // declared body and would cause a read past the TLV bounds.
        std::vector<uint8_t> items = {
            0x21, 0x00,
            0x00, 0x02,
            0x01, 0x00        // only id + one reserved byte inside body
        };
        auto bytes = make_associate_ac_with_items(items);
        auto result = pdu_decoder::decode_associate_ac(bytes);
        if (result.is_ok()) {
            CHECK(result.value().presentation_contexts.empty());
        } else {
            CHECK(result.is_err());
        }
    }

    SECTION("nested sub-item length exceeding outer PC body is ignored") {
        // Construct a presentation-context-RQ whose inner abstract-syntax
        // sub-item declares a length larger than the outer PC body. The
        // decoder should stop processing the inner sub-items without
        // reading past pc_end.
        std::vector<uint8_t> items = {
            0x20, 0x00,
            0x00, 0x08,       // outer PC body = 8 bytes
            0x01,             // context id
            0x00, 0x00, 0x00, // reserved
            // inner TLV: abstract syntax with length = 0xFFFF
            0x30, 0x00,
            0xFF, 0xFF
        };
        auto bytes = make_associate_rq_with_items(items);
        auto result = pdu_decoder::decode_associate_rq(bytes);
        REQUIRE(result.is_ok());
        // Inner sub-item was dropped; PC parsed with no abstract syntax.
        REQUIRE(result.value().presentation_contexts.size() == 1);
        CHECK(result.value().presentation_contexts[0].abstract_syntax.empty());
    }

    SECTION("variable item declared length exceeding outer buffer is rejected") {
        // Outer item declares length 0x40 but only a few bytes follow.
        // decode_variable_items must return buffer_overflow.
        std::vector<uint8_t> items = {
            0x10, 0x00,
            0x00, 0x40,       // item length far beyond what follows
            0x01, 0x02, 0x03
        };
        auto bytes = make_associate_rq_with_items(items);
        // PDU-header length field is consistent (the outer length equals
        // ASSOCIATE_HEADER + items.size()), so validate_pdu_header passes.
        // The inner variable-items decoder should reject the oversized item.
        auto result = pdu_decoder::decode_associate_rq(bytes);
        // decode_associate_rq silently ignores variable-items errors, so the
        // top-level result is ok with no contexts. The important assertion
        // is that no out-of-bounds read occurs (verified under ASAN in CI).
        if (result.is_ok()) {
            CHECK(result.value().presentation_contexts.empty());
            CHECK(result.value().user_info.implementation_class_uid.empty());
        } else {
            CHECK(result.is_err());
        }
    }

    SECTION("P-DATA-TF PDV item length exceeding PDU body is rejected") {
        // PDV item_length = 0x100 but PDU body only has 10 bytes left.
        std::vector<uint8_t> bytes = {
            0x04, 0x00,                   // P-DATA-TF, reserved
            0x00, 0x00, 0x00, 0x0A,       // PDU length = 10
            0x00, 0x00, 0x01, 0x00,       // PDV length = 256 (exceeds body)
            0x01, 0x00,                   // context id + control (partial)
            0xAA, 0xBB, 0xCC, 0xDD
        };
        auto result = pdu_decoder::decode_p_data_tf(bytes);
        CHECK(result.is_err());
    }

    SECTION("P-DATA-TF PDV item length of 1 (< minimum 2) is rejected") {
        // The PDV body must contain at least context_id + control (2 bytes).
        std::vector<uint8_t> bytes = {
            0x04, 0x00,
            0x00, 0x00, 0x00, 0x05,       // PDU length = 5
            0x00, 0x00, 0x00, 0x01,       // PDV length = 1
            0x01                          // stray byte
        };
        auto result = pdu_decoder::decode_p_data_tf(bytes);
        CHECK(result.is_err());
    }

    SECTION("SCP/SCU role selection with oversized UID length is rejected") {
        // User-information sub-item 0x54 where the inner uid_length field
        // is larger than the sub-item's remaining bytes. Encoded path must
        // skip the role rather than read past sub_length.
        //
        // Build: User Info (0x50) containing only a role-selection (0x54)
        // sub-item with uid_length = 0xFFFF but sub_length = 4.
        std::vector<uint8_t> user_info_body = {
            0x54, 0x00,
            0x00, 0x04,       // sub_length = 4 (just enough for the header)
            0xFF, 0xFF,       // uid_length = 65535 (attacker-declared)
            0x01, 0x00        // scu/scp role bytes (logically present but
                              // unreachable given the declared uid_length)
        };
        std::vector<uint8_t> items;
        items.push_back(0x50);
        items.push_back(0x00);
        items.push_back(static_cast<uint8_t>(
            (user_info_body.size() >> 8) & 0xFF));
        items.push_back(static_cast<uint8_t>(user_info_body.size() & 0xFF));
        items.insert(items.end(), user_info_body.begin(), user_info_body.end());

        auto bytes = make_associate_rq_with_items(items);
        auto result = pdu_decoder::decode_associate_rq(bytes);
        REQUIRE(result.is_ok());
        // Role was dropped; no out-of-bounds read occurred.
        CHECK(result.value().user_info.role_selections.empty());
    }

    SECTION("SCP/SCU role selection with odd uid_length near uint16 max") {
        // Guard against uint16_t wrap when computing padded_uid_length
        // (odd uid_length + 1). Even though bounds checks will reject the
        // payload, the padding computation must not overflow and produce
        // a misleadingly small value.
        std::vector<uint8_t> user_info_body = {
            0x54, 0x00,
            0x00, 0x04,       // sub_length = 4
            0xFF, 0xFF        // uid_length = 65535 (odd, max uint16)
            // no role bytes -> declared size 4 is respected
        };
        std::vector<uint8_t> items;
        items.push_back(0x50);
        items.push_back(0x00);
        items.push_back(static_cast<uint8_t>(
            (user_info_body.size() >> 8) & 0xFF));
        items.push_back(static_cast<uint8_t>(user_info_body.size() & 0xFF));
        items.insert(items.end(), user_info_body.begin(), user_info_body.end());

        auto bytes = make_associate_rq_with_items(items);
        auto result = pdu_decoder::decode_associate_rq(bytes);
        REQUIRE(result.is_ok());
        CHECK(result.value().user_info.role_selections.empty());
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
