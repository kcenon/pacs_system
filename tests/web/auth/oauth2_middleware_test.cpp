/**
 * @file oauth2_middleware_test.cpp
 * @brief Unit tests for OAuth 2.0 middleware and JWKS provider
 *
 * @see RFC 6749 - OAuth 2.0 Authorization Framework
 * @see Issue #852 - Implement OAuth 2.0 authorization for DICOMweb endpoints
 */

// IMPORTANT: Include Crow FIRST before any PACS headers
#include "crow.h"

#include <kcenon/pacs/web/auth/jwks_provider.h>
#include <kcenon/pacs/web/auth/jwt_validator.h>
#include <kcenon/pacs/web/auth/oauth2_config.h>
#include <kcenon/pacs/web/auth/oauth2_middleware.h>

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

    return result;
}

std::int64_t now_timestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string make_test_token(const std::string& header_json,
                            const std::string& payload_json,
                            const std::string& sig = "test-signature") {
    return base64url_encode(header_json) + "." +
           base64url_encode(payload_json) + "." +
           base64url_encode(sig);
}

oauth2_config test_config() {
    oauth2_config config;
    config.enabled = true;
    config.issuer = "https://auth.example.com";
    config.audience = "pacs-api";
    config.clock_skew_seconds = 60;
    config.allowed_algorithms = {"RS256", "ES256"};
    return config;
}

std::string valid_header(const std::string& alg = "RS256") {
    return R"({"alg":")" + alg + R"(","typ":"JWT","kid":"key-1"})";
}

std::string valid_payload(std::int64_t exp_offset = 3600,
                          const std::string& scope = "dicomweb.read dicomweb.write") {
    auto now = now_timestamp();
    return R"({"iss":"https://auth.example.com","sub":"user-123",)"
           R"("aud":"pacs-api","exp":)" + std::to_string(now + exp_offset) +
           R"(,"iat":)" + std::to_string(now) +
           R"(,"nbf":)" + std::to_string(now - 10) +
           R"(,"jti":"token-abc-123",)"
           R"("scope":")" + scope + R"("})";
}

}  // namespace

// ============================================================================
// JWKS Provider Tests
// ============================================================================

TEST_CASE("jwks_provider basic operations", "[web][auth][jwks]") {

    SECTION("default state has no keys") {
        jwks_provider provider;
        CHECK(provider.keys().empty());
        CHECK(provider.is_cache_expired());
    }

    SECTION("load_from_json rejects invalid JSON") {
        jwks_provider provider;
        CHECK_FALSE(provider.load_from_json("not json"));
        CHECK_FALSE(provider.load_from_json(R"({"nokeys":true})"));
        CHECK_FALSE(provider.load_from_json(R"({"keys":"not array"})"));
    }

    SECTION("load_from_json rejects empty key set") {
        jwks_provider provider;
        CHECK_FALSE(provider.load_from_json(R"({"keys":[]})"));
    }

#ifdef PACS_WITH_DIGITAL_SIGNATURES
    SECTION("load_from_json loads RSA key") {
        jwks_provider provider;
        // A minimal RSA JWK with test key data
        std::string jwks = R"({
            "keys": [{
                "kty": "RSA",
                "kid": "test-key-1",
                "alg": "RS256",
                "use": "sig",
                "n": "0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw",
                "e": "AQAB"
            }]
        })";

        CHECK(provider.load_from_json(jwks));
        REQUIRE(provider.keys().size() == 1);
        CHECK(provider.keys()[0].kid == "test-key-1");
        CHECK(provider.keys()[0].kty == "RSA");
        CHECK(provider.keys()[0].alg == "RS256");
        CHECK_FALSE(provider.keys()[0].pem.empty());
        CHECK(provider.keys()[0].pem.find("BEGIN PUBLIC KEY") != std::string::npos);
    }

    SECTION("get_key returns key by kid") {
        jwks_provider provider;
        std::string jwks = R"({
            "keys": [{
                "kty": "RSA",
                "kid": "key-abc",
                "alg": "RS256",
                "use": "sig",
                "n": "0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw",
                "e": "AQAB"
            }]
        })";
        provider.load_from_json(jwks);

        auto key = provider.get_key("key-abc");
        REQUIRE(key.has_value());
        CHECK(key->kid == "key-abc");

        CHECK_FALSE(provider.get_key("nonexistent").has_value());
    }

    SECTION("get_key_by_alg returns key by algorithm") {
        jwks_provider provider;
        std::string jwks = R"({
            "keys": [{
                "kty": "RSA",
                "kid": "rs-key",
                "alg": "RS256",
                "use": "sig",
                "n": "0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw",
                "e": "AQAB"
            }]
        })";
        provider.load_from_json(jwks);

        auto key = provider.get_key_by_alg("RS256");
        REQUIRE(key.has_value());
        CHECK(key->alg == "RS256");

        CHECK_FALSE(provider.get_key_by_alg("ES256").has_value());
    }
