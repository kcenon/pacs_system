/**
 * @file logger_adapter.cpp
 * @brief Implementation of DICOM audit logging adapter
 */

#include <pacs/integration/logger_adapter.hpp>

#include <kcenon/logger/core/logger.h>
#include <kcenon/logger/interfaces/logger_types.h>
#include <kcenon/logger/writers/console_writer.h>
#include <kcenon/logger/writers/rotating_file_writer.h>

#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace pacs::integration {

// =============================================================================
// Implementation Class
// =============================================================================

class logger_adapter::impl {
public:
    impl() = default;
    ~impl() { shutdown(); }

    void initialize(const logger_config& config) {
        std::lock_guard lock(mutex_);

        if (initialized_) {
            return;
        }

        config_ = config;
        min_level_.store(config.min_level);

        // Create log directory if needed
        if (config.enable_file || config.enable_audit_log) {
            std::filesystem::create_directories(config.log_directory);
        }

        // Initialize main logger
        logger_ = std::make_unique<kcenon::logger::logger>(
            config.async_mode, config.buffer_size);

        // Set minimum log level
        logger_->set_min_level(convert_log_level(config.min_level));

        // Add console writer if enabled
        if (config.enable_console) {
            logger_->add_writer(std::make_unique<kcenon::logger::console_writer>());
        }

        // Add file writer if enabled
        if (config.enable_file) {
            auto log_path = config.log_directory / "pacs.log";
            auto writer = std::make_unique<kcenon::logger::rotating_file_writer>(
                log_path.string(),
                config.max_file_size_mb * 1024 * 1024,  // Convert MB to bytes
                config.max_files);
            logger_->add_writer(std::move(writer));
        }

        // Start the logger
        logger_->start();

        // Initialize audit log file path if enabled
        if (config.enable_audit_log) {
            audit_log_path_ = config.log_directory / "audit.json";
        }

        initialized_ = true;
    }

    void shutdown() {
        std::lock_guard lock(mutex_);

        if (!initialized_) {
            return;
        }

        if (logger_) {
            logger_->flush();
            logger_->stop();
            logger_.reset();
        }

        initialized_ = false;
    }

    [[nodiscard]] auto is_initialized() const noexcept -> bool {
        return initialized_.load();
    }

    void log(log_level level, const std::string& message) {
        if (!initialized_ || !logger_) {
            return;
        }

        if (!is_level_enabled(level)) {
            return;
        }

        logger_->log(convert_log_level(level), message);
    }

    void log(log_level level,
             const std::string& message,
             [[maybe_unused]] const std::string& file,
             [[maybe_unused]] int line,
             [[maybe_unused]] const std::string& function) {
        // Delegate to the simple log method.
        // Source location is now auto-captured by the underlying logger.
        // The file, line, and function parameters are ignored.
        log(level, message);
    }

    [[nodiscard]] auto is_level_enabled(log_level level) const noexcept -> bool {
        return static_cast<int>(level) >= static_cast<int>(min_level_.load());
    }

    void flush() {
        if (logger_) {
            logger_->flush();
        }
    }

    void set_min_level(log_level level) {
        min_level_.store(level);
        if (logger_) {
            logger_->set_min_level(convert_log_level(level));
        }
    }

    [[nodiscard]] auto get_min_level() const noexcept -> log_level {
        return min_level_.load();
    }

    [[nodiscard]] auto get_config() const -> const logger_config& { return config_; }

    void write_audit_log(const std::string& event_type,
                         const std::string& outcome,
                         const std::map<std::string, std::string>& fields) {
        if (!config_.enable_audit_log) {
            return;
        }

        std::lock_guard lock(audit_mutex_);

        std::ofstream file(audit_log_path_, std::ios::app);
        if (!file) {
            return;
        }

        // Build JSON entry
        std::ostringstream json;
        json << "{";
        json << "\"timestamp\":\"" << format_iso8601() << "\",";
        json << "\"event_type\":\"" << escape_json(event_type) << "\",";
        json << "\"outcome\":\"" << escape_json(outcome) << "\"";

        for (const auto& [key, value] : fields) {
            json << ",\"" << escape_json(key) << "\":\"" << escape_json(value) << "\"";
        }

        json << "}\n";

        file << json.str();
        file.flush();
    }

private:
    [[nodiscard]] static auto convert_log_level(log_level level) -> kcenon::logger::log_level {
        switch (level) {
            case log_level::trace:
                return kcenon::logger::log_level::trace;
            case log_level::debug:
                return kcenon::logger::log_level::debug;
            case log_level::info:
                return kcenon::logger::log_level::info;
            case log_level::warn:
                return kcenon::logger::log_level::warn;
            case log_level::error:
                return kcenon::logger::log_level::error;
            case log_level::fatal:
                return kcenon::logger::log_level::fatal;
            case log_level::off:
            default:
                return kcenon::logger::log_level::off;
        }
    }

