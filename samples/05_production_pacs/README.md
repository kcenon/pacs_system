# Level 5: Production PACS

A production-grade PACS sample demonstrating enterprise features including TLS security, role-based access control (RBAC), automatic anonymization, REST API, and health monitoring.

## Learning Objectives

After completing this sample, you will understand:

1. **Configuration Management** - YAML-based server configuration
2. **TLS Security** - Secure DICOM communication
3. **Access Control** - Role-based permissions (RBAC)
4. **Anonymization** - De-identification profiles (HIPAA, GDPR)
5. **REST API** - Web-based PACS access
6. **Health Monitoring** - Service health checks and metrics
7. **Event Architecture** - Decoupled event handling

## Prerequisites

- Completed Level 4 (Mini PACS)
- Understanding of DICOM security concepts
- Familiarity with REST APIs (for REST API features)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Production PACS                          │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │    Config    │  │   Security   │  │  Monitoring  │      │
│  │   (YAML)     │  │   Manager    │  │   Health     │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                 │               │
│  ┌──────▼─────────────────▼─────────────────▼───────┐      │
│  │                    Mini PACS                      │      │
│  │         (All Level 4 Services)                   │      │
│  └──────────────────────┬───────────────────────────┘      │
│                         │                                   │
│  ┌──────────────────────▼───────────────────────────┐      │
│  │                REST API Server                    │      │
│  │  /api/v1/patients, /studies, /series, /health    │      │
│  └──────────────────────────────────────────────────┘      │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    Event Bus                        │   │
│  │  image_received, query_executed, association_*     │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Building

```bash
# From the repository root
cmake -B build -DPACS_BUILD_SAMPLES=ON
cmake --build build

# The executable will be in build/samples/
./build/samples/production_pacs
```

## Configuration

The Production PACS uses YAML configuration files for flexible deployment.

### Default Configuration

If no configuration file is provided, the server uses sensible defaults:

```bash
./build/samples/production_pacs
```

### Custom Configuration

Specify a configuration file as the first argument:

```bash
./build/samples/production_pacs /path/to/config.yaml
```

### Configuration File Format

```yaml
# Server configuration
server:
  ae_title: "PROD_PACS"
  port: 11112
  max_associations: 100
  idle_timeout_seconds: 300

  # TLS configuration
  tls:
    enabled: true
    certificate: "/etc/pacs/server.pem"
    private_key: "/etc/pacs/server.key"
    ca_certificate: "/etc/pacs/ca.pem"

# Storage configuration
storage:
  root_path: "./pacs_data"
  naming_scheme: "uid_hierarchical"
  duplicate_policy: "replace"
  database:
    path: "./pacs_data/index.db"

# Security configuration
security:
  access_control:
    enabled: true
    default_role: "viewer"
  allowed_ae_titles:
    - "CT_SCANNER_*"
    - "WORKSTATION_*"
  anonymization:
    auto_anonymize: false
    profile: "hipaa_safe_harbor"

# REST API configuration
rest_api:
  enabled: true
  port: 8080
  cors_enabled: true

# Monitoring configuration
monitoring:
  health_check_interval_seconds: 30
  metrics_enabled: true

# Logging configuration
logging:
  level: "info"
  audit_log_path: "/var/log/pacs/audit.log"
```

See `config/pacs_config.example.yaml` for a complete annotated example.

## Features

### 1. TLS Security

Enable secure DICOM connections with TLS 1.2+:

```yaml
server:
  tls:
    enabled: true
    certificate: "/path/to/cert.pem"
    private_key: "/path/to/key.pem"
    ca_certificate: "/path/to/ca.pem"
    min_version: "1.2"
```

Test with TLS:

```bash
echoscu --tls --add-cert-file ca.pem -aec PROD_PACS localhost 11112
```

### 2. Role-Based Access Control

Control which AE titles can perform which operations:

```yaml
security:
  access_control:
    enabled: true
    default_role: "viewer"
  allowed_ae_titles:
    - "CT_SCANNER_*"    # Wildcards supported
    - "WORKSTATION_1"
```

Roles:
- `viewer`: Read-only (C-FIND, C-GET)
- `technologist`: Can store images (C-STORE)
- `administrator`: Full access

### 3. Automatic Anonymization

Automatically de-identify incoming images:

```yaml
security:
  anonymization:
    auto_anonymize: true
    profile: "hipaa_safe_harbor"
```

Available profiles:
- `basic`: Basic de-identification
- `hipaa_safe_harbor`: HIPAA Safe Harbor compliance
- `retain_longitudinal`: Preserve UIDs for longitudinal studies
- `retain_device_identity`: Keep device information
- `gdpr`: GDPR compliant de-identification

