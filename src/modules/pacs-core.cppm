// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file pacs-core.cppm
 * @brief C++20 module partition for DICOM core types.
 *
 * This module partition exports the fundamental DICOM data types:
 * - dicom_tag: Tag representation (Group, Element pairs)
 * - dicom_element: Data Element (Tag, VR, Value)
 * - dicom_dataset: Ordered collection of Data Elements
 * - dicom_file: DICOM file parsing and serialization
 * - dicom_dictionary: Tag definitions and lookup
 * - tag_info: Tag metadata
 * - pool_manager: Resource pooling utilities
 * - events: Event definitions
 * - Result<T>: Error handling pattern
 *
 * Part of the kcenon.pacs module.
 */

module;

// Standard library imports needed before module declaration
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// PACS core headers
#include <kcenon/pacs/core/dicom_tag.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/core/tag_info.h>
#include <kcenon/pacs/core/dicom_dictionary.h>
#include <kcenon/pacs/core/dicom_element.h>
#include <kcenon/pacs/core/dicom_dataset.h>
#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/pool_manager.h>
#include <kcenon/pacs/core/events.h>
#include <kcenon/pacs/core/result.h>

export module kcenon.pacs:core;

// ============================================================================
// Re-export kcenon::pacs::core namespace
// ============================================================================

export namespace kcenon::pacs::core {

// Tag types
using kcenon::pacs::core::dicom_tag;

// Tag info
using kcenon::pacs::core::value_multiplicity;
using kcenon::pacs::core::tag_info;

// Dictionary
using kcenon::pacs::core::dicom_dictionary;

// Element types
using kcenon::pacs::core::dicom_element;

// Dataset types
using kcenon::pacs::core::dicom_dataset;

// File types
using kcenon::pacs::core::dicom_file;

// Pool manager
using kcenon::pacs::core::pool_statistics;
using kcenon::pacs::core::tracked_pool;
using kcenon::pacs::core::pool_manager;
using kcenon::pacs::core::make_pooled_element;
using kcenon::pacs::core::make_pooled_dataset;

} // namespace kcenon::pacs::core

// ============================================================================
// Re-export kcenon::pacs::core::tags namespace
// ============================================================================
// A namespace cannot be re-exported as a whole, so every constant in
// dicom_tag_constants.h is listed here.

