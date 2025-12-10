/**
 * @file digital_signature.hpp
 * @brief DICOM Digital Signature creation and verification per PS3.15
 *
 * This file provides the digital_signature class which implements DICOM
 * digital signature functionality as specified in DICOM PS3.15 (Security
 * and System Management Profiles).
 *
 * @see DICOM PS3.15 Section C - Digital Signatures
 * @copyright Copyright (c) 2025
 */

#pragma once

#include "certificate.hpp"
#include "signature_types.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag.hpp>

#include <kcenon/common/patterns/result.h>

#include <optional>
#include <span>
#include <vector>

namespace pacs::security {

/**
 * @brief DICOM Digital Signature creation and verification
 *
 * This class provides static methods for signing DICOM datasets and verifying
 * existing signatures according to DICOM PS3.15 specifications.
 *
 * A DICOM Digital Signature consists of:
 * - Digital Signature Sequence (0400,0561) containing signature items
 * - Each item contains MAC, certificate, timestamp, and signed tag list
 *
 * Thread Safety: Static methods are thread-safe.
 *
 * @example
 * @code
 * // Sign a dataset
 * auto cert = certificate::load_from_pem("signer.pem").value();
 * auto key = private_key::load_from_pem("signer_key.pem").value();
 *
 * dicom_dataset dataset;
 * // ... populate dataset ...
 *
 * auto result = digital_signature::sign(dataset, cert, key);
 * if (result.is_ok()) {
 *     std::cout << "Dataset signed successfully\n";
 * }
 *
 * // Verify signature
 * auto status = digital_signature::verify(dataset);
 * if (status.is_ok() && status.value() == signature_status::valid) {
 *     std::cout << "Signature is valid\n";
 * }
 * @endcode
 */
class digital_signature {
public:
    // ========================================================================
    // Signature Creation
    // ========================================================================

    /**
     * @brief Sign a DICOM dataset
     *
     * Creates a digital signature for the entire dataset and adds the
     * Digital Signature Sequence (0400,0561) to the dataset.
     *
     * @param dataset The dataset to sign (modified in place)
     * @param cert Signer's X.509 certificate
     * @param key Signer's private key (must match certificate's public key)
     * @param algo Signature algorithm to use (default: RSA-SHA256)
     * @return Result indicating success or error
     *
     * @note The dataset is modified to include the signature sequence.
     * @note All existing data elements are signed by default.
     */
    [[nodiscard]] static auto sign(
        core::dicom_dataset& dataset,
        const certificate& cert,
        const private_key& key,
        signature_algorithm algo = signature_algorithm::rsa_sha256
    ) -> kcenon::common::VoidResult;

    /**
     * @brief Sign specific tags in a DICOM dataset
     *
     * Creates a digital signature for only the specified tags.
     *
     * @param dataset The dataset to sign (modified in place)
     * @param cert Signer's X.509 certificate
     * @param key Signer's private key
     * @param tags_to_sign List of tags to include in signature
     * @param algo Signature algorithm to use
     * @return Result indicating success or error
     */
    [[nodiscard]] static auto sign_tags(
        core::dicom_dataset& dataset,
        const certificate& cert,
        const private_key& key,
        std::span<const core::dicom_tag> tags_to_sign,
        signature_algorithm algo = signature_algorithm::rsa_sha256
    ) -> kcenon::common::VoidResult;

    // ========================================================================
    // Signature Verification
    // ========================================================================

    /**
     * @brief Verify digital signatures in a dataset
     *
     * Verifies all digital signatures present in the Digital Signature
     * Sequence (0400,0561).
     *
     * @param dataset The dataset to verify
     * @return Result containing signature status or error
     *
     * @note Returns signature_status::no_signature if no signatures present.
     * @note Returns signature_status::invalid if any signature fails verification.
     */
    [[nodiscard]] static auto verify(
        const core::dicom_dataset& dataset
    ) -> kcenon::common::Result<signature_status>;

    /**
     * @brief Verify digital signatures with a trusted certificate store
     *
     * Verifies signatures and validates the signer's certificate against
     * a set of trusted certificates.
     *
     * @param dataset The dataset to verify
     * @param trusted_certs Trusted certificates for validation
     * @return Result containing signature status or error
     */
    [[nodiscard]] static auto verify_with_trust(
        const core::dicom_dataset& dataset,
        const std::vector<certificate>& trusted_certs
    ) -> kcenon::common::Result<signature_status>;

    // ========================================================================
    // Signature Information
    // ========================================================================

    /**
     * @brief Get information about signatures in a dataset
     *
     * Extracts signature metadata from the Digital Signature Sequence
     * without performing verification.
     *
     * @param dataset The dataset to inspect
     * @return Optional containing signature info, or nullopt if no signature
     */
    [[nodiscard]] static auto get_signature_info(
        const core::dicom_dataset& dataset
    ) -> std::optional<signature_info>;

    /**
     * @brief Get all signatures in a dataset
     *
     * Extracts information for all signatures if multiple are present.
     *
     * @param dataset The dataset to inspect
     * @return Vector of signature info (empty if no signatures)
     */
    [[nodiscard]] static auto get_all_signatures(
        const core::dicom_dataset& dataset
    ) -> std::vector<signature_info>;

    /**
     * @brief Check if a dataset contains digital signatures
     *
     * @param dataset The dataset to check
     * @return true if Digital Signature Sequence is present
     */
    [[nodiscard]] static auto has_signature(
        const core::dicom_dataset& dataset
    ) -> bool;

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /**
     * @brief Remove all digital signatures from a dataset
     *
     * Removes the Digital Signature Sequence (0400,0561) from the dataset.
     *
     * @param dataset The dataset to modify
     * @return true if signatures were removed, false if none present
     */
    static auto remove_signatures(
        core::dicom_dataset& dataset
    ) -> bool;

    /**
     * @brief Generate a new Digital Signature UID
     *
     * Creates a new unique identifier for a digital signature.
     *
     * @return New UID string
     */
    [[nodiscard]] static auto generate_signature_uid() -> std::string;

private:
    // Internal helper methods
    static auto compute_mac(
        const core::dicom_dataset& dataset,
        std::span<const core::dicom_tag> tags,
        mac_algorithm algo
    ) -> std::vector<std::uint8_t>;

    static auto sign_mac(
        std::span<const std::uint8_t> mac_data,
        const private_key& key,
        signature_algorithm algo
    ) -> kcenon::common::Result<std::vector<std::uint8_t>>;

    static auto verify_mac_signature(
        std::span<const std::uint8_t> mac_data,
        std::span<const std::uint8_t> signature,
        const certificate& cert,
        signature_algorithm algo
    ) -> bool;
};

} // namespace pacs::security
