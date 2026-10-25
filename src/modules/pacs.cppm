// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs.cppm
 * @brief Primary C++20 module for pacs_system.
 *
 * This is the main module interface for the pacs_system library.
 * It aggregates all module partitions to provide a single import point.
 * The partitions re-export the names that the public headers declare, in
 * the same kcenon::pacs namespaces.
 *
 * Usage:
 * @code
 * import kcenon.pacs;
 *
 * namespace core = kcenon::pacs::core;
 * namespace encoding = kcenon::pacs::encoding;
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
 * - kcenon.pacs:network - DICOM network protocol (PDU, Association)
 * - kcenon.pacs:network_dimse - DIMSE messages, command fields, status checks
 * - kcenon.pacs:services - SCP/SCU service implementations
 * - kcenon.pacs:services_cache - Query cache, cursors, streaming
 * - kcenon.pacs:services_validation - IOD validators
 * - kcenon.pacs:workflow - Task scheduling and prefetching
 * - kcenon.pacs:web - REST API and DICOMweb endpoints
 * - kcenon.pacs:integration - External system adapters
 * - kcenon.pacs:ai - AI/ML service integration
 * - kcenon.pacs:monitoring - Health checks and metrics
 * - kcenon.pacs:di - Dependency injection utilities
 *
 * The storage, workflow, web, integration, ai, and monitoring partitions
 * export their names only when the matching library target is built
 * (PACS_MODULES_WITH_<NAME>, see cmake/targets.cmake); otherwise they are
 * empty.
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
export import :network_dimse;

// Tier 4: Services (depends on core, network, storage)
export import :services;
export import :services_cache;
export import :services_validation;

// Tier 5: Application (depends on services)
export import :workflow;
export import :web;
export import :integration;

// Tier 6: Optional features
export import :ai;
export import :monitoring;
export import :di;

export namespace kcenon::pacs {

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

} // namespace kcenon::pacs
