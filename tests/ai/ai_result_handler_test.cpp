/**
 * @file ai_result_handler_test.cpp
 * @brief Unit tests for AI result handler
 */

#include <pacs/ai/ai_result_handler.hpp>
#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag_constants.hpp>
#include <pacs/storage/storage_interface.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

using namespace pacs::ai;
using namespace pacs::core;
using namespace pacs::storage;

// ============================================================================
// Mock Storage Implementation
// ============================================================================

class mock_storage : public storage_interface {
public:
    mock_storage() = default;

    [[nodiscard]] auto store(const dicom_dataset& dataset) -> VoidResult override {
        auto uid = dataset.get_string(tags::sop_instance_uid);
        if (uid.empty()) {
            return VoidResult::err(kcenon::common::error_info("Missing SOP Instance UID"));
        }
        datasets_[uid] = dataset;
        return VoidResult::ok();
    }

    [[nodiscard]] auto retrieve(std::string_view sop_instance_uid)
        -> Result<dicom_dataset> override {
        auto it = datasets_.find(std::string(sop_instance_uid));
        if (it == datasets_.end()) {
            return Result<dicom_dataset>::err(
                kcenon::common::error_info("Dataset not found"));
        }
        return it->second;
    }

    [[nodiscard]] auto remove(std::string_view sop_instance_uid) -> VoidResult override {
        datasets_.erase(std::string(sop_instance_uid));
        return VoidResult::ok();
    }

    [[nodiscard]] auto exists(std::string_view sop_instance_uid) const -> bool override {
        return datasets_.find(std::string(sop_instance_uid)) != datasets_.end();
    }

    [[nodiscard]] auto find(const dicom_dataset& /* query */)
        -> Result<std::vector<dicom_dataset>> override {
        std::vector<dicom_dataset> results;
        for (const auto& [uid, ds] : datasets_) {
            results.push_back(ds);
        }
        return results;
    }

    [[nodiscard]] auto get_statistics() const -> storage_statistics override {
        storage_statistics stats;
        stats.total_instances = datasets_.size();
        return stats;
    }

    [[nodiscard]] auto verify_integrity() -> VoidResult override {
        return VoidResult::ok();
    }

private:
    std::map<std::string, dicom_dataset> datasets_;
};

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

dicom_dataset create_sr_dataset(const std::string& sop_instance_uid,
                                const std::string& study_uid,
                                const std::string& series_uid) {
    dicom_dataset ds;

    // Required tags
    ds.set_string(tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.88.22");  // Enhanced SR
    ds.set_string(tags::sop_instance_uid, encoding::vr_type::UI, sop_instance_uid);
    ds.set_string(tags::study_instance_uid, encoding::vr_type::UI, study_uid);
    ds.set_string(tags::series_instance_uid, encoding::vr_type::UI, series_uid);
    ds.set_string(tags::modality, encoding::vr_type::CS, "SR");

    return ds;
}

dicom_dataset create_seg_dataset(const std::string& sop_instance_uid,
                                 const std::string& study_uid,
                                 const std::string& series_uid) {
    dicom_dataset ds;

    // Required tags
    ds.set_string(tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.66.4");  // Segmentation
    ds.set_string(tags::sop_instance_uid, encoding::vr_type::UI, sop_instance_uid);
    ds.set_string(tags::study_instance_uid, encoding::vr_type::UI, study_uid);
    ds.set_string(tags::series_instance_uid, encoding::vr_type::UI, series_uid);
    ds.set_string(tags::modality, encoding::vr_type::CS, "SEG");

    // Segmentation-specific tags
    ds.set_string(dicom_tag(0x0062, 0x0001), encoding::vr_type::CS, "BINARY");

    return ds;
}

dicom_dataset create_pr_dataset(const std::string& sop_instance_uid,
                                const std::string& study_uid,
                                const std::string& series_uid) {
    dicom_dataset ds;

    // Required tags
    ds.set_string(tags::sop_class_uid, encoding::vr_type::UI,
                  "1.2.840.10008.5.1.4.1.1.11.1");  // Grayscale Softcopy PS
    ds.set_string(tags::sop_instance_uid, encoding::vr_type::UI, sop_instance_uid);
    ds.set_string(tags::study_instance_uid, encoding::vr_type::UI, study_uid);
    ds.set_string(tags::series_instance_uid, encoding::vr_type::UI, series_uid);
    ds.set_string(tags::modality, encoding::vr_type::CS, "PR");

    return ds;
}

}  // namespace

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_CASE("ai_result_handler default configuration", "[ai][handler]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    SECTION("default config values") {
        auto config = handler->get_config();
        CHECK(config.validate_source_references == true);
        CHECK(config.validate_sr_templates == true);
        CHECK(config.auto_link_to_source == true);
        CHECK(config.accepted_sr_templates.empty());
        CHECK(config.max_segments == 256);
    }

    SECTION("configure changes settings") {
        ai_handler_config new_config;
        new_config.validate_source_references = false;
        new_config.validate_sr_templates = false;
        new_config.max_segments = 128;

        handler->configure(new_config);

        auto config = handler->get_config();
        CHECK(config.validate_source_references == false);
        CHECK(config.validate_sr_templates == false);
        CHECK(config.max_segments == 128);
    }
}

