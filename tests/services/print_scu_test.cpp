/**
 * @file print_scu_test.cpp
 * @brief Unit tests for Print Management SCU service (PS3.4 Annex H)
 */

#include <kcenon/pacs/services/print_scu.h>
#include <kcenon/pacs/network/dimse/command_field.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/status_codes.h>
#include <kcenon/pacs/services/sop_class_registry.h>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;

// ============================================================================
// print_scu Construction Tests
// ============================================================================

TEST_CASE("print_scu construction", "[services][print][scu]") {
    SECTION("default construction succeeds") {
        print_scu scu;
        CHECK(scu.sessions_created() == 0);
        CHECK(scu.film_boxes_created() == 0);
        CHECK(scu.images_set() == 0);
        CHECK(scu.prints_executed() == 0);
        CHECK(scu.printer_queries() == 0);
    }

    SECTION("construction with config succeeds") {
        print_scu_config config;
        config.timeout = std::chrono::milliseconds{60000};
        config.auto_generate_uid = false;

        print_scu scu(config);
        CHECK(scu.sessions_created() == 0);
        CHECK(scu.film_boxes_created() == 0);
    }

    SECTION("construction with nullptr logger succeeds") {
        print_scu scu(nullptr);
        CHECK(scu.sessions_created() == 0);
    }

    SECTION("construction with config and nullptr logger succeeds") {
        print_scu_config config;
        print_scu scu(config, nullptr);
        CHECK(scu.sessions_created() == 0);
    }
}

// ============================================================================
// print_scu_config Tests
// ============================================================================

TEST_CASE("print_scu_config defaults", "[services][print][scu]") {
    print_scu_config config;

    CHECK(config.timeout == std::chrono::milliseconds{30000});
    CHECK(config.auto_generate_uid == true);
}

TEST_CASE("print_scu_config customization", "[services][print][scu]") {
    print_scu_config config;
    config.timeout = std::chrono::milliseconds{60000};
    config.auto_generate_uid = false;

    CHECK(config.timeout == std::chrono::milliseconds{60000});
    CHECK(config.auto_generate_uid == false);
}

// ============================================================================
// print_result Structure Tests
// ============================================================================

