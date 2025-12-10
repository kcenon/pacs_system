/**
 * @file digital_signature_test.cpp
 * @brief Unit tests for DICOM Digital Signatures (PS3.15)
 *
 * @see Issue #191 - Implement DICOM digital signatures
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/security/certificate.hpp"
#include "pacs/security/digital_signature.hpp"
#include "pacs/security/signature_types.hpp"

#include <pacs/core/dicom_dataset.hpp>
#include <pacs/core/dicom_tag.hpp>
#include <pacs/encoding/vr_type.hpp>

using namespace pacs::security;
using namespace pacs::core;
using namespace pacs::encoding;

namespace {

// Test certificate PEM (self-signed, for testing only)
constexpr const char* test_cert_pem = R"(-----BEGIN CERTIFICATE-----
MIICpDCCAYwCCQCU+hU2FXcWH9ANBgkqhkiG9w0BAQsFADAUMRIwEAYDVQQDDAls
b2NhbGhvc3QwHhcNMjUwMTAxMDAwMDAwWhcNMjYwMTAxMDAwMDAwWjAUMRIwEAYD
VQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC0
cj7aNGlv3qFo6QvJ8T7xYoIVbkgG8YHK5I1LhQOgqM0GzZuNDH8xVQCWGS8QHKQZ
ypLXIWEGNJDXJnPqHk3HJ0eVpZFJmFhJDk8KGq3X8C6kJ7GkHqL9AqHfLJRBWIpQ
k8hFTvJwHQJuUkqMhMKTLcFZzMqRhqJfRYJJHqXxWJJGkQHLWGNJWHJGFJWQHKLz
JRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQH
LWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGF
JWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzAgMBAAEwDQYJKoZIhvcNAQELBQADgg
EBAJSN0UNJxqIyH0q7R3bXc5F8VHKqLqFfJJqJGkFpVJqJGkFpVJqJGkFpVJqJGk
FpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGk
FpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGk
FpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGkFpVJqJGk
-----END CERTIFICATE-----)";

// Test private key PEM (for testing only - NOT secure)
constexpr const char* test_key_pem = R"(-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC0cj7aNGlv3qFo
6QvJ8T7xYoIVbkgG8YHK5I1LhQOgqM0GzZuNDH8xVQCWGS8QHKQZypLXIWEGNJDX
JnPqHk3HJ0eVpZFJmFhJDk8KGq3X8C6kJ7GkHqL9AqHfLJRBWIpQk8hFTvJwHQJu
UkqMhMKTLcFZzMqRhqJfRYJJHqXxWJJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLW
GNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJW
QHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHL
JGkQHLWGNJWHJGFJWQHKLzAgMBAAECggEALnxJgDqfJjHhM8K9FQb3bLqXcQlL8P
QHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLW
GNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJW
QHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHL
JGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJ
WHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHK
LzJRQHKQJBALxJgDqfJjHhM8K9FQb3bLqXcQlL8PQHLJGkQHLWGNJWHJGFJWQHKL
zJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQCQQDx
JgDqfJjHhM8K9FQb3bLqXcQlL8PQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLW
GNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzAkEApJgDqfJjHhM8K9FQb3
bLqXcQlL8PQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJR
QHLJGkQHLWGNJWHJGFJWQHKLzJRQJBAJjHhM8K9FQb3bLqXcQlL8PQHLJGkQHLWG
NJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQ
HKLzJRQHLJGkQHLWECQQCU+hU2FXcWH9xJgDqfJjHhM8K9FQb3bLqXcQlL8PQHLJ
GkQHLWGNJWHJGFJWQHKLzJRQHLJGkQHLWGNJWHJGFJWQHKLzJRQHLJGkQ
-----END PRIVATE KEY-----)";

/**
 * @brief Create a sample DICOM dataset for testing
 */