#endif  // PACS_WITH_DIGITAL_SIGNATURES

    SECTION("set_cache_ttl and is_cache_expired") {
        jwks_provider provider;
        provider.set_cache_ttl(0);  // Immediately expired

        // No keys loaded yet, so always expired
        CHECK(provider.is_cache_expired());
    }

    SECTION("fetcher callback is invoked on refresh") {
        jwks_provider provider;
        bool fetcher_called = false;
        provider.set_fetcher([&](const std::string& url) -> std::optional<std::string> {
            fetcher_called = true;
            CHECK(url == "https://auth.example.com/.well-known/jwks.json");
            return std::nullopt;  // Simulate failure
        });
        provider.set_jwks_url("https://auth.example.com/.well-known/jwks.json");

        CHECK_FALSE(provider.refresh());
        CHECK(fetcher_called);
    }

    SECTION("skips non-signature keys") {
        jwks_provider provider;
        // Key with use=enc should be skipped
        std::string jwks = R"({
            "keys": [{
                "kty": "RSA",
                "kid": "enc-key",
                "use": "enc",
                "n": "0vx7agoebGcQSuuPiLJXZptN9nndrQmbXEps2aiAFbWhM78LhWx4cbbfAAtVT86zwu1RK7aPFFxuhDR1L6tSoc_BJECPebWKRXjBZCiFV4n3oknjhMstn64tZ_2W-5JsGY4Hc5n9yBXArwl93lqt7_RN5w6Cf0h4QyQ5v-65YGjQR0_FDW2QvzqY368QQMicAtaSqzs8KJZgnYb9c7d0zgdAZHzu6qMQvRL5hajrn1n91CbOpbISD08qNLyrdkt-bFTWhAI4vMQFh6WeZu0fM4lFd2NcRwr3XPksINHaQ-G_xBniIqbw0Ls1jF44-csFCur-kEgU8awapJzKnqDKgw",
                "e": "AQAB"
            }]
        })";
        CHECK_FALSE(provider.load_from_json(jwks));
    }
}

// ============================================================================
// OAuth 2.0 Middleware Tests
// ============================================================================

TEST_CASE("oauth2_middleware enabled/disabled state", "[web][auth][middleware]") {

    SECTION("disabled by default") {
        oauth2_config config;
        oauth2_middleware middleware(config);
        CHECK_FALSE(middleware.enabled());
    }

    SECTION("enabled when config.enabled is true") {
        auto config = test_config();
        oauth2_middleware middleware(config);
        CHECK(middleware.enabled());
    }
}

