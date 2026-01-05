# SDS - Dependency Injection Module

> **Version:** 1.1.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-05
> **Status:** Complete

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-DI-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#479](https://github.com/kcenon/pacs_system/issues/479), [#468](https://github.com/kcenon/pacs_system/issues/468), [#312](https://github.com/kcenon/pacs_system/issues/312), [#309](https://github.com/kcenon/pacs_system/issues/309) |

### Related Requirements

| Requirement | Description |
|-------------|-------------|
| SRS-MAINT-005 | Modular Design with Dependency Injection |
| SRS-INT-005 | logger_system Integration |
| SRS-MAINT-001 | Code Coverage and Testability |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Service Interfaces](#2-service-interfaces)
- [3. Service Registration](#3-service-registration)
- [4. Logger Interface](#4-logger-interface)
- [5. Test Support](#5-test-support)
- [6. Class Diagrams](#6-class-diagrams)
- [7. Sequence Diagrams](#7-sequence-diagrams)
- [8. Traceability](#8-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the design of the Dependency Injection (DI) module for the PACS System. The module provides:

- **Service Interfaces**: Abstract interfaces for DICOM storage, network, and codec services
- **Service Registration**: Centralized service registration with `kcenon::common::di::service_container`
- **Logger Abstraction**: Injectable logger interface for all PACS services
- **Test Support**: Mock implementations and test utilities for unit testing

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Service Interfaces | 1 header | DES-DI-001 |
| Service Registration | 1 header | DES-DI-002 |
| Logger Interface | 1 header | DES-DI-003 |
| Test Support | 1 header | DES-DI-004 |

### 1.3 Design Identifier Convention

```
DES-DI-<NUMBER>

Where:
- DES: Design Specification prefix
- DI: Dependency Injection module identifier
- NUMBER: 3-digit sequential number (001-004)
```

### 1.4 Integration with kcenon Ecosystem

| System | Integration Point |
|--------|-------------------|
| common_system | `service_container` for DI, `Result<T>` for error handling |
| logger_system | `logger_adapter` for logging infrastructure |
| thread_system | Thread-safe service resolution |

---

## 2. Service Interfaces

### 2.1 DES-DI-001: Service Interfaces

**Traces to:** SRS-MAINT-005 (Modular Design)

**File:** `include/pacs/di/service_interfaces.hpp`

The service_interfaces header provides type aliases and interface definitions for PACS services that can be injected through the DI container.

#### 2.1.1 Interface Definitions

```cpp
namespace pacs::di {

// Storage interface alias
using IDicomStorage = storage::storage_interface;

// Codec interface alias
using IDicomCodec = encoding::compression::compression_codec;

// Network service interface
class IDicomNetwork {
public:
    virtual ~IDicomNetwork() = default;

    // Server operations
    [[nodiscard]] virtual std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& tls_cfg = {}) = 0;

    // Client operations
    [[nodiscard]] virtual integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) = 0;

    [[nodiscard]] virtual integration::Result<integration::network_adapter::session_ptr>
        connect(const std::string& host, uint16_t port,
                std::chrono::milliseconds timeout = std::chrono::seconds{30}) = 0;

protected:
    IDicomNetwork() = default;
    IDicomNetwork(const IDicomNetwork&) = default;
    IDicomNetwork& operator=(const IDicomNetwork&) = default;
    IDicomNetwork(IDicomNetwork&&) = default;
    IDicomNetwork& operator=(IDicomNetwork&&) = default;
};

// Default network implementation
class DicomNetworkService final : public IDicomNetwork {
public:
    DicomNetworkService() = default;
    ~DicomNetworkService() override = default;

    [[nodiscard]] std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& tls_cfg = {}) override;

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) override;

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const std::string& host, uint16_t port,
                std::chrono::milliseconds timeout = std::chrono::seconds{30}) override;
};

} // namespace pacs::di
```

#### 2.1.2 Interface Dependencies

| Interface | Base Type | Purpose |
|-----------|-----------|---------|
| `IDicomStorage` | `storage::storage_interface` | DICOM dataset persistence |
| `IDicomCodec` | `compression::compression_codec` | Image compression/decompression |
| `IDicomNetwork` | Abstract class | DICOM network operations |

#### 2.1.3 Design Rationale

- **Type Aliases**: Simplify DI registration by unifying interface names
- **Abstract Interface**: `IDicomNetwork` allows mock injection for network testing
- **Default Implementation**: `DicomNetworkService` delegates to `network_adapter`
- **Thread Safety**: Concrete implementations must ensure thread safety

---

## 3. Service Registration

### 3.1 DES-DI-002: Service Registration

**Traces to:** SRS-MAINT-005 (Modular Design)

**File:** `include/pacs/di/service_registration.hpp`

The service_registration header provides functions for registering PACS services with the `kcenon::common::di::service_container`.

#### 3.1.1 Registration Configuration

```cpp
namespace pacs::di {

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

} // namespace pacs::di
```

#### 3.1.2 Core Registration Functions

```cpp
namespace pacs::di {

// Register all PACS services with the container
[[nodiscard]] kcenon::common::VoidResult register_services(
    kcenon::common::di::IServiceContainer& container,
    const registration_config& config = {});

// Create a configured service container with all PACS services
[[nodiscard]] std::shared_ptr<kcenon::common::di::service_container>
    create_container(const registration_config& config = {});

} // namespace pacs::di
```

#### 3.1.3 Typed Registration Functions

```cpp
namespace pacs::di {

// Register storage service with custom implementation
template<typename TStorage>
    requires std::derived_from<TStorage, IDicomStorage>
[[nodiscard]] kcenon::common::VoidResult register_storage(
    kcenon::common::di::IServiceContainer& container,
    std::function<std::shared_ptr<TStorage>(kcenon::common::di::IServiceContainer&)> factory,
    kcenon::common::di::service_lifetime lifetime = kcenon::common::di::service_lifetime::singleton);

// Register storage service with pre-created instance
[[nodiscard]] kcenon::common::VoidResult register_storage_instance(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<IDicomStorage> instance);

// Register network service with custom implementation
template<typename TNetwork>
    requires std::derived_from<TNetwork, IDicomNetwork>
[[nodiscard]] kcenon::common::VoidResult register_network(
    kcenon::common::di::IServiceContainer& container,
    std::function<std::shared_ptr<TNetwork>(kcenon::common::di::IServiceContainer&)> factory,
    kcenon::common::di::service_lifetime lifetime = kcenon::common::di::service_lifetime::singleton);

// Register network service with pre-created instance
[[nodiscard]] kcenon::common::VoidResult register_network_instance(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<IDicomNetwork> instance);

// Register logger service with custom implementation
template<typename TLogger>
    requires std::derived_from<TLogger, ILogger>
[[nodiscard]] kcenon::common::VoidResult register_logger(
    kcenon::common::di::IServiceContainer& container,
    std::function<std::shared_ptr<TLogger>(kcenon::common::di::IServiceContainer&)> factory,
    kcenon::common::di::service_lifetime lifetime = kcenon::common::di::service_lifetime::singleton);

// Register logger service with pre-created instance
[[nodiscard]] kcenon::common::VoidResult register_logger_instance(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<ILogger> instance);

} // namespace pacs::di
```

#### 3.1.4 Usage Example

```cpp
#include <pacs/di/service_registration.hpp>

// Create container with default services
auto container = pacs::di::create_container();

// Resolve services
auto storage = container->resolve<pacs::di::IDicomStorage>().value();
auto network = container->resolve<pacs::di::IDicomNetwork>().value();
auto logger = container->resolve<pacs::di::ILogger>().value();

// Custom configuration
pacs::di::registration_config config;
config.storage_path = "/data/pacs";
config.enable_network = false;

auto custom_container = pacs::di::create_container(config);
```

#### 3.1.5 Registered Services

| Interface | Default Implementation | Lifetime |
|-----------|----------------------|----------|
| `IDicomStorage` | `storage::file_storage` | Singleton |
| `IDicomNetwork` | `DicomNetworkService` | Singleton |
| `ILogger` | `LoggerService` | Singleton |

---

## 4. Logger Interface

### 4.1 DES-DI-003: Logger Interface

**Traces to:** SRS-INT-005 (logger_system Integration)

**File:** `include/pacs/di/ilogger.hpp`

The logger interface provides a standardized logging API that can be injected into PACS services, enabling testable code through mock implementations.

#### 4.1.1 ILogger Interface

```cpp
namespace pacs::di {

class ILogger {
public:
    virtual ~ILogger() = default;

    // Log level methods
    virtual void trace(std::string_view message) = 0;
    virtual void debug(std::string_view message) = 0;
    virtual void info(std::string_view message) = 0;
    virtual void warn(std::string_view message) = 0;
    virtual void error(std::string_view message) = 0;
    virtual void fatal(std::string_view message) = 0;

    // Level check
    [[nodiscard]] virtual bool is_enabled(integration::log_level level) const noexcept = 0;

    // Formatted logging (convenience templates)
    template <typename... Args>
    void trace_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void debug_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void info_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void warn_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void error_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);

    template <typename... Args>
    void fatal_fmt(pacs::compat::format_string<Args...> fmt, Args&&... args);

protected:
    ILogger() = default;
    ILogger(const ILogger&) = default;
    ILogger& operator=(const ILogger&) = default;
    ILogger(ILogger&&) = default;
    ILogger& operator=(ILogger&&) = default;
};

} // namespace pacs::di
```

#### 4.1.2 NullLogger Implementation

```cpp
namespace pacs::di {

class NullLogger final : public ILogger {
public:
    NullLogger() = default;
    ~NullLogger() override = default;

    void trace(std::string_view /*message*/) override {}
    void debug(std::string_view /*message*/) override {}
    void info(std::string_view /*message*/) override {}
    void warn(std::string_view /*message*/) override {}
    void error(std::string_view /*message*/) override {}
    void fatal(std::string_view /*message*/) override {}

    [[nodiscard]] bool is_enabled(integration::log_level /*level*/) const noexcept override {
        return false;
    }
};

// Global null logger instance
[[nodiscard]] std::shared_ptr<ILogger> null_logger();

} // namespace pacs::di
```

#### 4.1.3 LoggerService Implementation

```cpp
namespace pacs::di {

class LoggerService final : public ILogger {
public:
    LoggerService() = default;
    ~LoggerService() override = default;

    void trace(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::trace, std::string{message});
    }

    void debug(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::debug, std::string{message});
    }

    void info(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::info, std::string{message});
    }

    void warn(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::warn, std::string{message});
    }

    void error(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::error, std::string{message});
    }

    void fatal(std::string_view message) override {
        integration::logger_adapter::log(integration::log_level::fatal, std::string{message});
    }

    [[nodiscard]] bool is_enabled(integration::log_level level) const noexcept override {
        return integration::logger_adapter::is_level_enabled(level);
    }
};

} // namespace pacs::di
```

#### 4.1.4 Logger Implementations Summary

| Implementation | Purpose | Thread Safety |
|----------------|---------|---------------|
| `ILogger` | Abstract interface | N/A |
| `NullLogger` | No-op for default/testing | Thread-safe (no state) |
| `LoggerService` | Production logging via logger_adapter | Thread-safe |

---

## 5. Test Support

### 5.1 DES-DI-004: Test Support

**Traces to:** SRS-MAINT-001 (Code Coverage)

**File:** `include/pacs/di/test_support.hpp`

The test_support header provides mock implementations and utilities for unit testing code that depends on PACS services.

#### 5.1.1 MockStorage

```cpp
namespace pacs::di::test {

class MockStorage : public IDicomStorage {
public:
    MockStorage() = default;
    ~MockStorage() override = default;

    // storage_interface implementation
    [[nodiscard]] storage::VoidResult store(const core::dicom_dataset& dataset) override;
    [[nodiscard]] storage::Result<core::dicom_dataset>
        retrieve(std::string_view sop_instance_uid) override;
    [[nodiscard]] storage::VoidResult remove(std::string_view sop_instance_uid) override;
    [[nodiscard]] bool exists(std::string_view sop_instance_uid) const override;
    [[nodiscard]] storage::Result<std::vector<core::dicom_dataset>>
        find(const core::dicom_dataset& query) override;
    [[nodiscard]] storage::storage_statistics get_statistics() const override;
    [[nodiscard]] storage::VoidResult verify_integrity() override;

    // Test utilities
    [[nodiscard]] std::size_t store_count() const;
    [[nodiscard]] std::vector<std::string> stored_uids() const;
    void clear();
    void on_store(std::function<void(const core::dicom_dataset&)> callback);

private:
    mutable std::mutex mutex_;
    std::map<std::string, core::dicom_dataset> datasets_;
    std::vector<std::string> store_calls_;
    std::function<void(const core::dicom_dataset&)> on_store_;
};

} // namespace pacs::di::test
```

#### 5.1.2 MockNetwork

```cpp
namespace pacs::di::test {

class MockNetwork : public IDicomNetwork {
public:
    MockNetwork() = default;
    ~MockNetwork() override = default;

    // IDicomNetwork implementation
    [[nodiscard]] std::unique_ptr<network::dicom_server> create_server(
        const network::server_config& config,
        const integration::tls_config& tls_cfg) override;

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const integration::connection_config& config) override;

    [[nodiscard]] integration::Result<integration::network_adapter::session_ptr>
        connect(const std::string& host, uint16_t port,
                std::chrono::milliseconds timeout) override;

    // Test utilities
    [[nodiscard]] std::size_t connection_attempt_count() const;
    [[nodiscard]] std::size_t server_creation_count() const;
    void set_fail_connections(bool fail);
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<std::pair<std::string, uint16_t>> connection_attempts_;
    std::vector<network::server_config> server_configs_;
    bool fail_connections_ = false;
};

} // namespace pacs::di::test
```

#### 5.1.3 TestContainerBuilder

```cpp
namespace pacs::di::test {

class TestContainerBuilder {
public:
    TestContainerBuilder() = default;

    // Configuration methods (fluent interface)
    TestContainerBuilder& with_storage(std::shared_ptr<MockStorage> storage);
    TestContainerBuilder& with_mock_storage();
    TestContainerBuilder& with_network(std::shared_ptr<MockNetwork> network);
    TestContainerBuilder& with_mock_network();

    // Build container
    [[nodiscard]] std::shared_ptr<kcenon::common::di::service_container> build();

    // Access mocks for assertions
    [[nodiscard]] std::shared_ptr<MockStorage> storage() const;
    [[nodiscard]] std::shared_ptr<MockNetwork> network() const;

private:
    std::shared_ptr<MockStorage> mock_storage_;
    std::shared_ptr<MockNetwork> mock_network_;
};

} // namespace pacs::di::test
```

#### 5.1.4 Convenience Functions

```cpp
namespace pacs::di::test {

// Create a test container with all mock services
[[nodiscard]] std::shared_ptr<kcenon::common::di::service_container>
    create_test_container();

// Register mock storage with a container
void register_mock_storage(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<MockStorage> mock);

// Register mock network with a container
void register_mock_network(
    kcenon::common::di::IServiceContainer& container,
    std::shared_ptr<MockNetwork> mock);

} // namespace pacs::di::test
```

#### 5.1.5 Usage Example

```cpp
#include <pacs/di/test_support.hpp>
#include <gtest/gtest.h>

TEST(StorageServiceTest, StoreDataset) {
    // Arrange
    auto mock_storage = std::make_shared<pacs::di::test::MockStorage>();
    auto container = pacs::di::test::TestContainerBuilder()
        .with_storage(mock_storage)
        .with_mock_network()
        .build();

    auto storage = container->resolve<pacs::di::IDicomStorage>().value();

    // Create test dataset
    pacs::core::dicom_dataset ds;
    ds.set_string(pacs::core::tags::sop_instance_uid, pacs::core::vr_type::UI, "1.2.3.4");

    // Act
    auto result = storage->store(ds);

    // Assert
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(mock_storage->store_count(), 1);
    EXPECT_TRUE(mock_storage->exists("1.2.3.4"));
}

TEST(NetworkServiceTest, ConnectionFailure) {
    // Arrange
    auto mock_network = std::make_shared<pacs::di::test::MockNetwork>();
    mock_network->set_fail_connections(true);

    auto container = pacs::di::test::TestContainerBuilder()
        .with_mock_storage()
        .with_network(mock_network)
        .build();

    auto network = container->resolve<pacs::di::IDicomNetwork>().value();

    // Act
    auto result = network->connect("localhost", 11112);

    // Assert
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(mock_network->connection_attempt_count(), 1);
}
```

---

## 6. Class Diagrams

### 6.1 Service Interfaces Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Service Interfaces                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────┐     ┌─────────────────────────┐               │
│  │ storage::storage_interface│     │ compression::compression_codec│          │
│  │     (IDicomStorage)     │     │     (IDicomCodec)       │               │
│  └─────────────────────────┘     └─────────────────────────┘               │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                        IDicomNetwork                                 │   │
│  │  <<interface>>                                                       │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │  + create_server(config, tls_cfg) : unique_ptr<dicom_server>        │   │
│  │  + connect(config) : Result<session_ptr>                            │   │
│  │  + connect(host, port, timeout) : Result<session_ptr>               │   │
│  └──────────────────────────────┬──────────────────────────────────────┘   │
│                                 │                                           │
│                    ┌────────────┴────────────┐                             │
│                    │   DicomNetworkService   │                             │
│                    │   <<implementation>>    │                             │
│                    ├─────────────────────────┤                             │
│                    │  Delegates to           │                             │
│                    │  network_adapter        │                             │
│                    └─────────────────────────┘                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Logger Interface Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            Logger Interfaces                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────────────┐ │
│  │                            ILogger                                     │ │
│  │  <<interface>>                                                         │ │
│  ├───────────────────────────────────────────────────────────────────────┤ │
│  │  + trace(message)                                                      │ │
│  │  + debug(message)                                                      │ │
│  │  + info(message)                                                       │ │
│  │  + warn(message)                                                       │ │
│  │  + error(message)                                                      │ │
│  │  + fatal(message)                                                      │ │
│  │  + is_enabled(level) : bool                                            │ │
│  │  + trace_fmt<Args...>(fmt, args...)                                    │ │
│  │  + debug_fmt<Args...>(fmt, args...)                                    │ │
│  │  + info_fmt<Args...>(fmt, args...)                                     │ │
│  │  + warn_fmt<Args...>(fmt, args...)                                     │ │
│  │  + error_fmt<Args...>(fmt, args...)                                    │ │
│  │  + fatal_fmt<Args...>(fmt, args...)                                    │ │
│  └──────────────────────────────┬────────────────────────────────────────┘ │
│                ┌────────────────┴────────────────┐                         │
│                │                                 │                         │
│  ┌─────────────┴───────────────┐   ┌────────────┴─────────────┐           │
│  │        NullLogger          │   │      LoggerService       │           │
│  │   <<implementation>>       │   │   <<implementation>>     │           │
│  ├─────────────────────────────┤   ├──────────────────────────┤           │
│  │  No-op implementation      │   │  Delegates to            │           │
│  │  for testing/default       │   │  logger_adapter          │           │
│  └─────────────────────────────┘   └──────────────────────────┘           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 6.3 Test Support Classes

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           Test Support Classes                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────────────────────┐   ┌───────────────────────────────┐     │
│  │         MockStorage           │   │         MockNetwork           │     │
│  │   <<test double>>             │   │   <<test double>>             │     │
│  ├───────────────────────────────┤   ├───────────────────────────────┤     │
│  │ - mutex_                      │   │ - mutex_                      │     │
│  │ - datasets_                   │   │ - connection_attempts_        │     │
│  │ - store_calls_                │   │ - server_configs_             │     │
│  │ - on_store_                   │   │ - fail_connections_           │     │
│  ├───────────────────────────────┤   ├───────────────────────────────┤     │
│  │ + store(dataset)              │   │ + create_server(config)       │     │
│  │ + retrieve(uid)               │   │ + connect(config)             │     │
│  │ + remove(uid)                 │   │ + connect(host, port)         │     │
│  │ + exists(uid)                 │   │ + connection_attempt_count()  │     │
│  │ + find(query)                 │   │ + server_creation_count()     │     │
│  │ + store_count()               │   │ + set_fail_connections()      │     │
│  │ + stored_uids()               │   │ + clear()                     │     │
│  │ + clear()                     │   │                               │     │
│  │ + on_store(callback)          │   │                               │     │
│  └───────────────────────────────┘   └───────────────────────────────┘     │
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────────────┐ │
│  │                      TestContainerBuilder                             │ │
│  │   <<builder pattern>>                                                 │ │
│  ├───────────────────────────────────────────────────────────────────────┤ │
│  │ - mock_storage_ : shared_ptr<MockStorage>                             │ │
│  │ - mock_network_ : shared_ptr<MockNetwork>                             │ │
│  ├───────────────────────────────────────────────────────────────────────┤ │
│  │ + with_storage(storage) : TestContainerBuilder&                       │ │
│  │ + with_mock_storage() : TestContainerBuilder&                         │ │
│  │ + with_network(network) : TestContainerBuilder&                       │ │
│  │ + with_mock_network() : TestContainerBuilder&                         │ │
│  │ + build() : shared_ptr<service_container>                             │ │
│  │ + storage() : shared_ptr<MockStorage>                                 │ │
│  │ + network() : shared_ptr<MockNetwork>                                 │ │
│  └───────────────────────────────────────────────────────────────────────┘ │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Sequence Diagrams

### 7.1 Service Registration

```
┌────────────┐    ┌──────────────────────┐    ┌────────────────────┐
│   Client   │    │  register_services() │    │  service_container │
└─────┬──────┘    └──────────┬───────────┘    └─────────┬──────────┘
      │                      │                          │
      │ create_container()   │                          │
      │─────────────────────>│                          │
      │                      │                          │
      │                      │ make_shared<container>   │
      │                      │─────────────────────────>│
      │                      │                          │
      │                      │ register_factory<IDicomStorage>
      │                      │─────────────────────────>│
      │                      │                          │
      │                      │ register_factory<IDicomNetwork>
      │                      │─────────────────────────>│
      │                      │                          │
      │                      │ register_factory<ILogger>│
      │                      │─────────────────────────>│
      │                      │                          │
      │<─────────────────────│ return container         │
      │                      │                          │
```

### 7.2 Service Resolution

```
┌────────────┐    ┌────────────────────┐    ┌─────────────────────────┐
│   Client   │    │  service_container │    │  Factory Function       │
└─────┬──────┘    └─────────┬──────────┘    └───────────┬─────────────┘
      │                     │                           │
      │ resolve<IDicomStorage>()                        │
      │────────────────────>│                           │
      │                     │                           │
      │                     │ (first call - singleton)  │
      │                     │ invoke factory            │
      │                     │──────────────────────────>│
      │                     │                           │
      │                     │   return file_storage     │
      │                     │<──────────────────────────│
      │                     │                           │
      │<────────────────────│ return cached instance    │
      │                     │                           │
      │ resolve<IDicomStorage>() (second call)          │
      │────────────────────>│                           │
      │                     │                           │
      │<────────────────────│ return cached instance    │
      │                     │ (no factory invocation)   │
```

### 7.3 Test Container Setup

```
┌────────────┐    ┌────────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  Test Case │    │ TestContainerBuilder│    │   MockStorage   │    │   MockNetwork   │
└─────┬──────┘    └─────────┬──────────┘    └───────┬─────────┘    └───────┬─────────┘
      │                     │                       │                      │
      │ TestContainerBuilder()                      │                      │
      │────────────────────>│                       │                      │
      │                     │                       │                      │
      │ with_mock_storage() │                       │                      │
      │────────────────────>│                       │                      │
      │                     │ make_shared<MockStorage>                     │
      │                     │──────────────────────>│                      │
      │                     │                       │                      │
      │<────────────────────│                       │                      │
      │                     │                       │                      │
      │ with_mock_network() │                       │                      │
      │────────────────────>│                       │                      │
      │                     │ make_shared<MockNetwork>                     │
      │                     │─────────────────────────────────────────────>│
      │                     │                       │                      │
      │<────────────────────│                       │                      │
      │                     │                       │                      │
      │ build()             │                       │                      │
      │────────────────────>│                       │                      │
      │                     │ register_instance(mock_storage)              │
      │                     │ register_instance(mock_network)              │
      │<────────────────────│ return container      │                      │
      │                     │                       │                      │
```

---

## 8. Traceability

### 8.1 Requirements to Design

| SRS ID | Design ID | Description |
|--------|-----------|-------------|
| SRS-MAINT-005 | DES-DI-001 | Service interfaces for modular design |
| SRS-MAINT-005 | DES-DI-002 | Service registration for dependency injection |
| SRS-INT-005 | DES-DI-003 | Logger interface for logger_system integration |
| SRS-MAINT-001 | DES-DI-004 | Test support utilities for code coverage |

### 8.2 Design to Implementation

| Design ID | Component | Header File | Source File |
|-----------|-----------|-------------|-------------|
| DES-DI-001 | service_interfaces | `include/pacs/di/service_interfaces.hpp` | (header-only) |
| DES-DI-002 | service_registration | `include/pacs/di/service_registration.hpp` | (header-only) |
| DES-DI-003 | ilogger | `include/pacs/di/ilogger.hpp` | (header-only) |
| DES-DI-004 | test_support | `include/pacs/di/test_support.hpp` | (header-only) |

### 8.3 Design to Test

| Design ID | Component | Test File |
|-----------|-----------|-----------|
| DES-DI-001 | service_interfaces | `tests/di/service_interfaces_test.cpp` |
| DES-DI-002 | service_registration | `tests/di/service_registration_test.cpp` |
| DES-DI-003 | ilogger | `tests/di/ilogger_test.cpp` |
| DES-DI-004 | test_support | `tests/di/test_support_test.cpp` |

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-01-04 | kcenon | Initial draft with placeholder content |
| 1.1.0 | 2026-01-05 | kcenon | Updated to match actual implementation; Added detailed class diagrams; Added sequence diagrams; Added complete code examples |

---

*Document Version: 1.1.0*
*Created: 2026-01-04*
*Updated: 2026-01-05*
*Author: kcenon@naver.com*
