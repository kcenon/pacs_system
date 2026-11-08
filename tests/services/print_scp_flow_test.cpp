/**
 * @file print_scp_flow_test.cpp
 * @brief The Annex H exchanges of print_scp, one request at a time
 */

#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/network/association.h>
#include <kcenon/pacs/network/dimse/dimse_message.h>
#include <kcenon/pacs/network/dimse/status_codes.h>
#include <kcenon/pacs/services/print_scp.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace kcenon::pacs;
using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::services;
using encoding::vr_type;

namespace {

constexpr uint8_t gray_context = 1;
constexpr uint8_t color_context = 3;
constexpr uint8_t printer_context = 5;

/// One requestor and one acceptor joined in memory, as the server would hold them.
struct print_link {
    association requester;
    association receiver;

    print_link()
        : requester(propose()), receiver(accept(requester)) {
        receiver.set_peer(&requester);
    }

    static association propose() {
        association_config proposed;
        proposed.calling_ae_title = "PRINT_SCU";
        proposed.called_ae_title = "PRINT_SCP";
        for (const auto& [id, syntax] : std::vector<std::pair<uint8_t, std::string_view>>{
                 {gray_context, basic_grayscale_print_meta_sop_class_uid},
                 {color_context, basic_color_print_meta_sop_class_uid},
                 {printer_context, printer_sop_class_uid}}) {
            proposed.proposed_contexts.emplace_back(id, std::string(syntax), std::vector<std::string>{"1.2.840.10008.1.2.1"});
        }
        return association::propose(proposed);
    }

    static association accept(association& requester) {
        scp_config accepted;
        accepted.ae_title = "PRINT_SCP";
        accepted.supported_abstract_syntaxes = {std::string(basic_grayscale_print_meta_sop_class_uid),
                                                std::string(basic_color_print_meta_sop_class_uid),
                                                std::string(printer_sop_class_uid)};
        accepted.supported_transfer_syntaxes = {"1.2.840.10008.1.2.1"};
        auto receiver = association::accept(requester.build_associate_rq(), accepted);
        REQUIRE(requester.process_associate_ac(receiver.build_associate_ac()));
        return receiver;
    }
};

dimse_message create_rq(std::string_view sop_class, const std::string& uid, core::dicom_dataset data) {
    dimse_message request{command_field::n_create_rq, 1};
    request.set_affected_sop_class_uid(sop_class);
    request.set_affected_sop_instance_uid(uid);
    request.set_dataset(std::move(data));
    return request;
}

dimse_message requested_rq(command_field command, std::string_view sop_class, const std::string& uid) {
    dimse_message request{command, 1};
    request.set_requested_sop_class_uid(sop_class);
    request.set_requested_sop_instance_uid(uid);
    return request;
}

core::dicom_dataset session_data() {
    core::dicom_dataset data;
    data.set_string(print_tags::number_of_copies, vr_type::IS, "2");
    data.set_string(print_tags::medium_type, vr_type::CS, "PAPER");
    data.set_string(print_tags::film_destination, vr_type::CS, "MAGAZINE");
    return data;
}

core::dicom_dataset box_data(const std::string& session_uid, const std::string& format = "STANDARD\\2,2") {
    core::dicom_dataset data;
    data.set_string(print_tags::image_display_format, vr_type::ST, format);
    core::dicom_dataset reference;
    reference.set_string(core::tags::referenced_sop_class_uid, vr_type::UI, std::string(basic_film_session_sop_class_uid));
    reference.set_string(core::tags::referenced_sop_instance_uid, vr_type::UI, session_uid);
    data.get_or_create_sequence(print_tags::referenced_film_session_sequence).push_back(std::move(reference));
    return data;
}

core::dicom_dataset image_data(core::dicom_tag sequence, uint16_t position = 1) {
    core::dicom_dataset item;
    item.set_numeric<uint16_t>(core::tags::rows, vr_type::US, 2);
    item.set_numeric<uint16_t>(core::tags::columns, vr_type::US, 2);
    item.insert(core::dicom_element(print_tags::pixel_data, vr_type::OW, std::vector<uint8_t>{1, 2, 3, 4}));
    core::dicom_dataset data;
    data.set_numeric<uint16_t>(print_tags::image_position, vr_type::US, position);
    data.get_or_create_sequence(sequence).push_back(std::move(item));
    return data;
}

/// Sends a request to the service and returns what it answered.
dimse_message exchange(print_scp& service, print_link& link, uint8_t context, const dimse_message& request) {
    REQUIRE(service.handle_message(link.receiver, context, request).is_ok());
    auto answer = link.requester.receive_dimse(std::chrono::seconds{2});
    REQUIRE(answer.is_ok());
    return answer.value().second;
}

status_code status_of(print_scp& service, print_link& link, uint8_t context, const dimse_message& request) {
    return exchange(service, link, context, request).status();
}

std::string first_image_box(const dimse_message& response) {
    REQUIRE(response.has_dataset());
    const auto* boxes = response.dataset().value().get().get_sequence(print_tags::referenced_image_box_sequence);
    REQUIRE(boxes != nullptr);
    REQUIRE_FALSE(boxes->empty());
    return boxes->front().get_string(core::tags::referenced_sop_instance_uid);
}

}  // namespace

