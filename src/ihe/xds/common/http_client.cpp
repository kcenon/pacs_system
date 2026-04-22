// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file http_client.cpp
 * @brief libcurl HTTPS client with RAII over CURL* and curl_slist*.
 */

#include "http_client.h"

#include <curl/curl.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace kcenon::pacs::ihe::xds::detail {

namespace {

struct curl_deleter {
    void operator()(CURL* c) const noexcept {
        if (c) curl_easy_cleanup(c);
    }
};
using curl_ptr = std::unique_ptr<CURL, curl_deleter>;

struct slist_deleter {
    void operator()(curl_slist* s) const noexcept {
        if (s) curl_slist_free_all(s);
    }
};
using slist_ptr = std::unique_ptr<curl_slist, slist_deleter>;

std::mutex& override_mutex() {
    static std::mutex m;
    return m;
}

transport_override& override_fn() {
    static transport_override fn;
    return fn;
}

// Response-size cap. Registers should answer with a small ebRS
// RegistryResponse (typically < 100 KiB even with a long error list).
// 8 MiB is a generous upper bound that also fits comfortably inside the
// pugixml parse we do downstream.
constexpr std::size_t kMaxResponseBytes = 8 * 1024 * 1024;

struct response_sink {
    std::string buffer;
    bool oversize{false};
};

std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb,
                           void* userdata) {
    const std::size_t total = size * nmemb;
    auto* sink = static_cast<response_sink*>(userdata);
    if (sink->buffer.size() + total > kMaxResponseBytes) {
        sink->oversize = true;
        return 0;
    }
    sink->buffer.append(ptr, total);
    return total;
}

std::size_t read_callback(char* buffer, std::size_t size, std::size_t nmemb,
                          void* userdata) {
    struct read_state {
        const std::string* body;
        std::size_t offset;
    };
    auto* state = static_cast<read_state*>(userdata);
    const std::size_t capacity = size * nmemb;
    const std::size_t remaining = state->body->size() - state->offset;
    const std::size_t to_copy = remaining < capacity ? remaining : capacity;
    std::memcpy(buffer, state->body->data() + state->offset, to_copy);
    state->offset += to_copy;
    return to_copy;
}

}  // namespace

void set_http_transport_override(transport_override fn) {
    std::lock_guard<std::mutex> lock(override_mutex());
    override_fn() = std::move(fn);
}

void clear_http_transport_override() {
    std::lock_guard<std::mutex> lock(override_mutex());
    override_fn() = {};
}

kcenon::common::Result<http_response> http_post(const http_options& opts,
                                                const std::string& content_type,
                                                const std::string& body) {
    if (opts.endpoint.empty()) {
        return kcenon::common::make_error<http_response>(
            static_cast<int>(error_code::transport_not_configured),
            "http_options.endpoint is empty", std::string(error_source));
    }

    {
        transport_override fn_copy;
        {
            std::lock_guard<std::mutex> lock(override_mutex());
            fn_copy = override_fn();
        }
        if (fn_copy) {
            transport_request req;
            req.url = opts.endpoint;
            req.content_type = content_type;
            req.soap_action = opts.soap_action;
            req.body = body;
            req.verify_peer = opts.verify_peer;
            return fn_copy(req);
        }
    }

    curl_ptr curl(curl_easy_init());
    if (!curl) {
        return kcenon::common::make_error<http_response>(
            static_cast<int>(error_code::transport_init_failed),
            "curl_easy_init failed", std::string(error_source));
    }

    slist_ptr headers;
    {
        curl_slist* raw = nullptr;
        const std::string ct_header = "Content-Type: " + content_type;
        raw = curl_slist_append(raw, ct_header.c_str());
        const std::string action_header =
            "SOAPAction: \"" + opts.soap_action + "\"";
        raw = curl_slist_append(raw, action_header.c_str());
        raw = curl_slist_append(raw, "Accept: multipart/related, application/xop+xml, application/soap+xml");
        raw = curl_slist_append(raw, "Expect:");
        headers.reset(raw);
    }

    response_sink response;

    curl_easy_setopt(curl.get(), CURLOPT_URL, opts.endpoint.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, opts.user_agent.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE_LARGE,
                     static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT_MS,
                     static_cast<long>(opts.connect_timeout.count()));
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS,
                     static_cast<long>(opts.request_timeout.count()));

    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER,
                     opts.verify_peer ? 1L : 0L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST,
                     opts.verify_host ? 2L : 0L);
    if (!opts.ca_bundle_path.empty()) {
        curl_easy_setopt(curl.get(), CURLOPT_CAINFO,
                         opts.ca_bundle_path.c_str());
    }
    if (!opts.client_certificate_path.empty()) {
        curl_easy_setopt(curl.get(), CURLOPT_SSLCERT,
                         opts.client_certificate_path.c_str());
    }
    if (!opts.client_private_key_path.empty()) {
        curl_easy_setopt(curl.get(), CURLOPT_SSLKEY,
                         opts.client_private_key_path.c_str());
    }

    (void)read_callback;  // kept for future streamed upload path

    const CURLcode rc = curl_easy_perform(curl.get());
    if (rc != CURLE_OK) {
        // write_callback returned 0 once the response exceeded
        // kMaxResponseBytes, which libcurl surfaces as CURLE_WRITE_ERROR.
        // Promote that specific case to a dedicated error code so callers
        // can distinguish DoS protection from other transport failures.
        if (rc == CURLE_WRITE_ERROR && response.oversize) {
            return kcenon::common::make_error<http_response>(
                static_cast<int>(error_code::transport_response_too_large),
                "registry response exceeds " +
                    std::to_string(kMaxResponseBytes) +
                    " byte cap; transfer aborted",
                std::string(error_source));
        }
        const bool tls =
            rc == CURLE_PEER_FAILED_VERIFICATION || rc == CURLE_SSL_CONNECT_ERROR ||
            rc == CURLE_SSL_CERTPROBLEM || rc == CURLE_SSL_CIPHER ||
            rc == CURLE_SSL_ISSUER_ERROR || rc == CURLE_SSL_CACERT_BADFILE;
        const auto code = tls ? error_code::transport_tls_error
                               : (rc == CURLE_OPERATION_TIMEDOUT
                                      ? error_code::transport_timeout
                                      : error_code::transport_curl_failed);
        return kcenon::common::make_error<http_response>(
            static_cast<int>(code),
            std::string("libcurl error ") + std::to_string(static_cast<int>(rc)) +
                ": " + curl_easy_strerror(rc),
            std::string(error_source));
    }

    http_response out;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &out.status_code);
    out.body = std::move(response.buffer);

    if (out.status_code < 200 || out.status_code >= 300) {
        return kcenon::common::make_error<http_response>(
            static_cast<int>(error_code::transport_http_error),
            std::string("unexpected HTTP status ") +
                std::to_string(out.status_code),
            std::string(error_source));
    }

    return out;
}

}  // namespace kcenon::pacs::ihe::xds::detail
