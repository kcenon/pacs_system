#include <catch2/catch_test_macros.hpp>

#include "pacs/network/pdu_encoder.hpp"
#include "pacs/network/pdu_types.hpp"

using namespace pacs::network;

// Helper function to extract 16-bit big-endian value
inline uint16_t read_uint16_be(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
}

// Helper function to extract 32-bit big-endian value
inline uint32_t read_uint32_be(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(
        (data[offset] << 24) |
        (data[offset + 1] << 16) |
        (data[offset + 2] << 8) |
        data[offset + 3]
    );
}

// Helper function to extract string from byte vector
inline std::string read_string(const std::vector<uint8_t>& data,
                                size_t offset, size_t length) {
    return std::string(data.begin() + static_cast<ptrdiff_t>(offset),
                       data.begin() + static_cast<ptrdiff_t>(offset + length));
}

TEST_CASE("pdu_encoder A-ASSOCIATE-RQ", "[network][pdu_encoder]") {
    SECTION("encodes minimal A-ASSOCIATE-RQ PDU") {
        associate_rq rq;
        rq.called_ae_title = "PACS_SCP";
        rq.calling_ae_title = "MY_SCU";
        rq.application_context = DICOM_APPLICATION_CONTEXT;
        rq.user_info.max_pdu_length = DEFAULT_MAX_PDU_LENGTH;
        rq.user_info.implementation_class_uid = "1.2.3.4.5";

        auto bytes = pdu_encoder::encode_associate_rq(rq);

        // Check PDU header
        CHECK(bytes[0] == 0x01);  // Type: A-ASSOCIATE-RQ
        CHECK(bytes[1] == 0x00);  // Reserved

        // Check PDU length is set correctly (should be > 0)
        uint32_t pdu_length = read_uint32_be(bytes, 2);
        CHECK(pdu_length == bytes.size() - 6);

        // Check Protocol Version
        CHECK(read_uint16_be(bytes, 6) == 0x0001);

        // Check Reserved
        CHECK(read_uint16_be(bytes, 8) == 0x0000);

        // Check Called AE Title (16 bytes, space-padded)
        std::string called_ae = read_string(bytes, 10, 16);
        CHECK(called_ae == "PACS_SCP        ");

        // Check Calling AE Title (16 bytes, space-padded)
        std::string calling_ae = read_string(bytes, 26, 16);
        CHECK(calling_ae == "MY_SCU          ");

        // Minimum size: header(6) + version(2) + reserved(2) + AE titles(32) + reserved(32)
        //              + app context item + user info item
        CHECK(bytes.size() > 68);
    }

    SECTION("encodes A-ASSOCIATE-RQ with presentation contexts") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3.4";

        // Add a presentation context
        presentation_context_rq pc;
        pc.id = 1;
        pc.abstract_syntax = "1.2.840.10008.5.1.4.1.1.2";  // CT Image Storage
        pc.transfer_syntaxes.push_back("1.2.840.10008.1.2");  // Implicit VR LE
        pc.transfer_syntaxes.push_back("1.2.840.10008.1.2.1");  // Explicit VR LE
        rq.presentation_contexts.push_back(pc);

        auto bytes = pdu_encoder::encode_associate_rq(rq);

        CHECK(bytes[0] == 0x01);  // Type
        CHECK(bytes.size() > 100);  // Should be larger with presentation context
    }

    SECTION("truncates AE titles longer than 16 characters") {
        associate_rq rq;
        rq.called_ae_title = "THIS_IS_A_VERY_LONG_AE_TITLE";
        rq.calling_ae_title = "ANOTHER_LONG_AE_TITLE_HERE";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3";

        auto bytes = pdu_encoder::encode_associate_rq(rq);

        std::string called_ae = read_string(bytes, 10, 16);
        CHECK(called_ae == "THIS_IS_A_VERY_L");

        std::string calling_ae = read_string(bytes, 26, 16);
        CHECK(calling_ae == "ANOTHER_LONG_AE_");
    }

    SECTION("uses default application context when empty") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        // application_context left empty
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3";

        auto bytes = pdu_encoder::encode_associate_rq(rq);

        // Find Application Context Item (type 0x10) after reserved bytes
        size_t pos = 74;  // After header + version + reserved + AE titles + reserved
        CHECK(bytes[pos] == 0x10);  // Application Context Item type
    }
}