// ============================================================================
// Structured Report Tests
// ============================================================================

TEST_CASE("ai_result_handler receive_structured_report", "[ai][handler][sr]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    // Disable source reference validation for testing
    ai_handler_config config;
    config.validate_source_references = false;
    handler->configure(config);

    SECTION("valid SR is stored successfully") {
        auto sr = create_sr_dataset(
            "1.2.3.4.5.6.7.8.9",
            "1.2.3.4.5.6.1",
            "1.2.3.4.5.6.2");

        auto result = handler->receive_structured_report(sr);
        CHECK(result.is_ok());
        CHECK(storage->exists("1.2.3.4.5.6.7.8.9"));
    }

    SECTION("SR missing required tags is rejected") {
        dicom_dataset sr;
        sr.set_string(tags::sop_class_uid, encoding::vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.88.22");

        auto result = handler->receive_structured_report(sr);
        CHECK(result.is_err());
    }

    SECTION("non-SR SOP class is rejected") {
        dicom_dataset not_sr;
        not_sr.set_string(tags::sop_class_uid, encoding::vr_type::UI,
                          "1.2.840.10008.5.1.4.1.1.2");  // CT Image Storage
        not_sr.set_string(tags::sop_instance_uid, encoding::vr_type::UI, "1.2.3.4.5");
        not_sr.set_string(tags::study_instance_uid, encoding::vr_type::UI, "1.2.3.4.5.6");
        not_sr.set_string(tags::series_instance_uid, encoding::vr_type::UI, "1.2.3.4.5.7");
        not_sr.set_string(tags::modality, encoding::vr_type::CS, "CT");

        auto result = handler->receive_structured_report(not_sr);
        CHECK(result.is_err());
    }
}

TEST_CASE("ai_result_handler validate_sr_template", "[ai][handler][sr]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    SECTION("SR without configured templates passes") {
        auto sr = create_sr_dataset("1.2.3.4.5", "1.2.3.1", "1.2.3.2");

        auto result = handler->validate_sr_template(sr);
        CHECK(result.status == validation_status::valid);
    }

    SECTION("SR template validation with accepted list") {
        ai_handler_config config;
        config.accepted_sr_templates = {"TID1500"};  // CAD SR template
        handler->configure(config);

        auto sr = create_sr_dataset("1.2.3.4.5", "1.2.3.1", "1.2.3.2");
        // Set a different template ID
        sr.set_string(dicom_tag(0x0040, 0xDB00), encoding::vr_type::CS, "TID9999");

        auto result = handler->validate_sr_template(sr);
        CHECK(result.status == validation_status::invalid_template);
    }
}

// ============================================================================
// Segmentation Tests
// ============================================================================

