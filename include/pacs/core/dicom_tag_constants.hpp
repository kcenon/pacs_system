/**
 * @file dicom_tag_constants.hpp
 * @brief Compile-time constants for commonly used DICOM tags
 *
 * This file provides inline constexpr definitions for standard DICOM tags
 * as defined in DICOM PS3.6 Data Dictionary. These constants can be used
 * directly in code without runtime overhead.
 *
 * Tags are organized by functional groups for easier navigation.
 *
 * @see DICOM PS3.6 - Data Dictionary
 */

#pragma once

#include "dicom_tag.hpp"

namespace pacs::core::tags {

// ============================================================================
// File Meta Information (Group 0x0002)
// ============================================================================

/// File Meta Information Group Length
inline constexpr dicom_tag file_meta_information_group_length{0x0002, 0x0000};

/// File Meta Information Version
inline constexpr dicom_tag file_meta_information_version{0x0002, 0x0001};

/// Media Storage SOP Class UID
inline constexpr dicom_tag media_storage_sop_class_uid{0x0002, 0x0002};

/// Media Storage SOP Instance UID
inline constexpr dicom_tag media_storage_sop_instance_uid{0x0002, 0x0003};

/// Transfer Syntax UID
inline constexpr dicom_tag transfer_syntax_uid{0x0002, 0x0010};

/// Implementation Class UID
inline constexpr dicom_tag implementation_class_uid{0x0002, 0x0012};

/// Implementation Version Name
inline constexpr dicom_tag implementation_version_name{0x0002, 0x0013};

/// Source Application Entity Title
inline constexpr dicom_tag source_application_entity_title{0x0002, 0x0016};

/// Private Information Creator UID
inline constexpr dicom_tag private_information_creator_uid{0x0002, 0x0100};

/// Private Information
inline constexpr dicom_tag private_information{0x0002, 0x0102};

// ============================================================================
// Command Group (Group 0x0000) - Used in DIMSE messages
// ============================================================================

/// Command Group Length
inline constexpr dicom_tag command_group_length{0x0000, 0x0000};

/// Affected SOP Class UID
inline constexpr dicom_tag affected_sop_class_uid{0x0000, 0x0002};

/// Requested SOP Class UID
inline constexpr dicom_tag requested_sop_class_uid{0x0000, 0x0003};

/// Command Field
inline constexpr dicom_tag command_field{0x0000, 0x0100};

/// Message ID
inline constexpr dicom_tag message_id{0x0000, 0x0110};

/// Message ID Being Responded To
inline constexpr dicom_tag message_id_being_responded_to{0x0000, 0x0120};

/// Move Destination
inline constexpr dicom_tag move_destination{0x0000, 0x0600};

/// Priority
inline constexpr dicom_tag priority{0x0000, 0x0700};

/// Command Data Set Type
inline constexpr dicom_tag command_data_set_type{0x0000, 0x0800};

/// Status
inline constexpr dicom_tag status{0x0000, 0x0900};

/// Affected SOP Instance UID
inline constexpr dicom_tag affected_sop_instance_uid{0x0000, 0x1000};

/// Number of Remaining Sub-operations
inline constexpr dicom_tag number_of_remaining_suboperations{0x0000, 0x1020};

/// Number of Completed Sub-operations
inline constexpr dicom_tag number_of_completed_suboperations{0x0000, 0x1021};

/// Number of Failed Sub-operations
inline constexpr dicom_tag number_of_failed_suboperations{0x0000, 0x1022};

/// Number of Warning Sub-operations
inline constexpr dicom_tag number_of_warning_suboperations{0x0000, 0x1023};

// ============================================================================
// SOP Common (Group 0x0008)
// ============================================================================

/// Specific Character Set
inline constexpr dicom_tag specific_character_set{0x0008, 0x0005};

/// Image Type
inline constexpr dicom_tag image_type{0x0008, 0x0008};

/// Instance Creation Date
inline constexpr dicom_tag instance_creation_date{0x0008, 0x0012};

/// Instance Creation Time
inline constexpr dicom_tag instance_creation_time{0x0008, 0x0013};

/// Instance Creator UID
inline constexpr dicom_tag instance_creator_uid{0x0008, 0x0014};

/// SOP Class UID
inline constexpr dicom_tag sop_class_uid{0x0008, 0x0016};

/// SOP Instance UID
inline constexpr dicom_tag sop_instance_uid{0x0008, 0x0018};

/// Study Date
inline constexpr dicom_tag study_date{0x0008, 0x0020};

/// Series Date
inline constexpr dicom_tag series_date{0x0008, 0x0021};

/// Acquisition Date
inline constexpr dicom_tag acquisition_date{0x0008, 0x0022};

/// Content Date
inline constexpr dicom_tag content_date{0x0008, 0x0023};

/// Study Time
inline constexpr dicom_tag study_time{0x0008, 0x0030};

/// Series Time
inline constexpr dicom_tag series_time{0x0008, 0x0031};

/// Acquisition Time
inline constexpr dicom_tag acquisition_time{0x0008, 0x0032};

/// Content Time
inline constexpr dicom_tag content_time{0x0008, 0x0033};

/// Accession Number
inline constexpr dicom_tag accession_number{0x0008, 0x0050};

/// Query/Retrieve Level
inline constexpr dicom_tag query_retrieve_level{0x0008, 0x0052};

/// Retrieve AE Title
inline constexpr dicom_tag retrieve_ae_title{0x0008, 0x0054};

/// Instance Availability
inline constexpr dicom_tag instance_availability{0x0008, 0x0056};

/// Modality
inline constexpr dicom_tag modality{0x0008, 0x0060};

/// Modalities in Study
inline constexpr dicom_tag modalities_in_study{0x0008, 0x0061};

/// Conversion Type
inline constexpr dicom_tag conversion_type{0x0008, 0x0064};

/// Manufacturer
inline constexpr dicom_tag manufacturer{0x0008, 0x0070};

/// Institution Name
inline constexpr dicom_tag institution_name{0x0008, 0x0080};

/// Institution Address
inline constexpr dicom_tag institution_address{0x0008, 0x0081};

/// Referring Physician's Name
inline constexpr dicom_tag referring_physician_name{0x0008, 0x0090};

/// Station Name
inline constexpr dicom_tag station_name{0x0008, 0x1010};

/// Study Description
inline constexpr dicom_tag study_description{0x0008, 0x1030};

/// Series Description
inline constexpr dicom_tag series_description{0x0008, 0x103E};

/// Performing Physician's Name
inline constexpr dicom_tag performing_physician_name{0x0008, 0x1050};

/// Name of Physician(s) Reading Study
inline constexpr dicom_tag name_of_physicians_reading_study{0x0008, 0x1060};

/// Operators' Name
inline constexpr dicom_tag operators_name{0x0008, 0x1070};

/// Manufacturer's Model Name
inline constexpr dicom_tag manufacturers_model_name{0x0008, 0x1090};

/// Referenced SOP Class UID (in Sequence)
inline constexpr dicom_tag referenced_sop_class_uid{0x0008, 0x1150};

/// Referenced SOP Instance UID (in Sequence)
inline constexpr dicom_tag referenced_sop_instance_uid{0x0008, 0x1155};

// ============================================================================
// Patient Module (Group 0x0010)
// ============================================================================

/// Patient's Name
inline constexpr dicom_tag patient_name{0x0010, 0x0010};

/// Patient ID
inline constexpr dicom_tag patient_id{0x0010, 0x0020};

/// Issuer of Patient ID
inline constexpr dicom_tag issuer_of_patient_id{0x0010, 0x0021};

/// Patient's Birth Date
inline constexpr dicom_tag patient_birth_date{0x0010, 0x0030};

/// Patient's Sex
inline constexpr dicom_tag patient_sex{0x0010, 0x0040};

/// Patient's Age
inline constexpr dicom_tag patient_age{0x0010, 0x1010};

/// Patient's Size
inline constexpr dicom_tag patient_size{0x0010, 0x1020};

/// Patient's Weight
inline constexpr dicom_tag patient_weight{0x0010, 0x1030};

/// Patient's Address
inline constexpr dicom_tag patient_address{0x0010, 0x1040};

/// Patient Comments
inline constexpr dicom_tag patient_comments{0x0010, 0x4000};

// ============================================================================
// Study and Series Identification (Group 0x0020)
// ============================================================================

/// Study Instance UID
inline constexpr dicom_tag study_instance_uid{0x0020, 0x000D};

/// Series Instance UID
inline constexpr dicom_tag series_instance_uid{0x0020, 0x000E};

/// Study ID
inline constexpr dicom_tag study_id{0x0020, 0x0010};

/// Series Number
inline constexpr dicom_tag series_number{0x0020, 0x0011};

/// Acquisition Number
inline constexpr dicom_tag acquisition_number{0x0020, 0x0012};

/// Instance Number
inline constexpr dicom_tag instance_number{0x0020, 0x0013};

/// Patient Orientation
inline constexpr dicom_tag patient_orientation{0x0020, 0x0020};

/// Image Position (Patient)
inline constexpr dicom_tag image_position_patient{0x0020, 0x0032};

/// Image Orientation (Patient)
inline constexpr dicom_tag image_orientation_patient{0x0020, 0x0037};

/// Frame of Reference UID
inline constexpr dicom_tag frame_of_reference_uid{0x0020, 0x0052};

/// Slice Location
inline constexpr dicom_tag slice_location{0x0020, 0x1041};

/// Number of Patient Related Studies
inline constexpr dicom_tag number_of_patient_related_studies{0x0020, 0x1200};

/// Number of Patient Related Series
inline constexpr dicom_tag number_of_patient_related_series{0x0020, 0x1202};

/// Number of Patient Related Instances
inline constexpr dicom_tag number_of_patient_related_instances{0x0020, 0x1204};

/// Number of Study Related Series
inline constexpr dicom_tag number_of_study_related_series{0x0020, 0x1206};

/// Number of Study Related Instances
inline constexpr dicom_tag number_of_study_related_instances{0x0020, 0x1208};

/// Number of Series Related Instances
inline constexpr dicom_tag number_of_series_related_instances{0x0020, 0x1209};

// ============================================================================
// Image Pixel Module (Group 0x0028)
// ============================================================================

/// Samples per Pixel
inline constexpr dicom_tag samples_per_pixel{0x0028, 0x0002};

/// Photometric Interpretation
inline constexpr dicom_tag photometric_interpretation{0x0028, 0x0004};

/// Rows
inline constexpr dicom_tag rows{0x0028, 0x0010};

/// Columns
inline constexpr dicom_tag columns{0x0028, 0x0011};

/// Pixel Spacing
inline constexpr dicom_tag pixel_spacing{0x0028, 0x0030};

/// Bits Allocated
inline constexpr dicom_tag bits_allocated{0x0028, 0x0100};

/// Bits Stored
inline constexpr dicom_tag bits_stored{0x0028, 0x0101};

/// High Bit
inline constexpr dicom_tag high_bit{0x0028, 0x0102};

/// Pixel Representation
inline constexpr dicom_tag pixel_representation{0x0028, 0x0103};

/// Smallest Image Pixel Value
inline constexpr dicom_tag smallest_image_pixel_value{0x0028, 0x0106};

/// Largest Image Pixel Value
inline constexpr dicom_tag largest_image_pixel_value{0x0028, 0x0107};

/// Window Center
inline constexpr dicom_tag window_center{0x0028, 0x1050};

/// Window Width
inline constexpr dicom_tag window_width{0x0028, 0x1051};

/// Rescale Intercept
inline constexpr dicom_tag rescale_intercept{0x0028, 0x1052};

/// Rescale Slope
inline constexpr dicom_tag rescale_slope{0x0028, 0x1053};

/// Rescale Type
inline constexpr dicom_tag rescale_type{0x0028, 0x1054};

// ============================================================================
// Scheduled Procedure Step (Modality Worklist - Group 0x0040)
// ============================================================================

/// Scheduled Station AE Title
inline constexpr dicom_tag scheduled_station_ae_title{0x0040, 0x0001};

/// Scheduled Procedure Step Start Date
inline constexpr dicom_tag scheduled_procedure_step_start_date{0x0040, 0x0002};

/// Scheduled Procedure Step Start Time
inline constexpr dicom_tag scheduled_procedure_step_start_time{0x0040, 0x0003};

/// Scheduled Procedure Step End Date
inline constexpr dicom_tag scheduled_procedure_step_end_date{0x0040, 0x0004};

/// Scheduled Procedure Step End Time
inline constexpr dicom_tag scheduled_procedure_step_end_time{0x0040, 0x0005};

/// Scheduled Performing Physician's Name
inline constexpr dicom_tag scheduled_performing_physician_name{0x0040, 0x0006};

/// Scheduled Procedure Step Description
inline constexpr dicom_tag scheduled_procedure_step_description{0x0040, 0x0007};

/// Scheduled Station Name
inline constexpr dicom_tag scheduled_station_name{0x0040, 0x0010};

/// Scheduled Procedure Step Location
inline constexpr dicom_tag scheduled_procedure_step_location{0x0040, 0x0011};

/// Scheduled Procedure Step ID
inline constexpr dicom_tag scheduled_procedure_step_id{0x0040, 0x0009};

/// Scheduled Procedure Step Sequence
inline constexpr dicom_tag scheduled_procedure_step_sequence{0x0040, 0x0100};

/// Requested Procedure ID
inline constexpr dicom_tag requested_procedure_id{0x0040, 0x1001};

/// Performed Procedure Step Start Date
inline constexpr dicom_tag performed_procedure_step_start_date{0x0040, 0x0244};

/// Performed Procedure Step Start Time
inline constexpr dicom_tag performed_procedure_step_start_time{0x0040, 0x0245};

/// Performed Procedure Step Status
inline constexpr dicom_tag performed_procedure_step_status{0x0040, 0x0252};

/// Performed Procedure Step ID
inline constexpr dicom_tag performed_procedure_step_id{0x0040, 0x0253};

// ============================================================================
// Pixel Data (Group 0x7FE0)
// ============================================================================

/// Pixel Data
inline constexpr dicom_tag pixel_data{0x7FE0, 0x0010};

// ============================================================================
// Item Delimiters (Group 0xFFFE)
// ============================================================================

/// Item
inline constexpr dicom_tag item{0xFFFE, 0xE000};

/// Item Delimitation Item
inline constexpr dicom_tag item_delimitation_item{0xFFFE, 0xE00D};

/// Sequence Delimitation Item
inline constexpr dicom_tag sequence_delimitation_item{0xFFFE, 0xE0DD};

}  // namespace pacs::core::tags