### 4. REST API

Access PACS data via HTTP:

```bash
# Health status
curl http://localhost:8080/api/v1/system/status

# Metrics
curl http://localhost:8080/api/v1/system/metrics

# List patients (requires authentication)
curl http://localhost:8080/api/v1/patients
```

### 5. Health Monitoring

Kubernetes-compatible health endpoints:

```bash
# Liveness probe
curl http://localhost:8080/api/v1/system/status

# Response:
{
  "status": "healthy",
  "uptime_seconds": 3600,
  "version": "0.1.0",
  "components": {
    "database": "healthy",
    "storage": "healthy"
  }
}
```

### 6. Event-Driven Architecture

Subscribe to system events:

```cpp
production_pacs pacs(config);

pacs.on_image_received([](const events::image_received_event& evt) {
    std::cout << "Image received: " << evt.sop_instance_uid << "\n";
});

pacs.on_access_denied([](const events::access_denied_event& evt) {
    log_security_event(evt);
});

pacs.start();
```

## Test Commands

### DICOM Connectivity

```bash
# Basic connectivity test
echoscu -aec PROD_PACS localhost 11112

# Store an image
storescu -aec PROD_PACS localhost 11112 image.dcm

# Query patients
findscu -aec PROD_PACS -P \
  -k QueryRetrieveLevel=PATIENT \
  -k PatientName="*" localhost 11112

# Query studies
findscu -aec PROD_PACS -S \
  -k QueryRetrieveLevel=STUDY \
  -k StudyDate="" localhost 11112
```

### TLS Connections

```bash
# Generate test certificates
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes

# Test with TLS
echoscu --tls --add-cert-file cert.pem -aec PROD_PACS localhost 11112
```

### REST API

```bash
# System health
curl -s http://localhost:8080/api/v1/system/status | jq .

# Metrics (Prometheus format)
curl http://localhost:8080/api/v1/system/metrics

# Query patients (if implemented)
curl -s http://localhost:8080/api/v1/patients | jq .
```

## File Structure

```
samples/05_production_pacs/
├── CMakeLists.txt           # Build configuration
├── config/
│   ├── pacs_config.yaml     # Default configuration
│   └── pacs_config.example.yaml  # Annotated example
├── config_loader.hpp        # YAML parser header
├── config_loader.cpp        # YAML parser implementation
├── production_pacs.hpp      # Main class header
├── production_pacs.cpp      # Main class implementation
├── main.cpp                 # Application entry point
└── README.md                # This file
```

## Concepts Covered

### Configuration Management

- YAML parsing without external dependencies
- Hierarchical configuration structure
- Default values and validation
- Environment-specific configurations

### Security

- TLS 1.2+ for DICOM connections
- Certificate management
- Role-based access control (RBAC)
- AE title whitelisting with wildcards
- Audit logging

### Anonymization

- DICOM PS3.15 Annex E profiles
- HIPAA Safe Harbor compliance
- GDPR requirements
- UID mapping for longitudinal studies

### Monitoring

- Health check endpoints
- Prometheus-style metrics
- Resource utilization tracking
- Association statistics

### Event Architecture

- Loose coupling through events
- Multiple handler support
- Thread-safe dispatch
- Extensibility for custom integrations

## Best Practices Demonstrated

1. **Separation of Concerns** - Configuration, security, and business logic are separate
2. **Defense in Depth** - Multiple security layers (TLS, RBAC, anonymization)
3. **Observability** - Health checks, metrics, and event logging
4. **Configuration over Code** - Behavior changes without recompilation
5. **Graceful Degradation** - Works with default config if file not found

## Next Steps

After completing this sample, consider:

1. **Production Deployment** - Containerization with Docker/Kubernetes
2. **High Availability** - Load balancing and failover
3. **Advanced Security** - OAuth2/OIDC integration
4. **Custom Integrations** - Connecting to RIS, HIS, or other systems

## References

- [DICOM PS3.15 - Security and System Management Profiles](https://dicom.nema.org/medical/dicom/current/output/chtml/part15/PS3.15.html)
- [DICOM PS3.4 - Service Class Specifications](https://dicom.nema.org/medical/dicom/current/output/chtml/part04/PS3.4.html)
- [HIPAA Technical Safeguards](https://www.hhs.gov/hipaa/for-professionals/security/laws-regulations/index.html)
- [GDPR Article 17 - Right to Erasure](https://gdpr-info.eu/art-17-gdpr/)