TEST_CASE("ai_result_handler receive_segmentation", "[ai][handler][seg]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    ai_handler_config config;
    config.validate_source_references = false;
    handler->configure(config);

    SECTION("valid SEG is stored successfully") {
        auto seg = create_seg_dataset(
            "1.2.3.4.5.6.7.8.10",
            "1.2.3.4.5.6.1",
            "1.2.3.4.5.6.3");

        auto result = handler->receive_segmentation(seg);
        CHECK(result.is_ok());
        CHECK(storage->exists("1.2.3.4.5.6.7.8.10"));
    }

    SECTION("SEG missing required tags is rejected") {
        dicom_dataset seg;
        seg.set_string(tags::sop_class_uid, encoding::vr_type::UI,
                       "1.2.840.10008.5.1.4.1.1.66.4");

        auto result = handler->receive_segmentation(seg);
        CHECK(result.is_err());
    }

    SECTION("non-SEG SOP class is rejected") {
        auto not_seg = create_sr_dataset("1.2.3.4.5", "1.2.3.1", "1.2.3.2");

        auto result = handler->receive_segmentation(not_seg);
        CHECK(result.is_err());
    }
}

TEST_CASE("ai_result_handler validate_segmentation", "[ai][handler][seg]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    SECTION("valid BINARY segmentation passes") {
        auto seg = create_seg_dataset("1.2.3.4.5", "1.2.3.1", "1.2.3.2");

        auto result = handler->validate_segmentation(seg);
        CHECK(result.status == validation_status::valid);
    }

    SECTION("valid FRACTIONAL segmentation passes") {
        auto seg = create_seg_dataset("1.2.3.4.5", "1.2.3.1", "1.2.3.2");
        seg.set_string(dicom_tag(0x0062, 0x0001), encoding::vr_type::CS, "FRACTIONAL");

        auto result = handler->validate_segmentation(seg);
        CHECK(result.status == validation_status::valid);
    }

    SECTION("invalid segmentation type is rejected") {
        auto seg = create_seg_dataset("1.2.3.4.5", "1.2.3.1", "1.2.3.2");
        seg.set_string(dicom_tag(0x0062, 0x0001), encoding::vr_type::CS, "INVALID");

        auto result = handler->validate_segmentation(seg);
        CHECK(result.status == validation_status::invalid_segment_data);
    }
}

// ============================================================================
// Presentation State Tests
// ============================================================================

TEST_CASE("ai_result_handler receive_presentation_state", "[ai][handler][pr]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    ai_handler_config config;
    config.validate_source_references = false;
    handler->configure(config);

    SECTION("valid PR is stored successfully") {
        auto pr = create_pr_dataset(
            "1.2.3.4.5.6.7.8.11",
            "1.2.3.4.5.6.1",
            "1.2.3.4.5.6.4");

        auto result = handler->receive_presentation_state(pr);
        CHECK(result.is_ok());
        CHECK(storage->exists("1.2.3.4.5.6.7.8.11"));
    }

    SECTION("PR missing required tags is rejected") {
        dicom_dataset pr;
        pr.set_string(tags::sop_class_uid, encoding::vr_type::UI,
                      "1.2.840.10008.5.1.4.1.1.11.1");

        auto result = handler->receive_presentation_state(pr);
        CHECK(result.is_err());
    }

    SECTION("non-PR SOP class is rejected") {
        auto not_pr = create_sr_dataset("1.2.3.4.5", "1.2.3.1", "1.2.3.2");

        auto result = handler->receive_presentation_state(not_pr);
        CHECK(result.is_err());
    }
}

// ============================================================================
// Source Linking Tests
// ============================================================================