TEST_CASE("pdu_encoder A-ASSOCIATE-AC", "[network][pdu_encoder]") {
    SECTION("encodes minimal A-ASSOCIATE-AC PDU") {
        associate_ac ac;
        ac.called_ae_title = "PACS_SCP";
        ac.calling_ae_title = "MY_SCU";
        ac.application_context = DICOM_APPLICATION_CONTEXT;
        ac.user_info.max_pdu_length = DEFAULT_MAX_PDU_LENGTH;
        ac.user_info.implementation_class_uid = "1.2.3.4.5";

        auto bytes = pdu_encoder::encode_associate_ac(ac);

        CHECK(bytes[0] == 0x02);  // Type: A-ASSOCIATE-AC
        CHECK(bytes[1] == 0x00);  // Reserved

        uint32_t pdu_length = read_uint32_be(bytes, 2);
        CHECK(pdu_length == bytes.size() - 6);
    }

    SECTION("encodes A-ASSOCIATE-AC with accepted presentation context") {
        associate_ac ac;
        ac.called_ae_title = "SERVER";
        ac.calling_ae_title = "CLIENT";
        ac.user_info.max_pdu_length = 16384;
        ac.user_info.implementation_class_uid = "1.2.3";

        presentation_context_ac pc;
        pc.id = 1;
        pc.result = presentation_context_result::acceptance;
        pc.transfer_syntax = "1.2.840.10008.1.2";  // Implicit VR LE
        ac.presentation_contexts.push_back(pc);

        auto bytes = pdu_encoder::encode_associate_ac(ac);

        CHECK(bytes[0] == 0x02);  // Type
        CHECK(bytes.size() > 100);
    }

    SECTION("encodes A-ASSOCIATE-AC with rejected presentation context") {
        associate_ac ac;
        ac.called_ae_title = "SERVER";
        ac.calling_ae_title = "CLIENT";
        ac.user_info.max_pdu_length = 16384;
        ac.user_info.implementation_class_uid = "1.2.3";

        presentation_context_ac pc;
        pc.id = 1;
        pc.result = presentation_context_result::abstract_syntax_not_supported;
        // No transfer syntax for rejected context
        ac.presentation_contexts.push_back(pc);

        auto bytes = pdu_encoder::encode_associate_ac(ac);

        CHECK(bytes[0] == 0x02);  // Type
    }
}

TEST_CASE("pdu_encoder A-ASSOCIATE-RJ", "[network][pdu_encoder]") {
    SECTION("encodes A-ASSOCIATE-RJ PDU with correct size") {
        associate_rj rj;
        rj.result = reject_result::rejected_permanent;
        rj.source = static_cast<uint8_t>(reject_source::service_user);
        rj.reason = static_cast<uint8_t>(reject_reason_user::called_ae_not_recognized);

        auto bytes = pdu_encoder::encode_associate_rj(rj);

        // A-ASSOCIATE-RJ is always 10 bytes
        CHECK(bytes.size() == 10);

        CHECK(bytes[0] == 0x03);  // Type: A-ASSOCIATE-RJ
        CHECK(bytes[1] == 0x00);  // Reserved

        CHECK(read_uint32_be(bytes, 2) == 4);  // PDU Length

        CHECK(bytes[6] == 0x00);  // Reserved
        CHECK(bytes[7] == 0x01);  // Result (rejected-permanent)
        CHECK(bytes[8] == 0x01);  // Source (service-user)
        CHECK(bytes[9] == 0x07);  // Reason (called-AE not recognized)
    }

    SECTION("encodes transient rejection") {
        associate_rj rj(reject_result::rejected_transient, 3, 1);

        auto bytes = pdu_encoder::encode_associate_rj(rj);

        CHECK(bytes[7] == 0x02);  // Result (rejected-transient)
        CHECK(bytes[8] == 0x03);  // Source (service-provider presentation)
        CHECK(bytes[9] == 0x01);  // Reason (temporary congestion)
    }
}

TEST_CASE("pdu_encoder A-RELEASE-RQ", "[network][pdu_encoder]") {
    SECTION("encodes A-RELEASE-RQ PDU") {
        auto bytes = pdu_encoder::encode_release_rq();

        // A-RELEASE-RQ is always 10 bytes
        CHECK(bytes.size() == 10);

        CHECK(bytes[0] == 0x05);  // Type: A-RELEASE-RQ
        CHECK(bytes[1] == 0x00);  // Reserved

        CHECK(read_uint32_be(bytes, 2) == 4);  // PDU Length

        // Reserved 4 bytes
        CHECK(read_uint32_be(bytes, 6) == 0);
    }
}

