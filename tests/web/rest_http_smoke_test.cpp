// BSD 3-Clause License
// Copyright (c) 2026, PACS System Contributors
// See the LICENSE file in the project root for full license information.

#include <catch2/catch_test_macros.hpp>
#include <asio.hpp>

#include "kcenon/pacs/web/rest_server.h"

#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

// Exercise Crow's real socket path with the ASIO version used by the build.
// A deadline keeps a broken Connection: close response from hanging CTest.
std::string request(uint16_t port, const std::string& method,
                    const std::string& path) {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);
    socket.connect({asio::ip::make_address("127.0.0.1"), port});
    const auto message = method + " " + path
        + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
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
        if (response.size() > 64 * 1024) {
            throw std::runtime_error("HTTP smoke response exceeds 64 KiB");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("HTTP smoke response timed out");
}

} // namespace

TEST_CASE("rest_server can serve DICOMweb without the REST API", "[web][http][smoke]") {
    asio::io_context io;
    asio::ip::tcp::acceptor reservation(io, {asio::ip::make_address("127.0.0.1"), 0});
    kcenon::pacs::web::rest_server_config config;
    config.bind_address = "127.0.0.1";
    config.port = reservation.local_endpoint().port();
    config.concurrency = 2;
    config.enable_rest_api = false;
    reservation.close();

    kcenon::pacs::web::rest_server server(config);
    server.start_async();
    REQUIRE(server.is_running());
    const auto rest = request(config.port, "GET", "/api/v1/patients");
    CHECK(rest.find("HTTP/1.1 404") == 0);
    const auto system = request(config.port, "GET", "/api/v1/system/status");
    CHECK(system.find("HTTP/1.1 404") == 0);
    // No database was given, so the route exists and says so.
    const auto dicomweb = request(config.port, "GET", "/dicomweb/studies");
    CHECK(dicomweb.find("HTTP/1.1 503") == 0);
    server.stop();
}

TEST_CASE("rest_server serves HTTP over the pinned Crow and ASIO", "[web][http][smoke]") {
    asio::io_context io;
    asio::ip::tcp::acceptor reservation(io, {asio::ip::make_address("127.0.0.1"), 0});
    kcenon::pacs::web::rest_server_config config;
    config.bind_address = "127.0.0.1";
    config.port = reservation.local_endpoint().port();
    config.concurrency = 2;
    reservation.close();

    kcenon::pacs::web::rest_server server(config);
    server.start_async();
    REQUIRE(server.is_running());

    const auto response = request(config.port, "GET", "/api/v1/system/config");
    INFO(response);
    CHECK(response.starts_with("HTTP/1.1 200 "));
    CHECK(response.find("application/json") != std::string::npos);
    CHECK(response.find("Access-Control-Allow-Origin: *") != std::string::npos);
    REQUIRE(response.find("\r\n\r\n") != std::string::npos);
    const auto body = response.substr(response.find("\r\n\r\n") + 4);
    CHECK(body.find("\"bind_address\":\"127.0.0.1\"") != std::string::npos);
    CHECK(body.find("\"port\":" + std::to_string(config.port)) != std::string::npos);

    CHECK(request(config.port, "GET", "/not-a-pacs-route").starts_with("HTTP/1.1 404 "));
    const auto preflight = request(config.port, "OPTIONS", "/api/v1/system/config");
    INFO(preflight);
    CHECK(preflight.starts_with("HTTP/1.1 204 "));
    // Crow's automatic OPTIONS response bypasses the application's wildcard
    // CORS handler in both 1.3.1 and 1.3.2. Preflight CORS coverage belongs to
    // the DICOMweb HTTP acceptance work (#1321); this checks method transport.

    server.stop();
    CHECK_FALSE(server.is_running());
}