TEST_CASE("oauth2_middleware authenticate extracts Bearer token",
          "[web][auth][middleware]") {

    auto config = test_config();
    oauth2_middleware middleware(config);

    SECTION("missing Authorization header returns 401") {
        crow::request req;
        crow::response res;

        auto ctx = middleware.authenticate(req, res);
        CHECK_FALSE(ctx.has_value());
        CHECK(res.code == 401);
    }

    SECTION("non-Bearer authorization returns 401") {
        crow::request req;
        req.add_header("Authorization", "Basic dXNlcjpwYXNz");
        crow::response res;

        auto ctx = middleware.authenticate(req, res);
        CHECK_FALSE(ctx.has_value());
        CHECK(res.code == 401);
    }

    SECTION("empty Bearer token returns 401") {
        crow::request req;
        req.add_header("Authorization", "Bearer ");
        crow::response res;

        auto ctx = middleware.authenticate(req, res);
        CHECK_FALSE(ctx.has_value());
        CHECK(res.code == 401);
    }

    SECTION("malformed JWT returns 401") {
        crow::request req;
        req.add_header("Authorization", "Bearer not-a-jwt");
        crow::response res;

        auto ctx = middleware.authenticate(req, res);
        CHECK_FALSE(ctx.has_value());
        CHECK(res.code == 401);
    }

    SECTION("expired token returns 401") {
        auto token = make_test_token(valid_header(), valid_payload(-3600));
        crow::request req;
        req.add_header("Authorization", "Bearer " + token);
        crow::response res;

        auto ctx = middleware.authenticate(req, res);
        CHECK_FALSE(ctx.has_value());
        CHECK(res.code == 401);
    }

    SECTION("valid token without JWKS provider fails signature verification") {
        auto token = make_test_token(valid_header(), valid_payload());
        crow::request req;
        req.add_header("Authorization", "Bearer " + token);
        crow::response res;

        // No JWKS provider set, so signature verification fails
        auto ctx = middleware.authenticate(req, res);
        CHECK_FALSE(ctx.has_value());
        CHECK(res.code == 401);
    }
}

TEST_CASE("oauth2_middleware require_scope checks scopes",
          "[web][auth][middleware]") {

    auto config = test_config();
    oauth2_middleware middleware(config);

    SECTION("require_scope succeeds with matching scope") {
        jwt_claims claims;
        claims.scopes = {"dicomweb.read", "dicomweb.write"};
        crow::response res;

        CHECK(middleware.require_scope(claims, res, "dicomweb.read"));
        CHECK(middleware.require_scope(claims, res, "dicomweb.write"));
    }

    SECTION("require_scope fails with missing scope") {
        jwt_claims claims;
        claims.scopes = {"dicomweb.read"};
        crow::response res;

        CHECK_FALSE(middleware.require_scope(claims, res, "dicomweb.write"));
        CHECK(res.code == 403);
    }

    SECTION("require_any_scope succeeds with one matching scope") {
        jwt_claims claims;
        claims.scopes = {"dicomweb.read"};
        crow::response res;

        CHECK(middleware.require_any_scope(claims, res,
            {"dicomweb.read", "dicomweb.search"}));
    }

    SECTION("require_any_scope fails with no matching scope") {
        jwt_claims claims;
        claims.scopes = {"dicomweb.read"};
        crow::response res;

        CHECK_FALSE(middleware.require_any_scope(claims, res,
            {"dicomweb.write", "dicomweb.delete"}));
        CHECK(res.code == 403);
    }

    SECTION("require_any_scope fails with empty claims") {
        jwt_claims claims;
        crow::response res;

        CHECK_FALSE(middleware.require_any_scope(claims, res,
            {"dicomweb.read"}));
        CHECK(res.code == 403);
    }
}

// ============================================================================
// DICOMweb Scope Constants Tests
// ============================================================================

TEST_CASE("dicomweb_scopes constants are defined",
          "[web][auth][scopes]") {
    CHECK(dicomweb_scopes::read == "dicomweb.read");
    CHECK(dicomweb_scopes::search == "dicomweb.search");
    CHECK(dicomweb_scopes::write == "dicomweb.write");
    CHECK(dicomweb_scopes::delete_resource == "dicomweb.delete");
}
