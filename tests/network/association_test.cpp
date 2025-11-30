/**
 * @file association_test.cpp
 * @brief Unit tests for DICOM Association class
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/network/association.hpp"
#include "pacs/network/pdu_types.hpp"

using namespace pacs::network;

// =============================================================================
// Test Constants
// =============================================================================

namespace {

// Standard DICOM UIDs
constexpr const char* VERIFICATION_SOP_CLASS = "1.2.840.10008.1.1";
constexpr const char* CT_IMAGE_STORAGE = "1.2.840.10008.5.1.4.1.1.2";
constexpr const char* IMPLICIT_VR_LE = "1.2.840.10008.1.2";
constexpr const char* EXPLICIT_VR_LE = "1.2.840.10008.1.2.1";

}  // namespace

// =============================================================================
// association_state Tests
// =============================================================================

TEST_CASE("association_state to_string", "[association][state]") {
    SECTION("all states have string representation") {
        CHECK(std::string(to_string(association_state::idle)) == "Idle (Sta1)");
        CHECK(std::string(to_string(association_state::awaiting_associate_ac)) ==
              "Awaiting A-ASSOCIATE-AC (Sta5)");
        CHECK(std::string(to_string(association_state::awaiting_associate_rq)) ==
              "Awaiting A-ASSOCIATE-RQ (Sta2)");
        CHECK(std::string(to_string(association_state::established)) ==
              "Established (Sta6)");
        CHECK(std::string(to_string(association_state::awaiting_release_rp)) ==
              "Awaiting A-RELEASE-RP (Sta7)");
        CHECK(std::string(to_string(association_state::awaiting_release_rq)) ==
              "Awaiting A-RELEASE-RQ (Sta8)");
        CHECK(std::string(to_string(association_state::released)) == "Released");
        CHECK(std::string(to_string(association_state::aborted)) == "Aborted");
    }
}

// =============================================================================
// association_error Tests
// =============================================================================

TEST_CASE("association_error to_string", "[association][error]") {
    SECTION("all errors have string representation") {
        CHECK(to_string(association_error::success) == "Success");
        CHECK(to_string(association_error::connection_failed) == "Connection failed");
        CHECK(to_string(association_error::association_rejected) == "Association rejected");
        CHECK(to_string(association_error::invalid_state) == "Invalid state for operation");
        CHECK(to_string(association_error::no_acceptable_context) ==
              "No acceptable presentation context");
    }
}

// =============================================================================
// association Construction Tests
// =============================================================================

TEST_CASE("association default construction", "[association][construction]") {
    association assoc;

    SECTION("starts in idle state") {
        CHECK(assoc.state() == association_state::idle);
    }

    SECTION("is not established") {
        CHECK_FALSE(assoc.is_established());
    }

    SECTION("is closed") {
        CHECK(assoc.is_closed());
    }

    SECTION("has default PDU size") {
        CHECK(assoc.max_pdu_size() == DEFAULT_MAX_PDU_LENGTH);
    }
}

TEST_CASE("association move semantics", "[association][construction]") {
    association assoc1;
    assoc1.set_state(association_state::established);

    SECTION("move constructor transfers state") {
        association assoc2(std::move(assoc1));
        CHECK(assoc2.state() == association_state::established);
        CHECK(assoc1.state() == association_state::idle);  // NOLINT
    }

    SECTION("move assignment transfers state") {
        association assoc2;
        assoc2 = std::move(assoc1);
        CHECK(assoc2.state() == association_state::established);
        CHECK(assoc1.state() == association_state::idle);  // NOLINT
    }
}

// =============================================================================
// SCU Connection Tests
// =============================================================================

TEST_CASE("association SCU connect", "[association][scu]") {
    association_config config;
    config.calling_ae_title = "TEST_SCU";
    config.called_ae_title = "TEST_SCP";
    config.max_pdu_length = 32768;
    config.implementation_class_uid = "1.2.3.4.5.6";
    config.implementation_version_name = "TEST_V1";

    config.proposed_contexts.push_back({
        1,
        VERIFICATION_SOP_CLASS,
        {EXPLICIT_VR_LE, IMPLICIT_VR_LE}
    });

    auto result = association::connect("localhost", 11112, config);

    SECTION("returns valid association") {
        REQUIRE(result.is_ok());
        auto& assoc = result.value();

        CHECK(assoc.state() == association_state::awaiting_associate_ac);
        CHECK(assoc.calling_ae() == "TEST_SCU");
        CHECK(assoc.called_ae() == "TEST_SCP");
        CHECK(assoc.max_pdu_size() == 32768);
    }

    SECTION("builds valid A-ASSOCIATE-RQ") {
        REQUIRE(result.is_ok());
        auto& assoc = result.value();

        auto rq = assoc.build_associate_rq();
        CHECK(rq.calling_ae_title == "TEST_SCU");
        CHECK(rq.called_ae_title == "TEST_SCP");
        CHECK(rq.application_context == DICOM_APPLICATION_CONTEXT);
        CHECK(rq.presentation_contexts.size() == 1);
        CHECK(rq.presentation_contexts[0].id == 1);
        CHECK(rq.presentation_contexts[0].abstract_syntax == VERIFICATION_SOP_CLASS);
        CHECK(rq.presentation_contexts[0].transfer_syntaxes.size() == 2);
        CHECK(rq.user_info.max_pdu_length == 32768);
    }
}

// =============================================================================
// SCP Accept Tests
// =============================================================================

TEST_CASE("association SCP accept", "[association][scp]") {
    // Build incoming A-ASSOCIATE-RQ
    associate_rq rq;
    rq.calling_ae_title = "REMOTE_SCU";
    rq.called_ae_title = "MY_SCP";
    rq.application_context = DICOM_APPLICATION_CONTEXT;
    rq.presentation_contexts.push_back({
        1,
        VERIFICATION_SOP_CLASS,
        {EXPLICIT_VR_LE, IMPLICIT_VR_LE}
    });
    rq.presentation_contexts.push_back({
        3,
        CT_IMAGE_STORAGE,
        {EXPLICIT_VR_LE}
    });
    rq.user_info.max_pdu_length = 65536;
    rq.user_info.implementation_class_uid = "1.2.3.4.5.6.7";
    rq.user_info.implementation_version_name = "REMOTE_V1";

    scp_config config;
    config.ae_title = "MY_SCP";
    config.supported_abstract_syntaxes = {
        VERIFICATION_SOP_CLASS,
        CT_IMAGE_STORAGE
    };
    config.supported_transfer_syntaxes = {
        EXPLICIT_VR_LE
    };
    config.max_pdu_length = 32768;
    config.implementation_class_uid = "9.8.7.6.5.4";
    config.implementation_version_name = "SCP_V1";

    auto assoc = association::accept(rq, config);

    SECTION("establishes association") {
        CHECK(assoc.is_established());
        CHECK(assoc.state() == association_state::established);
    }

    SECTION("has correct AE titles") {
        CHECK(assoc.calling_ae() == "REMOTE_SCU");
        CHECK(assoc.called_ae() == "MY_SCP");
    }

    SECTION("negotiates PDU size") {
        CHECK(assoc.max_pdu_size() == 32768);  // min(65536, 32768)
    }

    SECTION("stores remote implementation info") {
        CHECK(assoc.remote_implementation_class() == "1.2.3.4.5.6.7");
        CHECK(assoc.remote_implementation_version() == "REMOTE_V1");
    }

    SECTION("negotiates presentation contexts") {
        CHECK(assoc.has_accepted_context(VERIFICATION_SOP_CLASS));
        CHECK(assoc.has_accepted_context(CT_IMAGE_STORAGE));

        auto pc_id = assoc.accepted_context_id(VERIFICATION_SOP_CLASS);
        REQUIRE(pc_id.has_value());
        CHECK(*pc_id == 1);

        auto& ts = assoc.context_transfer_syntax(1);
        CHECK(ts.uid() == EXPLICIT_VR_LE);
    }

    SECTION("builds valid A-ASSOCIATE-AC") {
        auto ac = assoc.build_associate_ac();
        CHECK(ac.calling_ae_title == "REMOTE_SCU");
        CHECK(ac.called_ae_title == "MY_SCP");
        CHECK(ac.presentation_contexts.size() == 2);

        // Both contexts should be accepted with Explicit VR LE
        for (const auto& pc : ac.presentation_contexts) {
            CHECK(pc.result == presentation_context_result::acceptance);
            CHECK(pc.transfer_syntax == EXPLICIT_VR_LE);
        }
    }
}

TEST_CASE("association SCP rejects unsupported abstract syntax", "[association][scp]") {
    associate_rq rq;
    rq.calling_ae_title = "REMOTE_SCU";
    rq.called_ae_title = "MY_SCP";
    rq.application_context = DICOM_APPLICATION_CONTEXT;
    rq.presentation_contexts.push_back({
        1,
        "1.2.3.4.5.6.UNSUPPORTED",  // Unsupported SOP Class
        {EXPLICIT_VR_LE}
    });
    rq.user_info.max_pdu_length = 16384;

    scp_config config;
    config.ae_title = "MY_SCP";
    config.supported_abstract_syntaxes = {VERIFICATION_SOP_CLASS};
    config.supported_transfer_syntaxes = {EXPLICIT_VR_LE};

    auto assoc = association::accept(rq, config);

    SECTION("does not establish") {
        CHECK_FALSE(assoc.is_established());
    }

    SECTION("accepted contexts list has rejection") {
        auto& contexts = assoc.accepted_contexts();
        REQUIRE(contexts.size() == 1);
        CHECK(contexts[0].result ==
              presentation_context_result::abstract_syntax_not_supported);
    }
}

TEST_CASE("association SCP rejects unsupported transfer syntax", "[association][scp]") {
    associate_rq rq;
    rq.calling_ae_title = "REMOTE_SCU";
    rq.called_ae_title = "MY_SCP";
    rq.application_context = DICOM_APPLICATION_CONTEXT;
    rq.presentation_contexts.push_back({
        1,
        VERIFICATION_SOP_CLASS,
        {"1.2.3.4.5.6.UNSUPPORTED_TS"}  // Unsupported Transfer Syntax
    });
    rq.user_info.max_pdu_length = 16384;

    scp_config config;
    config.ae_title = "MY_SCP";
    config.supported_abstract_syntaxes = {VERIFICATION_SOP_CLASS};
    config.supported_transfer_syntaxes = {EXPLICIT_VR_LE};

    auto assoc = association::accept(rq, config);

    SECTION("does not establish") {
        CHECK_FALSE(assoc.is_established());
    }

    SECTION("accepted contexts list has rejection") {
        auto& contexts = assoc.accepted_contexts();
        REQUIRE(contexts.size() == 1);
        CHECK(contexts[0].result ==
              presentation_context_result::transfer_syntaxes_not_supported);
    }
}

// =============================================================================
// A-ASSOCIATE-AC Processing Tests
// =============================================================================

TEST_CASE("association process A-ASSOCIATE-AC", "[association][negotiation]") {
    association_config config;
    config.calling_ae_title = "TEST_SCU";
    config.called_ae_title = "TEST_SCP";
    config.proposed_contexts.push_back({
        1,
        VERIFICATION_SOP_CLASS,
        {EXPLICIT_VR_LE, IMPLICIT_VR_LE}
    });

    auto result = association::connect("localhost", 104, config);
    REQUIRE(result.is_ok());
    auto& assoc = result.value();

    SECTION("successful negotiation establishes association") {
        associate_ac ac;
        ac.calling_ae_title = "TEST_SCU";
        ac.called_ae_title = "TEST_SCP";
        ac.application_context = DICOM_APPLICATION_CONTEXT;
        ac.presentation_contexts.push_back({
            1,
            presentation_context_result::acceptance,
            EXPLICIT_VR_LE
        });
        ac.user_info.max_pdu_length = 32768;
        ac.user_info.implementation_class_uid = "9.8.7.6.5";

        bool success = assoc.process_associate_ac(ac);
        CHECK(success);
        CHECK(assoc.is_established());
        CHECK(assoc.has_accepted_context(VERIFICATION_SOP_CLASS));
    }

    SECTION("rejected context does not establish") {
        associate_ac ac;
        ac.calling_ae_title = "TEST_SCU";
        ac.called_ae_title = "TEST_SCP";
        ac.presentation_contexts.push_back({
            1,
            presentation_context_result::abstract_syntax_not_supported,
            ""
        });
        ac.user_info.max_pdu_length = 16384;

        bool success = assoc.process_associate_ac(ac);
        CHECK_FALSE(success);
        CHECK_FALSE(assoc.is_established());
    }
}

// =============================================================================
// A-ASSOCIATE-RJ Processing Tests
// =============================================================================

TEST_CASE("association process A-ASSOCIATE-RJ", "[association][rejection]") {
    association_config config;
    config.calling_ae_title = "TEST_SCU";
    config.called_ae_title = "TEST_SCP";

    auto result = association::connect("localhost", 104, config);
    REQUIRE(result.is_ok());
    auto& assoc = result.value();

    associate_rj rj{
        reject_result::rejected_permanent,
        static_cast<uint8_t>(reject_source::service_user),
        static_cast<uint8_t>(reject_reason_user::called_ae_not_recognized)
    };

    assoc.process_associate_rj(rj);

    SECTION("transitions to idle") {
        CHECK(assoc.state() == association_state::idle);
    }

    SECTION("stores rejection info") {
        auto info = assoc.get_rejection_info();
        REQUIRE(info.has_value());
        CHECK(info->result == reject_result::rejected_permanent);
        CHECK(info->source == static_cast<uint8_t>(reject_source::service_user));
    }
}

// =============================================================================
// State Machine Tests
// =============================================================================

TEST_CASE("association state transitions", "[association][state]") {
    association assoc;
    CHECK(assoc.state() == association_state::idle);

    SECTION("idle to awaiting_associate_ac (SCU)") {
        assoc.set_state(association_state::awaiting_associate_ac);
        CHECK(assoc.state() == association_state::awaiting_associate_ac);
    }

    SECTION("established to released") {
        assoc.set_state(association_state::established);
        CHECK(assoc.is_established());

        auto release_result = assoc.release();
        CHECK(release_result.is_ok());
        CHECK(assoc.state() == association_state::released);
        CHECK(assoc.is_closed());
    }

    SECTION("abort from any state") {
        assoc.set_state(association_state::established);
        assoc.abort(0, 0);
        CHECK(assoc.state() == association_state::aborted);
        CHECK(assoc.is_closed());
    }
}

// =============================================================================
// DIMSE Operation Tests
// =============================================================================

TEST_CASE("association DIMSE operations require established state", "[association][dimse]") {
    association assoc;

    SECTION("send_dimse fails when not established") {
        dimse::dimse_message msg{dimse::command_field::c_echo_rq, 1};
        auto result = assoc.send_dimse(1, msg);
        CHECK(result.is_err());
    }

    SECTION("receive_dimse fails when not established") {
        auto result = assoc.receive_dimse();
        CHECK(result.is_err());
    }
}

// =============================================================================
// Release Tests
// =============================================================================

TEST_CASE("association release", "[association][release]") {
    association assoc;
    assoc.set_state(association_state::established);

    SECTION("release succeeds from established state") {
        auto result = assoc.release();
        CHECK(result.is_ok());
        CHECK(assoc.state() == association_state::released);
    }

    SECTION("release fails from idle state") {
        association idle_assoc;
        auto result = idle_assoc.release();
        CHECK(result.is_err());
    }

    SECTION("double release fails") {
        auto first_release = assoc.release();
        CHECK(first_release.is_ok());
        auto result = assoc.release();
        CHECK(result.is_err());
    }
}

// =============================================================================
// Abort Tests
// =============================================================================

TEST_CASE("association abort", "[association][abort]") {
    association assoc;
    assoc.set_state(association_state::established);

    SECTION("abort transitions to aborted state") {
        assoc.abort(
            static_cast<uint8_t>(abort_source::service_user),
            static_cast<uint8_t>(abort_reason::not_specified));

        CHECK(assoc.state() == association_state::aborted);
        CHECK(assoc.is_closed());
    }

    SECTION("process_abort transitions to aborted") {
        assoc.process_abort(abort_source::service_provider, abort_reason::unexpected_pdu);

        CHECK(assoc.state() == association_state::aborted);
    }
}

// =============================================================================
// Presentation Context Tests
// =============================================================================

TEST_CASE("accepted_presentation_context", "[association][presentation_context]") {
    accepted_presentation_context ctx{
        1,
        VERIFICATION_SOP_CLASS,
        EXPLICIT_VR_LE,
        presentation_context_result::acceptance
    };

    SECTION("is_accepted returns true for acceptance") {
        CHECK(ctx.is_accepted());
    }

    SECTION("is_accepted returns false for rejection") {
        ctx.result = presentation_context_result::abstract_syntax_not_supported;
        CHECK_FALSE(ctx.is_accepted());
    }
}

TEST_CASE("context_transfer_syntax throws for invalid ID", "[association][presentation_context]") {
    association assoc;
    CHECK_THROWS_AS(assoc.context_transfer_syntax(99), std::out_of_range);
}

// =============================================================================
// Rejection Factory Tests
// =============================================================================

TEST_CASE("association::reject factory", "[association][rejection]") {
    auto rj = association::reject(
        reject_result::rejected_transient,
        static_cast<uint8_t>(reject_source::service_provider_presentation),
        static_cast<uint8_t>(reject_reason_provider_presentation::temporary_congestion)
    );

    CHECK(rj.result == reject_result::rejected_transient);
    CHECK(rj.source == static_cast<uint8_t>(reject_source::service_provider_presentation));
    CHECK(rj.reason ==
          static_cast<uint8_t>(reject_reason_provider_presentation::temporary_congestion));
}

// =============================================================================
// rejection_info Tests
// =============================================================================

TEST_CASE("rejection_info description", "[association][rejection]") {
    SECTION("service user rejection") {
        rejection_info info{
            reject_result::rejected_permanent,
            static_cast<uint8_t>(reject_source::service_user),
            static_cast<uint8_t>(reject_reason_user::calling_ae_not_recognized)
        };

        CHECK(info.description.find("permanent") != std::string::npos);
        CHECK(info.description.find("service-user") != std::string::npos);
        CHECK(info.description.find("calling AE") != std::string::npos);
    }

    SECTION("service provider ACSE rejection") {
        rejection_info info{
            reject_result::rejected_transient,
            static_cast<uint8_t>(reject_source::service_provider_acse),
            static_cast<uint8_t>(reject_reason_provider_acse::protocol_version_not_supported)
        };

        CHECK(info.description.find("transient") != std::string::npos);
        CHECK(info.description.find("ACSE") != std::string::npos);
        CHECK(info.description.find("protocol version") != std::string::npos);
    }
}