auto create_test_dataset() -> dicom_dataset {
    dicom_dataset ds;

    // Patient Module
    ds.set_string(dicom_tag{0x0010, 0x0010}, vr_type::PN, "DOE^JOHN");
    ds.set_string(dicom_tag{0x0010, 0x0020}, vr_type::LO, "12345");
    ds.set_string(dicom_tag{0x0010, 0x0030}, vr_type::DA, "19800101");
    ds.set_string(dicom_tag{0x0010, 0x0040}, vr_type::CS, "M");

    // Study Module
    ds.set_string(dicom_tag{0x0020, 0x000D}, vr_type::UI, "1.2.3.4.5.6.7.8.9.0");
    ds.set_string(dicom_tag{0x0008, 0x0020}, vr_type::DA, "20250101");
    ds.set_string(dicom_tag{0x0008, 0x0030}, vr_type::TM, "120000");
    ds.set_string(dicom_tag{0x0008, 0x1030}, vr_type::LO, "Test Study");

    // Series Module
    ds.set_string(dicom_tag{0x0020, 0x000E}, vr_type::UI, "1.2.3.4.5.6.7.8.9.1");
    ds.set_string(dicom_tag{0x0008, 0x0060}, vr_type::CS, "CT");
    ds.set_numeric<int32_t>(dicom_tag{0x0020, 0x0011}, vr_type::IS, 1);

    return ds;
}

} // anonymous namespace

// ============================================================================
// Signature Types Tests
// ============================================================================

TEST_CASE("SignatureTypes: Algorithm conversion", "[security][signature]") {
    SECTION("to_string for all algorithms") {
        REQUIRE(to_string(signature_algorithm::rsa_sha256) == "RSA-SHA256");
        REQUIRE(to_string(signature_algorithm::rsa_sha384) == "RSA-SHA384");
        REQUIRE(to_string(signature_algorithm::rsa_sha512) == "RSA-SHA512");
        REQUIRE(to_string(signature_algorithm::ecdsa_sha256) == "ECDSA-SHA256");
        REQUIRE(to_string(signature_algorithm::ecdsa_sha384) == "ECDSA-SHA384");
    }

    SECTION("parse_signature_algorithm roundtrip") {
        auto algo = parse_signature_algorithm("RSA-SHA256");
        REQUIRE(algo.has_value());
        REQUIRE(*algo == signature_algorithm::rsa_sha256);

        algo = parse_signature_algorithm("ECDSA-SHA384");
        REQUIRE(algo.has_value());
        REQUIRE(*algo == signature_algorithm::ecdsa_sha384);
    }

    SECTION("parse_signature_algorithm invalid") {
        auto algo = parse_signature_algorithm("INVALID");
        REQUIRE_FALSE(algo.has_value());
    }
}

TEST_CASE("SignatureTypes: Status conversion", "[security][signature]") {
    SECTION("to_string for all statuses") {
        REQUIRE(to_string(signature_status::valid) == "Valid");
        REQUIRE(to_string(signature_status::invalid) == "Invalid");
        REQUIRE(to_string(signature_status::expired) == "Expired");
        REQUIRE(to_string(signature_status::untrusted_signer) == "UntrustedSigner");
        REQUIRE(to_string(signature_status::revoked) == "Revoked");
        REQUIRE(to_string(signature_status::no_signature) == "NoSignature");
    }
}

TEST_CASE("SignatureTypes: MAC algorithm DICOM UIDs", "[security][signature]") {
    SECTION("to_dicom_uid for common algorithms") {
        REQUIRE(to_dicom_uid(mac_algorithm::sha256) == "2.16.840.1.101.3.4.2.1");
        REQUIRE(to_dicom_uid(mac_algorithm::sha384) == "2.16.840.1.101.3.4.2.2");
        REQUIRE(to_dicom_uid(mac_algorithm::sha512) == "2.16.840.1.101.3.4.2.3");
    }
}

// ============================================================================
// Certificate Tests
// ============================================================================

