/**
 * @file print_scp_test.cpp
 * @brief Unit tests for Print Management SCP service (PS3.4 Annex H)
 */

#include <pacs/services/print_scp.hpp>
#include <pacs/network/dimse/command_field.hpp>
#include <pacs/network/dimse/dimse_message.hpp>
#include <pacs/network/dimse/status_codes.hpp>
#include <pacs/services/sop_class_registry.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace kcenon::pacs::services;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::core;

// ============================================================================
// print_scp Construction Tests
// ============================================================================

TEST_CASE("print_scp construction", "[services][print]") {
    print_scp scp;

    SECTION("service name is correct") {
        CHECK(scp.service_name() == "Print SCP");
    }

    SECTION("supports multiple SOP classes") {
        auto classes = scp.supported_sop_classes();
        CHECK(classes.size() == 7);
    }
}

// ============================================================================
// SOP Class Support Tests
// ============================================================================

TEST_CASE("print_scp SOP class support", "[services][print]") {
    print_scp scp;

    SECTION("supports Basic Film Session") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.1.1"));
        CHECK(scp.supports_sop_class(basic_film_session_sop_class_uid));
    }

    SECTION("supports Basic Film Box") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.1.2"));
        CHECK(scp.supports_sop_class(basic_film_box_sop_class_uid));
    }

    SECTION("supports Basic Grayscale Image Box") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.1.4"));
        CHECK(scp.supports_sop_class(basic_grayscale_image_box_sop_class_uid));
    }

    SECTION("supports Basic Color Image Box") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.1.4.1"));
        CHECK(scp.supports_sop_class(basic_color_image_box_sop_class_uid));
    }

    SECTION("supports Printer") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.1.16"));
        CHECK(scp.supports_sop_class(printer_sop_class_uid));
    }

    SECTION("supports Print Management Meta SOP classes") {
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.1.9"));
        CHECK(scp.supports_sop_class(basic_grayscale_print_meta_sop_class_uid));
        CHECK(scp.supports_sop_class("1.2.840.10008.5.1.1.18"));
        CHECK(scp.supports_sop_class(basic_color_print_meta_sop_class_uid));
    }

    SECTION("does not support unrelated SOP classes") {
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.1.1"));
        CHECK_FALSE(scp.supports_sop_class("1.2.840.10008.5.1.4.1.1.2"));
        CHECK_FALSE(scp.supports_sop_class(""));
    }
}

// ============================================================================
// SOP Class UID Constants
// ============================================================================

