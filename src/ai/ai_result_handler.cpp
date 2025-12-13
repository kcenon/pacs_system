/**
 * @file ai_result_handler.cpp
 * @brief Implementation of the AI result handler
 */

#include "pacs/ai/ai_result_handler.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_map>

namespace pacs::ai {

// Use common_system's ok() function
using kcenon::common::ok;
using kcenon::common::make_error;

// =============================================================================
// SOP Class UIDs for AI-related objects
// =============================================================================

namespace sop_class {
// Structured Report SOP Classes
constexpr std::string_view basic_text_sr = "1.2.840.10008.5.1.4.1.1.88.11";
constexpr std::string_view enhanced_sr = "1.2.840.10008.5.1.4.1.1.88.22";
constexpr std::string_view comprehensive_sr = "1.2.840.10008.5.1.4.1.1.88.33";
constexpr std::string_view comprehensive_3d_sr = "1.2.840.10008.5.1.4.1.1.88.34";

// Segmentation SOP Class
constexpr std::string_view segmentation = "1.2.840.10008.5.1.4.1.1.66.4";

// Presentation State SOP Classes
constexpr std::string_view grayscale_softcopy_ps = "1.2.840.10008.5.1.4.1.1.11.1";
constexpr std::string_view color_softcopy_ps = "1.2.840.10008.5.1.4.1.1.11.2";
constexpr std::string_view pseudo_color_softcopy_ps = "1.2.840.10008.5.1.4.1.1.11.3";
constexpr std::string_view blending_softcopy_ps = "1.2.840.10008.5.1.4.1.1.11.4";
}  // namespace sop_class

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/**
 * @brief Determine AI result type from SOP Class UID
 */
[[nodiscard]] auto determine_result_type(std::string_view sop_class_uid)
    -> std::optional<ai_result_type> {
    // Structured Reports
    if (sop_class_uid == sop_class::basic_text_sr ||
        sop_class_uid == sop_class::enhanced_sr ||
        sop_class_uid == sop_class::comprehensive_sr ||
        sop_class_uid == sop_class::comprehensive_3d_sr) {
        return ai_result_type::structured_report;
    }

    // Segmentation
    if (sop_class_uid == sop_class::segmentation) {
        return ai_result_type::segmentation;
    }

    // Presentation States
    if (sop_class_uid == sop_class::grayscale_softcopy_ps ||
        sop_class_uid == sop_class::color_softcopy_ps ||
        sop_class_uid == sop_class::pseudo_color_softcopy_ps ||
        sop_class_uid == sop_class::blending_softcopy_ps) {
        return ai_result_type::presentation_state;
    }

    return std::nullopt;
}

/**
 * @brief Extract algorithm information from dataset
 */
[[nodiscard]] auto extract_algorithm_info(const core::dicom_dataset& dataset)
    -> std::pair<std::string, std::string> {
    std::string name;
    std::string version;

    // Try Content Creator's Name for SR
    name = dataset.get_string(core::dicom_tag(0x0070, 0x0084));  // Content Creator's Name
    if (name.empty()) {
        // Try Manufacturer for general DICOM objects
        name = dataset.get_string(core::dicom_tag(0x0008, 0x0070));  // Manufacturer
    }

    // Try Software Versions
    version = dataset.get_string(core::dicom_tag(0x0018, 0x1020));  // Software Versions

    return {name, version};
}

}  // anonymous namespace

// =============================================================================
// Implementation Class (pimpl)
// =============================================================================

class ai_result_handler::impl {
public:
    impl(std::shared_ptr<storage::storage_interface> storage,
         std::shared_ptr<storage::index_database> database)
        : storage_(std::move(storage))
        , database_(std::move(database)) {}

    // Configuration
    ai_handler_config config_;

    // Callbacks
    ai_result_received_callback received_callback_;
    pre_store_validator pre_store_validator_;

    // Storage and database
    std::shared_ptr<storage::storage_interface> storage_;
    std::shared_ptr<storage::index_database> database_;

    // In-memory cache for AI result metadata (for quick lookups)
    std::map<std::string, ai_result_info> ai_results_cache_;

    // Source linking map: AI result UID -> source references
    std::map<std::string, source_reference> source_links_;

    // =========================================================================
    // Internal Helper Methods
    // =========================================================================

