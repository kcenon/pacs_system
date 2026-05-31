# Contributing to PACS System

Thank you for considering contributing to PACS System\! This document provides guidelines and instructions for contributors.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Standards](#code-standards)
- [Testing](#testing)
- [Pull Requests](#pull-requests)

## Getting Started

### Prerequisites

- C++20 compatible compiler (GCC 13+, Clang 16+, MSVC 2022+)
- CMake 3.20 or higher
- Git
- vcpkg (recommended)

### Building from Source

```bash
git clone https://github.com/kcenon/pacs_system.git
cd pacs_system
cmake --preset default
cmake --build build
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

### TLS Integration Tests

TLS test certificates are not stored in version control. Generate them before running TLS integration tests:

```bash
./tools/integration_tests/test_data/certs/generate_test_certs.sh
```

## Development Workflow

1. Fork the repository
2. Branch from `develop` (the integration branch) — `git checkout develop && git pull && git checkout -b feature/your-feature`
3. Make your changes
4. Run tests and ensure they pass
5. Commit your changes (see commit message guidelines below)
6. Push to your fork (`git push origin feature/your-feature`)
7. Open a Pull Request targeting `develop`. Releases to `main` are cut by maintainers via a separate `develop` -> `main` PR.

### Commit Message Guidelines

Follow the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `refactor`: Code refactoring
- `test`: Adding or modifying tests
- `perf`: Performance improvement
- `chore`: Maintenance tasks

## Code Standards

### C++ Style

- Use C++20 features when beneficial
- Follow existing code style (clang-format configuration provided)
- Prefer RAII for resource management
- Use `auto` for obvious types
- Avoid raw pointers; use smart pointers
- Prefer standard library over custom implementations

### Naming Conventions

- **Classes/Structs**: `snake_case`
- **Functions/Methods**: `snake_case`
- **Variables**: `snake_case`
- **Constants**: `UPPER_SNAKE_CASE`
- **Template Parameters**: `PascalCase`
- **Namespaces**: `kcenon::pacs`

### Documentation

Use Doxygen-style comments:

```cpp
/**
 * @brief Brief description
 * @param param Parameter description
 * @return Return value description
 * @thread_safety Thread-safe / Not thread-safe
 */
auto function(int param) -> int;
```

## Testing

### Test Requirements

All code contributions must include tests:

- **Unit tests**: Test individual components
- **Integration tests**: Test component interactions
- **Platform tests**: Test platform-specific code paths

### Writing Tests

Use Catch2 v3 (the project-wide test framework — note this differs from the kcenon
ecosystem default of Google Test; see [`docs/ECOSYSTEM.md`](docs/ECOSYSTEM.md) and
[#1141](https://github.com/kcenon/pacs_system/issues/1141)):

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Component: basic functionality", "[component]") {
    SECTION("default state") {
        // Test implementation
    }
}
```

### Test Coverage

Aim for:
- New code: > 80% coverage
- Critical paths: 100% coverage
- Error handling: Test failure scenarios

### Release Gates

Before a release, the four medical-domain gates (DICOM conformance,
TLS/ATNA audit logging, anonymization, storage/index migration) must be
demonstrably green. Each gate has an exact command or workflow name and is
followable without reading test source code. See
[`docs/RELEASE_GATES.md`](docs/RELEASE_GATES.md) for the gate matrix and the
pre-release checklist. The cheapest pre-flight check needs no build:

```bash
python3 tests/storage/check_storage_boundary.py "$(pwd)"
```

## Pull Requests

### PR Checklist

Before submitting a PR, ensure:

- [ ] Code compiles without warnings
- [ ] All tests pass
- [ ] New tests added for new functionality
- [ ] Documentation updated
- [ ] Code follows project style
- [ ] Commit messages follow conventional format
- [ ] No merge conflicts with main branch

### Review Process

1. Automated checks run (build, tests, linting)
2. Maintainer reviews code
3. Address review comments
4. Approved PR is merged (squash merge preferred)

## Getting Help

- Check [existing issues](https://github.com/kcenon/pacs_system/issues)
- Open a [discussion](https://github.com/kcenon/pacs_system/discussions) for questions

## License

By contributing, you agree that your contributions will be licensed under the BSD 3-Clause License.
