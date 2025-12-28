# Thread Pool Migration Guide

This guide explains how to migrate from the deprecated `thread_adapter` singleton pattern to the new `thread_pool_adapter` with dependency injection.

## Overview

The `thread_adapter` class used a singleton pattern with static methods, making it difficult to:
- Mock the thread pool for unit testing
- Configure different thread pool instances per component
- Test error conditions in isolation

The new `thread_pool_interface` and `thread_pool_adapter` classes enable proper dependency injection.

## Migration Steps

### Step 1: Update Includes

**Before:**
```cpp
#include <pacs/integration/thread_adapter.hpp>
```

**After:**
```cpp
#include <pacs/integration/thread_pool_adapter.hpp>
// or for interface only:
#include <pacs/integration/thread_pool_interface.hpp>
```

### Step 2: Create Thread Pool Instance

**Before (singleton):**
```cpp
pacs::integration::thread_pool_config config;
config.min_threads = 4;
config.max_threads = 16;
pacs::integration::thread_adapter::configure(config);
pacs::integration::thread_adapter::start();
```

**After (injectable instance):**
```cpp
pacs::integration::thread_pool_config config;
config.min_threads = 4;
config.max_threads = 16;

auto pool = std::make_shared<pacs::integration::thread_pool_adapter>(config);
pool->start();
```

### Step 3: Update Component Constructors

**Before (using singleton):**
```cpp
class my_service {
public:
    void process_async() {
        thread_adapter::submit([this] { do_work(); });
    }
};
```

**After (dependency injection):**
```cpp
class my_service {
public:
    explicit my_service(std::shared_ptr<thread_pool_interface> pool)
        : pool_(std::move(pool)) {}

    void process_async() {
        pool_->submit([this] { do_work(); });
    }

private:
    std::shared_ptr<thread_pool_interface> pool_;
};
```

### Step 4: Update Task Submission

**Before:**
```cpp
auto future = thread_adapter::submit([]() { return 42; });
thread_adapter::submit_fire_and_forget([]() { log("done"); });
thread_adapter::submit_with_priority(job_priority::high, []() { urgent_work(); });
```

**After:**
```cpp
auto future = pool->submit([]() { /* work */ });
pool->submit_fire_and_forget([]() { log("done"); });
pool->submit_with_priority(job_priority::high, []() { urgent_work(); });
```

### Step 5: Update Shutdown

**Before:**
```cpp
thread_adapter::shutdown(true);
```

**After:**
```cpp
pool->shutdown(true);
// Or let RAII handle it when pool goes out of scope
```

## Testing with Mock

The new pattern enables easy mocking for unit tests:

```cpp
#include "tests/mocks/mock_thread_pool.hpp"

TEST_CASE("my_service processes data") {
    // Create mock pool
    auto mock_pool = std::make_shared<pacs::integration::testing::mock_thread_pool>();
    mock_pool->start();

    // Inject into service
    my_service service(mock_pool);

    // Test the service
    service.process_async();

    // Verify mock interactions
    REQUIRE(mock_pool->get_submitted_task_count() == 1);
}
```

### Mock Configuration Options

```cpp
// Execute tasks immediately (default)
mock_pool->set_mode(mock_thread_pool::execution_mode::synchronous);

// Record tasks but don't execute
mock_pool->set_mode(mock_thread_pool::execution_mode::recording);

// Simulate failures
mock_pool->set_should_fail(true);
```

## Backward Compatibility

The old `thread_adapter` class is still available but marked as deprecated:

```cpp
// This still works but generates deprecation warnings
thread_adapter::configure(config);  // warning: deprecated
thread_adapter::start();            // warning: deprecated
```

To suppress warnings in legacy code during migration:

```cpp
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// Legacy code using thread_adapter
#pragma clang diagnostic pop
```

## Related Issues

- #405 - Replace Singleton Pattern with Dependency Injection in thread_adapter
- #409 - Define abstract thread_pool_interface
- #410 - Implement thread_pool_adapter class
- #411 - Create mock thread pool for testing
- #412 - Update existing tests to use DI pattern
- #413 - Deprecate static API and maintain backward compatibility
- #414 - Update documentation and migration guide

## API Reference

### thread_pool_interface

Abstract interface for thread pool operations:

```cpp
class thread_pool_interface {
public:
    virtual auto start() -> bool = 0;
    virtual auto is_running() const noexcept -> bool = 0;
    virtual void shutdown(bool wait_for_completion = true) = 0;

    virtual auto submit(std::function<void()> task) -> std::future<void> = 0;
    virtual auto submit_with_priority(job_priority priority,
                                      std::function<void()> task) -> std::future<void> = 0;
    virtual void submit_fire_and_forget(std::function<void()> task) = 0;

    virtual auto get_thread_count() const -> std::size_t = 0;
    virtual auto get_pending_task_count() const -> std::size_t = 0;
    virtual auto get_idle_worker_count() const -> std::size_t = 0;
};
```

### thread_pool_adapter

Concrete implementation using kcenon::thread::thread_pool:

```cpp
class thread_pool_adapter final : public thread_pool_interface {
public:
    explicit thread_pool_adapter(const thread_pool_config& config);
    explicit thread_pool_adapter(std::shared_ptr<kcenon::thread::thread_pool> pool);

    // thread_pool_interface implementation...

    // Additional methods
    auto get_underlying_pool() const -> std::shared_ptr<kcenon::thread::thread_pool>;
    auto get_config() const noexcept -> const thread_pool_config&;
};
```