TEST_CASE("pdu_encoder A-RELEASE-RP", "[network][pdu_encoder]") {
    SECTION("encodes A-RELEASE-RP PDU") {
        auto bytes = pdu_encoder::encode_release_rp();

        // A-RELEASE-RP is always 10 bytes
        CHECK(bytes.size() == 10);

        CHECK(bytes[0] == 0x06);  // Type: A-RELEASE-RP
        CHECK(bytes[1] == 0x00);  // Reserved

        CHECK(read_uint32_be(bytes, 2) == 4);  // PDU Length

        // Reserved 4 bytes
        CHECK(read_uint32_be(bytes, 6) == 0);
    }
}

TEST_CASE("pdu_encoder A-ABORT", "[network][pdu_encoder]") {
    SECTION("encodes A-ABORT PDU with raw values") {
        auto bytes = pdu_encoder::encode_abort(2, 1);

        // A-ABORT is always 10 bytes
        CHECK(bytes.size() == 10);

        CHECK(bytes[0] == 0x07);  // Type: A-ABORT
        CHECK(bytes[1] == 0x00);  // Reserved

        CHECK(read_uint32_be(bytes, 2) == 4);  // PDU Length

        CHECK(bytes[6] == 0x00);  // Reserved
        CHECK(bytes[7] == 0x00);  // Reserved
        CHECK(bytes[8] == 0x02);  // Source
        CHECK(bytes[9] == 0x01);  // Reason
    }

    SECTION("encodes A-ABORT PDU with typed enums") {
        auto bytes = pdu_encoder::encode_abort(
            abort_source::service_provider,
            abort_reason::unexpected_pdu
        );

        CHECK(bytes.size() == 10);
        CHECK(bytes[8] == 0x02);  // Source: service-provider
        CHECK(bytes[9] == 0x02);  // Reason: unexpected PDU
    }

    SECTION("encodes A-ABORT from service user") {
        auto bytes = pdu_encoder::encode_abort(
            abort_source::service_user,
            abort_reason::not_specified
        );

        CHECK(bytes[8] == 0x00);  // Source: service-user
        CHECK(bytes[9] == 0x00);  // Reason: not specified
    }
}

TEST_CASE("pdu_encoder P-DATA-TF", "[network][pdu_encoder]") {
    SECTION("encodes P-DATA-TF with single PDV") {
        presentation_data_value pdv;
        pdv.context_id = 1;
        pdv.is_command = false;
        pdv.is_last = true;
        pdv.data = {0x00, 0x01, 0x02, 0x03};

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);

        CHECK(bytes[0] == 0x04);  // Type: P-DATA-TF
        CHECK(bytes[1] == 0x00);  // Reserved

        // PDU Length
        uint32_t pdu_length = read_uint32_be(bytes, 2);
        CHECK(pdu_length == bytes.size() - 6);

        // PDV Item Length (4 bytes for length field + 1 context_id + 1 control + 4 data)
        uint32_t pdv_length = read_uint32_be(bytes, 6);
        CHECK(pdv_length == 6);  // 1 + 1 + 4

        // Presentation Context ID
        CHECK(bytes[10] == 0x01);

        // Message Control Header: Data (0) + Last (2) = 0x02
        CHECK(bytes[11] == 0x02);

        // Data
        CHECK(bytes[12] == 0x00);
        CHECK(bytes[13] == 0x01);
        CHECK(bytes[14] == 0x02);
        CHECK(bytes[15] == 0x03);
    }

    SECTION("encodes P-DATA-TF with command fragment") {
        presentation_data_value pdv;
        pdv.context_id = 3;
        pdv.is_command = true;
        pdv.is_last = false;
        pdv.data = {0xAA, 0xBB};

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);

        // Message Control Header: Command (1) + Not Last (0) = 0x01
        CHECK(bytes[11] == 0x01);
    }

    SECTION("encodes P-DATA-TF with last command fragment") {
        presentation_data_value pdv;
        pdv.context_id = 5;
        pdv.is_command = true;
        pdv.is_last = true;
        pdv.data = {0xFF};

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);

        // Message Control Header: Command (1) + Last (2) = 0x03
        CHECK(bytes[11] == 0x03);
    }

    SECTION("encodes P-DATA-TF with multiple PDVs") {
        std::vector<presentation_data_value> pdvs;

        pdvs.emplace_back(1, true, false, std::vector<uint8_t>{0x01, 0x02});
        pdvs.emplace_back(1, true, true, std::vector<uint8_t>{0x03, 0x04});
        pdvs.emplace_back(1, false, true, std::vector<uint8_t>{0x05, 0x06, 0x07});

        auto bytes = pdu_encoder::encode_p_data_tf(pdvs);

        CHECK(bytes[0] == 0x04);  // Type

        // Verify PDU length covers all PDVs
        uint32_t pdu_length = read_uint32_be(bytes, 2);
        CHECK(pdu_length == bytes.size() - 6);

        // Expected size:
        // 6 (header) + 3*(4 length + 1 id + 1 control) + 2 + 2 + 3 = 6 + 18 + 7 = 31
        CHECK(bytes.size() == 31);
    }

    SECTION("encodes P-DATA-TF with empty data") {
        presentation_data_value pdv;
        pdv.context_id = 1;
        pdv.is_command = true;
        pdv.is_last = true;
        // Empty data

        auto bytes = pdu_encoder::encode_p_data_tf(pdv);

        CHECK(bytes[0] == 0x04);

        // PDV Item Length should be 2 (context_id + control)
        uint32_t pdv_length = read_uint32_be(bytes, 6);
        CHECK(pdv_length == 2);
    }
}

