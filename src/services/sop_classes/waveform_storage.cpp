// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file waveform_storage.cpp
 * @brief Implementation of Waveform Storage SOP Classes
 */

#include "kcenon/pacs/services/sop_classes/waveform_storage.h"

#include <algorithm>
#include <array>

namespace kcenon::pacs::services::sop_classes {

// =============================================================================
// Transfer Syntaxes
// =============================================================================

std::vector<std::string> get_waveform_transfer_syntaxes() {
    return {
        // Explicit VR Little Endian (preferred for interoperability)
        "1.2.840.10008.1.2.1",
        // Implicit VR Little Endian (universal baseline)
        "1.2.840.10008.1.2",
    };
}

// =============================================================================
// Waveform Type Classification
// =============================================================================

waveform_type get_waveform_type(std::string_view uid) noexcept {
    if (uid == twelve_lead_ecg_storage_uid) return waveform_type::ecg_12lead;
    if (uid == general_ecg_storage_uid) return waveform_type::ecg_general;
    if (uid == ambulatory_ecg_storage_uid) return waveform_type::ecg_ambulatory;
    if (uid == hemodynamic_waveform_storage_uid) return waveform_type::hemodynamic;
    if (uid == cardiac_ep_waveform_storage_uid) return waveform_type::cardiac_ep;
    if (uid == basic_voice_audio_storage_uid) return waveform_type::audio_basic;
    if (uid == general_audio_waveform_storage_uid) return waveform_type::audio_general;
    if (uid == arterial_pulse_waveform_storage_uid) return waveform_type::arterial_pulse;
    if (uid == respiratory_waveform_storage_uid) return waveform_type::respiratory;
    if (uid == multichannel_respiratory_waveform_storage_uid) return waveform_type::respiratory_multi;
    if (uid == routine_scalp_eeg_storage_uid) return waveform_type::eeg_routine;
    if (uid == emg_waveform_storage_uid) return waveform_type::emg;
    if (uid == eog_waveform_storage_uid) return waveform_type::eog;
    if (uid == sleep_eeg_storage_uid) return waveform_type::eeg_sleep;
    if (uid == body_position_waveform_storage_uid) return waveform_type::body_position;
    if (uid == waveform_presentation_state_storage_uid) return waveform_type::presentation_state;
    if (uid == waveform_annotation_storage_uid) return waveform_type::annotation;
    return waveform_type::unknown;
}

std::string_view to_string(waveform_type type) noexcept {
    switch (type) {
        case waveform_type::ecg_12lead: return "12-Lead ECG";
        case waveform_type::ecg_general: return "General ECG";
        case waveform_type::ecg_ambulatory: return "Ambulatory ECG";
        case waveform_type::hemodynamic: return "Hemodynamic";
        case waveform_type::cardiac_ep: return "Cardiac Electrophysiology";
        case waveform_type::audio_basic: return "Basic Voice Audio";
        case waveform_type::audio_general: return "General Audio";
        case waveform_type::arterial_pulse: return "Arterial Pulse";
        case waveform_type::respiratory: return "Respiratory";
        case waveform_type::respiratory_multi: return "Multi-channel Respiratory";
        case waveform_type::eeg_routine: return "Routine Scalp EEG";
        case waveform_type::emg: return "Electromyogram";
        case waveform_type::eog: return "Electrooculogram";
        case waveform_type::eeg_sleep: return "Sleep EEG";
        case waveform_type::body_position: return "Body Position";
        case waveform_type::presentation_state: return "Waveform Presentation State";
        case waveform_type::annotation: return "Waveform Annotation";
        case waveform_type::unknown: return "Unknown";
    }
    return "Unknown";
}

// =============================================================================
// SOP Class Information Registry
// =============================================================================

namespace {

static constexpr std::array<waveform_sop_class_info, 17> kWaveformRegistry = {{
    {twelve_lead_ecg_storage_uid, "12-lead ECG Waveform Storage",
     waveform_type::ecg_12lead, false},
    {general_ecg_storage_uid, "General ECG Waveform Storage",
     waveform_type::ecg_general, false},
    {ambulatory_ecg_storage_uid, "Ambulatory ECG Waveform Storage",
     waveform_type::ecg_ambulatory, false},
    {hemodynamic_waveform_storage_uid, "Hemodynamic Waveform Storage",
     waveform_type::hemodynamic, false},
    {cardiac_ep_waveform_storage_uid, "Basic Cardiac EP Waveform Storage",
     waveform_type::cardiac_ep, false},
    {basic_voice_audio_storage_uid, "Basic Voice Audio Waveform Storage",
     waveform_type::audio_basic, false},
    {general_audio_waveform_storage_uid, "General Audio Waveform Storage",
     waveform_type::audio_general, false},
    {arterial_pulse_waveform_storage_uid, "Arterial Pulse Waveform Storage",
     waveform_type::arterial_pulse, false},
    {respiratory_waveform_storage_uid, "Respiratory Waveform Storage",
     waveform_type::respiratory, false},
    {multichannel_respiratory_waveform_storage_uid, "Multi-channel Respiratory Waveform Storage",
     waveform_type::respiratory_multi, false},
    {routine_scalp_eeg_storage_uid, "Routine Scalp EEG Waveform Storage",
     waveform_type::eeg_routine, false},
    {emg_waveform_storage_uid, "Electromyogram Waveform Storage",
     waveform_type::emg, false},
    {eog_waveform_storage_uid, "Electrooculogram Waveform Storage",
     waveform_type::eog, false},
    {sleep_eeg_storage_uid, "Sleep EEG Waveform Storage",
     waveform_type::eeg_sleep, false},
    {body_position_waveform_storage_uid, "Body Position Waveform Storage",
     waveform_type::body_position, false},
    {waveform_presentation_state_storage_uid, "Waveform Presentation State Storage",
     waveform_type::presentation_state, false},
    {waveform_annotation_storage_uid, "Waveform Annotation Storage",
     waveform_type::annotation, false},
}};

}  // namespace

std::vector<std::string>
get_waveform_storage_sop_classes(bool include_presentation_state,
                                  bool include_annotation) {
    std::vector<std::string> result;
    result.reserve(kWaveformRegistry.size());

    for (const auto& entry : kWaveformRegistry) {
        if (entry.type == waveform_type::presentation_state &&
            !include_presentation_state) {
            continue;
        }
        if (entry.type == waveform_type::annotation && !include_annotation) {
            continue;
        }
        result.emplace_back(entry.uid);
    }
    return result;
}

const waveform_sop_class_info*
get_waveform_sop_class_info(std::string_view uid) noexcept {
    auto it = std::find_if(kWaveformRegistry.begin(), kWaveformRegistry.end(),
                           [&uid](const waveform_sop_class_info& info) {
                               return info.uid == uid;
                           });
    return (it != kWaveformRegistry.end()) ? &(*it) : nullptr;
}

bool is_waveform_storage_sop_class(std::string_view uid) noexcept {
    return get_waveform_sop_class_info(uid) != nullptr;
}

bool is_waveform_presentation_state_sop_class(std::string_view uid) noexcept {
    return uid == waveform_presentation_state_storage_uid;
}

bool is_waveform_annotation_sop_class(std::string_view uid) noexcept {
    return uid == waveform_annotation_storage_uid;
}

}  // namespace kcenon::pacs::services::sop_classes
