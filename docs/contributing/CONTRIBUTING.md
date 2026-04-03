---
doc_id: "PAC-PROJ-007"
doc_title: "Contributing to PACS System"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "PROJ"
---

# Contributing to PACS System

> **SSOT**: This document is the single source of truth for **Contributing to PACS System**.

**Version:** 0.1.0.0
**Last Updated:** 2026-03-18

Thank you for your interest in contributing to PACS System! This guide will help you get started.

---

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Code Style](#code-style)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)

---

## Code of Conduct

This project adheres to a code of conduct. By participating, you are expected to:

- Be respectful and inclusive
- Welcome newcomers and help them learn
- Focus on what is best for the community
- Show empathy towards other community members

---

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- C++20 compatible compiler
  - **Linux**: GCC 13+ or Clang 16+
  - **macOS**: Xcode 15+ or Apple Clang 16+
  - **Windows**: Visual Studio 2022+
- CMake 3.20 or higher
- Git
- vcpkg (for dependency management)
- **Required ecosystem dependencies**:
  - common_system (Result<T> pattern, interfaces)
  - container_system (DICOM serialization)
  - network_system (DICOM PDU/Association)

### First-Time Contributors

New to open source? Start here:

1. **Browse issues labeled `good first issue`**:
   - [Good First Issues](https://github.com/kcenon/pacs_system/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)

2. **Read the documentation**:
   - [Architecture Guide](../ARCHITECTURE.md)
   - [API Reference](../API_REFERENCE.md)
   - [DICOM Conformance Statement](../DICOM_CONFORMANCE_STATEMENT.md)

---

## Development Setup

### 1. Fork and Clone

```bash
# Fork the repository on GitHub, then clone your fork
git clone https://github.com/YOUR_USERNAME/pacs_system.git
cd pacs_system
```

### 2. Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DPACS_BUILD_TESTS=ON
cmake --build build
```

### 3. Run Tests

```bash
cd build
ctest --output-on-failure
```

---

## Making Changes

### Branch Naming

Create a feature branch from `main`:

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b fix/issue-description
```

### Commit Messages

- Use conventional commit format: `type(scope): description`
- Types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`
- Write in English
- Keep the subject line under 72 characters

---

## Code Style

- Follow existing code conventions in the repository
- Use Doxygen comments for public API (`/** @brief ... */`)
- English for all code, comments, and documentation
- C++20 features preferred where appropriate
- Use `pacs::` namespace for all public API

### Formatting

The project uses clang-format. Format your code before submitting:

```bash
clang-format -i src/**/*.cpp include/**/*.h
```

---

## Testing

### Writing Tests

- Use Google Test framework
- Place unit tests alongside source in `tests/` directories
- Name test files with `test_` prefix

### Running Tests

```bash
# Build with tests enabled
cmake -B build -DPACS_BUILD_TESTS=ON
cmake --build build

# Run all tests
cd build && ctest --output-on-failure

# Run specific test
./build/bin/pacs_tests --gtest_filter=DicomElement*
```

---

## Submitting Changes

1. Ensure all tests pass
2. Run static analysis (clang-tidy) if available
3. Push your branch to your fork
4. Create a Pull Request against `main`
5. Fill in the PR template with the 5W1H framework
6. Wait for CI to pass and code review

### Pull Request Guidelines

- Keep PRs focused and small (< 500 lines preferred)
- Link related issues using `Closes #N` or `Relates to #N`
- Include test coverage for new functionality
- Update documentation if changing public API

---

## Reporting Issues

Use the [GitHub Issues](https://github.com/kcenon/pacs_system/issues) with the 5W1H framework:

- **What**: Clear description of the issue
- **Why**: Impact and context
- **Who**: Affected users/components
- **When**: When the issue occurs
- **Where**: Affected files/components
- **How**: Steps to reproduce
