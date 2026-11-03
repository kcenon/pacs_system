// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file jwks_provider_test.cpp
 * @brief Keys published in a JWKS verify tokens signed by someone else
 *
 * The vectors were made by the openssl command (validation/peers/http/http_peer.py) and the private keys
 * thrown away. No PACS code signed them, which is what makes them a test: a key read in the wrong byte
 * order still loads, and only a real signature shows that it does not verify.
 */

#include <catch2/catch_test_macros.hpp>

#ifdef PACS_WITH_DIGITAL_SIGNATURES

#include "kcenon/pacs/web/auth/jwks_provider.h"
#include "kcenon/pacs/web/auth/jwt_validator.h"

#include <string>

using namespace kcenon::pacs::web::auth;

namespace {

const std::string jwks_document = R"json({
  "keys": [
    {
      "kty": "RSA",
      "kid": "probe-rsa",
      "use": "sig",
      "alg": "RS256",
      "n": "mEwrY_n1QERSskTv0FreQpvJXBjGZPjilUazNhhRS7-dcP9PSOOynTrWcufHdAtIhe6_x7uHQfCBBWRzBoZ4xaCYFEDAy0FtWJClCl-_XWYXKfmsY4-rou4mvByNhX6Q3GzPwHzNoEcA3A5hjVG95-ZkJKJFfgmuXAh7-__rblHRPYhpi5t2mDiM0MOv0Zq934JG0TL-vSXR_uatQ16t2VsXW9qJryfCNuR3HkqV8enx49VvuKuaauaeOV-5PscjfDiZOxNP1gIUTiVgYXPobRwN_1LG8deVqLciUZgu64xmouD74MOZe8mCtmbtH7p_sGT8dZXqqtZ3rwNQpLqEkQ",
      "e": "AQAB"
    },
    {
      "kty": "EC",
      "crv": "P-256",
      "kid": "probe-ec",
      "use": "sig",
      "alg": "ES256",
      "x": "j_RIWxvz4Uv82v2x8lFuptKQAA_Q8uXq5ZrhVCgJXCo",
      "y": "7s9nEQEQiZfHodjcGaG8epiFLXirj3DbupCzy399j0A"
    }
  ]
})json";

const std::string rs256_token =
    "eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCIsICJraWQiOiAicHJvYmUtcnNhIn0.eyJpc3MiOiAiaHR0cHM6Ly9pc3N1ZXIu"
    "ZXhhbXBsZS50ZXN0IiwgImF1ZCI6ICJwYWNzIiwgInN1YiI6ICJ2ZWN0b3IiLCAiZXhwIjogNDEwMjQ0NDgwMCwgImlhdCI6IDE3"
    "NjcyMjU2MDAsICJzY29wZSI6ICJkaWNvbXdlYi5yZWFkIn0.Pf_aXB6wzTuFKfrz_7OEaH6ZZlNT11pDYLZaGgJ6bmUR5J3PltWW"
    "FqqXDd-qmTmpQaGVNdL9fTK8kRafS88pyv6fmOiULBHcBsSZU8qFYIoPYhTyi7MZ9jqhSVzTucZGoTe73oVy0QNIkDkAchf3Btiz"
    "aNplw8Xpw2zg2QkBcTTpdCUtw36zVHJcyqSUtsAT31TmShlwNp1INrM-P8uiqd5F-wAiRvpUV5yNiInHqzX0sh4Z3b5VMdxzbFXF"
    "xrlPlqPJIB4nVBjFISQeHjCoErpXjCDtwbM17LSonQFqg6gyWlF-BEXRi83ajoT_EvOxC9R0Rq1mOb6-4H_qZ5KjTg";