    [[nodiscard]] static auto format_iso8601() -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::tm tm_val{};
#ifdef _WIN32
        localtime_s(&tm_val, &time_t_val);
#else
        localtime_r(&time_t_val, &tm_val);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_val, "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        oss << std::put_time(&tm_val, "%z");
        return oss.str();
    }

    [[nodiscard]] static auto escape_json(const std::string& str) -> std::string {
        std::ostringstream oss;
        for (char c : str) {
            switch (c) {
                case '"':
                    oss << "\\\"";
                    break;
                case '\\':
                    oss << "\\\\";
                    break;
                case '\b':
                    oss << "\\b";
                    break;
                case '\f':
                    oss << "\\f";
                    break;
                case '\n':
                    oss << "\\n";
                    break;
                case '\r':
                    oss << "\\r";
                    break;
                case '\t':
                    oss << "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 32) {
                        oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(c);
                    } else {
                        oss << c;
                    }
                    break;
            }
        }
        return oss.str();
    }

    mutable std::mutex mutex_;
    mutable std::mutex audit_mutex_;
    std::atomic<bool> initialized_{false};
    std::atomic<log_level> min_level_{log_level::info};
    logger_config config_;
    std::unique_ptr<kcenon::logger::logger> logger_;
    std::filesystem::path audit_log_path_;
};

// =============================================================================
// Static Member Initialization
// =============================================================================

std::unique_ptr<logger_adapter::impl> logger_adapter::pimpl_ =
    std::make_unique<logger_adapter::impl>();

// =============================================================================
// Initialization
// =============================================================================

void logger_adapter::initialize(const logger_config& config) {
    pimpl_->initialize(config);
}

void logger_adapter::shutdown() { pimpl_->shutdown(); }

auto logger_adapter::is_initialized() noexcept -> bool {
    return pimpl_->is_initialized();
}

// =============================================================================
// Standard Logging
// =============================================================================

void logger_adapter::log(log_level level, const std::string& message) {
    pimpl_->log(level, message);
}

// Suppress deprecation warning for deprecated method implementation
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