TEST_CASE("print_scp prints what a session holds", "[services][print]") {
    print_link link;
    print_scp service;
    std::vector<print_job> jobs;
    service.set_job_handler([&jobs](const print_job& job) {
        jobs.push_back(job);
        return network::Result<std::monostate>{std::monostate{}};
    });
    const std::string session = "1.2.3.1", box = "1.2.3.2";

    REQUIRE(status_of(service, link, gray_context, create_rq(basic_film_session_sop_class_uid, session, session_data())) == status_success);

    SECTION("a Film Box gets one Image Box per place, of the class its context prints") {
        const auto answer = exchange(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session)));
        REQUIRE(answer.status() == status_success);
        const auto* boxes = answer.dataset().value().get().get_sequence(print_tags::referenced_image_box_sequence);
        REQUIRE(boxes != nullptr);
        CHECK(boxes->size() == 4);
        CHECK(boxes->front().get_string(core::tags::referenced_sop_class_uid) == basic_grayscale_image_box_sop_class_uid);
    }
    SECTION("under Color Print Meta the class is Color") {
        const auto answer = exchange(service, link, color_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session, "STANDARD\\1,1")));
        REQUIRE(answer.status() == status_success);
        const auto* boxes = answer.dataset().value().get().get_sequence(print_tags::referenced_image_box_sequence);
        REQUIRE(boxes != nullptr);
        CHECK(boxes->front().get_string(core::tags::referenced_sop_class_uid) == basic_color_image_box_sop_class_uid);
    }
    SECTION("a job holds the session, the box and the image") {
        const auto created = exchange(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session)));
        auto set = requested_rq(command_field::n_set_rq, basic_grayscale_image_box_sop_class_uid, first_image_box(created));
        set.set_dataset(image_data(print_tags::basic_grayscale_image_sequence));
        REQUIRE(status_of(service, link, gray_context, set) == status_success);

        auto print = requested_rq(command_field::n_action_rq, basic_film_box_sop_class_uid, box);
        print.set_action_type_id(1);
        const auto answer = exchange(service, link, gray_context, print);
        CHECK(answer.status() == status_success);
        CHECK(answer.affected_sop_instance_uid() == box);

        REQUIRE(jobs.size() == 1);
        CHECK(jobs[0].session.sop_instance_uid == session);
        CHECK(jobs[0].session.number_of_copies == 2);
        CHECK(jobs[0].box.sop_instance_uid == box);
        REQUIRE(jobs[0].images.size() == 1);
        CHECK(jobs[0].images[0].image_position == 1);
    }
    SECTION("a job handler's failure is C602H, not success") {
        service.set_job_handler([](const print_job&) {
            return kcenon::common::make_error<std::monostate>(1, "disk full", "test");
        });
        const auto created = exchange(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session)));
        auto set = requested_rq(command_field::n_set_rq, basic_grayscale_image_box_sop_class_uid, first_image_box(created));
        set.set_dataset(image_data(print_tags::basic_grayscale_image_sequence));
        REQUIRE(status_of(service, link, gray_context, set) == status_success);
        auto print = requested_rq(command_field::n_action_rq, basic_film_box_sop_class_uid, box);
        print.set_action_type_id(1);
        CHECK(status_of(service, link, gray_context, print) == 0xC602);
    }
    SECTION("an image in the wrong place or of the wrong kind is refused") {
        const auto created = exchange(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session)));
        const auto image_box = first_image_box(created);
        auto misplaced = requested_rq(command_field::n_set_rq, basic_grayscale_image_box_sop_class_uid, image_box);
        misplaced.set_dataset(image_data(print_tags::basic_grayscale_image_sequence, 3));
        CHECK(status_of(service, link, gray_context, misplaced) == status_error_invalid_attribute_value);
        auto wrong_kind = requested_rq(command_field::n_set_rq, basic_grayscale_image_box_sop_class_uid, image_box);
        wrong_kind.set_dataset(image_data(print_tags::basic_color_image_sequence));
        CHECK(status_of(service, link, gray_context, wrong_kind) == status_error_missing_attribute);
    }
}

