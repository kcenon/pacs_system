/**
 * @file jwt_validator_test.cpp
 * @brief Unit tests for JWT token validation
 *
 * @see RFC 7519 - JSON Web Token (JWT)
 * @see Issue #860 - Add OAuth 2.0 configuration and JWT token validation core
 */

#include <pacs/web/auth/jwt_validator.hpp>
#include <pacs/web/auth/oauth2_config.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>

using namespace kcenon::pacs::web::auth;

// ============================================================================
// Test Helpers
// ============================================================================

namespace {

/// Base64url encode a string (for test token construction)
std::string base64url_encode(std::string_view input) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string result;
    result.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < input.size()) {
        auto b0 = static_cast<unsigned char>(input[i]);
        auto b1 = static_cast<unsigned char>(input[i + 1]);
        auto b2 = static_cast<unsigned char>(input[i + 2]);
        result += table[b0 >> 2];
        result += table[((b0 & 0x03) << 4) | (b1 >> 4)];
        result += table[((b1 & 0x0F) << 2) | (b2 >> 6)];
        result += table[b2 & 0x3F];
        i += 3;
    }

    if (i + 1 == input.size()) {
        auto b0 = static_cast<unsigned char>(input[i]);
        result += table[b0 >> 2];
        result += table[(b0 & 0x03) << 4];
    } else if (i + 2 == input.size()) {
        auto b0 = static_cast<unsigned char>(input[i]);
        auto b1 = static_cast<unsigned char>(input[i + 1]);
        result += table[b0 >> 2];
        result += table[((b0 & 0x03) << 4) | (b1 >> 4)];
        result += table[(b1 & 0x0F) << 2];
    }

    // No padding in Base64url
    return result;
}

/// Get current Unix timestamp
std::int64_t now_timestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

/// Build a JWT token string from header/payload JSON (with dummy signature)
std::string make_test_token(const std::string& header_json,
                            const std::string& payload_json,
                            const std::string& sig = "test-signature") {
    return base64url_encode(header_json) + "." +
           base64url_encode(payload_json) + "." +
           base64url_encode(sig);
}

/// Default test config
oauth2_config test_config() {
    oauth2_config config;
    config.enabled = true;
    config.issuer = "https://auth.example.com";
    config.audience = "pacs-api";
    config.clock_skew_seconds = 60;
    config.allowed_algorithms = {"RS256", "ES256"};
    return config;
}

/// Build a valid header JSON string
std::string valid_header(const std::string& alg = "RS256") {
    return R"({"alg":")" + alg + R"(","typ":"JWT","kid":"key-1"})";
}

/// Build a valid payload JSON string with configurable exp
std::string valid_payload(std::int64_t exp_offset = 3600) {
    auto now = now_timestamp();
    return R"({"iss":"https://auth.example.com","sub":"user-123",)"
           R"("aud":"pacs-api","exp":)" + std::to_string(now + exp_offset) +
           R"(,"iat":)" + std::to_string(now) +
           R"(,"nbf":)" + std::to_string(now - 10) +
           R"(,"jti":"token-abc-123",)"
           R"("scope":"dicomweb.read dicomweb.write"})";
}

}  // namespace

// ============================================================================
// Base64url Tests
// ============================================================================

TEST_CASE("base64url_decode handles standard cases",
          "[web][auth][base64url]") {

    SECTION("empty string") {
        auto result = base64url_decode("");
        REQUIRE(result.has_value());
        CHECK(result->empty());
    }

    SECTION("standard Base64url decoding") {
        // "Hello" = SGVsbG8
        auto result = base64url_decode("SGVsbG8");
        REQUIRE(result.has_value());
        CHECK(*result == "Hello");
    }

    SECTION("decode with URL-safe characters") {
        // Characters that differ from standard Base64: - and _
        auto encoded = base64url_encode("test?data>here");
        auto result = base64url_decode(encoded);
        REQUIRE(result.has_value());
        CHECK(*result == "test?data>here");
    }

    SECTION("invalid characters return nullopt") {
        auto result = base64url_decode("SGVs!G8=");
        CHECK_FALSE(result.has_value());
    }

    SECTION("roundtrip encode/decode") {
        std::string original = R"({"alg":"RS256","typ":"JWT"})";
        auto encoded = base64url_encode(original);
        auto decoded = base64url_decode(encoded);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == original);
    }
}

// ============================================================================
// JWT Decode Tests
// ============================================================================

