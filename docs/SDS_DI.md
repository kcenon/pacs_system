# SDS - Dependency Injection Module

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2026-01-04
> **Status:** Initial

---

## Document Information

| Item | Description |
|------|-------------|
| Document ID | PACS-SDS-DI-001 |
| Project | PACS System |
| Author | kcenon@naver.com |
| Related Issues | [#479](https://github.com/kcenon/pacs_system/issues/479), [#468](https://github.com/kcenon/pacs_system/issues/468) |

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Service Interfaces](#2-service-interfaces)
- [3. Service Registration](#3-service-registration)
- [4. Logger Interface](#4-logger-interface)
- [5. Test Support](#5-test-support)
- [6. Traceability](#6-traceability)

---

## 1. Overview

### 1.1 Purpose

This document specifies the Dependency Injection module. The module provides:

- **Service Interfaces**: Abstract interfaces for dependency inversion
- **Service Registration**: Centralized service factory and registration
- **Logger Abstraction**: Injectable logger interface
- **Test Support**: Mock injection utilities for unit testing

### 1.2 Scope

| Component | Files | Design IDs |
|-----------|-------|------------|
| Service Interfaces | 1 header | DES-DI-001 |
| Service Registration | 1 header | DES-DI-002 |
| Logger Interface | 1 header | DES-DI-003 |
| Test Support | 1 header | DES-DI-004 |

---

## 2. Service Interfaces

### 2.1 DES-DI-001: Service Interfaces

**Traces to:** SRS-MAINT-005 (Modular Design)

**File:** `include/pacs/di/service_interfaces.hpp`

```cpp
namespace pacs::di {

// Storage service alias
using storage_service = std::shared_ptr<storage::storage_interface>;

// Database service alias
using database_service = std::shared_ptr<storage::index_database>;

// Logger service alias
using logger_service = std::shared_ptr<ilogger>;

// Security service alias
using security_service = std::shared_ptr<security::access_control_manager>;

// Service locator pattern
class service_locator {
public:
    template<typename T>
    static void register_service(std::shared_ptr<T> service);

    template<typename T>
    [[nodiscard]] static auto get_service() -> std::shared_ptr<T>;

    static void clear();

private:
    static inline std::unordered_map<std::type_index, std::any> services_;
    static inline std::mutex mutex_;
};

} // namespace pacs::di
```

**Design Rationale:**
- Type-safe service aliases for common dependencies
- Service locator for global service access (use sparingly)
- Type-index based storage for compile-time type safety

---

## 3. Service Registration

### 3.1 DES-DI-002: Service Registration

**Traces to:** SRS-MAINT-005 (Modular Design)

**File:** `include/pacs/di/service_registration.hpp`

```cpp
namespace pacs::di {

// Factory function type
template<typename T>
using service_factory = std::function<std::shared_ptr<T>()>;

class service_registry {
public:
    // Register factory
    template<typename Interface, typename Implementation>
    void register_type();

    template<typename Interface>
    void register_factory(service_factory<Interface> factory);

    // Register instance
    template<typename Interface>
    void register_instance(std::shared_ptr<Interface> instance);

    // Resolve
    template<typename Interface>
    [[nodiscard]] auto resolve() -> std::shared_ptr<Interface>;

    template<typename Interface>
    [[nodiscard]] auto try_resolve() -> std::optional<std::shared_ptr<Interface>>;

    // Scope management
    void begin_scope(const std::string& name);
    void end_scope();

private:
    struct registration {
        std::any factory_or_instance;
        bool is_singleton{false};
    };

    std::unordered_map<std::type_index, registration> registrations_;
    std::vector<std::string> scope_stack_;
    mutable std::mutex mutex_;
};

// Global registry access
service_registry& get_registry();

} // namespace pacs::di
```

---

## 4. Logger Interface

### 4.1 DES-DI-003: Logger Interface

**Traces to:** SRS-INT-005 (logger_system Integration)

**File:** `include/pacs/di/ilogger.hpp`

```cpp
namespace pacs::di {

enum class log_level {
    trace,
    debug,
    info,
    warn,
    error,
    fatal
};

class ilogger {
public:
    virtual ~ilogger() = default;

    virtual void log(log_level level,
                     std::string_view message,
                     const std::source_location& loc =
                         std::source_location::current()) = 0;

    virtual void set_level(log_level level) = 0;
    [[nodiscard]] virtual auto get_level() const -> log_level = 0;

    // Convenience methods
    void trace(std::string_view msg);
    void debug(std::string_view msg);
    void info(std::string_view msg);
    void warn(std::string_view msg);
    void error(std::string_view msg);
    void fatal(std::string_view msg);
};

// Default implementation using logger_system
class logger_system_adapter : public ilogger {
public:
    void log(log_level level,
             std::string_view message,
             const std::source_location& loc) override;

    void set_level(log_level level) override;
    [[nodiscard]] auto get_level() const -> log_level override;

private:
    log_level level_{log_level::info};
};

} // namespace pacs::di
```

---

## 5. Test Support

### 5.1 DES-DI-004: Test Support

**Traces to:** SRS-MAINT-001 (Code Coverage)

**File:** `include/pacs/di/test_support.hpp`

```cpp
namespace pacs::di::testing {

// Mock logger for unit tests
class mock_logger : public ilogger {
public:
    void log(log_level level,
             std::string_view message,
             const std::source_location& loc) override;

    void set_level(log_level level) override;
    [[nodiscard]] auto get_level() const -> log_level override;

    // Test assertions
    [[nodiscard]] auto log_count(log_level level) const -> std::size_t;
    [[nodiscard]] auto last_message() const -> std::string_view;
    void clear();

private:
    std::vector<std::pair<log_level, std::string>> logs_;
    log_level level_{log_level::trace};
};

// RAII service override for tests
template<typename Interface>
class scoped_service_override {
public:
    scoped_service_override(std::shared_ptr<Interface> mock);
    ~scoped_service_override();

private:
    std::optional<std::shared_ptr<Interface>> original_;
};

// Test fixture helper
class test_services {
public:
    static void setup_mocks();
    static void teardown();

    static std::shared_ptr<mock_logger> logger();
};

} // namespace pacs::di::testing
```

**Testing Pattern:**
```cpp
// Example unit test
TEST(StorageTest, StoreValidDataset) {
    // Arrange
    pacs::di::testing::scoped_service_override<ilogger> log_override(
        std::make_shared<mock_logger>());

    auto storage = create_storage_with_mock_db();

    // Act
    auto result = storage->store(test_dataset);

    // Assert
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(pacs::di::testing::test_services::logger()->log_count(
        log_level::info), 1);
}
```

---

## 6. Traceability

### 6.1 Requirements to Design

| SRS ID | Design ID(s) | Implementation |
|--------|--------------|----------------|
| SRS-MAINT-005 | DES-DI-001, DES-DI-002 | Service interfaces and registration |
| SRS-INT-005 | DES-DI-003 | Logger interface abstraction |
| SRS-MAINT-001 | DES-DI-004 | Test support utilities |

---

*Document Version: 1.0.0*
*Created: 2026-01-04*
*Author: kcenon@naver.com*
