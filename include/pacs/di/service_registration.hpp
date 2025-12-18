/**
 * @file service_registration.hpp
 * @brief ServiceContainer registration module for PACS services
 *
 * This file provides functions for registering DICOM services with
 * common_system's ServiceContainer, enabling dependency injection
 * throughout the pacs_system.
 *
 * Usage:
 * @code
 * auto container = std::make_shared<kcenon::common::di::service_container>();
 * pacs::di::register_services(*container);
 *
 * auto storage = container->resolve<pacs::di::IDicomStorage>();
 * auto network = container->resolve<pacs::di::IDicomNetwork>();
 * @endcode
 *
 * @see Issue #312 - ServiceContainer based DI Integration
 */

#pragma once

#include "service_interfaces.hpp"

#include <pacs/storage/file_storage.hpp>
#include <pacs/encoding/compression/codec_factory.hpp>

#include <kcenon/common/di/service_container.h>

#include <filesystem>
#include <memory>

namespace pacs::di {

// =============================================================================
// Service Registration Configuration
// =============================================================================

/**
 * @brief Configuration for service registration
 *
 * Allows customization of default service implementations and settings.
 */
struct registration_config {
    /// Default storage path for file_storage (empty = use temp directory)
    std::filesystem::path storage_path;

    /// Enable network services (default: true)
    bool enable_network = true;

    /// Enable codec services (default: true)
    bool enable_codecs = true;

    /// Enable logger services (default: true)
    bool enable_logger = true;

    /// Use singleton lifetime for services (default: true)
    bool use_singletons = true;