export namespace kcenon::pacs::core::tags {

// File Meta Information (Group 0x0002)
using kcenon::pacs::core::tags::file_meta_information_group_length;
using kcenon::pacs::core::tags::file_meta_information_version;
using kcenon::pacs::core::tags::media_storage_sop_class_uid;
using kcenon::pacs::core::tags::media_storage_sop_instance_uid;
using kcenon::pacs::core::tags::transfer_syntax_uid;
using kcenon::pacs::core::tags::implementation_class_uid;
using kcenon::pacs::core::tags::implementation_version_name;
using kcenon::pacs::core::tags::source_application_entity_title;
using kcenon::pacs::core::tags::private_information_creator_uid;
using kcenon::pacs::core::tags::private_information;

// Command Group (Group 0x0000) - Used in DIMSE messages
using kcenon::pacs::core::tags::command_group_length;
using kcenon::pacs::core::tags::affected_sop_class_uid;
using kcenon::pacs::core::tags::requested_sop_class_uid;
using kcenon::pacs::core::tags::command_field;
using kcenon::pacs::core::tags::message_id;
using kcenon::pacs::core::tags::message_id_being_responded_to;
using kcenon::pacs::core::tags::move_destination;
using kcenon::pacs::core::tags::priority;
using kcenon::pacs::core::tags::command_data_set_type;
using kcenon::pacs::core::tags::status;
using kcenon::pacs::core::tags::affected_sop_instance_uid;
using kcenon::pacs::core::tags::number_of_remaining_suboperations;
using kcenon::pacs::core::tags::number_of_completed_suboperations;
using kcenon::pacs::core::tags::number_of_failed_suboperations;
using kcenon::pacs::core::tags::number_of_warning_suboperations;

// SOP Common (Group 0x0008)
using kcenon::pacs::core::tags::specific_character_set;
using kcenon::pacs::core::tags::image_type;
using kcenon::pacs::core::tags::instance_creation_date;
using kcenon::pacs::core::tags::instance_creation_time;
using kcenon::pacs::core::tags::instance_creator_uid;
using kcenon::pacs::core::tags::sop_class_uid;
using kcenon::pacs::core::tags::sop_instance_uid;
using kcenon::pacs::core::tags::study_date;
using kcenon::pacs::core::tags::series_date;
using kcenon::pacs::core::tags::acquisition_date;
using kcenon::pacs::core::tags::content_date;
using kcenon::pacs::core::tags::study_time;
using kcenon::pacs::core::tags::series_time;
using kcenon::pacs::core::tags::acquisition_time;
using kcenon::pacs::core::tags::content_time;
using kcenon::pacs::core::tags::accession_number;
using kcenon::pacs::core::tags::query_retrieve_level;
using kcenon::pacs::core::tags::retrieve_ae_title;
using kcenon::pacs::core::tags::instance_availability;
using kcenon::pacs::core::tags::modality;
using kcenon::pacs::core::tags::modalities_in_study;
using kcenon::pacs::core::tags::conversion_type;
using kcenon::pacs::core::tags::manufacturer;
using kcenon::pacs::core::tags::institution_name;
using kcenon::pacs::core::tags::institution_address;
using kcenon::pacs::core::tags::referring_physician_name;
using kcenon::pacs::core::tags::station_name;
using kcenon::pacs::core::tags::study_description;
using kcenon::pacs::core::tags::series_description;
using kcenon::pacs::core::tags::performing_physician_name;
using kcenon::pacs::core::tags::name_of_physicians_reading_study;
using kcenon::pacs::core::tags::operators_name;
using kcenon::pacs::core::tags::manufacturers_model_name;
using kcenon::pacs::core::tags::referenced_sop_class_uid;
using kcenon::pacs::core::tags::referenced_sop_instance_uid;
using kcenon::pacs::core::tags::transaction_uid;
using kcenon::pacs::core::tags::failure_reason;
using kcenon::pacs::core::tags::failed_sop_sequence;
using kcenon::pacs::core::tags::referenced_sop_sequence;

// Patient Module (Group 0x0010)
using kcenon::pacs::core::tags::patient_name;
using kcenon::pacs::core::tags::patient_id;
using kcenon::pacs::core::tags::issuer_of_patient_id;
using kcenon::pacs::core::tags::patient_birth_date;
using kcenon::pacs::core::tags::patient_sex;
using kcenon::pacs::core::tags::patient_age;
using kcenon::pacs::core::tags::patient_size;
using kcenon::pacs::core::tags::patient_weight;
using kcenon::pacs::core::tags::patient_address;
using kcenon::pacs::core::tags::patient_comments;

// Study and Series Identification (Group 0x0020)
using kcenon::pacs::core::tags::study_instance_uid;
using kcenon::pacs::core::tags::series_instance_uid;
using kcenon::pacs::core::tags::study_id;
using kcenon::pacs::core::tags::series_number;
using kcenon::pacs::core::tags::acquisition_number;
using kcenon::pacs::core::tags::instance_number;
using kcenon::pacs::core::tags::patient_orientation;
using kcenon::pacs::core::tags::image_position_patient;
using kcenon::pacs::core::tags::image_orientation_patient;
using kcenon::pacs::core::tags::frame_of_reference_uid;
using kcenon::pacs::core::tags::slice_location;
using kcenon::pacs::core::tags::number_of_patient_related_studies;
using kcenon::pacs::core::tags::number_of_patient_related_series;
using kcenon::pacs::core::tags::number_of_patient_related_instances;
using kcenon::pacs::core::tags::number_of_study_related_series;
using kcenon::pacs::core::tags::number_of_study_related_instances;
using kcenon::pacs::core::tags::number_of_series_related_instances;

// Image Pixel Module (Group 0x0028)
using kcenon::pacs::core::tags::samples_per_pixel;
using kcenon::pacs::core::tags::photometric_interpretation;
using kcenon::pacs::core::tags::rows;
using kcenon::pacs::core::tags::columns;
using kcenon::pacs::core::tags::pixel_spacing;
using kcenon::pacs::core::tags::bits_allocated;
using kcenon::pacs::core::tags::bits_stored;
using kcenon::pacs::core::tags::high_bit;
using kcenon::pacs::core::tags::pixel_representation;
using kcenon::pacs::core::tags::smallest_image_pixel_value;
using kcenon::pacs::core::tags::largest_image_pixel_value;
using kcenon::pacs::core::tags::window_center;
using kcenon::pacs::core::tags::window_width;
using kcenon::pacs::core::tags::rescale_intercept;
using kcenon::pacs::core::tags::rescale_slope;
using kcenon::pacs::core::tags::rescale_type;

// Scheduled Procedure Step (Modality Worklist - Group 0x0040)
using kcenon::pacs::core::tags::scheduled_station_ae_title;
using kcenon::pacs::core::tags::scheduled_procedure_step_start_date;
using kcenon::pacs::core::tags::scheduled_procedure_step_start_time;
using kcenon::pacs::core::tags::scheduled_procedure_step_end_date;
using kcenon::pacs::core::tags::scheduled_procedure_step_end_time;
using kcenon::pacs::core::tags::scheduled_performing_physician_name;
using kcenon::pacs::core::tags::scheduled_procedure_step_description;
using kcenon::pacs::core::tags::scheduled_station_name;
using kcenon::pacs::core::tags::scheduled_procedure_step_location;
using kcenon::pacs::core::tags::scheduled_procedure_step_id;
using kcenon::pacs::core::tags::scheduled_procedure_step_sequence;
using kcenon::pacs::core::tags::requested_procedure_id;
using kcenon::pacs::core::tags::requested_procedure_description;
using kcenon::pacs::core::tags::performed_procedure_step_start_date;
using kcenon::pacs::core::tags::performed_procedure_step_start_time;
using kcenon::pacs::core::tags::performed_procedure_step_status;
using kcenon::pacs::core::tags::performed_procedure_step_id;

// Pixel Data (Group 0x7FE0)
using kcenon::pacs::core::tags::pixel_data;

// Item Delimiters (Group 0xFFFE)
using kcenon::pacs::core::tags::item;
using kcenon::pacs::core::tags::item_delimitation_item;
using kcenon::pacs::core::tags::sequence_delimitation_item;

} // namespace kcenon::pacs::core::tags