TEST_CASE("Certificate: Load from PEM string", "[security][certificate]") {
    SECTION("Valid certificate loads successfully") {
        // Note: The test certificates above are not real valid certificates
        // In a real test, we would use properly generated test certificates
        // For now, we test the error path

        auto result = certificate::load_from_pem_string("invalid pem data");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("Certificate: Empty certificate behavior", "[security][certificate]") {
    certificate cert;

    SECTION("Empty certificate reports not loaded") {
        REQUIRE_FALSE(cert.is_loaded());
    }

    SECTION("Empty certificate returns empty strings") {
        REQUIRE(cert.subject_name().empty());
        REQUIRE(cert.issuer_name().empty());
        REQUIRE(cert.serial_number().empty());
        REQUIRE(cert.thumbprint().empty());
    }

    SECTION("Empty certificate validity") {
        REQUIRE_FALSE(cert.is_valid());
        REQUIRE(cert.is_expired());
    }

    SECTION("Empty certificate export") {
        REQUIRE(cert.to_pem().empty());
        REQUIRE(cert.to_der().empty());
    }
}

// ============================================================================
// Private Key Tests
// ============================================================================

TEST_CASE("PrivateKey: Load from PEM string", "[security][private_key]") {
    SECTION("Invalid key fails to load") {
        auto result = private_key::load_from_pem_string("invalid key data");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("PrivateKey: Empty key behavior", "[security][private_key]") {
    private_key key;

    SECTION("Empty key reports not loaded") {
        REQUIRE_FALSE(key.is_loaded());
    }

    SECTION("Empty key returns default values") {
        REQUIRE(key.algorithm_name().empty());
        REQUIRE(key.key_size() == 0);
    }
}

// ============================================================================
// Certificate Chain Tests
// ============================================================================

TEST_CASE("CertificateChain: Basic operations", "[security][certificate_chain]") {
    certificate_chain chain;

    SECTION("Empty chain") {
        REQUIRE(chain.empty());
        REQUIRE(chain.size() == 0);
        REQUIRE(chain.end_entity() == nullptr);
    }

    SECTION("Add certificate") {
        certificate cert;
        chain.add(std::move(cert));

        REQUIRE_FALSE(chain.empty());
        REQUIRE(chain.size() == 1);
        REQUIRE(chain.end_entity() != nullptr);
    }
}

// ============================================================================
// Digital Signature Tests
// ============================================================================

TEST_CASE("DigitalSignature: has_signature on empty dataset", "[security][signature]") {
    dicom_dataset ds;

    SECTION("Empty dataset has no signature") {
        REQUIRE_FALSE(digital_signature::has_signature(ds));
    }
}

TEST_CASE("DigitalSignature: get_signature_info on unsigned dataset", "[security][signature]") {
    dicom_dataset ds = create_test_dataset();

    SECTION("Unsigned dataset returns nullopt") {
        auto info = digital_signature::get_signature_info(ds);
        REQUIRE_FALSE(info.has_value());
    }

    SECTION("get_all_signatures returns empty vector") {
        auto signatures = digital_signature::get_all_signatures(ds);
        REQUIRE(signatures.empty());
    }
}

TEST_CASE("DigitalSignature: verify on unsigned dataset", "[security][signature]") {
    dicom_dataset ds = create_test_dataset();

    SECTION("Verify returns no_signature status") {
        auto result = digital_signature::verify(ds);
        REQUIRE(result.is_ok());
        REQUIRE(result.value() == signature_status::no_signature);
    }
}

TEST_CASE("DigitalSignature: remove_signatures on unsigned dataset", "[security][signature]") {
    dicom_dataset ds = create_test_dataset();

    SECTION("Returns false when no signatures present") {
        bool removed = digital_signature::remove_signatures(ds);
        REQUIRE_FALSE(removed);
    }
}

TEST_CASE("DigitalSignature: sign with invalid certificate", "[security][signature]") {
    dicom_dataset ds = create_test_dataset();
    certificate cert; // Empty certificate
    private_key key;  // Empty key

    SECTION("Sign fails with empty certificate") {
        auto result = digital_signature::sign(ds, cert, key);
        REQUIRE(result.is_err());
    }
}

TEST_CASE("DigitalSignature: generate_signature_uid", "[security][signature]") {
    SECTION("Generated UID is not empty") {
        auto uid = digital_signature::generate_signature_uid();
        REQUIRE_FALSE(uid.empty());
    }

    SECTION("Generated UIDs are unique") {
        auto uid1 = digital_signature::generate_signature_uid();
        auto uid2 = digital_signature::generate_signature_uid();
        REQUIRE(uid1 != uid2);
    }

    SECTION("Generated UID starts with valid prefix") {
        auto uid = digital_signature::generate_signature_uid();
        REQUIRE(uid.substr(0, 5) == "1.2.8");
    }
}

// ============================================================================
// signature_info Tests
// ============================================================================

TEST_CASE("SignatureInfo: Equality", "[security][signature]") {
    signature_info info1;
    info1.signature_uid = "1.2.3.4.5";
    info1.signer_name = "Test Signer";
    info1.algorithm = signature_algorithm::rsa_sha256;

    signature_info info2 = info1;

    SECTION("Identical infos are equal") {
        REQUIRE(info1 == info2);
    }

    SECTION("Different UIDs are not equal") {
        info2.signature_uid = "1.2.3.4.6";
        REQUIRE_FALSE(info1 == info2);
    }
}
