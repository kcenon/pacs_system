/**
 * @file main.cpp
 * @brief Command-line host for the DIMSE-N SCU classes that have no tool of their own
 *
 * Runs one Storage Commitment, UPS or N-GET operation and prints one JSON object with
 * the status the peer answered, so that an external check can compare it.
 *
 * Exit codes: 0 Success or Warning; 1 the peer answered with a failure status;
 * 2 usage or connection error; 3 no commitment report arrived in time.
 */

#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/encoding/vr_type.h"
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/network/server_config.h"
#include "kcenon/pacs/network/v2/dicom_server_v2.h"
#include "kcenon/pacs/services/n_get_scu.h"
#include "kcenon/pacs/services/storage_commitment_scu.h"
#include "kcenon/pacs/services/ups/ups_push_scu.h"
#include "kcenon/pacs/services/ups/ups_scp.h"
#include "kcenon/pacs/services/ups/ups_watch_scu.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace kcenon::pacs;

constexpr auto operation_timeout = std::chrono::seconds{30};
constexpr const char* uncompressed[] = {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"};

struct options {
    std::string host;
    uint16_t port{0};
    std::string called_ae;
    std::string operation;
    std::string calling_ae{"DIMSE_N_SCU"};
    std::map<std::string, std::string> values;
    std::vector<std::string> references;
    std::vector<std::string> tags;
    bool lock{false};

    [[nodiscard]] std::string value(const std::string& name) const {
        const auto found = values.find(name);
        return found == values.end() ? std::string{} : found->second;
    }
};

[[nodiscard]] std::string hex_status(uint16_t status) {
    char text[8];
    std::snprintf(text, sizeof(text), "%04X", status);
    return text;
}

[[nodiscard]] std::string quoted(const std::string& text) {
    std::string out = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        if (static_cast<unsigned char>(c) >= 0x20) {
            out += c;
        }
    }
    return out + "\"";
}

[[nodiscard]] std::string dataset_json(const core::dicom_dataset& dataset) {
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& [tag, element] : dataset) {
        if (element.is_sequence()) {
            continue;
        }
        auto value = element.as_string();
        char key[12];
        std::snprintf(key, sizeof(key), "%04X%04X", tag.group(), tag.element());
        out << (first ? "" : ", ") << quoted(key) << ": " << quoted(value.is_ok() ? value.value() : std::string{});
        first = false;
    }
    out << "}";
    return out.str();
}

[[nodiscard]] int report_status(const std::string& operation, uint16_t status, const std::string& extra = {}) {
    std::cout << "{\"operation\": " << quoted(operation) << ", \"status\": " << quoted(hex_status(status)) << extra << "}\n";
    const bool failure = status != 0x0000 && (status & 0xF000) != 0xB000;
    return failure ? 1 : 0;
}

[[nodiscard]] network::Result<network::association> connect(const options& opts, const std::vector<std::string>& syntaxes) {
    network::association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "DIMSE_N_SCU";
    uint8_t context_id = 1;
    for (const auto& syntax : syntaxes) {
        config.proposed_contexts.push_back({context_id, syntax, {uncompressed[0], uncompressed[1]}});
        context_id += 2;
    }
    return network::association::connect(opts.host, opts.port, config, operation_timeout);
}

[[nodiscard]] std::optional<std::vector<services::sop_reference>> parse_references(const std::vector<std::string>& values) {
    std::vector<services::sop_reference> references;
    for (const auto& value : values) {
        const auto colon = value.find(':');
        if (colon == std::string::npos) {
            return std::nullopt;
        }
        services::sop_reference reference;
        reference.sop_class_uid = value.substr(0, colon);
        reference.sop_instance_uid = value.substr(colon + 1);
        references.push_back(std::move(reference));
    }
    return references;
}