TEST_CASE("jwt_validator decode parses valid tokens",
          "[web][auth][jwt]") {
    jwt_validator validator(test_config());

    SECTION("valid RS256 token") {
        auto token_str = make_test_token(valid_header(), valid_payload());
        auto [token, err] = validator.decode(token_str);

        CHECK(err == jwt_error::none);
        CHECK(token.header.alg == "RS256");
        CHECK(token.header.typ == "JWT");
        CHECK(token.header.kid == "key-1");
        CHECK(token.claims.iss == "https://auth.example.com");
        CHECK(token.claims.sub == "user-123");
        CHECK(token.claims.aud == "pacs-api");
        CHECK(token.claims.exp > 0);
        CHECK(token.claims.jti == "token-abc-123");
    }

    SECTION("valid ES256 token") {
        auto token_str = make_test_token(valid_header("ES256"), valid_payload());
        auto [token, err] = validator.decode(token_str);

        CHECK(err == jwt_error::none);
        CHECK(token.header.alg == "ES256");
    }

    SECTION("parses scope claim") {
        auto token_str = make_test_token(valid_header(), valid_payload());
        auto [token, err] = validator.decode(token_str);

        CHECK(err == jwt_error::none);
        REQUIRE(token.claims.scopes.size() == 2);
        CHECK(token.claims.scopes[0] == "dicomweb.read");
        CHECK(token.claims.scopes[1] == "dicomweb.write");
    }

    SECTION("stores header_payload for signature verification") {
        auto token_str = make_test_token(valid_header(), valid_payload());
        auto [token, err] = validator.decode(token_str);

        CHECK(err == jwt_error::none);
        // header_payload should be the first two segments joined by dot
        auto dot = token_str.rfind('.');
        CHECK(token.header_payload == token_str.substr(0, dot));
    }
}

TEST_CASE("jwt_validator decode detects malformed tokens",
          "[web][auth][jwt]") {
    jwt_validator validator(test_config());

    SECTION("empty string") {
        auto [token, err] = validator.decode("");
        CHECK(err == jwt_error::malformed_token);
    }

    SECTION("no dots") {
        auto [token, err] = validator.decode("noDotsHere");
        CHECK(err == jwt_error::malformed_token);
    }

    SECTION("only one dot") {
        auto [token, err] = validator.decode("one.dot");
        CHECK(err == jwt_error::malformed_token);
    }

    SECTION("four dots") {
        auto [token, err] = validator.decode("a.b.c.d");
        CHECK(err == jwt_error::malformed_token);
    }

    SECTION("invalid base64url in header") {
        auto [token, err] = validator.decode("!!!.payload.sig");
        CHECK(err == jwt_error::invalid_base64);
    }

    SECTION("invalid JSON in header") {
        auto token_str = base64url_encode("not json") + "." +
                         base64url_encode(valid_payload()) + "." +
                         base64url_encode("sig");
        auto [token, err] = validator.decode(token_str);
        CHECK(err == jwt_error::invalid_json);
    }

    SECTION("missing alg in header") {
        auto token_str = make_test_token(
            R"({"typ":"JWT","kid":"key-1"})", valid_payload());
        auto [token, err] = validator.decode(token_str);
        CHECK(err == jwt_error::missing_required_claim);
    }
}

TEST_CASE("jwt_validator decode rejects unsupported algorithms",
          "[web][auth][jwt]") {
    jwt_validator validator(test_config());

    SECTION("HS256 is not allowed") {
        auto token_str = make_test_token(
            R"({"alg":"HS256","typ":"JWT"})", valid_payload());
        auto [token, err] = validator.decode(token_str);
        CHECK(err == jwt_error::unsupported_algorithm);
    }

    SECTION("none algorithm is rejected") {
        auto token_str = make_test_token(
            R"({"alg":"none","typ":"JWT"})", valid_payload());
        auto [token, err] = validator.decode(token_str);
        CHECK(err == jwt_error::unsupported_algorithm);
    }
}

// ============================================================================
// Claims Validation Tests
// ============================================================================

