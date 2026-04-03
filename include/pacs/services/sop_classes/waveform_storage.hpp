// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file waveform_storage.hpp
 * @brief Waveform Presentation State and Annotation Storage SOP Classes
 *
 * Provides SOP Class definitions and utilities for waveform presentation
 * state and annotation storage, supporting cardiology (ECG, hemodynamics)
 * and neurology (EEG) workflows.
 *
 * @see DICOM PS3.3 Section A.34 -- Waveform IOD
 * @see DICOM PS3.3 -- Waveform Presentation State IOD
 * @see DICOM PS3.3 -- Waveform Annotation IOD
 * @author kcenon
 * @since 1.0.0
 */

#ifndef PACS_SERVICES_SOP_CLASSES_WAVEFORM_STORAGE_HPP
#define PACS_SERVICES_SOP_CLASSES_WAVEFORM_STORAGE_HPP

#include <string>
#include <string_view>
#include <vector>

namespace kcenon::pacs::services::sop_classes {

// =============================================================================
// Waveform Storage SOP Class UIDs
// =============================================================================

/// @name Base Waveform Storage SOP Classes
/// @{

/// 12-lead ECG Waveform Storage SOP Class UID
inline constexpr std::string_view twelve_lead_ecg_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.1.1";

/// General ECG Waveform Storage SOP Class UID
inline constexpr std::string_view general_ecg_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.1.2";

/// Ambulatory ECG Waveform Storage SOP Class UID
inline constexpr std::string_view ambulatory_ecg_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.1.3";

/// Hemodynamic Waveform Storage SOP Class UID
inline constexpr std::string_view hemodynamic_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.2.1";

/// Basic Cardiac Electrophysiology Waveform Storage SOP Class UID
inline constexpr std::string_view cardiac_ep_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.3.1";

/// Basic Voice Audio Waveform Storage SOP Class UID
inline constexpr std::string_view basic_voice_audio_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.4.1";

/// General Audio Waveform Storage SOP Class UID
inline constexpr std::string_view general_audio_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.4.2";

/// Arterial Pulse Waveform Storage SOP Class UID
inline constexpr std::string_view arterial_pulse_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.5.1";

/// Respiratory Waveform Storage SOP Class UID
inline constexpr std::string_view respiratory_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.6.1";

/// Multi-channel Respiratory Waveform Storage SOP Class UID
inline constexpr std::string_view multichannel_respiratory_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.6.2";

/// Routine Scalp Electroencephalogram Waveform Storage SOP Class UID
inline constexpr std::string_view routine_scalp_eeg_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.7.1";

/// Electromyogram Waveform Storage SOP Class UID
inline constexpr std::string_view emg_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.7.2";

/// Electrooculogram Waveform Storage SOP Class UID
inline constexpr std::string_view eog_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.7.3";

/// Sleep Electroencephalogram Waveform Storage SOP Class UID
inline constexpr std::string_view sleep_eeg_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.7.4";

/// Body Position Waveform Storage SOP Class UID
inline constexpr std::string_view body_position_waveform_storage_uid =
    "1.2.840.10008.5.1.4.1.1.9.8.1";

/// @}

/// @name Waveform Presentation State and Annotation SOP Classes
/// @{

/// Waveform Presentation State Storage SOP Class UID
inline constexpr std::string_view waveform_presentation_state_storage_uid =
    "1.2.840.10008.5.1.4.1.1.11.11";

/// Waveform Annotation Storage SOP Class UID
inline constexpr std::string_view waveform_annotation_storage_uid =
    "1.2.840.10008.5.1.4.1.1.11.12";

/// @}

// =============================================================================
// Waveform Type Classification
// =============================================================================

/**
 * @brief Waveform type classification
 */
enum class waveform_type {
    ecg_12lead,         ///< 12-lead ECG
    ecg_general,        ///< General ECG
    ecg_ambulatory,     ///< Ambulatory ECG (Holter)
    hemodynamic,        ///< Hemodynamic monitoring
    cardiac_ep,         ///< Cardiac electrophysiology
    audio_basic,        ///< Basic voice audio
    audio_general,      ///< General audio
    arterial_pulse,     ///< Arterial pulse waveform
    respiratory,        ///< Single-channel respiratory
    respiratory_multi,  ///< Multi-channel respiratory
    eeg_routine,        ///< Routine scalp EEG
    emg,                ///< Electromyogram
    eog,                ///< Electrooculogram
    eeg_sleep,          ///< Sleep EEG
    body_position,      ///< Body position
    presentation_state, ///< Waveform Presentation State
    annotation,         ///< Waveform Annotation
    unknown             ///< Unknown waveform type
};

/**
 * @brief Get waveform type from SOP Class UID
 * @param uid The SOP Class UID
 * @return The waveform type
 */
[[nodiscard]] waveform_type get_waveform_type(std::string_view uid) noexcept;

/**
 * @brief Get human-readable name for waveform type
 * @param type The waveform type
 * @return Human-readable type name
 */
[[nodiscard]] std::string_view to_string(waveform_type type) noexcept;

// =============================================================================
// Waveform SOP Class Information
// =============================================================================

/**
 * @brief Information about a Waveform Storage SOP Class
 */
struct waveform_sop_class_info {
    std::string_view uid;           ///< SOP Class UID
    std::string_view name;          ///< Human-readable name
    waveform_type type;             ///< Waveform type classification
    bool is_retired;                ///< Whether this SOP class is retired
};

/**
 * @brief Get all Waveform Storage SOP Class UIDs
 *
 * @param include_presentation_state Include presentation state class (default: true)
 * @param include_annotation Include annotation class (default: true)
 * @return Vector of Waveform Storage SOP Class UIDs
 */
[[nodiscard]] std::vector<std::string>
get_waveform_storage_sop_classes(bool include_presentation_state = true,
                                  bool include_annotation = true);

/**
 * @brief Get information about a specific Waveform SOP Class
 *
 * @param uid The SOP Class UID to look up
 * @return Pointer to SOP class info, or nullptr if not a waveform storage class
 */
[[nodiscard]] const waveform_sop_class_info*
get_waveform_sop_class_info(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a Waveform Storage SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is any waveform storage SOP class
 */
[[nodiscard]] bool is_waveform_storage_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a Waveform Presentation State SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is the waveform presentation state SOP class
 */
[[nodiscard]] bool is_waveform_presentation_state_sop_class(std::string_view uid) noexcept;

/**
 * @brief Check if a SOP Class UID is a Waveform Annotation SOP Class
 *
 * @param uid The SOP Class UID to check
 * @return true if this is the waveform annotation SOP class
 */
[[nodiscard]] bool is_waveform_annotation_sop_class(std::string_view uid) noexcept;

/**
 * @brief Get recommended transfer syntaxes for waveform objects
 *
 * @return Vector of transfer syntax UIDs in priority order
 */
[[nodiscard]] std::vector<std::string> get_waveform_transfer_syntaxes();

}  // namespace kcenon::pacs::services::sop_classes

#endif  // PACS_SERVICES_SOP_CLASSES_WAVEFORM_STORAGE_HPP