    [[nodiscard]] auto validate_common_tags(const core::dicom_dataset& dataset)
        -> validation_result {
        validation_result result;
        result.status = validation_status::valid;

        // Check required DICOM tags
        std::vector<std::pair<core::dicom_tag, std::string>> required_tags = {
            {core::tags::sop_class_uid, "SOP Class UID"},
            {core::tags::sop_instance_uid, "SOP Instance UID"},
            {core::tags::study_instance_uid, "Study Instance UID"},
            {core::tags::series_instance_uid, "Series Instance UID"},
            {core::tags::modality, "Modality"}
        };

        for (const auto& [tag, name] : required_tags) {
            if (dataset.get_string(tag).empty()) {
                result.missing_tags.push_back(name);
            }
        }

        if (!result.missing_tags.empty()) {
            result.status = validation_status::missing_required_tags;
            result.error_message = "Missing required DICOM tags";
        }

        return result;
    }

    [[nodiscard]] auto validate_source_references_exist(
        const core::dicom_dataset& dataset) -> validation_result {
        validation_result result;
        result.status = validation_status::valid;

        if (!config_.validate_source_references) {
            return result;
        }

        // Get Study Instance UID
        auto study_uid = dataset.get_string(core::tags::study_instance_uid);
        if (study_uid.empty()) {
            result.status = validation_status::invalid_reference;
            result.error_message = "Missing Study Instance UID for reference validation";
            return result;
        }

        // Check if the study exists in the database
        // Note: This would require a study lookup method in index_database
        // For now, we just validate the UID format
        if (study_uid.length() < 10) {  // Basic UID validation
            result.status = validation_status::invalid_reference;
            result.invalid_references.push_back(study_uid);
            result.error_message = "Invalid Study Instance UID format";
        }

        return result;
    }

    [[nodiscard]] auto store_ai_result(
        const core::dicom_dataset& dataset,
        ai_result_type type) -> VoidResult {
        // Store the dataset
        auto store_result = storage_->store(dataset);
        if (store_result.is_err()) {
            return store_result;
        }

        // Extract metadata for caching
        auto sop_instance_uid = dataset.get_string(core::tags::sop_instance_uid);
        auto [algo_name, algo_version] = extract_algorithm_info(dataset);

        ai_result_info info;
        info.sop_instance_uid = sop_instance_uid;
        info.type = type;
        info.sop_class_uid = dataset.get_string(core::tags::sop_class_uid);
        info.series_instance_uid = dataset.get_string(core::tags::series_instance_uid);
        info.source_study_uid = dataset.get_string(core::tags::study_instance_uid);
        info.algorithm_name = algo_name;
        info.algorithm_version = algo_version;
        info.received_at = std::chrono::system_clock::now();

        // Cache the result info
        ai_results_cache_[sop_instance_uid] = info;

        // Auto-link to source if configured
        if (config_.auto_link_to_source && !info.source_study_uid.empty()) {
            source_reference ref;
            ref.study_instance_uid = info.source_study_uid;
            source_links_[sop_instance_uid] = ref;
        }

        // Call notification callback
        if (received_callback_) {
            received_callback_(info);
        }

        return ok();
    }
};

// =============================================================================
// Construction / Destruction
// =============================================================================

auto ai_result_handler::create(
    std::shared_ptr<storage::storage_interface> storage,
    std::shared_ptr<storage::index_database> database)
    -> std::unique_ptr<ai_result_handler> {
    return std::unique_ptr<ai_result_handler>(
        new ai_result_handler(std::move(storage), std::move(database)));
}

ai_result_handler::ai_result_handler(
    std::shared_ptr<storage::storage_interface> storage,
    std::shared_ptr<storage::index_database> database)
    : pimpl_(std::make_unique<impl>(std::move(storage), std::move(database))) {}

ai_result_handler::~ai_result_handler() = default;

ai_result_handler::ai_result_handler(ai_result_handler&&) noexcept = default;

auto ai_result_handler::operator=(ai_result_handler&&) noexcept
    -> ai_result_handler& = default;

// =============================================================================
// Configuration
// =============================================================================

void ai_result_handler::configure(const ai_handler_config& config) {
    pimpl_->config_ = config;
}

auto ai_result_handler::get_config() const -> ai_handler_config {
    return pimpl_->config_;
}

void ai_result_handler::set_received_callback(ai_result_received_callback callback) {
    pimpl_->received_callback_ = std::move(callback);
}

void ai_result_handler::set_pre_store_validator(pre_store_validator validator) {
    pimpl_->pre_store_validator_ = std::move(validator);
}

// =============================================================================
// Structured Report Operations
// =============================================================================

