/**
 * @file thumbnail_service_test.cpp
 * @brief Unit tests for thumbnail service
 *
 * @see Issue #543 - Implement Thumbnail API for DICOM Viewer
 * @copyright Copyright (c) 2025
 * @license MIT
 */

#include <catch2/catch_test_macros.hpp>

#include "pacs/web/thumbnail_service.hpp"

using namespace pacs::web;

TEST_CASE("thumbnail_params default values", "[web][thumbnail]") {
    thumbnail_params params;

    REQUIRE(params.size == 128);
    REQUIRE(params.format == "jpeg");
    REQUIRE(params.quality == 60);
    REQUIRE(params.frame == 1);
}

TEST_CASE("thumbnail_params custom values", "[web][thumbnail]") {
    thumbnail_params params;
    params.size = 256;
    params.format = "png";
    params.quality = 90;
    params.frame = 5;

    REQUIRE(params.size == 256);
    REQUIRE(params.format == "png");
    REQUIRE(params.quality == 90);
    REQUIRE(params.frame == 5);
}

TEST_CASE("thumbnail_cache_entry structure", "[web][thumbnail]") {
    thumbnail_cache_entry entry;
    entry.data = {0x01, 0x02, 0x03};
    entry.content_type = "image/jpeg";
    entry.created_at = std::chrono::system_clock::now();
    entry.last_accessed = entry.created_at;

    REQUIRE(entry.data.size() == 3);
    REQUIRE(entry.content_type == "image/jpeg");
    REQUIRE(entry.created_at == entry.last_accessed);
}

TEST_CASE("thumbnail_result success", "[web][thumbnail]") {
    thumbnail_cache_entry entry;
    entry.data = {0xFF, 0xD8};  // JPEG magic bytes
    entry.content_type = "image/jpeg";

    auto result = thumbnail_result::ok(entry);

    REQUIRE(result.success == true);
    REQUIRE(result.error_message.empty());
    REQUIRE(result.entry.data.size() == 2);
    REQUIRE(result.entry.content_type == "image/jpeg");
}

TEST_CASE("thumbnail_result error", "[web][thumbnail]") {
    auto result = thumbnail_result::error("Instance not found");

    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Instance not found");
    REQUIRE(result.entry.data.empty());
}

TEST_CASE("thumbnail_service construction", "[web][thumbnail]") {
    // Test with nullptr database
    auto service = thumbnail_service(nullptr);

    REQUIRE(service.cache_size() == 0);
    REQUIRE(service.cache_entry_count() == 0);
    REQUIRE(service.max_cache_size() == 64 * 1024 * 1024);
}

TEST_CASE("thumbnail_service cache management", "[web][thumbnail]") {
    auto service = thumbnail_service(nullptr);

    SECTION("initial state") {
        REQUIRE(service.cache_size() == 0);
        REQUIRE(service.cache_entry_count() == 0);
    }

    SECTION("set max cache size") {
        service.set_max_cache_size(32 * 1024 * 1024);
        REQUIRE(service.max_cache_size() == 32 * 1024 * 1024);
    }

    SECTION("clear cache") {
        service.clear_cache();
        REQUIRE(service.cache_size() == 0);
        REQUIRE(service.cache_entry_count() == 0);
    }
}

TEST_CASE("thumbnail_service parameter validation", "[web][thumbnail]") {
    // Note: Using nullptr database, so after parameter validation passes,
    // the error will be "Database not configured"
    auto service = thumbnail_service(nullptr);

    SECTION("invalid size") {
        thumbnail_params params;
        params.size = 100;  // Invalid: not 64, 128, 256, or 512

        auto result = service.get_thumbnail("1.2.3.4.5", params);

        REQUIRE(result.success == false);
        REQUIRE(result.error_message.find("Invalid size") != std::string::npos);
    }

    SECTION("valid sizes proceed to database check") {
        for (uint16_t size : {64, 128, 256, 512}) {
            thumbnail_params params;
            params.size = size;

            auto result = service.get_thumbnail("1.2.3.4.5", params);

            // Should fail with "Database not configured" not "Invalid size"
            REQUIRE(result.success == false);
            REQUIRE(result.error_message.find("Invalid size") == std::string::npos);
            REQUIRE(result.error_message == "Database not configured");
        }
    }

    SECTION("invalid format") {
        thumbnail_params params;
        params.format = "bmp";  // Invalid: not jpeg or png

        auto result = service.get_thumbnail("1.2.3.4.5", params);

        REQUIRE(result.success == false);
        REQUIRE(result.error_message.find("Invalid format") != std::string::npos);
    }

    SECTION("valid formats proceed to database check") {
        for (const auto& format : {"jpeg", "png"}) {
            thumbnail_params params;
            params.format = format;

            auto result = service.get_thumbnail("1.2.3.4.5", params);

            // Should fail with "Database not configured" not "Invalid format"
            REQUIRE(result.success == false);
            REQUIRE(result.error_message.find("Invalid format") == std::string::npos);
            REQUIRE(result.error_message == "Database not configured");
        }
    }

    SECTION("invalid quality") {
        thumbnail_params params;
        params.quality = 0;  // Invalid: must be 1-100

        auto result = service.get_thumbnail("1.2.3.4.5", params);

        REQUIRE(result.success == false);
        REQUIRE(result.error_message.find("Invalid quality") != std::string::npos);

        params.quality = 101;  // Invalid: must be 1-100
        result = service.get_thumbnail("1.2.3.4.5", params);

        REQUIRE(result.success == false);
        REQUIRE(result.error_message.find("Invalid quality") != std::string::npos);
    }
}

TEST_CASE("thumbnail_service with nullptr database", "[web][thumbnail]") {
    auto service = thumbnail_service(nullptr);

    thumbnail_params params;
    auto result = service.get_thumbnail("1.2.3.4.5", params);

    // Should fail because database is nullptr
    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Database not configured");
}