TEST_CASE("jwt_validator validate_claims checks temporal claims",
          "[web][auth][jwt]") {
    jwt_validator validator(test_config());

    SECTION("valid claims pass") {
        auto token_str = make_test_token(valid_header(), valid_payload());
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) == jwt_error::none);
    }

    SECTION("expired token is rejected") {
        auto token_str = make_test_token(
            valid_header(), valid_payload(-3600));  // Expired 1 hour ago
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) ==
              jwt_error::token_expired);
    }

    SECTION("expired within clock skew is accepted") {
        auto token_str = make_test_token(
            valid_header(), valid_payload(-30));  // Expired 30s ago (skew=60)
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) == jwt_error::none);
    }

    SECTION("not-yet-valid token is rejected") {
        auto now = now_timestamp();
        std::string payload =
            R"({"iss":"https://auth.example.com","sub":"user-123",)"
            R"("aud":"pacs-api","exp":)" + std::to_string(now + 7200) +
            R"(,"nbf":)" + std::to_string(now + 3600) + R"(})";
        auto token_str = make_test_token(valid_header(), payload);
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) ==
              jwt_error::token_not_yet_valid);
    }
}

TEST_CASE("jwt_validator validate_claims checks issuer and audience",
          "[web][auth][jwt]") {

    SECTION("wrong issuer is rejected") {
        jwt_validator validator(test_config());

        auto now = now_timestamp();
        std::string payload =
            R"({"iss":"https://wrong.example.com","sub":"user-123",)"
            R"("aud":"pacs-api","exp":)" + std::to_string(now + 3600) + R"(})";
        auto token_str = make_test_token(valid_header(), payload);
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) ==
              jwt_error::invalid_issuer);
    }

    SECTION("wrong audience is rejected") {
        jwt_validator validator(test_config());

        auto now = now_timestamp();
        std::string payload =
            R"({"iss":"https://auth.example.com","sub":"user-123",)"
            R"("aud":"wrong-api","exp":)" + std::to_string(now + 3600) + R"(})";
        auto token_str = make_test_token(valid_header(), payload);
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) ==
              jwt_error::invalid_audience);
    }

    SECTION("empty issuer config skips issuer check") {
        auto config = test_config();
        config.issuer = "";
        jwt_validator validator(config);

        auto now = now_timestamp();
        std::string payload =
            R"({"iss":"anything","sub":"user-123",)"
            R"("aud":"pacs-api","exp":)" + std::to_string(now + 3600) + R"(})";
        auto token_str = make_test_token(valid_header(), payload);
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) == jwt_error::none);
    }

    SECTION("empty audience config skips audience check") {
        auto config = test_config();
        config.audience = "";
        jwt_validator validator(config);

        auto now = now_timestamp();
        std::string payload =
            R"({"iss":"https://auth.example.com","sub":"user-123",)"
            R"("aud":"anything","exp":)" + std::to_string(now + 3600) + R"(})";
        auto token_str = make_test_token(valid_header(), payload);
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK(validator.validate_claims(token.claims) == jwt_error::none);
    }
}

TEST_CASE("jwt_validator validate_claims requires sub claim",
          "[web][auth][jwt]") {
    jwt_validator validator(test_config());

    auto now = now_timestamp();
    std::string payload =
        R"({"iss":"https://auth.example.com",)"
        R"("aud":"pacs-api","exp":)" + std::to_string(now + 3600) + R"(})";
    auto token_str = make_test_token(valid_header(), payload);
    auto [token, err] = validator.decode(token_str);
    REQUIRE(err == jwt_error::none);

    CHECK(validator.validate_claims(token.claims) ==
          jwt_error::missing_required_claim);
}

// ============================================================================
// Scope Checking Tests
// ============================================================================

TEST_CASE("jwt_validator scope checking",
          "[web][auth][jwt]") {

    SECTION("has_scope finds matching scope") {
        jwt_claims claims;
        claims.scopes = {"dicomweb.read", "dicomweb.write"};

        CHECK(jwt_validator::has_scope(claims, "dicomweb.read"));
        CHECK(jwt_validator::has_scope(claims, "dicomweb.write"));
        CHECK_FALSE(jwt_validator::has_scope(claims, "dicomweb.delete"));
    }

    SECTION("has_scope handles empty scopes") {
        jwt_claims claims;
        CHECK_FALSE(jwt_validator::has_scope(claims, "dicomweb.read"));
    }

    SECTION("has_any_scope finds at least one match") {
        jwt_claims claims;
        claims.scopes = {"dicomweb.read"};

        CHECK(jwt_validator::has_any_scope(
            claims, {"dicomweb.read", "dicomweb.search"}));
        CHECK_FALSE(jwt_validator::has_any_scope(
            claims, {"dicomweb.write", "dicomweb.delete"}));
    }

    SECTION("has_any_scope with empty scopes") {
        jwt_claims claims;
        CHECK_FALSE(jwt_validator::has_any_scope(
            claims, {"dicomweb.read"}));
    }
}