// ============================================================================
// Re-export kcenon::pacs namespace (Result pattern)
// ============================================================================
// The kcenon::pacs::error_codes constants are namespace-scope constexpr
// variables with internal linkage, which a module cannot export. Include
// <kcenon/pacs/core/result.h> next to `import kcenon.pacs;` to use them.

export namespace kcenon::pacs {

// Result types
using kcenon::pacs::Result;
using kcenon::pacs::VoidResult;
using kcenon::pacs::error_info;

// Utility functions
using kcenon::pacs::ok;
using kcenon::pacs::make_error;
using kcenon::pacs::try_catch;
using kcenon::pacs::try_catch_void;
using kcenon::pacs::pacs_error;
using kcenon::pacs::pacs_void_error;

} // namespace kcenon::pacs

// ============================================================================
// Re-export kcenon::pacs::events namespace
// ============================================================================

export namespace kcenon::pacs::events {

// Association events
using kcenon::pacs::events::association_established_event;
using kcenon::pacs::events::association_released_event;
using kcenon::pacs::events::association_aborted_event;

// Storage events
using kcenon::pacs::events::image_received_event;
using kcenon::pacs::events::storage_failed_event;

// Query events
using kcenon::pacs::events::query_level;
using kcenon::pacs::events::query_level_to_string;
using kcenon::pacs::events::query_executed_event;
using kcenon::pacs::events::query_failed_event;

// Retrieve events
using kcenon::pacs::events::retrieve_operation;
using kcenon::pacs::events::retrieve_operation_to_string;
using kcenon::pacs::events::retrieve_started_event;
using kcenon::pacs::events::retrieve_completed_event;

} // namespace kcenon::pacs::events