[[nodiscard]] std::string commitment_json(const services::commitment_result& result, const char* association) {
    std::ostringstream out;
    out << ", \"event_type\": " << (result.failed_references.empty() ? 1 : 2)
        << ", \"association\": " << quoted(association)
        << ", \"transaction\": " << quoted(result.transaction_uid) << ", \"committed\": [";
    bool first = true;
    for (const auto& reference : result.success_references) {
        out << (first ? "" : ", ") << quoted(reference.sop_instance_uid);
        first = false;
    }
    out << "], \"failed\": [";
    first = true;
    for (const auto& [reference, reason] : result.failed_references) {
        out << (first ? "" : ", ") << "{\"instance\": " << quoted(reference.sop_instance_uid)
            << ", \"reason\": " << quoted(hex_status(static_cast<uint16_t>(reason))) << "}";
        first = false;
    }
    out << "]";
    return out.str();
}

[[nodiscard]] int run_commitment(const options& opts) {
    using namespace network::dimse;

    const auto references = parse_references(opts.references);
    if (!references || references->empty() || opts.value("transaction").empty()) {
        std::cerr << "stgcmt needs --transaction and --ref CLASS:INSTANCE\n";
        return 2;
    }
    const auto wait = std::chrono::seconds{opts.value("wait").empty() ? 30 : std::stoi(opts.value("wait"))};

    auto scu = std::make_shared<services::storage_commitment_scu>();
    std::mutex mutex;
    std::condition_variable arrived;
    std::optional<services::commitment_result> received;
    scu->set_commitment_callback([&](const std::string&, const services::commitment_result& result) {
        std::lock_guard<std::mutex> lock(mutex);
        received = result;
        arrived.notify_all();
    });

    // A listener under this AE Title takes the report the SCP sends on an association of its own.
    std::unique_ptr<network::v2::dicom_server_v2> listener;
    if (!opts.value("listen").empty()) {
        network::server_config config;
        config.ae_title = opts.calling_ae;
        config.port = static_cast<uint16_t>(std::stoi(opts.value("listen")));
        listener = std::make_unique<network::v2::dicom_server_v2>(config);
        listener->register_service(std::make_shared<services::storage_commitment_event_scu>(scu));
        if (listener->start().is_err()) {
            std::cerr << "Cannot listen on " << opts.value("listen") << "\n";
            return 2;
        }
    }

    auto connected = connect(opts, {std::string(services::storage_commitment_push_model_sop_class_uid)});
    if (connected.is_err()) {
        std::cerr << "Association failed: " << connected.error().message << "\n";
        return 2;
    }
    auto& assoc = connected.value();
    const auto context_id = assoc.accepted_context_id(services::storage_commitment_push_model_sop_class_uid);
    if (!context_id) {
        std::cerr << "Storage Commitment not accepted\n";
        return 2;
    }

    if (!opts.value("action").empty()) {
        // Only to see a refusal reach the caller; the library always sends the defined action.
        auto request = make_n_action_rq(1, services::storage_commitment_push_model_sop_class_uid,
                                        services::storage_commitment_push_model_sop_instance_uid,
                                        static_cast<uint16_t>(std::stoi(opts.value("action"))));
        core::dicom_dataset information;
        information.set_string(core::tags::transaction_uid, encoding::vr_type::UI, opts.value("transaction"));
        request.set_dataset(std::move(information));
        if (assoc.send_dimse(*context_id, request).is_err()) {
            std::cerr << "N-ACTION not sent\n";
            return 2;
        }
    } else if (auto sent = scu->request_commitment(assoc, *context_id, opts.value("transaction"), *references); sent.is_err()) {
        std::cerr << "N-ACTION not sent: " << sent.error().message << "\n";
        return 2;
    }

    auto answered = assoc.receive_dimse(operation_timeout);
    if (answered.is_err() || answered.value().second.command() != command_field::n_action_rsp) {
        std::cerr << "No N-ACTION response\n";
        return 2;
    }
    const auto status = static_cast<uint16_t>(answered.value().second.status());
    if (status != 0x0000) {
        (void)assoc.release(operation_timeout);
        return report_status(opts.operation, status);
    }

    const char* delivered_on = "different";
    if (listener) {
        // J.3.3: the SCU may release now; the report comes on another association.
        (void)assoc.release(operation_timeout);
    } else {
        delivered_on = "same";
        auto event = assoc.receive_dimse(std::chrono::duration_cast<std::chrono::milliseconds>(wait));
        if (event.is_err() || event.value().second.command() != command_field::n_event_report_rq) {
            std::cerr << "No N-EVENT-REPORT on this association\n";
            return 3;
        }
        const auto& report = event.value().second;
        const auto handled = scu->handle_event_report(report);
        auto response = make_n_event_report_rsp(report.message_id(), report.affected_sop_class_uid(),
                                                report.affected_sop_instance_uid(), report.event_type_id().value_or(0),
                                                handled.is_ok() ? status_success : status_error_processing_failure);
        (void)assoc.send_dimse(event.value().first, response);
        (void)assoc.release(operation_timeout);
    }

    std::unique_lock<std::mutex> lock(mutex);
    if (!arrived.wait_for(lock, wait, [&] { return received.has_value(); })) {
        std::cerr << "No N-EVENT-REPORT within the wait\n";
        return 3;
    }
    if (listener) {
        listener->stop();
    }
    return report_status(opts.operation, status, commitment_json(*received, delivered_on));
}

