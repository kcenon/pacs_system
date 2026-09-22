# Level 2: Echo Server

A DICOM Echo Server (Verification SCP) sample demonstrating network communication fundamentals. This builds on Level 1 concepts and introduces DICOM networking.

## Learning Objectives

By completing this sample, you will understand:

- **Server Configuration** - AE Title, port, timeouts, PDU size
- **Association Management** - Connection states, negotiation flow
- **Service Class Provider (SCP)** - Handling incoming DIMSE requests
- **C-ECHO Operation** - Verification service for connectivity testing
- **Graceful Shutdown** - Signal handling and resource cleanup

## Prerequisites

- Completed Level 1: Hello DICOM
- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- pacs_system library built with samples enabled
- DCMTK installed (for testing with `echoscu`)

## Build & Run

```bash
# Configure with samples enabled
cmake -B build -DPACS_BUILD_SAMPLES=ON

# Build this sample
cmake --build build --target sample_02_echo_server

# Run the server (default port 11112)
./build/samples/echo_server

# Or specify a custom port
./build/samples/echo_server 11113
```

## Testing

With the server running, open another terminal and test with DCMTK:

```bash
# Single C-ECHO test
echoscu -v -aec ECHO_SCP localhost 11112

# Multiple tests (stress test)
for i in {1..10}; do echoscu -aec ECHO_SCP localhost 11112; done

# Verbose output showing DIMSE details
echoscu -d -aec ECHO_SCP localhost 11112
```

### Expected Output

**Server side:**
```
[14:30:22.456] [CONNECT] ECHOSCU -> ECHO_SCP (active: 1)
[14:30:22.458] [RELEASE] ECHOSCU disconnected (active: 0)
```

**Client side (echoscu -v):**
```
I: Requesting Association
I: Association Accepted
I: Sending Echo Request
I: Received Echo Response (Status: Success)
I: Releasing Association
```

## Key Concepts Explained

### DICOM Association

An association is a logical connection between two DICOM applications:

```
   SCU (Client)                          SCP (Server)
       │                                     │
       │  A-ASSOCIATE-RQ ─────────────────►  │  Request association
       │                                     │
       │  ◄───────────────── A-ASSOCIATE-AC  │  Accept association
       │                                     │
       │  C-ECHO-RQ ──────────────────────►  │  Ping request
       │                                     │
       │  ◄────────────────────── C-ECHO-RSP │  Ping response
       │                                     │
       │  A-RELEASE-RQ ───────────────────►  │  Request release
       │                                     │
       │  ◄─────────────────── A-RELEASE-RP  │  Confirm release
       │                                     │
```

### Server Configuration

Key parameters for DICOM servers:

| Parameter | Description | Typical Value |
|-----------|-------------|---------------|
| AE Title | Application Entity identifier | 16 chars max |
| Port | TCP listening port | 104 (standard), 11112 (alternate) |
| Max PDU Size | Maximum data unit size | 16384 - 65536 bytes |
| Idle Timeout | Disconnect after inactivity | 30 - 300 seconds |
| Max Associations | Concurrent connection limit | 10 - 100 |

### Service Registration

```cpp
// Create server with configuration
net::dicom_server server{config};

// Register Verification SCP (handles C-ECHO)
server.register_service(std::make_shared<svc::verification_scp>());

// You can register multiple services:
// server.register_service(std::make_shared<svc::storage_scp>(path));
// server.register_service(std::make_shared<svc::query_scp>(db));
```

### Event Callbacks

```cpp
// Monitor association lifecycle
server.on_association_established([](const net::association& assoc) {
    std::cout << "Connected: " << assoc.calling_ae() << "\n";
});

server.on_association_released([](const net::association& assoc) {
    std::cout << "Disconnected: " << assoc.calling_ae() << "\n";
});

server.on_error([](const std::string& error) {
    std::cerr << "Error: " << error << "\n";
});
```

## Troubleshooting

### Port Already in Use

```bash
# Find process using the port
lsof -i :11112

# Or use a different port
./build/samples/echo_server 11113
```

### Connection Refused

- Ensure the server is running
- Check firewall settings
- Verify the port number matches

### AE Title Mismatch

DCMTK's `echoscu` uses `-aec` to specify the called (server) AE Title:
```bash
echoscu -aec ECHO_SCP localhost 11112
```

## Next Steps

Proceed to **Level 3: Storage Server** to learn:
- C-STORE operation for receiving DICOM images
- File storage and organization
- Transfer syntax handling
- IOD (Information Object Definition) validation

## Related Documentation

- [DICOM PS3.7 - Message Exchange](http://dicom.nema.org/medical/dicom/current/output/chtml/part07/PS3.7.html)
- [DICOM PS3.8 - Network Communication](http://dicom.nema.org/medical/dicom/current/output/chtml/part08/PS3.8.html)
- [DICOM PS3.4 Section A.4 - Verification Service](http://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_A.4.html)
