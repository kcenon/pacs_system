/**
 * @file ai_contract_test.cpp
 * @brief The AI connector and result handler against an inference service in another process (#1323)
 *
 * Hidden ([.]): validation/dcmtk/tests/ai_probe.py starts validation/peers/ai/ai_contract_service.py,
 * which shares no code with PACS, and runs these cases with:
 *   PACS_AI_SERVICE_URL  base URL of the service, such as http://127.0.0.1:8080/v1
 *   PACS_AI_TOKEN        the bearer token the service accepts
 *   PACS_AI_RESULTS_DIR  where the service writes result objects, as <SOP Instance UID>.dcm
 *   PACS_AI_SOURCE       the object the inference is about; it is archived before each case
 *   PACS_AI_WORK         a directory for the archive and its index
 * A missing variable fails a case rather than skipping it.
 */

#include <kcenon/pacs/ai/ai_result_handler.h>
#include <kcenon/pacs/ai/ai_service_connector.h>
#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/integration/logger_adapter.h>
#include <kcenon/pacs/integration/monitoring_adapter.h>
#include <kcenon/pacs/storage/file_storage.h>
#include <kcenon/pacs/storage/index_database.h>
#include <kcenon/pacs/storage/instance_archive.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace kcenon::pacs;

namespace {

constexpr const char* segmentation_class = "1.2.840.10008.5.1.4.1.1.66.4";

std::string environment(const char* name) {
    const char* value = std::getenv(name);
    INFO("environment variable " << name << " is not set");
    REQUIRE(value != nullptr);
    return value;
}

/// An archive holding the source object, a result handler over it, and a connector to the service.
class contract_fixture {
public:
    contract_fixture() {
        integration::logger_config log_config;
        log_config.enable_console = false;
        log_config.enable_file = false;
        integration::logger_adapter::initialize(log_config);
        integration::monitoring_config monitoring_config;
        monitoring_config.enable_metrics = true;
        integration::monitoring_adapter::initialize(monitoring_config);

        const std::filesystem::path work = environment("PACS_AI_WORK");
        results_ = environment("PACS_AI_RESULTS_DIR");
        storage::file_storage_config files_config;
        files_config.root_path = work / "archive";
        files_ = std::make_shared<storage::file_storage>(files_config);
        auto opened = storage::index_database::open((work / "index.db").string());
        REQUIRE(opened.is_ok());
        database_ = std::move(opened.value());

        auto source = core::dicom_file::open(environment("PACS_AI_SOURCE"));
        REQUIRE(source.is_ok());
        source_ = source.value().dataset();
        const auto archived =
            storage::archive_instance(*files_, *database_, source_, source.value().transfer_syntax());
        INFO(archived.message);
        // An earlier case of the same run may have archived it already.
        REQUIRE((archived.status == storage::archive_status::stored ||
                 archived.status == storage::archive_status::duplicate_refused));
        handler = ai::ai_result_handler::create(files_, database_);
        connect(environment("PACS_AI_TOKEN"));
    }

    ~contract_fixture() {
        ai::ai_service_connector::shutdown();
        integration::monitoring_adapter::shutdown();
        integration::logger_adapter::shutdown();
    }

    contract_fixture(const contract_fixture&) = delete;
    contract_fixture& operator=(const contract_fixture&) = delete;

    void connect(const std::string& token) {
        ai::ai_service_connector::shutdown();
        ai::ai_service_config config;
        config.base_url = environment("PACS_AI_SERVICE_URL");
        config.service_name = "contract";
        config.auth_type = ai::authentication_type::bearer_token;
        config.bearer_token = token;
        config.connection_timeout = std::chrono::seconds(5);
        config.request_timeout = std::chrono::seconds(10);
        config.polling_interval = std::chrono::milliseconds(100);
        REQUIRE(ai::ai_service_connector::initialize(config).is_ok());
    }

    [[nodiscard]] std::string study() const { return source_.get_string(core::tags::study_instance_uid); }

    [[nodiscard]] ai::inference_request request(const std::string& model) const {
        ai::inference_request request;
        request.study_instance_uid = study();
        request.series_instance_uid = source_.get_string(core::tags::series_instance_uid);
        request.model_id = model;
        request.parameters["threshold"] = "0.5";
        request.metadata["patient_id"] = source_.get_string(core::tags::patient_id);
        request.metadata["patient_name"] = source_.get_string(core::tags::patient_name);
        request.metadata["source_sop_class"] = source_.get_string(core::tags::sop_class_uid);
        request.metadata["source_sop_instance"] = source_.get_string(core::tags::sop_instance_uid);
        return request;
    }

    [[nodiscard]] core::dicom_dataset result(const std::string& sop_instance_uid) const {
        auto file = core::dicom_file::open(results_ / (sop_instance_uid + ".dcm"));
        INFO("result object " << sop_instance_uid);
        REQUIRE(file.is_ok());
        return file.value().dataset();
    }

