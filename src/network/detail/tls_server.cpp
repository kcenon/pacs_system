// BSD 3-Clause License
// See the LICENSE file in the project root for full license information.
#include "kcenon/pacs/network/detail/tls_server.h"

#if defined(PACS_WITH_NETWORK_SYSTEM) && defined(PACS_WITH_TLS)
#include <kcenon/network/interfaces/i_protocol_server.h>
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <array>
#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace kcenon::pacs::network::detail {
namespace {
using protocol_server = kcenon::network::interfaces::i_protocol_server;
using session_interface = kcenon::network::interfaces::i_session;
using net_result = kcenon::network::VoidResult;
using tcp = asio::ip::tcp;

struct callbacks {
    std::mutex mutex;
    protocol_server::connection_callback_t connected;
    protocol_server::disconnection_callback_t disconnected;
    protocol_server::receive_callback_t received;
    protocol_server::error_callback_t error;

    template<class Callback> auto copy(Callback callbacks::* member) {
        std::lock_guard lock(mutex);
        return this->*member;
    }
};

class tls_session final : public session_interface,
                          public std::enable_shared_from_this<tls_session> {
public:
    tls_session(std::shared_ptr<asio::io_context> io,
                std::shared_ptr<asio::ssl::context> context,
                tcp::socket socket, std::string id,
                std::shared_ptr<callbacks> events,
                std::function<void(const std::string&)> remove)
        : io_(std::move(io)), context_(std::move(context)),
          stream_(std::move(socket), *context_), timer_(*io_), id_(std::move(id)),
          events_(std::move(events)), remove_(std::move(remove)) {}

    std::string_view id() const override { return id_; }
    bool is_connected() const override { return connected_ && !closing_; }

    void start(std::chrono::seconds timeout) {
        timer_.expires_after(timeout);
        timer_.async_wait([self = shared_from_this()](std::error_code ec) {
            if (!ec) self->fail(asio::error::timed_out);
        });
        stream_.async_handshake(asio::ssl::stream_base::server,
            [self = shared_from_this()](std::error_code ec) {
                self->timer_.cancel();
                if (self->closed_) return;
                if (ec) { self->fail(ec); return; }
                self->connected_ = true;
                if (auto cb = self->events_->copy(&callbacks::connected)) cb(self);
                if (!self->closing_) self->read();
            });
    }

    net_result send(std::vector<uint8_t>&& data) override {
        if (!is_connected()) return error_info{"TLS session is closed"};
        // Bound queued outbound bytes even when the peer stops reading.
        constexpr size_t limit = 64 * 1024 * 1024;
        const auto size = data.size();
        auto pending = queued_bytes_.load();
        do {
            if (size > limit || pending > limit - size)
                return error_info{"TLS send queue is full"};
        } while (!queued_bytes_.compare_exchange_weak(pending, pending + size));
        asio::post(*io_, [self = shared_from_this(), data = std::move(data)]() mutable {
            if (self->closed_) { self->queued_bytes_ -= data.size(); return; }
            self->queue_.push_back(std::make_shared<std::vector<uint8_t>>(std::move(data)));
            if (!self->writing_) self->write();
        });
        return kcenon::network::ok();
    }

    void close() override {
        if (closing_.exchange(true)) return;
        asio::post(*io_, [self = shared_from_this()] {
            // A-RELEASE-RP/A-ABORT accepted by send() must leave the queue first.
            if (!self->writing_ && self->queue_.empty()) self->force_close();
            else {
                self->timer_.expires_after(std::chrono::seconds{1});
                self->timer_.async_wait([self](std::error_code ec) {
                    if (!ec) self->force_close();
                });
            }
        });
    }

    // Only called on this server's I/O thread, including server shutdown.
    void force_close() {
        if (closed_) return;
        closed_ = true;
        closing_ = true;
        connected_ = false;
        timer_.cancel();
        std::error_code ignored;
        stream_.lowest_layer().cancel(ignored);
        stream_.lowest_layer().shutdown(tcp::socket::shutdown_both, ignored);
        stream_.lowest_layer().close(ignored);
        queue_.clear();
        if (auto cb = events_->copy(&callbacks::disconnected)) cb(id_);
        remove_(id_);
    }

private:
    void fail(std::error_code ec) {
        if (closed_) return;
        if (auto cb = events_->copy(&callbacks::error)) cb(id_, ec);
        force_close();
    }
    void read() {
        stream_.async_read_some(asio::buffer(buffer_),
            [self = shared_from_this()](std::error_code ec, size_t size) {
                if (self->closed_) return;
                if (ec) { self->fail(ec); return; }
                std::vector<uint8_t> data(self->buffer_.begin(), self->buffer_.begin() + size);
                if (auto cb = self->events_->copy(&callbacks::received)) cb(self->id_, data);
                if (!self->closing_) self->read();
            });
    }
    void write() {
        if (closed_ || queue_.empty()) return;
        writing_ = true;
        auto data = queue_.front();
        asio::async_write(stream_, asio::buffer(*data),
            [self = shared_from_this(), data](std::error_code ec, size_t) {
                self->queued_bytes_ -= data->size();
                self->writing_ = false;
                if (self->closed_) return;
                if (ec) { self->fail(ec); return; }
                self->queue_.pop_front();
                if (!self->queue_.empty()) self->write();
                else if (self->closing_) self->force_close();
            });
    }

    std::shared_ptr<asio::io_context> io_;
    std::shared_ptr<asio::ssl::context> context_;
    asio::ssl::stream<tcp::socket> stream_;
    asio::steady_timer timer_;
    const std::string id_;
    std::shared_ptr<callbacks> events_;
    std::function<void(const std::string&)> remove_;
    std::array<uint8_t, 64 * 1024> buffer_{};
    std::deque<std::shared_ptr<std::vector<uint8_t>>> queue_;
    std::atomic<size_t> queued_bytes_{0};
    std::atomic<bool> connected_{false}, closing_{false};
    bool closed_{false}, writing_{false}; // I/O thread only
};

class tls_server final : public protocol_server {
public:
    explicit tls_server(server_config config)
        : config_(std::move(config)), io_(std::make_shared<asio::io_context>()),
          context_(std::make_shared<asio::ssl::context>(asio::ssl::context::tls_server)),
          acceptor_(*io_), events_(std::make_shared<callbacks>()) {
        const auto& tls = config_.tls;
        if (!tls.is_valid()) throw std::invalid_argument("TLS server needs certificate and key");
        if (tls.min_version != tls_config::tls_version::v1_2 &&
            tls.min_version != tls_config::tls_version::v1_3)
            throw std::invalid_argument("Unsupported TLS minimum version");
        if (SSL_CTX_set_min_proto_version(context_->native_handle(),
                tls.min_version == tls_config::tls_version::v1_3 ? TLS1_3_VERSION : TLS1_2_VERSION) != 1)
            throw std::runtime_error("Cannot set TLS minimum version");
        context_->use_certificate_chain_file(tls.cert_path.string());
        context_->use_private_key_file(tls.key_path.string(), asio::ssl::context::pem);
        if (SSL_CTX_check_private_key(context_->native_handle()) != 1)
            throw std::invalid_argument("TLS server certificate/key mismatch");
        if (tls.verify_peer) {
            if (tls.ca_path.empty()) context_->set_default_verify_paths();
            else context_->load_verify_file(tls.ca_path.string());
            context_->set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
        } else context_->set_verify_mode(asio::ssl::verify_none);
    }
    ~tls_server() override { (void)stop(); }