TEST_CASE("print_scp answers what it cannot do with the status of Annex H", "[services][print]") {
    print_link link;
    print_scp service;
    const std::string session = "1.2.3.1", box = "1.2.3.2";
    const auto action = [](std::string_view sop_class, const std::string& uid, uint16_t type) {
        auto request = requested_rq(command_field::n_action_rq, sop_class, uid);
        request.set_action_type_id(type);
        return request;
    };

    SECTION("a copy count that is not a number") {
        auto data = session_data();
        data.set_string(print_tags::number_of_copies, vr_type::IS, "many");
        CHECK(status_of(service, link, gray_context, create_rq(basic_film_session_sop_class_uid, session, data)) == status_error_invalid_attribute_value);
    }

    REQUIRE(status_of(service, link, gray_context, create_rq(basic_film_session_sop_class_uid, session, session_data())) == status_success);

    SECTION("one Film Session per association") {
        CHECK(status_of(service, link, gray_context, create_rq(basic_film_session_sop_class_uid, "1.2.3.9", session_data())) == status_error_resource_limitation);
    }
    SECTION("a Film Box must name its session, and a session that exists") {
        auto unnamed = box_data(session);
        unnamed.remove(print_tags::referenced_film_session_sequence);
        CHECK(status_of(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, unnamed)) == status_error_missing_attribute);
        CHECK(status_of(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data("9.9.9"))) == status_error_invalid_attribute_value);
    }
    SECTION("a display format that cannot be laid out") {
        for (const auto* format : {"STANDARD\\A,B", "STANDARD\\0,2", "STANDARD\\2", "SLIDE", "CUSTOM\\1", "STANDARD\\999999,999999", "ROW\\"}) {
            CHECK(status_of(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session, format))) == status_error_invalid_attribute_value);
        }
    }
    SECTION("ROW and COL formats count their places") {
        const auto answer = exchange(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session, "ROW\\1,2")));
        REQUIRE(answer.status() == status_success);
        CHECK(answer.dataset().value().get().get_sequence(print_tags::referenced_image_box_sequence)->size() == 3);
    }
    SECTION("instances that do not exist") {
        CHECK(status_of(service, link, gray_context, action(basic_film_box_sop_class_uid, "9.9.9", 1)) == status_error_no_such_object_instance);
        CHECK(status_of(service, link, gray_context, requested_rq(command_field::n_delete_rq, basic_film_box_sop_class_uid, "9.9.9")) == status_error_no_such_object_instance);
        CHECK(status_of(service, link, gray_context, requested_rq(command_field::n_delete_rq, basic_film_session_sop_class_uid, "9.9.9")) == status_error_no_such_object_instance);
        auto set = requested_rq(command_field::n_set_rq, basic_grayscale_image_box_sop_class_uid, "9.9.9");
        set.set_dataset(image_data(print_tags::basic_grayscale_image_sequence));
        CHECK(status_of(service, link, gray_context, set) == status_error_no_such_object_instance);
    }
    SECTION("an action that does not exist, an empty page, and a session with no box") {
        REQUIRE(status_of(service, link, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session))) == status_success);
        CHECK(status_of(service, link, gray_context, action(basic_film_box_sop_class_uid, box, 7)) == status_error_no_such_action_type);
        CHECK(status_of(service, link, gray_context, action(basic_film_box_sop_class_uid, box, 1)) == 0xB603);
        REQUIRE(status_of(service, link, gray_context, requested_rq(command_field::n_delete_rq, basic_film_box_sop_class_uid, box)) == status_success);
        CHECK(status_of(service, link, gray_context, action(basic_film_session_sop_class_uid, session, 1)) == 0xC600);
    }
    SECTION("the Printer has one instance") {
        const auto wrong = exchange(service, link, printer_context, requested_rq(command_field::n_get_rq, printer_sop_class_uid, "9.9.9"));
        CHECK(wrong.status() == status_error_no_such_object_instance);
        const auto right = exchange(service, link, printer_context, requested_rq(command_field::n_get_rq, printer_sop_class_uid, std::string(printer_sop_instance_uid)));
        REQUIRE(right.status() == status_success);
        CHECK(right.dataset().value().get().get_string(print_tags::printer_status_tag) == "NORMAL");
        CHECK(right.dataset().value().get().get_string(print_tags::printer_status_info) == "NORMAL");
    }
}

TEST_CASE("a Film Session ends with its association and is not reachable from another", "[services][print]") {
    print_link first;
    print_link second;
    print_scp service;
    const std::string session = "1.2.3.1", box = "1.2.3.2";

    REQUIRE(status_of(service, first, gray_context, create_rq(basic_film_session_sop_class_uid, session, session_data())) == status_success);
    REQUIRE(status_of(service, first, gray_context, create_rq(basic_film_box_sop_class_uid, box, box_data(session))) == status_success);

    auto print = requested_rq(command_field::n_action_rq, basic_film_box_sop_class_uid, box);
    print.set_action_type_id(1);
    CHECK(status_of(service, second, gray_context, print) == status_error_no_such_object_instance);
    CHECK(status_of(service, second, gray_context, requested_rq(command_field::n_delete_rq, basic_film_session_sop_class_uid, session)) == status_error_no_such_object_instance);

    // Still the first association's, until that one ends.
    CHECK(status_of(service, first, gray_context, print) == 0xB603);
    service.association_ended(first.receiver);
    CHECK(status_of(service, first, gray_context, print) == status_error_no_such_object_instance);

    // Its UID is free again.
    CHECK(status_of(service, second, gray_context, create_rq(basic_film_session_sop_class_uid, session, session_data())) == status_success);
}