    std::unique_ptr<ai::ai_result_handler> handler;

private:
    std::filesystem::path results_;
    std::shared_ptr<storage::file_storage> files_;
    std::shared_ptr<storage::index_database> database_;
    core::dicom_dataset source_;
};

ai::inference_status finish(const std::string& job_id, std::chrono::milliseconds timeout,
                            std::vector<ai::inference_status_code>* seen = nullptr) {
    auto finished = ai::ai_service_connector::wait_for_completion(
        job_id, timeout, [seen](const ai::inference_status& status) {
            if (seen != nullptr) {
                seen->push_back(status.status);
            }
        });
    INFO((finished.is_err() ? finished.error().message : std::string{}));
    REQUIRE(finished.is_ok());
    return finished.value();
}

}  // namespace

TEST_CASE("a completed inference's results reach the archive tied to their source", "[.][ai-contract]") {
    contract_fixture fixture;
    REQUIRE(ai::ai_service_connector::check_health());

    const auto models = ai::ai_service_connector::list_models();
    REQUIRE(models.is_ok());
    std::vector<std::string> ids;
    for (const auto& model : models.value()) {
        ids.push_back(model.model_id);
    }
    CHECK(ids == std::vector<std::string>{"chest-cad", "always-fails", "never-finishes", "bad-reference"});

    const auto submitted = ai::ai_service_connector::request_inference(fixture.request("chest-cad"));
    INFO((submitted.is_err() ? submitted.error().message : std::string{}));
    REQUIRE(submitted.is_ok());
    std::vector<ai::inference_status_code> seen;
    const auto status = finish(submitted.value(), std::chrono::seconds(10), &seen);
    CHECK(seen.front() == ai::inference_status_code::running);
    CHECK(status.status == ai::inference_status_code::completed);
    // The service answers "error_message": null; it used to read as the next key's name.
    CHECK_FALSE(status.error_message.has_value());
    REQUIRE(status.result_uids.size() == 2);

    for (const auto& uid : status.result_uids) {
        const auto result = fixture.result(uid);
        const auto stored = result.get_string(core::tags::sop_class_uid) == segmentation_class
                                ? fixture.handler->receive_segmentation(result)
                                : fixture.handler->receive_structured_report(result);
        INFO(uid << ": " << (stored.is_err() ? stored.error().message : std::string{}));
        REQUIRE(stored.is_ok());
        CHECK(fixture.handler->exists(uid));
        const auto source = fixture.handler->get_source_reference(uid);
        REQUIRE(source.is_ok());
        CHECK(source.value().study_instance_uid == fixture.study());
    }
    const auto listed = fixture.handler->find_ai_results_for_study(fixture.study());
    REQUIRE(listed.is_ok());
    CHECK(listed.value().size() == 2);
}

TEST_CASE("a failed inference reports the service's error and no result", "[.][ai-contract]") {
    contract_fixture fixture;
    const auto submitted = ai::ai_service_connector::request_inference(fixture.request("always-fails"));
    REQUIRE(submitted.is_ok());
    const auto status = finish(submitted.value(), std::chrono::seconds(10));
    CHECK(status.status == ai::inference_status_code::failed);
    REQUIRE(status.error_message.has_value());
    CHECK(*status.error_message == "the model crashed on this input");
    CHECK(status.result_uids.empty());
}

TEST_CASE("an inference that never finishes times out and is cancelled at the service", "[.][ai-contract]") {
    contract_fixture fixture;
    const auto submitted = ai::ai_service_connector::request_inference(fixture.request("never-finishes"));
    REQUIRE(submitted.is_ok());
    const auto status = finish(submitted.value(), std::chrono::milliseconds(800));
    CHECK(status.status == ai::inference_status_code::timeout);
    REQUIRE(ai::ai_service_connector::cancel(submitted.value()).is_ok());
    const auto after = ai::ai_service_connector::check_status(submitted.value());
    REQUIRE(after.is_ok());
    CHECK(after.value().status == ai::inference_status_code::cancelled);
}

TEST_CASE("a result about a study the archive does not hold is refused and leaves nothing", "[.][ai-contract]") {
    contract_fixture fixture;
    const auto submitted = ai::ai_service_connector::request_inference(fixture.request("bad-reference"));
    REQUIRE(submitted.is_ok());
    const auto status = finish(submitted.value(), std::chrono::seconds(10));
    CHECK(status.status == ai::inference_status_code::completed);
    REQUIRE(status.result_uids.size() == 1);
    const auto uid = status.result_uids.front();
    const auto stored = fixture.handler->receive_structured_report(fixture.result(uid));
    REQUIRE(stored.is_err());
    CHECK(stored.error().message.find("is not in the archive") != std::string::npos);
    CHECK_FALSE(fixture.handler->exists(uid));
    CHECK(fixture.handler->get_source_reference(uid).is_err());
}

TEST_CASE("a request without the service's token is refused", "[.][ai-contract]") {
    contract_fixture fixture;
    fixture.connect("not-the-token");
    const auto submitted = ai::ai_service_connector::request_inference(fixture.request("chest-cad"));
    REQUIRE(submitted.is_err());
    CHECK(submitted.error().code == 401);
}