TEST_CASE("print SOP class UID constants", "[services][print]") {
    CHECK(basic_film_session_sop_class_uid == "1.2.840.10008.5.1.1.1");
    CHECK(basic_film_box_sop_class_uid == "1.2.840.10008.5.1.1.2");
    CHECK(basic_grayscale_image_box_sop_class_uid == "1.2.840.10008.5.1.1.4");
    CHECK(basic_color_image_box_sop_class_uid == "1.2.840.10008.5.1.1.4.1");
    CHECK(printer_sop_class_uid == "1.2.840.10008.5.1.1.16");
    CHECK(basic_grayscale_print_meta_sop_class_uid == "1.2.840.10008.5.1.1.9");
    CHECK(basic_color_print_meta_sop_class_uid == "1.2.840.10008.5.1.1.18");
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_CASE("print_scp statistics", "[services][print]") {
    print_scp scp;

    SECTION("initial statistics are zero") {
        CHECK(scp.sessions_created() == 0);
        CHECK(scp.film_boxes_created() == 0);
        CHECK(scp.images_set() == 0);
        CHECK(scp.prints_executed() == 0);
        CHECK(scp.printer_queries() == 0);
    }

    SECTION("reset_statistics clears all counters") {
        scp.reset_statistics();
        CHECK(scp.sessions_created() == 0);
        CHECK(scp.film_boxes_created() == 0);
        CHECK(scp.images_set() == 0);
        CHECK(scp.prints_executed() == 0);
        CHECK(scp.printer_queries() == 0);
    }
}

// ============================================================================
// Handler Configuration Tests
// ============================================================================

TEST_CASE("print_scp handler configuration", "[services][print]") {
    print_scp scp;

    SECTION("can set session handler") {
        bool handler_called = false;
        scp.set_session_handler([&handler_called](const film_session& /*session*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("can set print handler") {
        bool handler_called = false;
        scp.set_print_handler([&handler_called](const std::string& /*uid*/) {
            handler_called = true;
            return Result<std::monostate>{std::monostate{}};
        });
        CHECK_FALSE(handler_called);
    }

    SECTION("can set printer status handler") {
        bool handler_called = false;
        scp.set_printer_status_handler([&handler_called]() {
            handler_called = true;
            return Result<std::pair<printer_status, dicom_dataset>>{
                std::pair{printer_status::normal, dicom_dataset{}}};
        });
        CHECK_FALSE(handler_called);
    }
}

// ============================================================================
// printer_status Enumeration Tests
// ============================================================================

TEST_CASE("printer_status to_string conversion", "[services][print]") {
    CHECK(to_string(printer_status::normal) == "NORMAL");
    CHECK(to_string(printer_status::warning) == "WARNING");
    CHECK(to_string(printer_status::failure) == "FAILURE");
}

// ============================================================================
// Data Structure Tests
// ============================================================================

TEST_CASE("film_session structure", "[services][print]") {
    film_session session;

    SECTION("default construction") {
        CHECK(session.sop_instance_uid.empty());
        CHECK(session.number_of_copies == 1);
        CHECK(session.print_priority == "MED");
        CHECK(session.medium_type == "BLUE FILM");
        CHECK(session.film_destination == "MAGAZINE");
        CHECK(session.film_box_uids.empty());
    }

    SECTION("can be initialized") {
        session.sop_instance_uid = "1.2.3.4.5.6";
        session.number_of_copies = 3;
        session.print_priority = "HIGH";
        session.medium_type = "PAPER";
        session.film_destination = "PROCESSOR";

        CHECK(session.sop_instance_uid == "1.2.3.4.5.6");
        CHECK(session.number_of_copies == 3);
        CHECK(session.print_priority == "HIGH");
        CHECK(session.medium_type == "PAPER");
        CHECK(session.film_destination == "PROCESSOR");
    }
}

TEST_CASE("film_box structure", "[services][print]") {
    film_box box;

    SECTION("default construction") {
        CHECK(box.sop_instance_uid.empty());
        CHECK(box.film_session_uid.empty());
        CHECK(box.image_display_format == "STANDARD\\1,1");
        CHECK(box.film_orientation == "PORTRAIT");
        CHECK(box.film_size_id == "8INX10IN");
        CHECK(box.image_box_uids.empty());
    }
}

TEST_CASE("image_box structure", "[services][print]") {
    image_box ib;

    SECTION("default construction") {
        CHECK(ib.sop_instance_uid.empty());
        CHECK(ib.film_box_uid.empty());
        CHECK(ib.image_position == 1);
        CHECK_FALSE(ib.has_pixel_data);
    }
}

// ============================================================================
// Print Tags Tests
// ============================================================================

TEST_CASE("print_tags namespace contains print-specific DICOM tags", "[services][print]") {
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
// scp_service Base Class Tests
// ============================================================================

TEST_CASE("print_scp is a scp_service", "[services][print]") {
    std::unique_ptr<scp_service> base_ptr = std::make_unique<print_scp>();

    CHECK(base_ptr->service_name() == "Print SCP");
    CHECK(base_ptr->supported_sop_classes().size() == 7);
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.5.1.1.1"));
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.5.1.1.2"));
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.5.1.1.4"));
    CHECK(base_ptr->supports_sop_class("1.2.840.10008.5.1.1.16"));
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_CASE("multiple print_scp instances are independent", "[services][print]") {
    print_scp scp1;
    print_scp scp2;

    CHECK(scp1.service_name() == scp2.service_name());
    CHECK(scp1.supported_sop_classes() == scp2.supported_sop_classes());
    CHECK(scp1.sessions_created() == scp2.sessions_created());
}

// ============================================================================
// DIMSE-N Command Field Tests
// ============================================================================

TEST_CASE("Print-related DIMSE-N command fields", "[services][print]") {
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

    SECTION("all are DIMSE-N commands") {
        CHECK(is_dimse_n(command_field::n_create_rq));
        CHECK(is_dimse_n(command_field::n_set_rq));
        CHECK(is_dimse_n(command_field::n_get_rq));
        CHECK(is_dimse_n(command_field::n_action_rq));
        CHECK(is_dimse_n(command_field::n_delete_rq));
    }
}

// ============================================================================
// SOP Class Registry Integration Tests
// ============================================================================

TEST_CASE("print SOP classes are registered in registry", "[services][print]") {
    auto& registry = sop_class_registry::instance();

    SECTION("all print SOP classes are registered") {
        CHECK(registry.is_supported("1.2.840.10008.5.1.1.1"));
        CHECK(registry.is_supported("1.2.840.10008.5.1.1.2"));
        CHECK(registry.is_supported("1.2.840.10008.5.1.1.4"));
        CHECK(registry.is_supported("1.2.840.10008.5.1.1.4.1"));
        CHECK(registry.is_supported("1.2.840.10008.5.1.1.16"));
        CHECK(registry.is_supported("1.2.840.10008.5.1.1.9"));
        CHECK(registry.is_supported("1.2.840.10008.5.1.1.18"));
    }

    SECTION("print SOP classes have correct category") {
        auto print_classes = registry.get_by_category(sop_class_category::print);
        CHECK(print_classes.size() == 7);
    }

    SECTION("print SOP class names are correct") {
        CHECK(get_sop_class_name("1.2.840.10008.5.1.1.1") == "Basic Film Session");
        CHECK(get_sop_class_name("1.2.840.10008.5.1.1.2") == "Basic Film Box");
        CHECK(get_sop_class_name("1.2.840.10008.5.1.1.4") == "Basic Grayscale Image Box");
        CHECK(get_sop_class_name("1.2.840.10008.5.1.1.4.1") == "Basic Color Image Box");
        CHECK(get_sop_class_name("1.2.840.10008.5.1.1.16") == "Printer");
    }
}

// ============================================================================
// print_job_status Enumeration Tests
// ============================================================================

TEST_CASE("print_job_status values", "[services][print]") {
    CHECK(static_cast<int>(print_job_status::pending) == 0);
    CHECK(static_cast<int>(print_job_status::printing) == 1);
    CHECK(static_cast<int>(print_job_status::done) == 2);
    CHECK(static_cast<int>(print_job_status::failure) == 3);
}