void logger_adapter::log(log_level level,
                         const std::string& message,
                         const std::string& file,
                         int line,
                         const std::string& function) {
    pimpl_->log(level, message, file, line, function);
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

auto logger_adapter::is_level_enabled(log_level level) noexcept -> bool {
    return pimpl_->is_level_enabled(level);
}

void logger_adapter::flush() { pimpl_->flush(); }

// =============================================================================
// DICOM Audit Logging
// =============================================================================

void logger_adapter::log_association_established(const std::string& calling_ae,
                                                 const std::string& called_ae,
                                                 const std::string& remote_ip) {
    info("Association established: {} -> {} from {}",
         calling_ae, called_ae, remote_ip);

    write_audit_log("ASSOCIATION_ESTABLISHED", "success",
                    {{"calling_ae", calling_ae},
                     {"called_ae", called_ae},
                     {"remote_ip", remote_ip}});
}

void logger_adapter::log_association_released(const std::string& calling_ae,
                                              const std::string& called_ae) {
    debug("Association released: {} -> {}", calling_ae, called_ae);

    write_audit_log("ASSOCIATION_RELEASED", "success",
                    {{"calling_ae", calling_ae}, {"called_ae", called_ae}});
}

void logger_adapter::log_c_store_received(const std::string& calling_ae,
                                          const std::string& patient_id,
                                          const std::string& study_uid,
                                          const std::string& sop_instance_uid,
                                          storage_status status) {
    auto outcome = (status == storage_status::success) ? "success" : "failure";
    auto status_str = storage_status_to_string(status);

    if (status == storage_status::success) {
        info("C-STORE received: patient={} study={} instance={} from {}",
             patient_id, study_uid, sop_instance_uid, calling_ae);
    } else {
        warn("C-STORE failed: patient={} status={} from {}",
             patient_id, status_str, calling_ae);
    }

    write_audit_log("C-STORE", outcome,
                    {{"calling_ae", calling_ae},
                     {"patient_id", patient_id},
                     {"study_uid", study_uid},
                     {"sop_instance_uid", sop_instance_uid},
                     {"status", status_str}});
}

void logger_adapter::log_c_find_executed(const std::string& calling_ae,
                                         query_level level,
                                         std::size_t matches_returned) {
    auto level_str = query_level_to_string(level);

    debug("C-FIND executed: level={} matches={} from {}",
          level_str, matches_returned, calling_ae);

    write_audit_log("C-FIND", "success",
                    {{"calling_ae", calling_ae},
                     {"query_level", level_str},
                     {"matches_returned", std::to_string(matches_returned)}});
}

void logger_adapter::log_c_move_executed(const std::string& calling_ae,
                                         const std::string& destination_ae,
                                         const std::string& study_uid,
                                         std::size_t instances_moved,
                                         move_status status) {
    auto outcome = (status == move_status::success ||
                    status == move_status::partial_success)
                       ? "success"
                       : "failure";
    auto status_str = move_status_to_string(status);

    if (status == move_status::success) {
        info("C-MOVE completed: study={} instances={} to {} from {}",
             study_uid, instances_moved, destination_ae, calling_ae);
    } else {
        warn("C-MOVE failed: study={} status={} to {} from {}",
             study_uid, status_str, destination_ae, calling_ae);
    }

    write_audit_log("C-MOVE", outcome,
                    {{"calling_ae", calling_ae},
                     {"destination_ae", destination_ae},
                     {"study_uid", study_uid},
                     {"instances_moved", std::to_string(instances_moved)},
                     {"status", status_str}});
}

void logger_adapter::log_security_event(security_event_type type,
                                        const std::string& description,
                                        const std::string& user_id) {
    auto type_str = security_event_to_string(type);

    // Security events are always logged at warn level or higher
    switch (type) {
        case security_event_type::authentication_success:
            info("Security event: {} - {}", type_str, description);
            break;
        case security_event_type::authentication_failure:
        case security_event_type::access_denied:
        case security_event_type::association_rejected:
        case security_event_type::invalid_request:
            warn("Security event: {} - {}", type_str, description);
            break;
        case security_event_type::configuration_change:
        case security_event_type::data_export:
            info("Security event: {} - {}", type_str, description);
            break;
    }

    std::map<std::string, std::string> fields = {
        {"security_event", type_str}, {"description", description}};

    if (!user_id.empty()) {
        fields["user_id"] = user_id;
    }

    write_audit_log("SECURITY", security_event_to_string(type), fields);
}

// =============================================================================
// Configuration
// =============================================================================

void logger_adapter::set_min_level(log_level level) {
    pimpl_->set_min_level(level);
}

auto logger_adapter::get_min_level() noexcept -> log_level {
    return pimpl_->get_min_level();
}

auto logger_adapter::get_config() -> const logger_config& {
    return pimpl_->get_config();
}

// =============================================================================
// Private Helpers
// =============================================================================

void logger_adapter::write_audit_log(
    const std::string& event_type,
    const std::string& outcome,
    const std::map<std::string, std::string>& fields) {
    pimpl_->write_audit_log(event_type, outcome, fields);
}

auto logger_adapter::storage_status_to_string(storage_status status) -> std::string {
    switch (status) {
        case storage_status::success:
            return "Success";
        case storage_status::out_of_resources:
            return "OutOfResources";
        case storage_status::dataset_error:
            return "DataSetError";
        case storage_status::cannot_understand:
            return "CannotUnderstand";
        case storage_status::processing_failure:
            return "ProcessingFailure";
        case storage_status::duplicate_rejected:
            return "DuplicateRejected";
        case storage_status::duplicate_stored:
            return "DuplicateStored";
        case storage_status::unknown_error:
        default:
            return "UnknownError";
    }
}

auto logger_adapter::move_status_to_string(move_status status) -> std::string {
    switch (status) {
        case move_status::success:
            return "Success";
        case move_status::partial_success:
            return "PartialSuccess";
        case move_status::refused_out_of_resources:
            return "RefusedOutOfResources";
        case move_status::refused_move_destination_unknown:
            return "RefusedMoveDestinationUnknown";
        case move_status::identifier_does_not_match:
            return "IdentifierDoesNotMatch";
        case move_status::unable_to_process:
            return "UnableToProcess";
        case move_status::cancelled:
            return "Cancelled";
        case move_status::unknown_error:
        default:
            return "UnknownError";
    }
}

auto logger_adapter::query_level_to_string(query_level level) -> std::string {
    switch (level) {
        case query_level::patient:
            return "PATIENT";
        case query_level::study:
            return "STUDY";
        case query_level::series:
            return "SERIES";
        case query_level::image:
            return "IMAGE";
        default:
            return "UNKNOWN";
    }
}

auto logger_adapter::security_event_to_string(security_event_type type) -> std::string {
    switch (type) {
        case security_event_type::authentication_success:
            return "authentication_success";
        case security_event_type::authentication_failure:
            return "authentication_failure";
        case security_event_type::access_denied:
            return "access_denied";
        case security_event_type::configuration_change:
            return "configuration_change";
        case security_event_type::data_export:
            return "data_export";
        case security_event_type::association_rejected:
            return "association_rejected";
        case security_event_type::invalid_request:
            return "invalid_request";
        default:
            return "unknown";
    }
}

auto logger_adapter::log_level_to_string(log_level level) -> std::string {
    switch (level) {
        case log_level::trace:
            return "TRACE";
        case log_level::debug:
            return "DEBUG";
        case log_level::info:
            return "INFO";
        case log_level::warn:
            return "WARN";
        case log_level::error:
            return "ERROR";
        case log_level::fatal:
            return "FATAL";
        case log_level::off:
        default:
            return "OFF";
    }
}

}  // namespace pacs::integration
