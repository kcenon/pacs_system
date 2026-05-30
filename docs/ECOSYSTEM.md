# Ecosystem Integration

**pacs_system** is the top-level DICOM medical imaging application in the kcenon ecosystem.

## Dependencies

| System | Relationship |
|--------|-------------|
| common_system | Core interfaces and patterns |
| logger_system | Async logging and OpenTelemetry integration |
| container_system | Type-safe containers for DICOM data handling |
| monitoring_system | Metrics, tracing, and alerting |
| database_system | Storage backend for medical imaging data |
| network_system | DICOM networking and communication protocols |

## Dependent Systems

No dependent systems. **pacs_system** is a top-level application in the ecosystem.

## All Systems

| System | Description | Docs |
|--------|------------|------|
| common_system | Foundation | [Docs](https://kcenon.github.io/common_system/) |
| thread_system | Thread pool, DAG scheduling | [Docs](https://kcenon.github.io/thread_system/) |
| logger_system | Async logging, OpenTelemetry | [Docs](https://kcenon.github.io/logger_system/) |
| container_system | Type-safe containers, SIMD | [Docs](https://kcenon.github.io/container_system/) |
| monitoring_system | Metrics, tracing, alerts | [Docs](https://kcenon.github.io/monitoring_system/) |
| database_system | Multi-backend DB | [Docs](https://kcenon.github.io/database_system/) |
| network_system | TCP/UDP/WebSocket/HTTP2/QUIC | [Docs](https://kcenon.github.io/network_system/) |
| pacs_system | DICOM medical imaging | [Docs](https://kcenon.github.io/pacs_system/) |

## Per-system conventions

`pacs_system` deliberately deviates from a small number of ecosystem-wide conventions. Each
deviation is recorded here so that downstream tooling and shared infrastructure can plan
around it explicitly.

| Convention | Ecosystem default | `pacs_system` choice | Rationale | Tracking |
|------------|-------------------|----------------------|-----------|----------|
| Test framework | GoogleTest (per the layout standard documented in `common_system/docs/kcenon-system-layout.md`) | Catch2 v3.4.0 | The pre-existing test corpus is built on Catch2 idioms (`TEST_CASE`, `SECTION`); migrating is high-cost and out of scope for the directory-structure standardization work in Epic [#1137](https://github.com/kcenon/pacs_system/issues/1137). Revisit when the corpus is rewritten or when shared GoogleTest fixture libraries become a concrete dependency. | [#1141](https://github.com/kcenon/pacs_system/issues/1141), master EPIC [kcenon/common_system#657](https://github.com/kcenon/common_system/issues/657) |
| CMake module set | Aligned with the canonical template at `common_system/cmake/template` | Adds `pacs_system-config.cmake.in` and `summary.cmake` on top of the template | pacs-specific install/export and build summary requirements; documented in `cmake/DEVIATIONS.md` and version-pinned in `cmake/VERSION`. | [#1140](https://github.com/kcenon/pacs_system/issues/1140) |

Downstream tooling (e.g. shared GoogleTest fixture libraries, ecosystem-wide test runners
that expect `gtest_discover_tests`) should treat `pacs_system` as an explicit exception
rather than a candidate for automatic inclusion.
