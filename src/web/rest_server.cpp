// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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

#include "rest_app.h"

#include "kcenon/pacs/web/endpoints/annotation_endpoints.h"
#include "kcenon/pacs/web/endpoints/association_endpoints.h"
#include "kcenon/pacs/web/endpoints/audit_endpoints.h"
#include "kcenon/pacs/web/endpoints/dicomweb_endpoints.h"
#include "kcenon/pacs/web/endpoints/jobs_endpoints.h"
#include "kcenon/pacs/web/endpoints/key_image_endpoints.h"
#include "kcenon/pacs/web/endpoints/measurement_endpoints.h"
#include "kcenon/pacs/web/endpoints/metadata_endpoints.h"
#include "kcenon/pacs/web/endpoints/routing_endpoints.h"
#include "kcenon/pacs/web/endpoints/patient_endpoints.h"
#include "kcenon/pacs/web/endpoints/remote_nodes_endpoints.h"
#include "kcenon/pacs/web/endpoints/security_endpoints.h"
#include "kcenon/pacs/web/endpoints/series_endpoints.h"
#include "kcenon/pacs/web/endpoints/study_endpoints.h"
#include "kcenon/pacs/web/endpoints/system_endpoints.h"
#include "kcenon/pacs/web/endpoints/thumbnail_endpoints.h"
#include "kcenon/pacs/web/endpoints/viewer_state_endpoints.h"
#include "kcenon/pacs/web/endpoints/wado_uri_endpoints.h"
#include "kcenon/pacs/web/endpoints/worklist_endpoints.h"
#include "kcenon/pacs/web/auth/oauth2_middleware.h"
#include "kcenon/pacs/web/rest_config.h"
#include "kcenon/pacs/web/rest_server.h"
#include "kcenon/pacs/web/rest_types.h"

#ifdef PACS_WITH_MONITORING
#include "kcenon/pacs/monitoring/health_checker.h"
#include "kcenon/pacs/monitoring/health_json.h"
#include "kcenon/pacs/monitoring/pacs_metrics.h"
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>

namespace kcenon::pacs::web {

// Forward declare internal registration functions
namespace endpoints {
void register_system_endpoints_impl(rest_app &app,
                                    std::shared_ptr<rest_server_context> ctx);
void register_patient_endpoints_impl(rest_app &app,
                                     std::shared_ptr<rest_server_context> ctx);
void register_study_endpoints_impl(rest_app &app,
                                   std::shared_ptr<rest_server_context> ctx);
void register_series_endpoints_impl(rest_app &app,
                                    std::shared_ptr<rest_server_context> ctx);
void register_worklist_endpoints_impl(rest_app &app,
                                      std::shared_ptr<rest_server_context> ctx);
void register_audit_endpoints_impl(rest_app &app,
                                   std::shared_ptr<rest_server_context> ctx);
void register_association_endpoints_impl(rest_app &app,
                                         std::shared_ptr<rest_server_context> ctx);
void register_dicomweb_endpoints_impl(rest_app &app,
                                      std::shared_ptr<rest_server_context> ctx);
void register_remote_nodes_endpoints_impl(rest_app &app,
                                          std::shared_ptr<rest_server_context> ctx);
void register_jobs_endpoints_impl(rest_app &app,
                                  std::shared_ptr<rest_server_context> ctx);
void register_routing_endpoints_impl(rest_app &app,
                                     std::shared_ptr<rest_server_context> ctx);
void register_thumbnail_endpoints_impl(rest_app &app,
                                       std::shared_ptr<rest_server_context> ctx);
void register_metadata_endpoints_impl(rest_app &app,
                                      std::shared_ptr<rest_server_context> ctx);
void register_annotation_endpoints_impl(rest_app &app,
                                        std::shared_ptr<rest_server_context> ctx);
void register_measurement_endpoints_impl(rest_app &app,
                                         std::shared_ptr<rest_server_context> ctx);
void register_key_image_endpoints_impl(rest_app &app,
                                       std::shared_ptr<rest_server_context> ctx);
void register_viewer_state_endpoints_impl(rest_app &app,
                                          std::shared_ptr<rest_server_context> ctx);
void register_wado_uri_endpoints_impl(rest_app &app,
                                      std::shared_ptr<rest_server_context> ctx);
void register_storage_commitment_endpoints_impl(
    rest_app &app, std::shared_ptr<rest_server_context> ctx);
// Every build registers the security endpoints; the public header declares only the
// crow::SimpleApp overload.
void register_security_endpoints_impl(rest_app &app,
                                      std::shared_ptr<rest_server_context> ctx);
#ifdef PACS_WITH_DATABASE_SYSTEM
void register_metrics_endpoints_impl(rest_app &app,
                                     std::shared_ptr<rest_server_context> ctx);
#endif
} // namespace endpoints

/**
 * @brief Implementation details for rest_server
 */
struct rest_server::impl {
  rest_server_config config;
  std::shared_ptr<rest_server_context> context;
  std::unique_ptr<rest_app> app;
  std::thread server_thread;
  std::atomic<bool> running{false};
  std::atomic<std::uint16_t> actual_port{0};