auto ai_result_handler::receive_structured_report(const core::dicom_dataset& sr)
    -> VoidResult {
    // Validate SOP Class
    auto sop_class = sr.get_string(core::tags::sop_class_uid);
    auto type = determine_result_type(sop_class);
    if (!type || *type != ai_result_type::structured_report) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Invalid SOP Class for Structured Report");
#else
        return std::string("Invalid SOP Class for Structured Report");
#endif
    }

    // Run common validation
    auto common_validation = pimpl_->validate_common_tags(sr);
    if (common_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            common_validation.error_message.value_or("Validation failed"));
#else
        return common_validation.error_message.value_or("Validation failed");
#endif
    }

    // Validate SR template if configured
    if (pimpl_->config_.validate_sr_templates) {
        auto template_validation = validate_sr_template(sr);
        if (template_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
            return kcenon::common::error_info(
                template_validation.error_message.value_or("SR template validation failed"));
#else
            return template_validation.error_message.value_or("SR template validation failed");
#endif
        }
    }

    // Validate source references if configured
    auto ref_validation = pimpl_->validate_source_references_exist(sr);
    if (ref_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            ref_validation.error_message.value_or("Source reference validation failed"));
#else
        return ref_validation.error_message.value_or("Source reference validation failed");
#endif
    }

    // Call pre-store validator if set
    if (pimpl_->pre_store_validator_ &&
        !pimpl_->pre_store_validator_(sr, ai_result_type::structured_report)) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Pre-store validation rejected the SR");
#else
        return std::string("Pre-store validation rejected the SR");
#endif
    }

    // Store the SR
    return pimpl_->store_ai_result(sr, ai_result_type::structured_report);
}

auto ai_result_handler::validate_sr_template(const core::dicom_dataset& sr)
    -> validation_result {
    validation_result result;
    result.status = validation_status::valid;

    // Note: Content Sequence (0040,A730) validation would be done here
    // for full SR template conformance checking

    // Check for Template Identifier if configured
    if (!pimpl_->config_.accepted_sr_templates.empty()) {
        // Tag (0040,DB00) Template Identifier
        const core::dicom_tag template_id_tag(0x0040, 0xDB00);
        auto template_id = sr.get_string(template_id_tag);

        bool template_found = false;
        for (const auto& accepted : pimpl_->config_.accepted_sr_templates) {
            if (template_id == accepted) {
                template_found = true;
                break;
            }
        }

        if (!template_found && !template_id.empty()) {
            result.status = validation_status::invalid_template;
            result.error_message = "SR template not in accepted list: " + template_id;
        }
    }

    return result;
}

auto ai_result_handler::get_cad_findings(std::string_view sr_sop_instance_uid)
    -> Result<std::vector<cad_finding>> {
    // Retrieve the SR dataset
    auto retrieve_result = pimpl_->storage_->retrieve(sr_sop_instance_uid);
    if (retrieve_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Failed to retrieve SR: " +
            std::string(sr_sop_instance_uid));
#else
        return std::string("Failed to retrieve SR: ") + std::string(sr_sop_instance_uid);
#endif
    }

    // Parse CAD findings from Content Sequence
    // This is a simplified implementation - full parsing would handle
    // the complete SR tree structure
    std::vector<cad_finding> findings;

    // Note: Actual implementation would parse the Content Sequence
    // recursively to extract CAD findings based on the SR template

    return findings;
}

// =============================================================================
// Segmentation Operations
// =============================================================================

auto ai_result_handler::receive_segmentation(const core::dicom_dataset& seg)
    -> VoidResult {
    // Validate SOP Class
    auto sop_class = seg.get_string(core::tags::sop_class_uid);
    auto type = determine_result_type(sop_class);
    if (!type || *type != ai_result_type::segmentation) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Invalid SOP Class for Segmentation");
#else
        return std::string("Invalid SOP Class for Segmentation");
#endif
    }

    // Run common validation
    auto common_validation = pimpl_->validate_common_tags(seg);
    if (common_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            common_validation.error_message.value_or("Validation failed"));
#else
        return common_validation.error_message.value_or("Validation failed");
#endif
    }

    // Validate segmentation data
    auto seg_validation = validate_segmentation(seg);
    if (seg_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            seg_validation.error_message.value_or("Segmentation validation failed"));
#else
        return seg_validation.error_message.value_or("Segmentation validation failed");
#endif
    }

    // Validate source references if configured
    auto ref_validation = pimpl_->validate_source_references_exist(seg);
    if (ref_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            ref_validation.error_message.value_or("Source reference validation failed"));
#else
        return ref_validation.error_message.value_or("Source reference validation failed");