    registration_config() = default;
};

// =============================================================================
// Service Registration Functions
// =============================================================================

/**
 * @brief Register all PACS services with the container
 *
 * Registers default implementations for:
 * - IDicomStorage: file_storage
 * - IDicomNetwork: DicomNetworkService
 * - IDicomCodec: codec_factory-based codecs
 *
 * @param container The service container to register services with
 * @param config Optional registration configuration
 * @return VoidResult indicating success or registration error
 *
 * @example
 * @code
 * kcenon::common::di::service_container container;
 * auto result = pacs::di::register_services(container);
 * if (result.is_err()) {
 *     std::cerr << "Registration failed: " << result.error().message << '\n';
 * }
 * @endcode
 */
[[nodiscard]] inline kcenon::common::VoidResult register_services(
    kcenon::common::di::IServiceContainer& container,
    const registration_config& config = {}) {

    using namespace kcenon::common::di;

    const auto lifetime = config.use_singletons
        ? service_lifetime::singleton
        : service_lifetime::transient;

    // -------------------------------------------------------------------------
    // Register IDicomStorage
    // -------------------------------------------------------------------------
    {
        auto storage_path = config.storage_path;
        if (storage_path.empty()) {
            storage_path = std::filesystem::temp_directory_path() / "pacs_storage";
        }

        auto result = container.register_factory<IDicomStorage>(
            [path = storage_path](IServiceContainer&) -> std::shared_ptr<IDicomStorage> {
                storage::file_storage_config storage_config;
                storage_config.root_path = path;
                storage_config.create_directories = true;
                return std::make_shared<storage::file_storage>(storage_config);
            },
            lifetime
        );

        if (result.is_err()) {
            return result;
        }
    }

    // -------------------------------------------------------------------------
    // Register IDicomNetwork
    // -------------------------------------------------------------------------
    if (config.enable_network) {
        auto result = container.register_factory<IDicomNetwork>(
            [](IServiceContainer&) -> std::shared_ptr<IDicomNetwork> {
                return std::make_shared<DicomNetworkService>();
            },
            lifetime
        );

        if (result.is_err()) {
            return result;
        }
    }

    // -------------------------------------------------------------------------
    // Register ILogger
    // -------------------------------------------------------------------------
    if (config.enable_logger) {
        auto result = container.register_factory<ILogger>(
            [](IServiceContainer&) -> std::shared_ptr<ILogger> {
                return std::make_shared<LoggerService>();
            },
            lifetime
        );

        if (result.is_err()) {
            return result;
        }
    }

    return kcenon::common::VoidResult::ok({});
}

/**
 * @brief Register storage service with custom implementation
 *
 * @tparam TStorage Storage implementation type (must derive from IDicomStorage)
 * @param container The service container
 * @param factory Factory function for creating storage instances
 * @param lifetime Service lifetime (default: singleton)
 * @return VoidResult indicating success or error
 */
template<typename TStorage>
    requires std::derived_from<TStorage, IDicomStorage>
[[nodiscard]] inline kcenon::common::VoidResult register_storage(
    kcenon::common::di::IServiceContainer& container,
    std::function<std::shared_ptr<TStorage>(kcenon::common::di::IServiceContainer&)> factory,
    kcenon::common::di::service_lifetime lifetime = kcenon::common::di::service_lifetime::singleton) {

    return container.register_factory<IDicomStorage>(
        [f = std::move(factory)](kcenon::common::di::IServiceContainer& c) -> std::shared_ptr<IDicomStorage> {
            return f(c);
        },
        lifetime
    );
}

/**
 * @brief Register storage service with pre-created instance
 *
 * @param container The service container
 * @param instance The storage instance to register
 * @return VoidResult indicating success or error
 */
[[nodiscard]] inline kcenon::common::VoidResult register_storage_instance(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<IDicomStorage> instance) {

    return container.register_instance<IDicomStorage>(std::move(instance));
}

/**
 * @brief Register network service with custom implementation
 *
 * @tparam TNetwork Network implementation type (must derive from IDicomNetwork)
 * @param container The service container
 * @param factory Factory function for creating network service instances
 * @param lifetime Service lifetime (default: singleton)
 * @return VoidResult indicating success or error
 */
template<typename TNetwork>
    requires std::derived_from<TNetwork, IDicomNetwork>
[[nodiscard]] inline kcenon::common::VoidResult register_network(
    kcenon::common::di::IServiceContainer& container,
    std::function<std::shared_ptr<TNetwork>(kcenon::common::di::IServiceContainer&)> factory,
    kcenon::common::di::service_lifetime lifetime = kcenon::common::di::service_lifetime::singleton) {

    return container.register_factory<IDicomNetwork>(
        [f = std::move(factory)](kcenon::common::di::IServiceContainer& c) -> std::shared_ptr<IDicomNetwork> {
            return f(c);
        },
        lifetime
    );
}

/**
 * @brief Register network service with pre-created instance
 *
 * @param container The service container
 * @param instance The network service instance to register
 * @return VoidResult indicating success or error
 */
[[nodiscard]] inline kcenon::common::VoidResult register_network_instance(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<IDicomNetwork> instance) {

    return container.register_instance<IDicomNetwork>(std::move(instance));
}

/**
 * @brief Register logger service with custom implementation
 *
 * @tparam TLogger Logger implementation type (must derive from ILogger)
 * @param container The service container
 * @param factory Factory function for creating logger instances
 * @param lifetime Service lifetime (default: singleton)
 * @return VoidResult indicating success or error
 */
template<typename TLogger>
    requires std::derived_from<TLogger, ILogger>
[[nodiscard]] inline kcenon::common::VoidResult register_logger(
    kcenon::common::di::IServiceContainer& container,
    std::function<std::shared_ptr<TLogger>(kcenon::common::di::IServiceContainer&)> factory,
    kcenon::common::di::service_lifetime lifetime = kcenon::common::di::service_lifetime::singleton) {

    return container.register_factory<ILogger>(
        [f = std::move(factory)](kcenon::common::di::IServiceContainer& c) -> std::shared_ptr<ILogger> {
            return f(c);
        },
        lifetime
    );
}

/**
 * @brief Register logger service with pre-created instance
 *
 * @param container The service container
 * @param instance The logger instance to register
 * @return VoidResult indicating success or error
 */
[[nodiscard]] inline kcenon::common::VoidResult register_logger_instance(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<ILogger> instance) {

    return container.register_instance<ILogger>(std::move(instance));
}

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Create a configured service container with all PACS services
 *
 * Factory function that creates a new service container and registers
 * all default PACS services.
 *
 * @param config Optional registration configuration
 * @return Shared pointer to configured service container
 *
 * @example
 * @code
 * auto container = pacs::di::create_container();
 * auto storage = container->resolve<pacs::di::IDicomStorage>().value();
 * @endcode
 */
[[nodiscard]] inline std::shared_ptr<kcenon::common::di::service_container>
create_container(const registration_config& config = {}) {
    auto container = std::make_shared<kcenon::common::di::service_container>();
    auto result = register_services(*container, config);
    if (result.is_err()) {
        return nullptr;
    }
    return container;
}

}  // namespace pacs::di