  /// Set by stop(); the app's tick acts on it on Crow's I/O thread
  std::atomic<bool> stop_requested{false};
  std::mutex mutex;

  /// Serializes start(), start_async() and stop(); guards app and
  /// server_thread
  std::mutex lifecycle_mutex;

  /// Signals wait() when running turns false
  std::mutex run_mutex;
  std::condition_variable run_cv;

  impl() : context(std::make_shared<rest_server_context>()) {}

  explicit impl(const rest_server_config &cfg)
      : config(cfg), context(std::make_shared<rest_server_context>()) {
    context->config = &config;
  }

  /// Mark the server stopped once app->run() has returned
  void set_stopped() {
    {
      std::lock_guard<std::mutex> lock(run_mutex);
      running = false;
    }
    run_cv.notify_all();
  }

  /// Wait until run() has started the server, failed to start it, or
  /// returned. Crow's stop() only stops a server that run() has already
  /// created, so calling it earlier races with run() and leaves the server
  /// running.
  void await_startup(rest_app &server_app) {
    while (server_app.wait_for_server_start(std::chrono::milliseconds(50)) ==
               std::cv_status::timeout &&
           running) {
    }
  }

  /// Crow's stop() writes a plain flag its accept loop reads on the I/O thread. Its tick runs on that
  /// thread, as its signal handler did, so stop() is called from there.
  void stop_from_io_thread(rest_app &server_app) {
    stop_requested = false;
    server_app.tick(std::chrono::milliseconds(50), [this, &server_app] {
      if (stop_requested.exchange(false)) {
        server_app.stop();
      }
    });
  }

  /// Join the server thread and free the app of a run() that has returned
  void release_finished_run() {
    if (server_thread.joinable()) {
      server_thread.join();
    }
    app.reset();
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

void rest_server::set_file_storage(
    std::shared_ptr<storage::file_storage> storage) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->file_storage = std::move(storage);
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

void rest_server::set_dicom_server(
    std::shared_ptr<network::dicom_server> server) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->dicom_server = std::move(server);
}

void rest_server::set_dicom_server_v2(
    std::shared_ptr<network::v2::dicom_server_v2> server) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->dicom_server_v2 = std::move(server);
}

void rest_server::set_oauth2_middleware(
    std::shared_ptr<auth::oauth2_middleware> middleware) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->context->oauth2 = std::move(middleware);
}

void rest_server::start() {
  std::unique_lock<std::mutex> lifecycle(impl_->lifecycle_mutex);
  if (impl_->running) {
    return; // Already running
  }
  impl_->release_finished_run();

  impl_->app = std::make_unique<rest_app>();
  auto &app = *impl_->app;
  app.get_middleware<rest_gate>().server = impl_->context;
  impl_->stop_from_io_thread(app);

  // Register system endpoints
  endpoints::register_system_endpoints_impl(app, impl_->context);

  app.bindaddr(impl_->config.bind_address)
      .port(impl_->config.port)
      .concurrency(static_cast<uint16_t>(impl_->config.concurrency));
  impl_->running = true;

  // Start server; stop() takes the lifecycle lock, so release it first
  lifecycle.unlock();
  try {
    app.run();
  } catch (...) {
    impl_->set_stopped();
    throw;
  }

  impl_->set_stopped();
}