#endif
    }

    // Call pre-store validator if set
    if (pimpl_->pre_store_validator_ &&
        !pimpl_->pre_store_validator_(seg, ai_result_type::segmentation)) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Pre-store validation rejected the segmentation");
#else
        return std::string("Pre-store validation rejected the segmentation");
#endif
    }

    // Store the SEG
    return pimpl_->store_ai_result(seg, ai_result_type::segmentation);
}

auto ai_result_handler::validate_segmentation(const core::dicom_dataset& seg)
    -> validation_result {
    validation_result result;
    result.status = validation_status::valid;

    // Note: Segment Sequence (0062,0002) validation would be done here
    // for full segmentation conformance checking

    // Check Segmentation Type
    // Tag (0062,0001) Segmentation Type
    const core::dicom_tag seg_type_tag(0x0062, 0x0001);
    auto seg_type = seg.get_string(seg_type_tag);

    if (seg_type != "BINARY" && seg_type != "FRACTIONAL" && !seg_type.empty()) {
        result.status = validation_status::invalid_segment_data;
        result.error_message = "Invalid segmentation type: " + seg_type;
        return result;
    }

    // Check segment count if max_segments is configured
    if (pimpl_->config_.max_segments > 0) {
        // Tag (0062,0004) Number of Segments
        // This would need proper implementation based on actual tag
    }

    return result;
}

auto ai_result_handler::get_segment_info(std::string_view seg_sop_instance_uid)
    -> Result<std::vector<segment_info>> {
    // Retrieve the SEG dataset
    auto retrieve_result = pimpl_->storage_->retrieve(seg_sop_instance_uid);
    if (retrieve_result.is_err()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Failed to retrieve SEG: " +
            std::string(seg_sop_instance_uid));
#else
        return std::string("Failed to retrieve SEG: ") + std::string(seg_sop_instance_uid);
#endif
    }

    // Parse segment information from Segment Sequence
    std::vector<segment_info> segments;

    // Note: Actual implementation would parse the Segment Sequence
    // to extract segment information

    return segments;
}

// =============================================================================
// Presentation State Operations
// =============================================================================

auto ai_result_handler::receive_presentation_state(const core::dicom_dataset& pr)
    -> VoidResult {
    // Validate SOP Class
    auto sop_class = pr.get_string(core::tags::sop_class_uid);
    auto type = determine_result_type(sop_class);
    if (!type || *type != ai_result_type::presentation_state) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Invalid SOP Class for Presentation State");
#else
        return std::string("Invalid SOP Class for Presentation State");
#endif
    }

    // Run common validation
    auto common_validation = pimpl_->validate_common_tags(pr);
    if (common_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            common_validation.error_message.value_or("Validation failed"));
#else
        return common_validation.error_message.value_or("Validation failed");
#endif
    }

    // Validate presentation state
    auto pr_validation = validate_presentation_state(pr);
    if (pr_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            pr_validation.error_message.value_or("Presentation state validation failed"));
#else
        return pr_validation.error_message.value_or("Presentation state validation failed");
#endif
    }

    // Validate source references if configured
    auto ref_validation = pimpl_->validate_source_references_exist(pr);
    if (ref_validation.status != validation_status::valid) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info(
            ref_validation.error_message.value_or("Source reference validation failed"));
#else
        return ref_validation.error_message.value_or("Source reference validation failed");
#endif
    }

    // Call pre-store validator if set
    if (pimpl_->pre_store_validator_ &&
        !pimpl_->pre_store_validator_(pr, ai_result_type::presentation_state)) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("Pre-store validation rejected the presentation state");
#else
        return std::string("Pre-store validation rejected the presentation state");
#endif
    }

    // Store the PR
    return pimpl_->store_ai_result(pr, ai_result_type::presentation_state);
}

auto ai_result_handler::validate_presentation_state(const core::dicom_dataset& pr)
    -> validation_result {
    validation_result result;
    result.status = validation_status::valid;

    // Note: Referenced Series Sequence (0008,1115) validation would be done here
    // for full presentation state conformance checking

    // Basic validation - ensure presentation state has valid SOP Class UID
    auto sop_class = pr.get_string(core::tags::sop_class_uid);
    if (sop_class.empty()) {
        result.status = validation_status::missing_required_tags;
        result.missing_tags.push_back("SOP Class UID");
    }

    return result;
}

// =============================================================================
// Source Linking Operations
// =============================================================================