TEST_CASE("ai_result_handler source linking", "[ai][handler][linking]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    ai_handler_config config;
    config.validate_source_references = false;
    handler->configure(config);

    const std::string sop_uid = "1.2.3.4.5.6.7.8.9";
    const std::string study_uid = "1.2.3.4.5.6.1";
    const std::string series_uid = "1.2.3.4.5.6.2";

    // Store a test SR
    auto sr = create_sr_dataset(sop_uid, study_uid, series_uid);
    handler->receive_structured_report(sr);

    SECTION("auto link to source creates reference") {
        auto ref_result = handler->get_source_reference(sop_uid);
        CHECK(ref_result.is_ok());
        CHECK(ref_result.value().study_instance_uid == study_uid);
    }

    SECTION("manual link_to_source updates reference") {
        const std::string new_study_uid = "1.2.3.4.5.6.99";

        auto result = handler->link_to_source(sop_uid, new_study_uid);
        CHECK(result.is_ok());

        auto ref_result = handler->get_source_reference(sop_uid);
        CHECK(ref_result.is_ok());
        CHECK(ref_result.value().study_instance_uid == new_study_uid);
    }

    SECTION("link_to_source with full reference") {
        source_reference ref;
        ref.study_instance_uid = "1.2.3.4.5.100";
        ref.series_instance_uid = "1.2.3.4.5.101";
        ref.sop_instance_uids = {"1.2.3.4.5.102", "1.2.3.4.5.103"};

        auto result = handler->link_to_source(sop_uid, ref);
        CHECK(result.is_ok());

        auto ref_result = handler->get_source_reference(sop_uid);
        CHECK(ref_result.is_ok());
        CHECK(ref_result.value().study_instance_uid == "1.2.3.4.5.100");
        CHECK(ref_result.value().series_instance_uid.value() == "1.2.3.4.5.101");
        CHECK(ref_result.value().sop_instance_uids.size() == 2);
    }

    SECTION("link_to_source for non-existent result fails") {
        auto result = handler->link_to_source("non.existent.uid", "1.2.3");
        CHECK(result.is_err());
    }
}

// ============================================================================
// Query Operations Tests
// ============================================================================

TEST_CASE("ai_result_handler query operations", "[ai][handler][query]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    ai_handler_config config;
    config.validate_source_references = false;
    handler->configure(config);

    const std::string study_uid = "1.2.3.4.5.6.1";

    // Store multiple AI results for the same study
    handler->receive_structured_report(
        create_sr_dataset("1.2.3.4.5.6.7.1", study_uid, "1.2.3.4.5.6.2.1"));
    handler->receive_segmentation(
        create_seg_dataset("1.2.3.4.5.6.7.2", study_uid, "1.2.3.4.5.6.2.2"));
    handler->receive_presentation_state(
        create_pr_dataset("1.2.3.4.5.6.7.3", study_uid, "1.2.3.4.5.6.2.3"));

    // Store another result for a different study
    handler->receive_structured_report(
        create_sr_dataset("1.2.3.4.5.6.7.4", "1.2.3.4.5.99", "1.2.3.4.5.6.2.4"));

    SECTION("find_ai_results_for_study returns all results for study") {
        auto result = handler->find_ai_results_for_study(study_uid);
        CHECK(result.is_ok());
        CHECK(result.value().size() == 3);
    }

    SECTION("find_ai_results_by_type filters by type") {
        auto sr_results = handler->find_ai_results_by_type(
            study_uid, ai_result_type::structured_report);
        CHECK(sr_results.is_ok());
        CHECK(sr_results.value().size() == 1);

        auto seg_results = handler->find_ai_results_by_type(
            study_uid, ai_result_type::segmentation);
        CHECK(seg_results.is_ok());
        CHECK(seg_results.value().size() == 1);

        auto pr_results = handler->find_ai_results_by_type(
            study_uid, ai_result_type::presentation_state);
        CHECK(pr_results.is_ok());
        CHECK(pr_results.value().size() == 1);
    }

    SECTION("get_ai_result_info returns info for stored result") {
        auto info = handler->get_ai_result_info("1.2.3.4.5.6.7.1");
        REQUIRE(info.has_value());
        CHECK(info->sop_instance_uid == "1.2.3.4.5.6.7.1");
        CHECK(info->type == ai_result_type::structured_report);
        CHECK(info->source_study_uid == study_uid);
    }

    SECTION("get_ai_result_info returns empty for non-existent") {
        auto info = handler->get_ai_result_info("non.existent.uid");
        CHECK_FALSE(info.has_value());
    }

    SECTION("exists returns true for stored results") {
        CHECK(handler->exists("1.2.3.4.5.6.7.1"));
        CHECK(handler->exists("1.2.3.4.5.6.7.2"));
        CHECK(handler->exists("1.2.3.4.5.6.7.3"));
    }

    SECTION("exists returns false for non-existent results") {
        CHECK_FALSE(handler->exists("non.existent.uid"));
    }
}