void rest_server::start_async() {
  std::lock_guard<std::mutex> lifecycle(impl_->lifecycle_mutex);
  if (impl_->running) {
    return; // Already running
  }
  impl_->release_finished_run();

  // Create the app on this thread, before the server thread exists, so
  // stop() never reads impl_->app while another thread assigns it
  impl_->app = std::make_unique<rest_app>();
  auto &app = *impl_->app;
  app.get_middleware<rest_gate>().server = impl_->context;
  // By default Crow takes SIGINT and SIGTERM for itself and stops only itself, so a process hosting
  // this server would never hear them. Here the host owns its signals and calls stop().
  app.signal_clear();
  impl_->stop_from_io_thread(app);

  // Register endpoints. These three check OAuth 2.0 tokens on every route.
  endpoints::register_dicomweb_endpoints_impl(app, impl_->context);
  endpoints::register_wado_uri_endpoints_impl(app, impl_->context);
  endpoints::register_storage_commitment_endpoints_impl(app, impl_->context);

  if (impl_->config.enable_rest_api) {
    endpoints::register_system_endpoints_impl(app, impl_->context);
    endpoints::register_security_endpoints_impl(app, impl_->context);
    endpoints::register_patient_endpoints_impl(app, impl_->context);
    endpoints::register_study_endpoints_impl(app, impl_->context);
    endpoints::register_series_endpoints_impl(app, impl_->context);
    endpoints::register_worklist_endpoints_impl(app, impl_->context);
    endpoints::register_audit_endpoints_impl(app, impl_->context);
    endpoints::register_association_endpoints_impl(app, impl_->context);
    endpoints::register_remote_nodes_endpoints_impl(app, impl_->context);
    endpoints::register_jobs_endpoints_impl(app, impl_->context);
    endpoints::register_routing_endpoints_impl(app, impl_->context);
    endpoints::register_thumbnail_endpoints_impl(app, impl_->context);
    endpoints::register_metadata_endpoints_impl(app, impl_->context);
    endpoints::register_annotation_endpoints_impl(app, impl_->context);
    endpoints::register_measurement_endpoints_impl(app, impl_->context);
    endpoints::register_key_image_endpoints_impl(app, impl_->context);
    endpoints::register_viewer_state_endpoints_impl(app, impl_->context);
#ifdef PACS_WITH_DATABASE_SYSTEM
    endpoints::register_metrics_endpoints_impl(app, impl_->context);
#endif
  }

  app.bindaddr(impl_->config.bind_address)
      .port(impl_->config.port)
      .concurrency(static_cast<uint16_t>(impl_->config.concurrency));
  impl_->running = true;

  // Start server
  try {
    impl_->server_thread = std::thread([this, &app]() {
      app.run();
      impl_->set_stopped();
    });
  } catch (...) {
    impl_->running = false;
    impl_->app.reset();
    throw;
  }

  // Return once the server accepts connections or has failed to start
  impl_->await_startup(app);
  if (!app.is_bound()) {
    // Startup failed (for example, the port is in use) and run() returns
    // without serving; release it so is_running() reports the failure
    impl_->release_finished_run();
  }
}

void rest_server::stop() {
  std::lock_guard<std::mutex> lifecycle(impl_->lifecycle_mutex);
  if (!impl_->app) {
    return; // Never started, or already stopped
  }

  impl_->await_startup(*impl_->app);
  impl_->stop_requested = true;
  {
    std::unique_lock<std::mutex> lock(impl_->run_mutex);
    if (!impl_->run_cv.wait_for(lock, std::chrono::seconds(10), [this] { return !impl_->running; })) {
      // The tick never ran: stop it from here, as before.
      lock.unlock();
      impl_->app->stop();
    }
  }

  if (impl_->server_thread.joinable()) {
    impl_->server_thread.join();
  }

  // A blocking start() may still be returning from run() on its own thread;
  // keep its app until it has
  if (!impl_->running) {
    impl_->app.reset();
  }
}

bool rest_server::is_running() const noexcept { return impl_->running; }

void rest_server::wait() {
  // Wait for run() to return instead of joining, so a concurrent stop() can
  // join the server thread
  std::unique_lock<std::mutex> lock(impl_->run_mutex);
  impl_->run_cv.wait(lock, [this] { return !impl_->running; });
}

std::uint16_t rest_server::port() const noexcept {
  return impl_->running ? impl_->config.port : 0;
}

} // namespace kcenon::pacs::web