[[nodiscard]] int run_n_get(const options& opts) {
    const auto sop_class = opts.value("class");
    const auto context = opts.value("context").empty() ? sop_class : opts.value("context");
    auto connected = connect(opts, {context});
    if (connected.is_err()) {
        std::cerr << "Association failed: " << connected.error().message << "\n";
        return 2;
    }
    auto& assoc = connected.value();
    std::vector<core::dicom_tag> tags;
    for (const auto& text : opts.tags) {
        const auto value = std::stoul(text, nullptr, 16);
        tags.emplace_back(static_cast<uint16_t>(value >> 16), static_cast<uint16_t>(value & 0xFFFF));
    }
    services::n_get_scu scu;
    auto result = scu.get(assoc, sop_class, opts.value("instance"), tags, context);
    if (result.is_err()) {
        std::cerr << "N-GET failed: " << result.error().message << "\n";
        return 2;
    }
    (void)assoc.release(operation_timeout);
    return report_status(opts.operation, result.value().status, ", \"dataset\": " + dataset_json(result.value().attributes));
}

[[nodiscard]] int run_ups(const options& opts) {
    auto connected = connect(opts, {std::string(services::ups_push_sop_class_uid), std::string(services::ups_pull_sop_class_uid),
                                    std::string(services::ups_watch_sop_class_uid)});
    if (connected.is_err()) {
        std::cerr << "Association failed: " << connected.error().message << "\n";
        return 2;
    }
    auto& assoc = connected.value();
    const auto uid = opts.value("uid");
    services::ups_push_scu push;
    services::ups_watch_scu watch;

    network::Result<services::ups_result> result = kcenon::common::make_error<services::ups_result>(1, "unknown operation", "dimse_n_scu");
    if (opts.operation == "ups-create") {
        services::ups_create_data data;
        data.workitem_uid = uid;
        data.procedure_step_label = opts.value("label");
        data.worklist_label = opts.value("worklist");
        if (!opts.value("priority").empty()) {
            data.priority = opts.value("priority");
        }
        result = push.create(assoc, data);
    } else if (opts.operation == "ups-get") {
        result = push.get(assoc, services::ups_get_data{uid, {}});
    } else if (opts.operation == "ups-set") {
        core::dicom_dataset changes;
        if (!opts.value("transaction").empty()) {
            changes.set_string(services::ups_tags::transaction_uid, encoding::vr_type::UI, opts.value("transaction"));
        }
        changes.set_string(services::ups_tags::procedure_step_label, encoding::vr_type::LO, opts.value("label"));
        result = push.set(assoc, services::ups_set_data{uid, std::move(changes)});
    } else if (opts.operation == "ups-state") {
        result = push.change_state(assoc, services::ups_change_state_data{uid, opts.value("state"), opts.value("transaction")});
    } else if (opts.operation == "ups-cancel") {
        result = push.request_cancel(assoc, services::ups_request_cancel_data{uid, opts.value("reason")});
    } else if (opts.operation == "ups-subscribe") {
        services::ups_subscribe_data data;
        data.workitem_uid = uid;
        data.receiving_ae = opts.value("receiving-ae");
        data.deletion_lock = opts.lock;
        result = watch.subscribe(assoc, data);
    } else if (opts.operation == "ups-unsubscribe") {
        services::ups_unsubscribe_data data;
        data.workitem_uid = uid;
        data.receiving_ae = opts.value("receiving-ae");
        result = watch.unsubscribe(assoc, data);
    } else if (opts.operation == "ups-suspend") {
        result = watch.suspend_global(assoc, opts.value("receiving-ae"));
    }
    if (result.is_err()) {
        std::cerr << opts.operation << " failed: " << result.error().message << "\n";
        return 2;
    }
    (void)assoc.release(operation_timeout);
    return report_status(opts.operation, result.value().status, ", \"dataset\": " + dataset_json(result.value().attributes));
}

