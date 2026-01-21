/**
 * @file rest_server.cpp
 * @brief REST API server implementation
 *
 * @copyright Copyright (c) 2025
 * @license MIT
 */

// IMPORTANT: Include Crow FIRST before any PACS headers to avoid forward
// declaration conflicts
#include "crow.h"

#include "pacs/web/endpoints/annotation_endpoints.hpp"
#include "pacs/web/endpoints/association_endpoints.hpp"
#include "pacs/web/endpoints/audit_endpoints.hpp"
#include "pacs/web/endpoints/dicomweb_endpoints.hpp"
#include "pacs/web/endpoints/jobs_endpoints.hpp"
#include "pacs/web/endpoints/measurement_endpoints.hpp"
#include "pacs/web/endpoints/metadata_endpoints.hpp"
#include "pacs/web/endpoints/routing_endpoints.hpp"
#include "pacs/web/endpoints/patient_endpoints.hpp"
#include "pacs/web/endpoints/remote_nodes_endpoints.hpp"
#include "pacs/web/endpoints/security_endpoints.hpp"
#include "pacs/web/endpoints/series_endpoints.hpp"
#include "pacs/web/endpoints/study_endpoints.hpp"
#include "pacs/web/endpoints/system_endpoints.hpp"
#include "pacs/web/endpoints/thumbnail_endpoints.hpp"
#include "pacs/web/endpoints/worklist_endpoints.hpp"
#include "pacs/web/rest_config.hpp"
#include "pacs/web/rest_server.hpp"
#include "pacs/web/rest_types.hpp"

#ifdef PACS_WITH_MONITORING
#include "pacs/monitoring/health_checker.hpp"
#include "pacs/monitoring/health_json.hpp"
#include "pacs/monitoring/pacs_metrics.hpp"
#endif

#include <atomic>
#include <mutex>
#include <sstream>
#include <thread>

namespace pacs::web {

// Forward declare internal registration functions
namespace endpoints {
void register_system_endpoints_impl(crow::SimpleApp &app,
                                    std::shared_ptr<rest_server_context> ctx);
void register_patient_endpoints_impl(crow::SimpleApp &app,
                                     std::shared_ptr<rest_server_context> ctx);
void register_study_endpoints_impl(crow::SimpleApp &app,
                                   std::shared_ptr<rest_server_context> ctx);
void register_series_endpoints_impl(crow::SimpleApp &app,
                                    std::shared_ptr<rest_server_context> ctx);
void register_worklist_endpoints_impl(crow::SimpleApp &app,
                                      std::shared_ptr<rest_server_context> ctx);
void register_audit_endpoints_impl(crow::SimpleApp &app,
                                   std::shared_ptr<rest_server_context> ctx);
void register_association_endpoints_impl(crow::SimpleApp &app,
                                         std::shared_ptr<rest_server_context> ctx);
void register_dicomweb_endpoints_impl(crow::SimpleApp &app,
                                      std::shared_ptr<rest_server_context> ctx);
void register_remote_nodes_endpoints_impl(crow::SimpleApp &app,
                                          std::shared_ptr<rest_server_context> ctx);
void register_jobs_endpoints_impl(crow::SimpleApp &app,
                                  std::shared_ptr<rest_server_context> ctx);
void register_routing_endpoints_impl(crow::SimpleApp &app,
                                     std::shared_ptr<rest_server_context> ctx);
void register_thumbnail_endpoints_impl(crow::SimpleApp &app,
                                       std::shared_ptr<rest_server_context> ctx);
void register_metadata_endpoints_impl(crow::SimpleApp &app,
                                      std::shared_ptr<rest_server_context> ctx);
void register_annotation_endpoints_impl(crow::SimpleApp &app,
                                        std::shared_ptr<rest_server_context> ctx);
void register_measurement_endpoints_impl(crow::SimpleApp &app,
                                         std::shared_ptr<rest_server_context> ctx);
} // namespace endpoints

/**
 * @brief Implementation details for rest_server
 */
struct rest_server::impl {
  rest_server_config config;
  std::shared_ptr<rest_server_context> context;
  std::unique_ptr<crow::SimpleApp> app;
  std::thread server_thread;
  std::atomic<bool> running{false};
  std::atomic<std::uint16_t> actual_port{0};
  std::mutex mutex;

  impl() : context(std::make_shared<rest_server_context>()) {}

  explicit impl(const rest_server_config &cfg)
      : config(cfg), context(std::make_shared<rest_server_context>()) {
    context->config = &config;
  }
};

rest_server::rest_server() : impl_(std::make_unique<impl>()) {}

rest_server::rest_server(const rest_server_config &config)
    : impl_(std::make_unique<impl>(config)) {}

rest_server::~rest_server() { stop(); }

rest_server::rest_server(rest_server &&other) noexcept = default;
rest_server &rest_server::operator=(rest_server &&other) noexcept = default;

const rest_server_config &rest_server::config() const noexcept {
  return impl_->config;
}

void rest_server::set_config(const rest_server_config &config) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->config = config;
  impl_->context->config = &impl_->config;
}