// ============================================================================
// Error Message Tests
// ============================================================================

TEST_CASE("jwt_error_message returns descriptions",
          "[web][auth][jwt]") {
    CHECK_FALSE(jwt_error_message(jwt_error::none).empty());
    CHECK_FALSE(jwt_error_message(jwt_error::malformed_token).empty());
    CHECK_FALSE(jwt_error_message(jwt_error::token_expired).empty());
    CHECK_FALSE(jwt_error_message(jwt_error::invalid_signature).empty());
    CHECK_FALSE(jwt_error_message(jwt_error::unsupported_algorithm).empty());
}

// ============================================================================
// Config Accessor Tests
// ============================================================================

TEST_CASE("jwt_validator config accessor",
          "[web][auth][jwt]") {
    auto config = test_config();
    jwt_validator validator(config);

    CHECK(validator.config().enabled);
    CHECK(validator.config().issuer == "https://auth.example.com");
    CHECK(validator.config().audience == "pacs-api");
    CHECK(validator.config().clock_skew_seconds == 60);
}

// ============================================================================
// Signature Verification Tests (requires OpenSSL)
// ============================================================================

#ifdef PACS_WITH_DIGITAL_SIGNATURES

TEST_CASE("jwt_validator RS256 signature verification",
          "[web][auth][jwt][signature]") {
    jwt_validator validator(test_config());

    SECTION("invalid PEM key returns false") {
        auto token_str = make_test_token(valid_header(), valid_payload());
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK_FALSE(validator.verify_rs256(token, "not a valid PEM key"));
    }

    SECTION("wrong key returns false") {
        auto token_str = make_test_token(valid_header(), valid_payload());
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        // A valid PEM key that doesn't match the signature
        constexpr auto test_rsa_pub = R"(-----BEGIN PUBLIC KEY-----
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2a2rwplBQLHHMNHqLyM0
yeIqnG7sCDR7gUED6gqFAEhGcIH+b1Rv0cEykIR/hGUKxbz7GF0Bep9bGg4DLRA
OwPWSwCBiiYajRVJJSmGMCnY42M1v4DnFVmNBOuIKGjVjreS0J8IJCC7IXAER7IN
yLH0GnDUYgjKwF8v2enEtAfiYGe8SJeNzpHXHJRyv0yMCHSMn1GMa5ON+Z6P0oU2
RE9NYFCjduJJMDm6FBtyb1FuRZzRvG9GYpZS7LWfWlPGDjSWE8hxGRHnB+Oy7cv
L+SmmKneqt4j3BWVq4OsT29svAoNW8z+G6IH8Pao8xVsHcRzpfTJHHHB4Bf6IGQR
QwIDAQAB
-----END PUBLIC KEY-----)";

        CHECK_FALSE(validator.verify_rs256(token, test_rsa_pub));
    }
}

TEST_CASE("jwt_validator ES256 signature verification",
          "[web][auth][jwt][signature]") {
    jwt_validator validator(test_config());

    SECTION("invalid PEM key returns false") {
        auto token_str = make_test_token(
            valid_header("ES256"), valid_payload());
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        CHECK_FALSE(validator.verify_es256(token, "not a valid PEM key"));
    }

    SECTION("wrong signature size returns false") {
        auto token_str = make_test_token(
            valid_header("ES256"), valid_payload(), "short");
        auto [token, err] = validator.decode(token_str);
        REQUIRE(err == jwt_error::none);

        // ES256 expects 64-byte raw signature, "short" is too small
        constexpr auto test_ec_pub = R"(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEV3kp9N7dNKUcVBMhOb8yqXNMFXdA
Ht1Nc1k5JD4GMPK7oL7vGFE8vJ29PjwKPrcKNe7jSx7sPGPYW03Y7H5QA==
-----END PUBLIC KEY-----)";

        CHECK_FALSE(validator.verify_es256(token, test_ec_pub));
    }
}

#else  // !PACS_WITH_DIGITAL_SIGNATURES

TEST_CASE("jwt_validator signature verification returns false without OpenSSL",
          "[web][auth][jwt][signature]") {
    jwt_validator validator(test_config());
    auto token_str = make_test_token(valid_header(), valid_payload());
    auto [token, err] = validator.decode(token_str);
    REQUIRE(err == jwt_error::none);

    CHECK_FALSE(validator.verify_rs256(token, "any key"));
    CHECK_FALSE(validator.verify_es256(token, "any key"));
}

#endif  // PACS_WITH_DIGITAL_SIGNATURES