TEST_CASE("pdu_encoder user information encoding", "[network][pdu_encoder]") {
    SECTION("encodes implementation version name") {
        associate_rq rq;
        rq.called_ae_title = "SERVER";
        rq.calling_ae_title = "CLIENT";
        rq.user_info.max_pdu_length = 16384;
        rq.user_info.implementation_class_uid = "1.2.3.4.5";
        rq.user_info.implementation_version_name = "PACS_V1.0";

        auto bytes = pdu_encoder::encode_associate_rq(rq);

        CHECK(bytes[0] == 0x01);  // Type

        // Should contain implementation version name item type (0x55)
        bool found_version_name = false;
        for (size_t i = 74; i < bytes.size() - 1; ++i) {
            if (bytes[i] == 0x55) {
                found_version_name = true;
                break;
            }
        }
        CHECK(found_version_name);
    }

    SECTION("encodes SCP/SCU role selection") {
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

        // Should contain role selection item type (0x54)
        bool found_role = false;
        for (size_t i = 74; i < bytes.size() - 1; ++i) {
            if (bytes[i] == 0x54) {
                found_role = true;
                break;
            }
        }
        CHECK(found_role);
    }
}

TEST_CASE("pdu_encoder pdu_type conversion", "[network][pdu_types]") {
    SECTION("to_string returns correct names") {
        CHECK(std::string(to_string(pdu_type::associate_rq)) == "A-ASSOCIATE-RQ");
        CHECK(std::string(to_string(pdu_type::associate_ac)) == "A-ASSOCIATE-AC");
        CHECK(std::string(to_string(pdu_type::associate_rj)) == "A-ASSOCIATE-RJ");
        CHECK(std::string(to_string(pdu_type::p_data_tf)) == "P-DATA-TF");
        CHECK(std::string(to_string(pdu_type::release_rq)) == "A-RELEASE-RQ");
        CHECK(std::string(to_string(pdu_type::release_rp)) == "A-RELEASE-RP");
        CHECK(std::string(to_string(pdu_type::abort)) == "A-ABORT");
    }

    SECTION("PDU types have correct byte values") {
        CHECK(static_cast<uint8_t>(pdu_type::associate_rq) == 0x01);
        CHECK(static_cast<uint8_t>(pdu_type::associate_ac) == 0x02);
        CHECK(static_cast<uint8_t>(pdu_type::associate_rj) == 0x03);
        CHECK(static_cast<uint8_t>(pdu_type::p_data_tf) == 0x04);
        CHECK(static_cast<uint8_t>(pdu_type::release_rq) == 0x05);
        CHECK(static_cast<uint8_t>(pdu_type::release_rp) == 0x06);
        CHECK(static_cast<uint8_t>(pdu_type::abort) == 0x07);
    }
}

TEST_CASE("pdu_encoder constants", "[network][pdu_types]") {
    SECTION("DICOM application context is correct") {
        CHECK(std::string(DICOM_APPLICATION_CONTEXT) == "1.2.840.10008.3.1.1.1");
    }

    SECTION("Protocol version is 1") {
        CHECK(DICOM_PROTOCOL_VERSION == 0x0001);
    }

    SECTION("AE title length is 16") {
        CHECK(AE_TITLE_LENGTH == 16);
    }

    SECTION("Default max PDU length is 16384") {
        CHECK(DEFAULT_MAX_PDU_LENGTH == 16384);
    }

    SECTION("Unlimited max PDU length is 0") {
        CHECK(UNLIMITED_MAX_PDU_LENGTH == 0);
    }
}
