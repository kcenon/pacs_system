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
