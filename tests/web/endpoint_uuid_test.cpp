/**
 * @file endpoint_uuid_test.cpp
 * @brief Thread-safety tests for the resource ID generator of the REST
 *        endpoints
 *
 * The annotation, measurement, key image, and viewer state POST routes draw
 * a new resource ID from one generator while Crow runs handlers on
 * concurrent threads.
 *
 * @see Issue #1262 - unsynchronized function-static RNGs in UID/UUID
 *      generators
 */

// IMPORTANT: Include Crow FIRST before any PACS headers
#include "crow.h"

#include "../../src/web/endpoints/endpoint_uuid.h"

#include "kcenon/pacs/storage/index_database.h"
#include "kcenon/pacs/web/endpoints/system_endpoints.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace kcenon::pacs::web::endpoints {
// Internal registration functions, declared the same way in rest_server.cpp
void register_annotation_endpoints_impl(crow::SimpleApp &app,
                                        std::shared_ptr<rest_server_context> ctx);
void register_measurement_endpoints_impl(crow::SimpleApp &app,
                                         std::shared_ptr<rest_server_context> ctx);
void register_key_image_endpoints_impl(crow::SimpleApp &app,
                                       std::shared_ptr<rest_server_context> ctx);
void register_viewer_state_endpoints_impl(crow::SimpleApp &app,
                                          std::shared_ptr<rest_server_context> ctx);
} // namespace kcenon::pacs::web::endpoints

using namespace kcenon::pacs::web;

TEST_CASE("generate_uuid returns unique version 4 UUIDs across threads",
          "[web][endpoints][threading]") {
  constexpr std::size_t num_threads = 8;
  constexpr std::size_t uuids_per_thread = 100;

  std::vector<std::vector<std::string>> uuids(num_threads);
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (std::size_t t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      uuids[t].reserve(uuids_per_thread);
      for (std::size_t i = 0; i < uuids_per_thread; ++i) {
        uuids[t].push_back(endpoints::detail::generate_uuid());
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  const std::regex uuid_v4(
      "[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}");
  std::set<std::string> unique;
  for (const auto &thread_uuids : uuids) {
    REQUIRE(thread_uuids.size() == uuids_per_thread);
    for (const auto &uuid : thread_uuids) {
      CHECK(std::regex_match(uuid, uuid_v4));
      unique.insert(uuid);
    }
  }
  CHECK(unique.size() == num_threads * uuids_per_thread);
}

TEST_CASE("resource POST endpoints draw IDs from concurrent handler threads",
          "[web][endpoints][threading]") {
  auto db_result = kcenon::pacs::storage::index_database::open(":memory:");
  REQUIRE(db_result.is_ok());

  auto ctx = std::make_shared<rest_server_context>();
  ctx->database = std::move(db_result.value());

  crow::SimpleApp app;
  endpoints::register_annotation_endpoints_impl(app, ctx);
  endpoints::register_measurement_endpoints_impl(app, ctx);
  endpoints::register_key_image_endpoints_impl(app, ctx);
  endpoints::register_viewer_state_endpoints_impl(app, ctx);
  app.validate();

  // Each handler draws the new ID before it validates the body. Every body
  // below passes the earlier checks and then lacks a required field, so the
  // handler answers 400 without touching index_database (not thread-safe).
  struct post_request {
    std::string url;
    std::string body;
  };
  const std::vector<post_request> requests = {
      {"/api/v1/annotations", R"({"annotation_type":"arrow"})"},
      {"/api/v1/measurements", R"({"measurement_type":"length"})"},
      {"/api/v1/studies/1.2.3/key-images", R"({"reason":"teaching"})"},
      {"/api/v1/viewer-states", R"({"user_id":"reader"})"},
  };

  constexpr int num_threads = 8;
  constexpr int rounds_per_thread = 25;

  std::atomic<int> unexpected_responses{0};
  std::vector<std::thread> threads;
  threads.reserve(num_threads);

  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&]() {
      for (int i = 0; i < rounds_per_thread; ++i) {
        for (const auto &r : requests) {
          crow::request req;
          req.method = crow::HTTPMethod::POST;
          req.url = r.url;
          req.raw_url = r.url;
          req.body = r.body;

          crow::response res;
          app.handle_full(req, res);
          if (res.code != 400 ||
              res.body.find("MISSING_FIELD") == std::string::npos) {
            ++unexpected_responses;
          }
        }
      }
    });
  }

  for (auto &thread : threads) {
    thread.join();
  }

  CHECK(unexpected_responses == 0);
}