TEST_CASE("print_result structure", "[services][print][scu]") {
    print_result result;

    SECTION("default construction") {
        CHECK(result.sop_instance_uid.empty());
        CHECK(result.status == 0);
        CHECK(result.error_comment.empty());
        CHECK(result.elapsed == std::chrono::milliseconds{0});
    }

    SECTION("is_success returns true for status 0x0000") {
        result.status = 0x0000;
        CHECK(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK_FALSE(result.is_error());
    }

    SECTION("is_warning returns true for 0xBxxx status") {
        result.status = 0xB000;
        CHECK_FALSE(result.is_success());
        CHECK(result.is_warning());
        CHECK_FALSE(result.is_error());

        result.status = 0xB123;
        CHECK(result.is_warning());

        result.status = 0xBFFF;
        CHECK(result.is_warning());
    }

    SECTION("is_error returns true for error status codes") {
        result.status = 0xC310;
        CHECK_FALSE(result.is_success());
        CHECK_FALSE(result.is_warning());
        CHECK(result.is_error());

        result.status = 0xA700;
        CHECK(result.is_error());

        result.status = 0x0110;
        CHECK(result.is_error());
    }

    SECTION("can store elapsed time") {
        result.elapsed = std::chrono::milliseconds{250};
        CHECK(result.elapsed.count() == 250);
    }

    SECTION("can store response data") {
        result.response_data.set_string(
            print_tags::printer_status_tag,
            kcenon::pacs::encoding::vr_type::CS,
            "NORMAL");
        CHECK(result.response_data.contains(print_tags::printer_status_tag));
    }
}

// ============================================================================
// print_session_data Structure Tests
// ============================================================================

TEST_CASE("print_session_data structure", "[services][print][scu]") {
    print_session_data data;

    SECTION("default construction") {
        CHECK(data.sop_instance_uid.empty());
        CHECK(data.number_of_copies == 1);
        CHECK(data.print_priority == "MED");
        CHECK(data.medium_type == "BLUE FILM");
        CHECK(data.film_destination == "MAGAZINE");
        CHECK(data.film_session_label.empty());
    }

    SECTION("can be initialized with values") {
        data.sop_instance_uid = "1.2.3.4.5.6";
        data.number_of_copies = 3;
        data.print_priority = "HIGH";
        data.medium_type = "PAPER";
        data.film_destination = "PROCESSOR";
        data.film_session_label = "CHEST_XRAY";

        CHECK(data.sop_instance_uid == "1.2.3.4.5.6");
        CHECK(data.number_of_copies == 3);
        CHECK(data.print_priority == "HIGH");
        CHECK(data.medium_type == "PAPER");
        CHECK(data.film_destination == "PROCESSOR");
        CHECK(data.film_session_label == "CHEST_XRAY");
    }
}

// ============================================================================
// print_film_box_data Structure Tests
// ============================================================================

TEST_CASE("print_film_box_data structure", "[services][print][scu]") {
    print_film_box_data data;

    SECTION("default construction") {
        CHECK(data.image_display_format == "STANDARD\\1,1");
        CHECK(data.film_orientation == "PORTRAIT");
        CHECK(data.film_size_id == "8INX10IN");
        CHECK(data.magnification_type.empty());
        CHECK(data.film_session_uid.empty());
    }

    SECTION("can be initialized with values") {
        data.image_display_format = "STANDARD\\2,2";
        data.film_orientation = "LANDSCAPE";
        data.film_size_id = "14INX17IN";
        data.magnification_type = "BILINEAR";
        data.film_session_uid = "1.2.3.4.5.6";

        CHECK(data.image_display_format == "STANDARD\\2,2");
        CHECK(data.film_orientation == "LANDSCAPE");
        CHECK(data.film_size_id == "14INX17IN");
        CHECK(data.magnification_type == "BILINEAR");
        CHECK(data.film_session_uid == "1.2.3.4.5.6");
    }
}

// ============================================================================
// print_image_data Structure Tests
// ============================================================================

TEST_CASE("print_image_data structure", "[services][print][scu]") {
    print_image_data data;

    SECTION("default construction") {
        CHECK(data.image_position == 0);
    }

    SECTION("can be initialized with values") {
        data.image_position = 3;
        CHECK(data.image_position == 3);
    }
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("print_scu statistics", "[services][print][scu]") {
    print_scu scu;

    SECTION("initial statistics are zero") {
        CHECK(scu.sessions_created() == 0);
        CHECK(scu.film_boxes_created() == 0);
        CHECK(scu.images_set() == 0);
        CHECK(scu.prints_executed() == 0);
        CHECK(scu.printer_queries() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scu.reset_statistics();
        CHECK(scu.sessions_created() == 0);
        CHECK(scu.film_boxes_created() == 0);
        CHECK(scu.images_set() == 0);
        CHECK(scu.prints_executed() == 0);
        CHECK(scu.printer_queries() == 0);
    }
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple print_scu instances are independent", "[services][print][scu]") {
    print_scu scu1;
    print_scu scu2;

    CHECK(scu1.sessions_created() == scu2.sessions_created());
    CHECK(scu1.film_boxes_created() == scu2.film_boxes_created());
    CHECK(scu1.images_set() == scu2.images_set());
    CHECK(scu1.prints_executed() == scu2.prints_executed());
    CHECK(scu1.printer_queries() == scu2.printer_queries());

    scu1.reset_statistics();
    CHECK(scu1.sessions_created() == 0);
    CHECK(scu2.sessions_created() == 0);
}

// ============================================================================
// Print SOP Class UID Constants Tests (from print_scp.hpp, used by SCU)
// ============================================================================

TEST_CASE("print SOP class UID constants accessible from print_scu", "[services][print][scu]") {
    CHECK(basic_film_session_sop_class_uid == "1.2.840.10008.5.1.1.1");
    CHECK(basic_film_box_sop_class_uid == "1.2.840.10008.5.1.1.2");
    CHECK(basic_grayscale_image_box_sop_class_uid == "1.2.840.10008.5.1.1.4");
    CHECK(basic_color_image_box_sop_class_uid == "1.2.840.10008.5.1.1.4.1");
    CHECK(printer_sop_class_uid == "1.2.840.10008.5.1.1.16");
    CHECK(basic_grayscale_print_meta_sop_class_uid == "1.2.840.10008.5.1.1.9");
    CHECK(basic_color_print_meta_sop_class_uid == "1.2.840.10008.5.1.1.18");
}

// ============================================================================
// printer_status Enumeration Tests (from print_scp.hpp, used by SCU)
// ============================================================================

TEST_CASE("printer_status to_string conversion (SCU)", "[services][print][scu]") {
    CHECK(to_string(printer_status::normal) == "NORMAL");
    CHECK(to_string(printer_status::warning) == "WARNING");
    CHECK(to_string(printer_status::failure) == "FAILURE");
}

// ============================================================================
// print_job_status Enumeration Tests (from print_scp.hpp, used by SCU)
// ============================================================================

TEST_CASE("print_job_status values (SCU)", "[services][print][scu]") {
    CHECK(static_cast<int>(print_job_status::pending) == 0);
    CHECK(static_cast<int>(print_job_status::printing) == 1);
    CHECK(static_cast<int>(print_job_status::done) == 2);
    CHECK(static_cast<int>(print_job_status::failure) == 3);
}

// ============================================================================
// Print Tags Tests (from print_scp.hpp, used by SCU)
// ============================================================================

TEST_CASE("print_tags accessible from print_scu", "[services][print][scu]") {
    using namespace print_tags;

    SECTION("film session tags") {
        CHECK(number_of_copies == dicom_tag{0x2000, 0x0010});
        CHECK(print_priority == dicom_tag{0x2000, 0x0020});
        CHECK(medium_type == dicom_tag{0x2000, 0x0030});
        CHECK(film_destination == dicom_tag{0x2000, 0x0040});
        CHECK(film_session_label == dicom_tag{0x2000, 0x0050});
    }

    SECTION("film box tags") {
        CHECK(image_display_format == dicom_tag{0x2010, 0x0010});
        CHECK(film_orientation == dicom_tag{0x2010, 0x0040});
        CHECK(film_size_id == dicom_tag{0x2010, 0x0050});
        CHECK(magnification_type == dicom_tag{0x2010, 0x0060});
        CHECK(referenced_film_session_sequence == dicom_tag{0x2010, 0x0500});
        CHECK(referenced_image_box_sequence == dicom_tag{0x2010, 0x0510});
    }

    SECTION("image box tags") {
        CHECK(image_position == dicom_tag{0x2020, 0x0010});
        CHECK(basic_grayscale_image_sequence == dicom_tag{0x2020, 0x0110});
        CHECK(basic_color_image_sequence == dicom_tag{0x2020, 0x0111});
    }

    SECTION("printer tags") {
        CHECK(printer_status_tag == dicom_tag{0x2110, 0x0010});
        CHECK(printer_status_info == dicom_tag{0x2110, 0x0020});
        CHECK(printer_name == dicom_tag{0x2110, 0x0030});
    }
}

// ============================================================================
// DIMSE-N Command Field Tests (Print-related)
// ============================================================================

TEST_CASE("Print-related DIMSE-N command fields (SCU)", "[services][print][scu]") {
    SECTION("N-CREATE command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_create_rq) == 0x0140);
        CHECK(static_cast<uint16_t>(command_field::n_create_rsp) == 0x8140);
    }

    SECTION("N-SET command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_set_rq) == 0x0120);
        CHECK(static_cast<uint16_t>(command_field::n_set_rsp) == 0x8120);
    }

    SECTION("N-GET command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_get_rq) == 0x0110);
        CHECK(static_cast<uint16_t>(command_field::n_get_rsp) == 0x8110);
    }

    SECTION("N-ACTION command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_action_rq) == 0x0130);
        CHECK(static_cast<uint16_t>(command_field::n_action_rsp) == 0x8130);
    }

    SECTION("N-DELETE command fields") {
        CHECK(static_cast<uint16_t>(command_field::n_delete_rq) == 0x0150);
        CHECK(static_cast<uint16_t>(command_field::n_delete_rsp) == 0x8150);
    }
}

// ============================================================================
// Data Structure Copy and Move Tests
// ============================================================================

TEST_CASE("print_session_data is copyable and movable", "[services][print][scu]") {
    print_session_data data;
    data.sop_instance_uid = "1.2.3.4.5.6";
    data.number_of_copies = 2;
    data.print_priority = "HIGH";

    SECTION("copy construction") {
        print_session_data copy = data;
        CHECK(copy.sop_instance_uid == "1.2.3.4.5.6");
        CHECK(copy.number_of_copies == 2);
        CHECK(copy.print_priority == "HIGH");
    }

    SECTION("move construction") {
        print_session_data moved = std::move(data);
        CHECK(moved.sop_instance_uid == "1.2.3.4.5.6");
        CHECK(moved.number_of_copies == 2);
        CHECK(moved.print_priority == "HIGH");
    }
}

TEST_CASE("print_film_box_data is copyable and movable", "[services][print][scu]") {
    print_film_box_data data;
    data.image_display_format = "STANDARD\\2,2";
    data.film_orientation = "LANDSCAPE";
    data.film_session_uid = "1.2.3.4.5.6";

    SECTION("copy construction") {
        print_film_box_data copy = data;
        CHECK(copy.image_display_format == "STANDARD\\2,2");
        CHECK(copy.film_orientation == "LANDSCAPE");
        CHECK(copy.film_session_uid == "1.2.3.4.5.6");
    }

    SECTION("move construction") {
        print_film_box_data moved = std::move(data);
        CHECK(moved.image_display_format == "STANDARD\\2,2");
        CHECK(moved.film_orientation == "LANDSCAPE");
        CHECK(moved.film_session_uid == "1.2.3.4.5.6");
    }
}

TEST_CASE("print_result is copyable and movable", "[services][print][scu]") {
    print_result result;
    result.sop_instance_uid = "1.2.3.4.5.6.7.8";
    result.status = 0x0000;
    result.elapsed = std::chrono::milliseconds{100};

    SECTION("copy construction") {
        print_result copy = result;
        CHECK(copy.sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(copy.status == 0x0000);
        CHECK(copy.elapsed.count() == 100);
        CHECK(copy.is_success());
    }

    SECTION("move construction") {
        print_result moved = std::move(result);
        CHECK(moved.sop_instance_uid == "1.2.3.4.5.6.7.8");
        CHECK(moved.status == 0x0000);
        CHECK(moved.elapsed.count() == 100);
        CHECK(moved.is_success());
    }
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("print SOP classes are registered (SCU view)", "[services][print][scu]") {
    auto& registry = sop_class_registry::instance();

    CHECK(registry.is_supported(basic_film_session_sop_class_uid));
    CHECK(registry.is_supported(basic_film_box_sop_class_uid));
    CHECK(registry.is_supported(basic_grayscale_image_box_sop_class_uid));
    CHECK(registry.is_supported(basic_color_image_box_sop_class_uid));
    CHECK(registry.is_supported(printer_sop_class_uid));
    CHECK(registry.is_supported(basic_grayscale_print_meta_sop_class_uid));
    CHECK(registry.is_supported(basic_color_print_meta_sop_class_uid));

    auto print_classes = registry.get_by_category(sop_class_category::print);
    CHECK(print_classes.size() == 7);
}