    net_result start(uint16_t port) override {
        std::lock_guard lock(lifecycle_mutex_);
        if (running_ || thread_.joinable()) return error_info{"TLS server is running or still stopping"};
        try {
            io_->restart();
            acceptor_.open(tcp::v4());
            acceptor_.set_option(tcp::acceptor::reuse_address(true));
            acceptor_.bind({tcp::v4(), port});
            acceptor_.listen();
            guard_.emplace(asio::make_work_guard(*io_));
            running_ = true;
            accept();
            thread_ = std::thread([this] { io_->run(); });
            return kcenon::network::ok();
        } catch (const std::exception& e) {
            running_ = false;
            std::error_code ignored;
            acceptor_.close(ignored);
            guard_.reset();
            io_->poll();
            return error_info{std::string("TLS server start failed: ") + e.what()};
        }
    }
    net_result stop() override {
        // The I/O callback path must not wait for a lifecycle lock held by an
        // owner that is joining this very thread.
        if (io_->get_executor().running_in_this_thread()) {
            if (running_.exchange(false)) close_sessions();
            running_.notify_all();
            return kcenon::network::ok();
        }
        std::lock_guard lock(lifecycle_mutex_);
        if (running_.exchange(false)) asio::post(*io_, [this] { close_sessions(); });
        if (thread_.joinable()) thread_.join();
        running_.notify_all();
        return kcenon::network::ok();
    }
    bool is_running() const override { return running_; }
    void wait_for_stop() override {
        while (running_.load()) running_.wait(true);
    }
    size_t connection_count() const override { return count_; }
    void set_connection_callback(connection_callback_t cb) override {
        std::lock_guard lock(events_->mutex); events_->connected = std::move(cb);
    }
    void set_disconnection_callback(disconnection_callback_t cb) override {
        std::lock_guard lock(events_->mutex); events_->disconnected = std::move(cb);
    }
    void set_receive_callback(receive_callback_t cb) override {
        std::lock_guard lock(events_->mutex); events_->received = std::move(cb);
    }
    void set_error_callback(error_callback_t cb) override {
        std::lock_guard lock(events_->mutex); events_->error = std::move(cb);
    }
private:
    void close_sessions() {
        std::error_code ignored;
        acceptor_.cancel(ignored);
        acceptor_.close(ignored);
        // Closing removes sessions, so copy before invoking callbacks.
        auto sessions = sessions_;
        for (auto& [id, session] : sessions) session->force_close();
        guard_.reset(); // Drain cancellations before destroying their context.
    }
    void accept() {
        acceptor_.async_accept([this](std::error_code ec, tcp::socket socket) {
            if (!running_) return;
            if (!ec && (config_.max_associations == 0 || sessions_.size() < config_.max_associations)) {
                socket.set_option(tcp::no_delay(true));
                const auto id = config_.ae_title + "#tls" + std::to_string(++next_id_);
                auto session = std::make_shared<tls_session>(io_, context_, std::move(socket),
                    id, events_, [this](const std::string& key) {
                        sessions_.erase(key); count_ = sessions_.size();
                    });
                sessions_.emplace(id, session);
                count_ = sessions_.size();
                session->start(config_.association_timeout);
            }
            if (running_) accept();
        });
    }
    server_config config_;
    std::shared_ptr<asio::io_context> io_;
    std::shared_ptr<asio::ssl::context> context_;
    tcp::acceptor acceptor_;
    std::shared_ptr<callbacks> events_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> guard_;
    std::map<std::string, std::shared_ptr<tls_session>> sessions_;
    uint64_t next_id_{0};
    std::atomic<bool> running_{false};
    std::atomic<size_t> count_{0};
    std::thread thread_;
    std::mutex lifecycle_mutex_;
};
} // namespace

Result<std::shared_ptr<protocol_server>> make_tls_server(const server_config& config) {
    try { return std::shared_ptr<protocol_server>(std::make_shared<tls_server>(config)); }
    catch (const std::exception& e) { return error_info{std::string("TLS configuration failed: ") + e.what()}; }
}
} // namespace kcenon::pacs::network::detail
#else
namespace kcenon::pacs::network::detail {
Result<std::shared_ptr<kcenon::network::interfaces::i_protocol_server>>
make_tls_server(const server_config&) {
    return error_info{"TLS server requires OpenSSL and network_system"};
}
}
#endif