[[nodiscard]] std::optional<options> parse(int argc, char* argv[]) {
    if (argc < 5) {
        return std::nullopt;
    }
    options opts;
    opts.host = argv[1];
    opts.port = static_cast<uint16_t>(std::stoi(argv[2]));
    opts.called_ae = argv[3];
    opts.operation = argv[4];
    for (int i = 5; i < argc; ++i) {
        const std::string name = argv[i];
        if (name == "--lock") {
            opts.lock = true;
        } else if (name.rfind("--", 0) != 0 || i + 1 >= argc) {
            return std::nullopt;
        } else if (name == "--calling-ae") {
            opts.calling_ae = argv[++i];
        } else if (name == "--ref") {
            opts.references.emplace_back(argv[++i]);
        } else if (name == "--tag") {
            opts.tags.emplace_back(argv[++i]);
        } else {
            opts.values[name.substr(2)] = argv[++i];
        }
    }
    return opts;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const bool asked = argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h");
        const auto opts = parse(argc, argv);
        if (!opts) {
            (asked ? std::cout : std::cerr) << "Usage: dimse_n_scu <host> <port> <called_ae> <operation> [options]\n"
                         "  stgcmt --transaction UID --ref CLASS:INSTANCE ... [--listen PORT] [--wait S] [--action N]\n"
                         "  n-get --class UID --instance UID [--context UID] [--tag GGGGEEEE ...]\n"
                         "  ups-create --uid UID --label TEXT [--worklist TEXT] [--priority LOW|MEDIUM|HIGH]\n"
                         "  ups-get --uid UID\n"
                         "  ups-set --uid UID --label TEXT [--transaction UID]\n"
                         "  ups-state --uid UID --state STATE --transaction UID\n"
                         "  ups-cancel --uid UID [--reason TEXT]\n"
                         "  ups-subscribe [--uid UID] --receiving-ae AE [--lock]\n"
                         "  ups-unsubscribe [--uid UID] --receiving-ae AE\n"
                         "  ups-suspend --receiving-ae AE\n"
                         "Common: --calling-ae AE\n";
            return asked ? 0 : 2;
        }
        if (opts->operation == "stgcmt") {
            return run_commitment(*opts);
        }
        if (opts->operation == "n-get") {
            return run_n_get(*opts);
        }
        if (opts->operation.rfind("ups-", 0) == 0) {
            return run_ups(*opts);
        }
        std::cerr << "Unknown operation: " << opts->operation << "\n";
    } catch (const std::exception& error) {
        std::cerr << "Invalid argument: " << error.what() << "\n";
    }
    return 2;
}
