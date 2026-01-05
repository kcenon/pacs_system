/**
 * @file dimse_message.cpp
 * @brief DIMSE message implementation
 */

#include <pacs/network/dimse/dimse_message.hpp>

#include <pacs/encoding/implicit_vr_codec.hpp>
#include <pacs/encoding/explicit_vr_codec.hpp>
#include <pacs/encoding/vr_type.hpp>

#include <stdexcept>

namespace pacs::network::dimse {

namespace {
/// Helper to convert dimse_error to pacs::error_info
inline pacs::error_info make_dimse_error(dimse_error err, const std::string& details = "") {
    std::string msg = std::string(to_string(err));
    if (!details.empty()) {
        msg += ": " + details;
    }
    return pacs::error_info{pacs::error_codes::dimse_error, msg, "network"};
}
}  // namespace

// ============================================================================
// Construction
// ============================================================================

dimse_message::dimse_message(command_field cmd, uint16_t message_id)
    : command_(cmd), message_id_(message_id) {
    // Initialize the command set with required fields
    command_set_.set_numeric(tag_command_field, encoding::vr_type::US,
                             static_cast<uint16_t>(cmd));
    command_set_.set_numeric(tag_message_id, encoding::vr_type::US, message_id);
    update_data_set_type();
}

// ============================================================================
// Command Set Access
// ============================================================================

auto dimse_message::command() const noexcept -> command_field {
    return command_;
}

auto dimse_message::message_id() const noexcept -> uint16_t {
    return message_id_;
}

auto dimse_message::command_set() noexcept -> core::dicom_dataset& {
    return command_set_;
}

auto dimse_message::command_set() const noexcept -> const core::dicom_dataset& {
    return command_set_;
}

// ============================================================================
// Dataset Access
// ============================================================================

auto dimse_message::has_dataset() const noexcept -> bool {
    return dataset_.has_value();
}

auto dimse_message::dataset() -> pacs::Result<std::reference_wrapper<core::dicom_dataset>> {
    if (!dataset_) {
        return make_dimse_error(dimse_error::invalid_data_format, "DIMSE message has no dataset");
    }
    return std::ref(*dataset_);
}

auto dimse_message::dataset() const -> pacs::Result<std::reference_wrapper<const core::dicom_dataset>> {
    if (!dataset_) {
        return make_dimse_error(dimse_error::invalid_data_format, "DIMSE message has no dataset");
    }
    return std::cref(*dataset_);
}

void dimse_message::set_dataset(core::dicom_dataset ds) {
    dataset_ = std::move(ds);
    update_data_set_type();
}

void dimse_message::clear_dataset() noexcept {
    dataset_.reset();
    update_data_set_type();
}

// ============================================================================
// Status (for responses)
// ============================================================================

auto dimse_message::status() const -> status_code {
    return command_set_.get_numeric<uint16_t>(tag_status).value_or(0);
}

void dimse_message::set_status(status_code status) {
    command_set_.set_numeric(tag_status, encoding::vr_type::US, status);
}

// ============================================================================
// Common Attributes
// ============================================================================

auto dimse_message::affected_sop_class_uid() const -> std::string {
    return command_set_.get_string(tag_affected_sop_class_uid);
}

void dimse_message::set_affected_sop_class_uid(std::string_view uid) {
    command_set_.set_string(tag_affected_sop_class_uid, encoding::vr_type::UI,
                            uid);
}

auto dimse_message::affected_sop_instance_uid() const -> std::string {
    return command_set_.get_string(tag_affected_sop_instance_uid);
}

void dimse_message::set_affected_sop_instance_uid(std::string_view uid) {
    command_set_.set_string(tag_affected_sop_instance_uid, encoding::vr_type::UI,
                            uid);
}

auto dimse_message::priority() const -> uint16_t {
    return command_set_.get_numeric<uint16_t>(tag_priority).value_or(priority_medium);
}

void dimse_message::set_priority(uint16_t priority) {
    command_set_.set_numeric(tag_priority, encoding::vr_type::US, priority);
}

auto dimse_message::message_id_responded_to() const -> uint16_t {
    return command_set_.get_numeric<uint16_t>(tag_message_id_responded_to).value_or(0);
}

void dimse_message::set_message_id_responded_to(uint16_t id) {
    command_set_.set_numeric(tag_message_id_responded_to, encoding::vr_type::US, id);
}

// ============================================================================
// DIMSE-N Specific Attributes
// ============================================================================

auto dimse_message::requested_sop_class_uid() const -> std::string {
    return command_set_.get_string(tag_requested_sop_class_uid);
}

void dimse_message::set_requested_sop_class_uid(std::string_view uid) {
    command_set_.set_string(tag_requested_sop_class_uid, encoding::vr_type::UI, uid);
}

auto dimse_message::requested_sop_instance_uid() const -> std::string {
    return command_set_.get_string(tag_requested_sop_instance_uid);
}

void dimse_message::set_requested_sop_instance_uid(std::string_view uid) {
    command_set_.set_string(tag_requested_sop_instance_uid, encoding::vr_type::UI, uid);
}

auto dimse_message::event_type_id() const -> std::optional<uint16_t> {
    return command_set_.get_numeric<uint16_t>(tag_event_type_id);
}

void dimse_message::set_event_type_id(uint16_t type_id) {
    command_set_.set_numeric(tag_event_type_id, encoding::vr_type::US, type_id);
}

auto dimse_message::action_type_id() const -> std::optional<uint16_t> {
    return command_set_.get_numeric<uint16_t>(tag_action_type_id);
}

void dimse_message::set_action_type_id(uint16_t type_id) {
    command_set_.set_numeric(tag_action_type_id, encoding::vr_type::US, type_id);
}

auto dimse_message::attribute_identifier_list() const -> std::vector<core::dicom_tag> {
    std::vector<core::dicom_tag> tags;
    const auto* elem = command_set_.get(tag_attribute_identifier_list);
    if (elem == nullptr) {
        return tags;
    }

    auto bytes = elem->raw_data();
    if (bytes.empty()) {
        return tags;
    }

    // Each tag is 4 bytes (group:2 + element:2), little-endian
    for (size_t i = 0; i + 3 < bytes.size(); i += 4) {
        uint16_t group = static_cast<uint16_t>(bytes[i]) |
                         (static_cast<uint16_t>(bytes[i + 1]) << 8);
        uint16_t element = static_cast<uint16_t>(bytes[i + 2]) |
                           (static_cast<uint16_t>(bytes[i + 3]) << 8);
        tags.emplace_back(group, element);
    }
    return tags;
}

void dimse_message::set_attribute_identifier_list(
    const std::vector<core::dicom_tag>& tags) {
    std::vector<uint8_t> bytes;
    bytes.reserve(tags.size() * 4);

    for (const auto& tag : tags) {
        // Little-endian encoding
        bytes.push_back(static_cast<uint8_t>(tag.group() & 0xFF));
        bytes.push_back(static_cast<uint8_t>((tag.group() >> 8) & 0xFF));
        bytes.push_back(static_cast<uint8_t>(tag.element() & 0xFF));
        bytes.push_back(static_cast<uint8_t>((tag.element() >> 8) & 0xFF));
    }

    command_set_.insert(core::dicom_element(tag_attribute_identifier_list,
                                            encoding::vr_type::AT, bytes));
}

// ============================================================================
// Sub-operation Counts
// ============================================================================

auto dimse_message::remaining_subops() const -> std::optional<uint16_t> {
    return command_set_.get_numeric<uint16_t>(tag_number_of_remaining_subops);
}

void dimse_message::set_remaining_subops(uint16_t count) {
    command_set_.set_numeric(tag_number_of_remaining_subops, encoding::vr_type::US,
                             count);
}

auto dimse_message::completed_subops() const -> std::optional<uint16_t> {
    return command_set_.get_numeric<uint16_t>(tag_number_of_completed_subops);
}

void dimse_message::set_completed_subops(uint16_t count) {
    command_set_.set_numeric(tag_number_of_completed_subops, encoding::vr_type::US,
                             count);
}

auto dimse_message::failed_subops() const -> std::optional<uint16_t> {
    return command_set_.get_numeric<uint16_t>(tag_number_of_failed_subops);
}

void dimse_message::set_failed_subops(uint16_t count) {
    command_set_.set_numeric(tag_number_of_failed_subops, encoding::vr_type::US,
                             count);
}

auto dimse_message::warning_subops() const -> std::optional<uint16_t> {
    return command_set_.get_numeric<uint16_t>(tag_number_of_warning_subops);
}

void dimse_message::set_warning_subops(uint16_t count) {
    command_set_.set_numeric(tag_number_of_warning_subops, encoding::vr_type::US,
                             count);
}

// ============================================================================
// Encoding/Decoding
// ============================================================================

auto dimse_message::encode(const dimse_message& msg,
                           const encoding::transfer_syntax& dataset_ts)
    -> dimse_result<encoded_message> {
    // Create a mutable copy for encoding
    dimse_message copy = msg;
    copy.update_command_group_length();

    // Command set is always Implicit VR Little Endian
    auto command_bytes = encoding::implicit_vr_codec::encode(copy.command_set_);

    // Encode dataset if present
    std::vector<uint8_t> dataset_bytes;
    if (copy.has_dataset()) {
        auto ds_result = copy.dataset();
        if (ds_result.is_err()) {
            return ds_result.error();
        }
        if (dataset_ts.vr_type() == encoding::vr_encoding::implicit) {
            dataset_bytes = encoding::implicit_vr_codec::encode(ds_result.value().get());
        } else {
            dataset_bytes = encoding::explicit_vr_codec::encode(ds_result.value().get());
        }
    }

    return encoded_message{std::move(command_bytes), std::move(dataset_bytes)};
}

auto dimse_message::decode(std::span<const uint8_t> command_data,
                           std::span<const uint8_t> dataset_data,
                           const encoding::transfer_syntax& dataset_ts)
    -> dimse_result<dimse_message> {
    // Decode command set (always Implicit VR Little Endian)
    auto cmd_result = encoding::implicit_vr_codec::decode(command_data);
    if (cmd_result.is_err()) {
        return make_dimse_error(dimse_error::decoding_error, "Failed to decode command set");
    }

    auto command_set = std::move(cmd_result.value());

    // Extract command field
    auto cmd_value = command_set.get_numeric<uint16_t>(tag_command_field);
    if (!cmd_value) {
        return make_dimse_error(dimse_error::missing_required_field, "CommandField tag not found");
    }
    auto cmd = static_cast<command_field>(*cmd_value);

    // Extract message ID
    auto msg_id = command_set.get_numeric<uint16_t>(tag_message_id);
    if (!msg_id && dimse::is_request(cmd)) {
        return make_dimse_error(dimse_error::missing_required_field, "MessageID tag not found for request");
    }
    // For responses, use message_id_responded_to if message_id is not present
    uint16_t message_id = msg_id.value_or(0);

    dimse_message msg(cmd, message_id);
    msg.command_set_ = std::move(command_set);

    // Decode dataset if present
    if (!dataset_data.empty()) {
        pacs::Result<core::dicom_dataset> ds_result =
            (dataset_ts.vr_type() == encoding::vr_encoding::implicit)
                ? encoding::implicit_vr_codec::decode(dataset_data)
                : encoding::explicit_vr_codec::decode(dataset_data);

        if (ds_result.is_err()) {
            return make_dimse_error(dimse_error::decoding_error, "Failed to decode dataset");
        }
        msg.dataset_ = std::move(ds_result.value());
    }

    return msg;
}

// ============================================================================
// Validation
// ============================================================================

auto dimse_message::is_valid() const noexcept -> bool {
    return command_set_.contains(tag_command_field) &&
           command_set_.contains(tag_message_id);
}

auto dimse_message::is_request() const noexcept -> bool {
    return dimse::is_request(command_);
}

auto dimse_message::is_response() const noexcept -> bool {
    return dimse::is_response(command_);
}

// ============================================================================
// Private Methods
// ============================================================================

void dimse_message::update_data_set_type() {
    if (dataset_) {
        command_set_.set_numeric(tag_command_data_set_type, encoding::vr_type::US,
                                 command_data_set_type_present);
    } else {
        command_set_.set_numeric(tag_command_data_set_type, encoding::vr_type::US,
                                 command_data_set_type_null);
    }
}

void dimse_message::update_command_group_length() {
    // First, remove CommandGroupLength to calculate without it
    command_set_.remove(tag_command_group_length);

    // Encode command set to get the size
    auto encoded = encoding::implicit_vr_codec::encode(command_set_);
    auto length = static_cast<uint32_t>(encoded.size());

    // Set CommandGroupLength
    command_set_.set_numeric(tag_command_group_length, encoding::vr_type::UL, length);
}

// ============================================================================
// Factory Functions
// ============================================================================

auto make_c_echo_rq(uint16_t message_id, std::string_view sop_class_uid)
    -> dimse_message {
    dimse_message msg(command_field::c_echo_rq, message_id);
    msg.set_affected_sop_class_uid(sop_class_uid);
    return msg;
}

auto make_c_echo_rsp(uint16_t message_id_responded_to, status_code status,
                     std::string_view sop_class_uid) -> dimse_message {
    dimse_message msg(command_field::c_echo_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_status(status);
    return msg;
}

auto make_c_store_rq(uint16_t message_id, std::string_view sop_class_uid,
                     std::string_view sop_instance_uid, uint16_t priority)
    -> dimse_message {
    dimse_message msg(command_field::c_store_rq, message_id);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_priority(priority);
    return msg;
}

auto make_c_store_rsp(uint16_t message_id_responded_to,
                      std::string_view sop_class_uid,
                      std::string_view sop_instance_uid, status_code status)
    -> dimse_message {
    dimse_message msg(command_field::c_store_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_status(status);
    return msg;
}

auto make_c_find_rq(uint16_t message_id, std::string_view sop_class_uid,
                    uint16_t priority) -> dimse_message {
    dimse_message msg(command_field::c_find_rq, message_id);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_priority(priority);
    return msg;
}

auto make_c_find_rsp(uint16_t message_id_responded_to,
                     std::string_view sop_class_uid, status_code status)
    -> dimse_message {
    dimse_message msg(command_field::c_find_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_status(status);
    return msg;
}

// ============================================================================
// DIMSE-N Factory Functions
// ============================================================================

auto make_n_create_rq(uint16_t message_id, std::string_view sop_class_uid,
                      std::string_view sop_instance_uid) -> dimse_message {
    dimse_message msg(command_field::n_create_rq, message_id);
    msg.set_affected_sop_class_uid(sop_class_uid);
    if (!sop_instance_uid.empty()) {
        msg.set_affected_sop_instance_uid(sop_instance_uid);
    }
    return msg;
}

auto make_n_create_rsp(uint16_t message_id_responded_to,
                       std::string_view sop_class_uid,
                       std::string_view sop_instance_uid, status_code status)
    -> dimse_message {
    dimse_message msg(command_field::n_create_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_status(status);
    return msg;
}

auto make_n_set_rq(uint16_t message_id, std::string_view sop_class_uid,
                   std::string_view sop_instance_uid) -> dimse_message {
    dimse_message msg(command_field::n_set_rq, message_id);
    msg.set_requested_sop_class_uid(sop_class_uid);
    msg.set_requested_sop_instance_uid(sop_instance_uid);
    return msg;
}

auto make_n_set_rsp(uint16_t message_id_responded_to,
                    std::string_view sop_class_uid,
                    std::string_view sop_instance_uid, status_code status)
    -> dimse_message {
    dimse_message msg(command_field::n_set_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_status(status);
    return msg;
}

auto make_n_get_rq(uint16_t message_id, std::string_view sop_class_uid,
                   std::string_view sop_instance_uid,
                   const std::vector<core::dicom_tag>& attribute_tags)
    -> dimse_message {
    dimse_message msg(command_field::n_get_rq, message_id);
    msg.set_requested_sop_class_uid(sop_class_uid);
    msg.set_requested_sop_instance_uid(sop_instance_uid);
    if (!attribute_tags.empty()) {
        msg.set_attribute_identifier_list(attribute_tags);
    }
    return msg;
}

auto make_n_get_rsp(uint16_t message_id_responded_to,
                    std::string_view sop_class_uid,
                    std::string_view sop_instance_uid, status_code status)
    -> dimse_message {
    dimse_message msg(command_field::n_get_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_status(status);
    return msg;
}

auto make_n_event_report_rq(uint16_t message_id, std::string_view sop_class_uid,
                            std::string_view sop_instance_uid,
                            uint16_t event_type_id) -> dimse_message {
    dimse_message msg(command_field::n_event_report_rq, message_id);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_event_type_id(event_type_id);
    return msg;
}

auto make_n_event_report_rsp(uint16_t message_id_responded_to,
                             std::string_view sop_class_uid,
                             std::string_view sop_instance_uid,
                             uint16_t event_type_id, status_code status)
    -> dimse_message {
    dimse_message msg(command_field::n_event_report_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_event_type_id(event_type_id);
    msg.set_status(status);
    return msg;
}

auto make_n_action_rq(uint16_t message_id, std::string_view sop_class_uid,
                      std::string_view sop_instance_uid, uint16_t action_type_id)
    -> dimse_message {
    dimse_message msg(command_field::n_action_rq, message_id);
    msg.set_requested_sop_class_uid(sop_class_uid);
    msg.set_requested_sop_instance_uid(sop_instance_uid);
    msg.set_action_type_id(action_type_id);
    return msg;
}

auto make_n_action_rsp(uint16_t message_id_responded_to,
                       std::string_view sop_class_uid,
                       std::string_view sop_instance_uid, uint16_t action_type_id,
                       status_code status) -> dimse_message {
    dimse_message msg(command_field::n_action_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_action_type_id(action_type_id);
    msg.set_status(status);
    return msg;
}

auto make_n_delete_rq(uint16_t message_id, std::string_view sop_class_uid,
                      std::string_view sop_instance_uid) -> dimse_message {
    dimse_message msg(command_field::n_delete_rq, message_id);
    msg.set_requested_sop_class_uid(sop_class_uid);
    msg.set_requested_sop_instance_uid(sop_instance_uid);
    return msg;
}

auto make_n_delete_rsp(uint16_t message_id_responded_to,
                       std::string_view sop_class_uid,
                       std::string_view sop_instance_uid, status_code status)
    -> dimse_message {
    dimse_message msg(command_field::n_delete_rsp, 0);
    msg.set_message_id_responded_to(message_id_responded_to);
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_affected_sop_instance_uid(sop_instance_uid);
    msg.set_status(status);
    return msg;
}

}  // namespace pacs::network::dimse
