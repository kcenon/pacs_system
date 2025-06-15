# PACS System API Versioning Guide

## Overview

The PACS System implements semantic versioning for its APIs to ensure compatibility and smooth upgrades. This guide explains the versioning strategy and how to use it effectively.

## Version Format

PACS System follows Semantic Versioning 2.0.0:

```
MAJOR.MINOR.PATCH
```

- **MAJOR**: Incompatible API changes
- **MINOR**: Backwards-compatible functionality additions  
- **PATCH**: Backwards-compatible bug fixes

Current Version: **1.0.0**

## API Version Checking

### Compile-Time Checking

Use the provided macro to ensure compatibility at compile time:

```cpp
// Require at least version 1.0
PACS_API_VERSION_CHECK(1, 0);
```

### Runtime Checking

Check version compatibility at runtime:

```cpp
#include "common/version/api_version.h"

// Check if compatible with version 1.0
if (pacs::ApiVersion::isCompatible(1, 0)) {
    // Use version 1.0 features
}

// Get current version
auto [major, minor, patch] = pacs::ApiVersion::getVersion();
std::cout << "PACS API Version: " << major << "." << minor << "." << patch << std::endl;
```

## Feature Detection

Check for specific capabilities:

```cpp
// Check if encryption is available
if (pacs::ApiCompatibility::hasFeature(pacs::ApiVersion::CAP_ENCRYPTION)) {
    // Use encryption features
}

// Get all capabilities
uint32_t caps = pacs::ApiVersion::getCapabilities();
```

### Available Capabilities

| Capability | Flag | Since Version |
|------------|------|---------------|
| Storage SCP | `CAP_STORAGE_SCP` | 1.0.0 |
| Query/Retrieve | `CAP_QUERY_RETRIEVE` | 1.0.0 |
| Worklist | `CAP_WORKLIST` | 1.0.0 |
| MPPS | `CAP_MPPS` | 1.0.0 |
| Encryption | `CAP_ENCRYPTION` | 1.0.0 |
| Audit Logging | `CAP_AUDIT_LOGGING` | 1.0.0 |
| Connection Pooling | `CAP_CONNECTION_POOLING` | 1.0.0 |
| PostgreSQL | `CAP_POSTGRESQL` | 1.0.0 |
| Circuit Breaker | `CAP_CIRCUIT_BREAKER` | 1.0.0 |
| Monitoring | `CAP_MONITORING` | 1.1.0 (planned) |

## REST API Versioning

### URL-based Versioning

REST endpoints include version in the URL:

```
/api/v1/studies
/api/v1/patients
/api/v2/studies  (future)
```

### Header-based Version Negotiation

Clients can specify desired API version:

```http
GET /api/studies
X-API-Version: 1
```

Response includes server version:

```http
HTTP/1.1 200 OK
X-API-Version: 1.0.0
```

### Version Discovery

Query available API versions:

```http
GET /api/version

Response:
{
  "version": "1.0.0",
  "major": 1,
  "minor": 0,
  "patch": 0,
  "build_date": "Jan 1 2024",
  "build_time": "12:00:00",
  "capabilities": 255
}
```

## Deprecation Policy

### Marking Deprecated APIs

Use the deprecation macro:

```cpp
class PACS_DEPRECATED("Use NewClass instead") OldClass {
    // ...
};

PACS_DEPRECATED("Use newMethod() instead")
void oldMethod();
```

### Deprecation Timeline

1. **Deprecation Notice**: Feature marked deprecated in MINOR release
2. **Grace Period**: Maintained for at least 2 MINOR releases
3. **Removal**: Can be removed in next MAJOR release

### Example Timeline

- v1.0.0: Feature X introduced
- v1.2.0: Feature X deprecated, Feature Y introduced
- v1.3.0: Feature X still available (deprecated)
- v1.4.0: Feature X still available (deprecated)
- v2.0.0: Feature X removed

## Migration Guide

### From 0.x to 1.0

Version 1.0 is the first stable release. Key changes:

1. **Security**: No more hardcoded credentials
   ```cpp
   // Before: Hardcoded admin/admin
   // After: Secure password generation
   SecurityManager mgr;
   auto password = mgr.generateSecurePassword();
   ```

2. **Database**: PostgreSQL support added
   ```cpp
   // SQLite (still supported)
   auto db = DatabaseFactory::create("sqlite", sqliteConfig);
   
   // PostgreSQL (new)
   auto db = DatabaseFactory::create("postgresql", pgConfig);
   ```

3. **Networking**: Connection pooling
   ```cpp
   // Before: Direct connections
   auto conn = createConnection(params);
   
   // After: Pooled connections
   auto pool = poolManager.getPool("REMOTE", params, poolConfig);
   pool->executeWithConnection([](auto* conn) { /* use */ });
   ```

## Client SDK Compatibility

### C++ SDK

```cpp
// Check minimum version
#if PACS_API_MAJOR < 1
    #error "PACS API version 1.0 or higher required"
#endif

// Runtime check
if (!pacs::ApiVersion::isCompatible(1, 0)) {
    throw std::runtime_error("Incompatible PACS API version");
}
```

### REST API Clients

```javascript
// JavaScript example
const response = await fetch('/api/version');
const version = await response.json();

if (version.major < 1) {
    throw new Error('PACS API version 1.0 or higher required');
}
```

## Version Negotiation Best Practices

1. **Always check version** on client initialization
2. **Handle version mismatches** gracefully
3. **Use feature detection** instead of version checking when possible
4. **Include version in logs** for debugging
5. **Test with multiple versions** during development

## Future Compatibility

### Planned Features

| Version | Features |
|---------|----------|
| 1.1.0 | Real-time monitoring, WebSocket support |
| 1.2.0 | Advanced analytics, ML integration |
| 2.0.0 | Breaking changes, new architecture |

### Maintaining Compatibility

When developing against PACS API:

1. **Use stable APIs** whenever possible
2. **Avoid internal APIs** (anything in `internal/` namespace)
3. **Check deprecation warnings** in build output
4. **Test against minimum supported version**
5. **Plan for migration** when using deprecated features

## API Stability Guarantees

### Stable APIs (1.0+)

- No breaking changes in MINOR/PATCH releases
- Deprecation warnings before removal
- Clear migration paths provided

### Experimental APIs

Marked with `experimental` namespace:

```cpp
namespace pacs::experimental {
    // APIs here may change without notice
}
```

### Internal APIs

Not part of public API:

```cpp
namespace pacs::internal {
    // Do not use - may change at any time
}
```

## Version Support Matrix

| PACS Version | EOL Date | Status |
|--------------|----------|---------|
| 1.0.x | TBD | Current |
| 0.x.x | 2024-12-31 | Deprecated |

## Getting Help

- Check current version: `/api/version`
- API documentation: `/api/docs`
- Migration guides: See documentation
- Support: Open an issue on GitHub