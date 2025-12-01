/**
 * @file sop_class_registry.hpp
 * @brief Central registry for all supported SOP Classes
 *
 * This file provides a centralized registry for managing and querying
 * supported DICOM SOP Classes across all modalities and service types.
 *
 * @see DICOM PS3.4 - Service Class Specifications
 * @see DICOM PS3.6 - Data Dictionary (SOP Class UIDs)
 */

#ifndef PACS_SERVICES_SOP_CLASS_REGISTRY_HPP
#define PACS_SERVICES_SOP_CLASS_REGISTRY_HPP

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pacs::services {

// =============================================================================
// SOP Class Categories
// =============================================================================

/**
 * @brief Category of SOP Class
 */
enum class sop_class_category {
    storage,            ///< Storage Service Class
    query_retrieve,     ///< Query/Retrieve Service Class
    worklist,           ///< Modality Worklist Service Class
    mpps,               ///< Modality Performed Procedure Step
    verification,       ///< Verification Service Class
    print,              ///< Print Management Service Class
    media,              ///< Media Storage Service Class
    other               ///< Other service classes
};

/**
 * @brief Modality type for storage SOP classes
 */
enum class modality_type {
    ct,         ///< Computed Tomography
    mr,         ///< Magnetic Resonance
    us,         ///< Ultrasound
    xa,         ///< X-Ray Angiographic
    xrf,        ///< X-Ray Radiofluoroscopic
    cr,         ///< Computed Radiography
    dx,         ///< Digital Radiography
    mg,         ///< Mammography
    nm,         ///< Nuclear Medicine
    pet,        ///< Positron Emission Tomography
    rt,         ///< Radiation Therapy
    sc,         ///< Secondary Capture
    sr,         ///< Structured Report
    other       ///< Other modalities
};

// =============================================================================
// SOP Class Information
// =============================================================================

/**
 * @brief Complete information about a SOP Class
 */
struct sop_class_info {
    std::string_view uid;               ///< SOP Class UID
    std::string_view name;              ///< Human-readable name
    sop_class_category category;        ///< Service class category
    modality_type modality;             ///< Modality (for storage classes)
    bool is_retired;                    ///< Whether this SOP class is retired
    bool supports_multiframe;           ///< Multi-frame support (for storage)
};

// =============================================================================
// SOP Class Registry
// =============================================================================

/**
 * @brief Central registry for SOP Classes
 *
 * Provides a unified interface for querying and managing SOP Classes
 * across all service types. Supports filtering by category, modality,
 * and other criteria.
 *
 * @example
 * @code
 * sop_class_registry registry;
 *
 * // Get all US storage classes
 * auto us_classes = registry.get_by_modality(modality_type::us);
 *
 * // Check if a UID is supported
 * if (registry.is_supported("1.2.840.10008.5.1.4.1.1.6.1")) {
 *     auto info = registry.get_info("1.2.840.10008.5.1.4.1.1.6.1");
 *     std::cout << info->name << "\n";
 * }
 * @endcode
 */
class sop_class_registry {
public:
    /**
     * @brief Get the singleton instance
     */
    [[nodiscard]] static sop_class_registry& instance();

    /**
     * @brief Check if a SOP Class UID is supported
     * @param uid The SOP Class UID to check
     * @return true if the SOP Class is registered
     */
    [[nodiscard]] bool is_supported(std::string_view uid) const;

    /**
     * @brief Get information about a SOP Class
     * @param uid The SOP Class UID
     * @return Pointer to info, or nullptr if not found
     */
    [[nodiscard]] const sop_class_info* get_info(std::string_view uid) const;

    /**
     * @brief Get all SOP Classes in a category
     * @param category The category to filter by
     * @return Vector of SOP Class UIDs
     */
    [[nodiscard]] std::vector<std::string>
    get_by_category(sop_class_category category) const;

    /**
     * @brief Get all storage SOP Classes for a modality
     * @param modality The modality type
     * @param include_retired Include retired SOP classes
     * @return Vector of SOP Class UIDs
     */
    [[nodiscard]] std::vector<std::string>
    get_by_modality(modality_type modality, bool include_retired = true) const;

    /**
     * @brief Get all storage SOP Classes
     * @param include_retired Include retired SOP classes
     * @return Vector of Storage SOP Class UIDs
     */
    [[nodiscard]] std::vector<std::string>
    get_all_storage_classes(bool include_retired = true) const;

    /**
     * @brief Get all registered SOP Class UIDs
     * @return Vector of all SOP Class UIDs
     */
    [[nodiscard]] std::vector<std::string> get_all() const;

    /**
     * @brief Register a new SOP Class
     *
     * Used to add custom or new SOP Classes dynamically.
     *
     * @param info The SOP Class information
     * @return true if registration succeeded
     */
    bool register_sop_class(const sop_class_info& info);

    /**
     * @brief Get the modality string for a modality type
     * @param modality The modality type
     * @return DICOM modality code (e.g., "US", "CT", "MR")
     */
    [[nodiscard]] static std::string_view
    modality_to_string(modality_type modality) noexcept;

    /**
     * @brief Parse a modality string to enum
     * @param modality The DICOM modality code
     * @return The modality type, or modality_type::other if unknown
     */
    [[nodiscard]] static modality_type
    parse_modality(std::string_view modality) noexcept;

private:
    sop_class_registry();
    ~sop_class_registry() = default;

    // Non-copyable, non-movable singleton
    sop_class_registry(const sop_class_registry&) = delete;
    sop_class_registry& operator=(const sop_class_registry&) = delete;
    sop_class_registry(sop_class_registry&&) = delete;
    sop_class_registry& operator=(sop_class_registry&&) = delete;

    void register_standard_sop_classes();
    void register_us_sop_classes();
    void register_ct_sop_classes();
    void register_mr_sop_classes();
    void register_other_sop_classes();

    std::unordered_map<std::string, sop_class_info> registry_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Check if a SOP Class UID is a storage class
 * @param uid The SOP Class UID
 * @return true if this is any storage SOP class
 */
[[nodiscard]] bool is_storage_sop_class(std::string_view uid);

/**
 * @brief Get the modality for a storage SOP Class
 * @param uid The SOP Class UID
 * @return The modality type, or modality_type::other if not a storage class
 */
[[nodiscard]] modality_type get_storage_modality(std::string_view uid);

/**
 * @brief Get human-readable name for a SOP Class
 * @param uid The SOP Class UID
 * @return The SOP Class name, or "Unknown" if not found
 */
[[nodiscard]] std::string_view get_sop_class_name(std::string_view uid);

}  // namespace pacs::services

#endif  // PACS_SERVICES_SOP_CLASS_REGISTRY_HPP
