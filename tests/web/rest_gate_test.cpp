// BSD 3-Clause License
// Copyright (c) 2021-2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file rest_gate_test.cpp
 * @brief The gate in front of rest_server: tokens, scopes and CORS preflight on the real socket path
 *
 * The tokens were signed by the openssl command (validation/peers/http/http_peer.py) and the private
 * key thrown away. No database is given, so a request the gate lets through reaches its route and
 * gets 503 from it: that tells a pass from the gate's own 401 and 403.
 */

#include <catch2/catch_test_macros.hpp>

#ifdef PACS_WITH_DIGITAL_SIGNATURES

#include <asio.hpp>

#include "kcenon/pacs/web/auth/jwks_provider.h"
#include "kcenon/pacs/web/auth/oauth2_middleware.h"
#include "kcenon/pacs/web/rest_server.h"

#include <array>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

const std::string jwks_document = R"json({
  "keys": [
    {
      "kty": "RSA",
      "kid": "probe-rsa",
      "use": "sig",
      "alg": "RS256",
      "n": "lhxlkL5nqZhfaP5Zq1KGjPjX9hIdxSGDUljezvK4SW60Jxl_vz-S81DD6xykwc9NknAUMxMh-NGlFjVur4_vFYQ9fNt_Mk1Ol0jsgL-N1oj6CHVF4d50fz-4WMbqtp8zjUtzt9JU0TfC_pnM6iSOywEip5GF3hZIp6Cqwcqhc_PkgdMDs5Qsp6tfXCbO9yj9dYJ2vrmf5cfsLJ0jH4nPCljBkE1qjXUDl1h9UsdRDhAK3epScGey1qrNXp4uxFrlCmIY5-IoJ08hmI6ET82J497K-gfjYzoBvdyszehqig_AmP_NFwn1zORh3f997ftUJ1yzjf0b84r38JAzlqApmQ",
      "e": "AQAB"
    }
  ]
})json";

const std::string pacs_read_token =
    "eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCIsICJraWQiOiAicHJvYmUtcnNhIn0.eyJpc3MiOiAiaHR0cHM6Ly9pc3N1ZXIu"
    "ZXhhbXBsZS50ZXN0IiwgImF1ZCI6ICJwYWNzIiwgInN1YiI6ICJ2ZWN0b3ItcGFjcy5yZWFkIiwgImV4cCI6IDQxMDI0NDQ4MDAs"
    "ICJpYXQiOiAxNzY3MjI1NjAwLCAic2NvcGUiOiAicGFjcy5yZWFkIn0.PCyRVOgQgaCKQokNqrUJy_LJgZFrPLs9PrrNXz_i1wrb"
    "pMBqgrpokuE11_wg2msPg59dcgALE2RrPdcC5HOFiEap3FArc6dodqAqcIbOOPXp65aBHEAUOhKuYMcwkP9bY2wbhlvefPNHQBKn"
    "7bmZkK_2NESQeS20QhrEnXjenxYW1h3YJKw4wk72EEAedCe3YRQ3CsKRKEtbXv_UDuw3ZYD7sc0266c-52cHfPGY5iD7uLH7CVtG"
    "c8jq4_lzaFq-vDVElZCiIMa4p2Eg8tZeaA3MIfrBctOhDTgeH3okyN0mWU33aA67qU3IGuRsrp3sFmO9Z-BCRXvJSNYn-HS-Jw";
const std::string pacs_write_token =
    "eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCIsICJraWQiOiAicHJvYmUtcnNhIn0.eyJpc3MiOiAiaHR0cHM6Ly9pc3N1ZXIu"
    "ZXhhbXBsZS50ZXN0IiwgImF1ZCI6ICJwYWNzIiwgInN1YiI6ICJ2ZWN0b3ItcGFjcy53cml0ZSIsICJleHAiOiA0MTAyNDQ0ODAw"
    "LCAiaWF0IjogMTc2NzIyNTYwMCwgInNjb3BlIjogInBhY3Mud3JpdGUifQ.S-yMB6ts750f7S4N6BIdhwud_5YKb50aiGQ1kBnlW"
    "XX37Vzlpeca3UxipXhR6wqpTqpp6fKmM_BUmLL9UFD5ANdDTglhSr42pwMPDnTLSLb6pZ5iSdoRn5DkuzjX7cY6jRu-x-KpG3PW3"
    "T-UCZKzcZrQMfYbwgQ5AR3-MAwqSXkK1XDR1P3M3A9okQrKqOqtYAgeT2ZWxS4OgLPeMRdMmKDMb-6RCQhbZENfvPxZ2qFCxMZZf"
    "IgbO0elms_tE4_v2BjO3U-Zvty4vs5kweYzi9bF1mZPj0Fd6Ary8Hb1EIh1uSzSVLGCGyIgcAl33AeUSzHOSG7_tRT5SE36QYBYA"
    "Q";