// ============================================================================
// Removal Operations Tests
// ============================================================================

TEST_CASE("ai_result_handler removal operations", "[ai][handler][removal]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    ai_handler_config config;
    config.validate_source_references = false;
    handler->configure(config);

    const std::string study_uid = "1.2.3.4.5.6.1";
    const std::string sop_uid = "1.2.3.4.5.6.7.8.9";

    // Store a test SR
    auto sr = create_sr_dataset(sop_uid, study_uid, "1.2.3.4.5.6.2");
    handler->receive_structured_report(sr);

    SECTION("remove deletes AI result") {
        CHECK(handler->exists(sop_uid));

        auto result = handler->remove(sop_uid);
        CHECK(result.is_ok());
        CHECK_FALSE(handler->exists(sop_uid));
    }

    SECTION("remove_ai_results_for_study removes all results") {
        // Add more results for the same study
        handler->receive_segmentation(
            create_seg_dataset("1.2.3.4.5.6.7.10", study_uid, "1.2.3.4.5.6.3"));

        auto result = handler->remove_ai_results_for_study(study_uid);
        CHECK(result.is_ok());
        CHECK(result.value() == 2);

        auto remaining = handler->find_ai_results_for_study(study_uid);
        CHECK(remaining.is_ok());
        CHECK(remaining.value().empty());
    }

    SECTION("remove for non-existent result succeeds") {
        auto result = handler->remove("non.existent.uid");
        CHECK(result.is_ok());
    }
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_CASE("ai_result_handler callbacks", "[ai][handler][callbacks]") {
    auto storage = std::make_shared<mock_storage>();
    auto handler = ai_result_handler::create(storage, nullptr);

    ai_handler_config config;
    config.validate_source_references = false;
    handler->configure(config);

    SECTION("received callback is called on store") {
        bool callback_called = false;
        std::string received_uid;

        handler->set_received_callback([&](const ai_result_info& info) {
            callback_called = true;
            received_uid = info.sop_instance_uid;
        });

        auto sr = create_sr_dataset("1.2.3.4.5.6.7.8.9", "1.2.3.4.5.6.1", "1.2.3.4.5.6.2");
        handler->receive_structured_report(sr);

        CHECK(callback_called);
        CHECK(received_uid == "1.2.3.4.5.6.7.8.9");
    }

    SECTION("pre_store_validator can reject storage") {
        handler->set_pre_store_validator([](const core::dicom_dataset&, ai_result_type) {
            return false;  // Reject all
        });

        auto sr = create_sr_dataset("1.2.3.4.5.6.7.8.9", "1.2.3.4.5.6.1", "1.2.3.4.5.6.2");
        auto result = handler->receive_structured_report(sr);

        CHECK(result.is_err());
        CHECK_FALSE(storage->exists("1.2.3.4.5.6.7.8.9"));
    }

    SECTION("pre_store_validator can accept storage") {
        handler->set_pre_store_validator([](const core::dicom_dataset&, ai_result_type) {
            return true;  // Accept all
        });

        auto sr = create_sr_dataset("1.2.3.4.5.6.7.8.9", "1.2.3.4.5.6.1", "1.2.3.4.5.6.2");
        auto result = handler->receive_structured_report(sr);

        CHECK(result.is_ok());
        CHECK(storage->exists("1.2.3.4.5.6.7.8.9"));
    }
}