const std::string es256_token =
    "eyJhbGciOiAiRVMyNTYiLCAidHlwIjogIkpXVCIsICJraWQiOiAicHJvYmUtZWMifQ.eyJpc3MiOiAiaHR0cHM6Ly9pc3N1ZXIuZ"
    "XhhbXBsZS50ZXN0IiwgImF1ZCI6ICJwYWNzIiwgInN1YiI6ICJ2ZWN0b3IiLCAiZXhwIjogNDEwMjQ0NDgwMCwgImlhdCI6IDE3N"
    "jcyMjU2MDAsICJzY29wZSI6ICJkaWNvbXdlYi5yZWFkIn0.TkQkvh4sj8r8od5hecSiQbLdTALlovt8LIgtQ6M4B-QmxS5gcEJE-"
    "cg8RiUr_ZdAaHhR6KBn8t84rrt-LuULzQ";

/// Signed by another RSA key, under the kid of the published one.
const std::string rs256_token_from_another_key =
    "eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCIsICJraWQiOiAicHJvYmUtcnNhIn0.eyJpc3MiOiAiaHR0cHM6Ly9pc3N1ZXIu"
    "ZXhhbXBsZS50ZXN0IiwgImF1ZCI6ICJwYWNzIiwgInN1YiI6ICJ2ZWN0b3IiLCAiZXhwIjogNDEwMjQ0NDgwMCwgImlhdCI6IDE3"
    "NjcyMjU2MDAsICJzY29wZSI6ICJkaWNvbXdlYi5yZWFkIn0.XTrYUIvmL5qc-gUz3Vz_NIRF7kcODokMUf1koUrDmZQqXDq3_xw1"
    "GK21NaPPwasI_qaYu_ErT1lVE5P5eu6-zMT73PNVUxCyuE7y9Mm_1Xu02VZj6OZ6t096illbLh7kKnyZ4ass3Kztx32JB_SMthCa"
    "0M5XIXeReuF9Fxw3xZgTNwMSeWGbu_Itp6h4JYJs1VgPbe_qT1RwTeM2UPGh_61U8YUu1T-Bp0y063JlyiVBSZDnh1SyncJfEJ23"
    "6jpMWFgkMWepP_TwY1Aweu1BEkzKb7gqCatFROgI0g6kQwN2h-iUIlxMch52FyeFURUm50lQoiZSyuK2qjmmkzkHjw";

[[nodiscard]] jwt_validator make_validator() {
    oauth2_config config;
    config.enabled = true;
    config.allowed_algorithms = {"RS256", "ES256"};
    return jwt_validator(config);
}

}  // namespace

TEST_CASE("jwks_provider publishes keys that verify real RS256 signatures", "[web][auth][jwks]") {
    jwks_provider provider;
    REQUIRE(provider.load_from_json(jwks_document));
    const auto key = provider.get_key("probe-rsa");
    REQUIRE(key);
    const auto validator = make_validator();

    const auto token = validator.decode(rs256_token);
    REQUIRE(token.second == jwt_error::none);
    CHECK(validator.verify_rs256(token.first, key->pem));

    const auto other = validator.decode(rs256_token_from_another_key);
    REQUIRE(other.second == jwt_error::none);
    CHECK_FALSE(validator.verify_rs256(other.first, key->pem));
}

TEST_CASE("jwks_provider publishes keys that verify real ES256 signatures", "[web][auth][jwks]") {
    jwks_provider provider;
    REQUIRE(provider.load_from_json(jwks_document));
    const auto key = provider.get_key("probe-ec");
    REQUIRE(key);
    const auto validator = make_validator();

    const auto token = validator.decode(es256_token);
    REQUIRE(token.second == jwt_error::none);
    CHECK(validator.verify_es256(token.first, key->pem));

    // A signature for another message: the header and payload of the RS256 token with this signature.
    auto tampered = es256_token;
    tampered[tampered.rfind('.') + 1] = tampered[tampered.rfind('.') + 1] == 'A' ? 'B' : 'A';
    const auto changed = validator.decode(tampered);
    REQUIRE(changed.second == jwt_error::none);
    CHECK_FALSE(validator.verify_es256(changed.first, key->pem));
}

#endif  // PACS_WITH_DIGITAL_SIGNATURES
