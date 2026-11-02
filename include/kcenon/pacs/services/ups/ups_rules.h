/**
 * @file ups_rules.h
 * @brief What PS3.4 Annex CC says each UPS request is answered with
 *
 * Pure functions over the state a request finds, so every status of the tables can be
 * tested without a database or an association.
 */

#ifndef PACS_SERVICES_UPS_RULES_HPP
#define PACS_SERVICES_UPS_RULES_HPP

#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/network/dimse/status_codes.h"

#include <string_view>

namespace kcenon::pacs::services {

inline constexpr network::dimse::status_code status_ups_already_canceled = 0xB304;
inline constexpr network::dimse::status_code status_ups_already_completed = 0xB306;
inline constexpr network::dimse::status_code status_ups_no_longer_updatable = 0xC300;
inline constexpr network::dimse::status_code status_ups_wrong_transaction = 0xC301;
inline constexpr network::dimse::status_code status_ups_already_in_progress = 0xC302;
inline constexpr network::dimse::status_code status_ups_scheduled_only_by_create = 0xC303;
inline constexpr network::dimse::status_code status_ups_final_state_unmet = 0xC304;
inline constexpr network::dimse::status_code status_ups_no_such_instance = 0xC307;
inline constexpr network::dimse::status_code status_ups_unknown_receiving_ae = 0xC308;
inline constexpr network::dimse::status_code status_ups_not_scheduled = 0xC309;
inline constexpr network::dimse::status_code status_ups_not_in_progress = 0xC310;
inline constexpr network::dimse::status_code status_ups_cannot_cancel_completed = 0xC311;
inline constexpr network::dimse::status_code status_ups_performer_unreachable = 0xC312;
inline constexpr network::dimse::status_code status_ups_action_not_appropriate = 0xC314;

/**
 * @brief Table CC.1.1-2 for N-ACTION Change UPS State on an existing instance
 *
 * In SCHEDULED no Transaction UID is recorded yet, so "correct" means one was offered.
 * A success status means the transition is allowed and final state requirements still
 * need checking when the target is COMPLETED or CANCELED.
 */
[[nodiscard]] network::dimse::status_code ups_change_state_status(
    std::string_view current_state,
    std::string_view recorded_transaction,
    std::string_view wanted_state,
    std::string_view offered_transaction);

/// CC.2.6.2 and CC.2.6.3 for N-SET on an existing instance.
[[nodiscard]] network::dimse::status_code ups_set_status(
    std::string_view current_state,
    std::string_view recorded_transaction,
    std::string_view offered_transaction);

/// Table CC.2.5-3 for the attributes of an N-CREATE: C309H, 0106H or 0120H, else success.
[[nodiscard]] network::dimse::status_code ups_create_status(const core::dicom_dataset& attributes);

/// Final State codes P (COMPLETED) and X (CANCELED) of Table CC.2.5-3.
[[nodiscard]] bool ups_final_state_met(
    const core::dicom_dataset& attributes,
    std::string_view wanted_state);

/// A handler answers with a PS3.4 status by using it as the error code; other errors become C001H.
[[nodiscard]] network::dimse::status_code ups_status_from_error_code(int code) noexcept;

}  // namespace kcenon::pacs::services

#endif  // PACS_SERVICES_UPS_RULES_HPP