auto ai_result_handler::link_to_source(
    std::string_view result_uid,
    std::string_view source_study_uid) -> VoidResult {
    source_reference ref;
    ref.study_instance_uid = std::string(source_study_uid);
    return link_to_source(result_uid, ref);
}

auto ai_result_handler::link_to_source(
    std::string_view result_uid,
    const source_reference& references) -> VoidResult {
    // Verify the AI result exists
    auto it = pimpl_->ai_results_cache_.find(std::string(result_uid));
    if (it == pimpl_->ai_results_cache_.end()) {
        // Try to check storage
        if (!pimpl_->storage_->exists(result_uid)) {
#ifdef PACS_WITH_COMMON_SYSTEM
            return kcenon::common::error_info("AI result not found: " +
                std::string(result_uid));
#else
            return std::string("AI result not found: ") + std::string(result_uid);
#endif
        }
    }

    // Store the source link
    pimpl_->source_links_[std::string(result_uid)] = references;

    // Update cached info if present
    if (it != pimpl_->ai_results_cache_.end()) {
        it->second.source_study_uid = references.study_instance_uid;
    }

    return ok();
}

auto ai_result_handler::get_source_reference(std::string_view result_uid)
    -> Result<source_reference> {
    auto it = pimpl_->source_links_.find(std::string(result_uid));
    if (it == pimpl_->source_links_.end()) {
#ifdef PACS_WITH_COMMON_SYSTEM
        return kcenon::common::error_info("No source reference found for: " +
            std::string(result_uid));
#else
        return std::string("No source reference found for: ") + std::string(result_uid);
#endif
    }
    return it->second;
}

// =============================================================================
// Query Operations
// =============================================================================

auto ai_result_handler::find_ai_results_for_study(std::string_view study_instance_uid)
    -> Result<std::vector<ai_result_info>> {
    std::vector<ai_result_info> results;

    for (const auto& [uid, info] : pimpl_->ai_results_cache_) {
        if (info.source_study_uid == study_instance_uid) {
            results.push_back(info);
        }
    }

    // Also check source links
    for (const auto& [uid, ref] : pimpl_->source_links_) {
        if (ref.study_instance_uid == study_instance_uid) {
            auto it = pimpl_->ai_results_cache_.find(uid);
            if (it != pimpl_->ai_results_cache_.end()) {
                // Already included from cache
                continue;
            }
            // Would need to retrieve from storage for complete info
        }
    }

    return results;
}

auto ai_result_handler::find_ai_results_by_type(
    std::string_view study_instance_uid,
    ai_result_type type) -> Result<std::vector<ai_result_info>> {
    std::vector<ai_result_info> results;

    for (const auto& [uid, info] : pimpl_->ai_results_cache_) {
        if (info.source_study_uid == study_instance_uid && info.type == type) {
            results.push_back(info);
        }
    }

    return results;
}

auto ai_result_handler::get_ai_result_info(std::string_view sop_instance_uid)
    -> std::optional<ai_result_info> {
    auto it = pimpl_->ai_results_cache_.find(std::string(sop_instance_uid));
    if (it != pimpl_->ai_results_cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ai_result_handler::exists(std::string_view sop_instance_uid) const -> bool {
    // Check cache first
    if (pimpl_->ai_results_cache_.find(std::string(sop_instance_uid)) !=
        pimpl_->ai_results_cache_.end()) {
        return true;
    }

    // Check storage
    return pimpl_->storage_->exists(sop_instance_uid);
}

// =============================================================================
// Removal Operations
// =============================================================================

auto ai_result_handler::remove(std::string_view sop_instance_uid) -> VoidResult {
    std::string uid_str(sop_instance_uid);

    // Remove from storage
    auto remove_result = pimpl_->storage_->remove(sop_instance_uid);
    if (remove_result.is_err()) {
        return remove_result;
    }

    // Remove from cache
    pimpl_->ai_results_cache_.erase(uid_str);

    // Remove source links
    pimpl_->source_links_.erase(uid_str);

    return ok();
}

auto ai_result_handler::remove_ai_results_for_study(std::string_view study_instance_uid)
    -> Result<std::size_t> {
    std::size_t removed_count = 0;

    // Find all AI results for this study
    std::vector<std::string> to_remove;
    for (const auto& [uid, info] : pimpl_->ai_results_cache_) {
        if (info.source_study_uid == study_instance_uid) {
            to_remove.push_back(uid);
        }
    }

    // Remove each one
    for (const auto& uid : to_remove) {
        auto result = remove(uid);
        if (result.is_ok()) {
            ++removed_count;
        }
    }

    return removed_count;
}

}  // namespace pacs::ai