const std::string pacs_admin_token =
    "eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCIsICJraWQiOiAicHJvYmUtcnNhIn0.eyJpc3MiOiAiaHR0cHM6Ly9pc3N1ZXIu"
    "ZXhhbXBsZS50ZXN0IiwgImF1ZCI6ICJwYWNzIiwgInN1YiI6ICJ2ZWN0b3ItcGFjcy5hZG1pbiIsICJleHAiOiA0MTAyNDQ0ODAw"
    "LCAiaWF0IjogMTc2NzIyNTYwMCwgInNjb3BlIjogInBhY3MuYWRtaW4ifQ.KS2cTC6_E2tHFeKfUd2_VNnOYwM8EIXYpjW08MNeY"
    "WInyr6e6oK9GOmx-NTn4HiTeDBXJdPMfK_VwHEVi_JoLV5HDaNocO9rzvY5IbquA9IMMeJoUHvDHQonsFCI9HmwgwgjDtTTJseBj"
    "yu_Z-FUdu7AKyIdONFPpz1C6Wx73njyRH3T1suTbuhYG1FNDn1h76m2nUe9QqWKLbZIxYH9593tHeWGm_mO5nFV2_xwB7rBV6hav"
    "aqShpR5D8loRqKgnVL0rewuzu1fG-w2lQxNv5CIMGRPCy3PGvE54lTfErdOurr4zNIb_iByPZmZ5EQPTGNnx8phDebv8mb6TeI5J"
    "Q";
const std::string dicomweb_read_token =
    "eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCIsICJraWQiOiAicHJvYmUtcnNhIn0.eyJpc3MiOiAiaHR0cHM6Ly9pc3N1ZXIu"
    "ZXhhbXBsZS50ZXN0IiwgImF1ZCI6ICJwYWNzIiwgInN1YiI6ICJ2ZWN0b3ItZGljb213ZWIucmVhZCIsICJleHAiOiA0MTAyNDQ0"
    "ODAwLCAiaWF0IjogMTc2NzIyNTYwMCwgInNjb3BlIjogImRpY29td2ViLnJlYWQifQ.LRB3vozO_iUH1W4aObhrNX3BHBsfYVcSE"
    "AWQcquC2JQQdBirnZW3bFKaDSGCx0lsodbEf8fyKPeoFVTHT1KKwv36rAM8x13fOzEICKa-6eBxrtoPF6j5bcNFljqG1K3YKGMXZ"
    "OGma8DpKk4GwvSMjORmM2cbcJfThTnTzxhM8Xr-7l8rZoVVcurCMQup1m9BfzS5SipgonyYJHECYn2aqOyEqJqIu1LPEeU8rWMYo"
    "gPxojeO0r80tT4IR9licX_sZT2ra8mTCwzkT-LoMjs0giI5lXBIam2joooUwY2PRc-8ygYAOToE7I-iDMNoS6wA6WZPmUlax8cmC"
    "eYHDH1BIg";