void rest_server::set_health_checker(
    std::shared_ptr<monitoring::health_checker> checker) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->health_checker = std::move(checker);
}

void rest_server::set_metrics_provider(
    std::shared_ptr<monitoring::pacs_metrics> metrics) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->metrics = std::move(metrics);
}

void rest_server::set_access_control_manager(
    std::shared_ptr<security::access_control_manager> manager) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->security_manager = std::move(manager);
}

void rest_server::set_database(
    std::shared_ptr<storage::index_database> database) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->database = std::move(database);
}

void rest_server::set_node_manager(
    std::shared_ptr<client::remote_node_manager> manager) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->node_manager = std::move(manager);
}

void rest_server::set_job_manager(
    std::shared_ptr<client::job_manager> manager) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->job_manager = std::move(manager);
}

void rest_server::set_routing_manager(
    std::shared_ptr<client::routing_manager> manager) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->routing_manager = std::move(manager);
}

void rest_server::start() {
  if (impl_->running.exchange(true)) {
    return; // Already running
  }

  impl_->app = std::make_unique<crow::SimpleApp>();
  auto &app = *impl_->app;

  // Register system endpoints
  endpoints::register_system_endpoints_impl(app, impl_->context);

  // Add CORS preflight handler
  if (impl_->config.enable_cors) {
    CROW_ROUTE(app, "/api/<path>")
        .methods(crow::HTTPMethod::OPTIONS)(
            [this](const crow::request & /*req*/,
                   const std::string & /*path*/) {
              crow::response res(204);
              res.add_header("Access-Control-Allow-Origin",
                             impl_->config.cors_allowed_origins);
              res.add_header("Access-Control-Allow-Methods",
                             "GET, POST, PUT, DELETE, OPTIONS");
              res.add_header("Access-Control-Allow-Headers",
                             "Content-Type, Authorization");
              res.add_header("Access-Control-Max-Age", "86400");
              return res;
            });
  }

  // Start server
  app.bindaddr(impl_->config.bind_address)
      .port(impl_->config.port)
      .concurrency(static_cast<uint16_t>(impl_->config.concurrency))
      .run();

  impl_->running = false;
}

void rest_server::start_async() {
  if (impl_->running.exchange(true)) {
    return; // Already running
  }

  impl_->server_thread = std::thread([this]() {
    impl_->app = std::make_unique<crow::SimpleApp>();
    auto &app = *impl_->app;

    // Register endpoints
    endpoints::register_system_endpoints_impl(app, impl_->context);
    endpoints::register_security_endpoints_impl(app, impl_->context);
    endpoints::register_patient_endpoints_impl(app, impl_->context);
    endpoints::register_study_endpoints_impl(app, impl_->context);
    endpoints::register_series_endpoints_impl(app, impl_->context);
    endpoints::register_worklist_endpoints_impl(app, impl_->context);
    endpoints::register_audit_endpoints_impl(app, impl_->context);
    endpoints::register_association_endpoints_impl(app, impl_->context);
    endpoints::register_dicomweb_endpoints_impl(app, impl_->context);
    endpoints::register_remote_nodes_endpoints_impl(app, impl_->context);
    endpoints::register_jobs_endpoints_impl(app, impl_->context);
    endpoints::register_routing_endpoints_impl(app, impl_->context);
    endpoints::register_thumbnail_endpoints_impl(app, impl_->context);
    endpoints::register_metadata_endpoints_impl(app, impl_->context);
    endpoints::register_annotation_endpoints_impl(app, impl_->context);
    endpoints::register_measurement_endpoints_impl(app, impl_->context);

    // Add CORS preflight handler
    if (impl_->config.enable_cors) {
      CROW_ROUTE(app, "/api/<path>")
          .methods(crow::HTTPMethod::OPTIONS)(
              [this](const crow::request & /*req*/,
                     const std::string & /*path*/) {
                crow::response res(204);
                res.add_header("Access-Control-Allow-Origin",
                               impl_->config.cors_allowed_origins);
                res.add_header("Access-Control-Allow-Methods",
                               "GET, POST, PUT, DELETE, OPTIONS");
                res.add_header("Access-Control-Allow-Headers",
                               "Content-Type, Authorization");
                res.add_header("Access-Control-Max-Age", "86400");
                return res;
              });
    }

    // Start server
    app.bindaddr(impl_->config.bind_address)
        .port(impl_->config.port)
        .concurrency(static_cast<uint16_t>(impl_->config.concurrency))
        .run();

    impl_->running = false;
  });
}

void rest_server::stop() {
  if (!impl_->running) {
    return;
  }

  if (impl_->app) {
    impl_->app->stop();
  }

  if (impl_->server_thread.joinable()) {
    impl_->server_thread.join();
  }

  impl_->running = false;
}

bool rest_server::is_running() const noexcept { return impl_->running; }

void rest_server::wait() {
  if (impl_->server_thread.joinable()) {
    impl_->server_thread.join();
  }
}

std::uint16_t rest_server::port() const noexcept {
  return impl_->running ? impl_->config.port : 0;
}

} // namespace pacs::web
