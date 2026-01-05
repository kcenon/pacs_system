// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs.cppm
 * @brief Primary C++20 module for pacs_system.
 *
 * This is the main module interface for the pacs_system library.
 * It aggregates all module partitions to provide a single import point.
 *
 * Usage:
 * @code
 * import kcenon.pacs;
 *
 * using namespace pacs;
 *
 * // Create a DICOM dataset
 * core::dicom_dataset ds;
 * ds.set_string(core::tags::patient_name, encoding::vr_type::PN, "DOE^JOHN");
 * @endcode
 *
 * Module Structure:
 * - kcenon.pacs:core - DICOM core types (dicom_tag, dicom_element, dicom_dataset)
 * - kcenon.pacs:encoding - Transfer syntax and compression codecs
 * - kcenon.pacs:storage - Storage backends and database adapters
 * - kcenon.pacs:security - RBAC, encryption, anonymization
 * - kcenon.pacs:network - DICOM network protocol (PDU, Association, DIMSE)
 * - kcenon.pacs:services - SCP service implementations
 * - kcenon.pacs:workflow - Task scheduling and prefetching
 * - kcenon.pacs:web - REST API and DICOMweb endpoints
 * - kcenon.pacs:integration - External system adapters
 * - kcenon.pacs:ai - AI/ML service integration (optional)
 * - kcenon.pacs:monitoring - Health checks and metrics
 * - kcenon.pacs:di - Dependency injection utilities
 */

export module kcenon.pacs;

// Tier 1: Core (No internal dependencies)
export import :core;

// Tier 2: Infrastructure (depends on core)
export import :encoding;
export import :storage;
export import :security;

// Tier 3: Protocol (depends on core, encoding)
export import :network;

// Tier 4: Services (depends on core, network, storage)
export import :services;

// Tier 5: Application (depends on services)
export import :workflow;
export import :web;
export import :integration;

// Tier 6: Optional features
#ifdef KCENON_WITH_AI
export import :ai;
#endif

export import :monitoring;
export import :di;

export namespace pacs {

/**
 * @brief Version information for pacs_system module.
 */
struct module_version {
    static constexpr int major = 0;
    static constexpr int minor = 1;
    static constexpr int patch = 0;
    static constexpr int tweak = 0;
    static constexpr const char* string = "0.1.0.0";
    static constexpr const char* module_name = "kcenon.pacs";
};

} // namespace pacs