std::string request(uint16_t port, const std::string& method, const std::string& path,
                    const std::string& token = {}, const std::string& extra = {}) {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    socket.connect({asio::ip::make_address("127.0.0.1"), port});
    std::string message = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
    if (!token.empty()) {
        message += "Authorization: Bearer " + token + "\r\n";
    }
    message += extra;
    message += method == "POST" ? "Content-Type: application/json\r\nContent-Length: 2\r\n\r\n{}" : "\r\n";
    asio::write(socket, asio::buffer(message));
    socket.non_blocking(true);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::string response;
    std::array<char, 4096> buffer{};
    while (std::chrono::steady_clock::now() < deadline) {
        asio::error_code error;
        const auto count = socket.read_some(asio::buffer(buffer), error);
        response.append(buffer.data(), count);
        if (error == asio::error::eof) {
            return response;
        }
        if (error && error != asio::error::would_block && error != asio::error::try_again) {
            throw std::runtime_error(error.message());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("HTTP response timed out");
}

std::string status_of(const std::string& response) {
    return response.substr(0, response.find("\r\n"));
}

struct gated_server {
    kcenon::pacs::web::rest_server_config config;
    std::unique_ptr<kcenon::pacs::web::rest_server> server;

    explicit gated_server(const std::string& origin) {
        asio::io_context io;
        asio::ip::tcp::acceptor reservation(io, {asio::ip::make_address("127.0.0.1"), 0});
        config.bind_address = "127.0.0.1";
        config.port = reservation.local_endpoint().port();
        config.concurrency = 2;
        config.cors_allowed_origins = origin;
        reservation.close();
        server = std::make_unique<kcenon::pacs::web::rest_server>(config);

        kcenon::pacs::web::auth::oauth2_config oauth;
        oauth.enabled = true;
        oauth.issuer = "https://issuer.example.test";
        oauth.audience = "pacs";
        oauth.allow_unknown_users = true;
        auto keys = std::make_shared<kcenon::pacs::web::auth::jwks_provider>();
        REQUIRE(keys->load_from_json(jwks_document));
        auto middleware = std::make_shared<kcenon::pacs::web::auth::oauth2_middleware>(oauth);
        middleware->set_jwks_provider(keys);
        server->set_oauth2_middleware(middleware);
        server->start_async();
        REQUIRE(server->is_running());
    }

    ~gated_server() { server->stop(); }
};

}  // namespace

TEST_CASE("rest_gate lets a request reach a REST route only with a token whose scope allows it",
          "[web][http][gate]") {
    gated_server gated("https://viewer.example.test");
    const auto port = gated.config.port;

    const auto anonymous = request(port, "GET", "/api/v1/patients");
    CHECK(status_of(anonymous) == "HTTP/1.1 401 Unauthorized");
    CHECK(anonymous.find("WWW-Authenticate: Bearer") != std::string::npos);
    // A refusal still names the allowed origin, or a browser could not read why it was refused.
    CHECK(anonymous.find("Access-Control-Allow-Origin: https://viewer.example.test") != std::string::npos);

    // 503 means the route ran: no database was given.
    struct row { const char* method; const char* path; const std::string* token; const char* status; };
    const row rows[] = {
        {"GET", "/api/v1/patients", &dicomweb_read_token, "HTTP/1.1 403 Forbidden"},
        {"GET", "/api/v1/patients", &pacs_read_token, "HTTP/1.1 503 Service Unavailable"},
        {"POST", "/api/v1/worklist", &pacs_read_token, "HTTP/1.1 403 Forbidden"},
        {"POST", "/api/v1/worklist", &pacs_write_token, "HTTP/1.1 503 Service Unavailable"},
        {"GET", "/api/v1/audit/logs", &pacs_write_token, "HTTP/1.1 403 Forbidden"},
        {"GET", "/api/v1/audit/logs", &pacs_admin_token, "HTTP/1.1 503 Service Unavailable"},
        {"DELETE", "/api/v1/studies/1.2.3", &pacs_write_token, "HTTP/1.1 403 Forbidden"},
        {"GET", "/api/v1/system/config", &pacs_read_token, "HTTP/1.1 403 Forbidden"},
        {"GET", "/api/v1/system/config", &pacs_admin_token, "HTTP/1.1 200 OK"},
        // DICOMweb checks its own scopes; the REST scopes neither open nor close it.
        {"GET", "/dicomweb/studies", &pacs_admin_token, "HTTP/1.1 403 Forbidden"},
        {"GET", "/dicomweb/studies", &dicomweb_read_token, "HTTP/1.1 503 Service Unavailable"},
    };
    for (const auto& item : rows) {
        INFO(item.method << " " << item.path);
        CHECK(status_of(request(port, item.method, item.path, *item.token)) == item.status);
    }
}

TEST_CASE("rest_gate answers CORS preflight with the configured origin", "[web][http][gate]") {
    gated_server gated("https://viewer.example.test");
    const auto preflight = request(gated.config.port, "OPTIONS", "/api/v1/patients", {},
                                   "Origin: https://viewer.example.test\r\nAccess-Control-Request-Method: GET\r\n");
    INFO(preflight);
    CHECK(status_of(preflight) == "HTTP/1.1 204 No Content");
    CHECK(preflight.find("Access-Control-Allow-Origin: https://viewer.example.test") != std::string::npos);
    CHECK(preflight.find("Access-Control-Allow-Headers: Accept, Authorization, Content-Type") != std::string::npos);
    CHECK(preflight.find("Access-Control-Allow-Methods: ") != std::string::npos);
}

TEST_CASE("rest_gate names no origin when none is configured", "[web][http][gate]") {
    gated_server gated("");
    const auto preflight = request(gated.config.port, "OPTIONS", "/api/v1/patients");
    CHECK(status_of(preflight) == "HTTP/1.1 204 No Content");
    CHECK(preflight.find("Access-Control-Allow-") == std::string::npos);
    const auto refused = request(gated.config.port, "GET", "/api/v1/patients");
    CHECK(refused.find("Access-Control-Allow-Origin") == std::string::npos);
}

#endif  // PACS_WITH_DIGITAL_SIGNATURES
