# PACS System Deployment Guide

This document describes how to deploy the PACS System in various environments.

## Prerequisites

- Docker and Docker Compose
- OpenSSL (for TLS certificate generation)
- Bash (for deployment scripts)

## Quick Start

The easiest way to deploy the PACS System is using the provided Docker Compose setup:

```bash
# Clone the repository
git clone https://github.com/example/pacs_system.git
cd pacs_system

# Run the deployment script
./deploy/deploy.sh

# Start the system
cd deploy
docker-compose up -d
```

This will:
1. Create necessary directories
2. Copy configuration files
3. Configure Docker Compose
4. Start the PACS System container

## Configuration Options

The deployment script supports several options:

```bash
./deploy/deploy.sh --help

# Options:
#   --tls               Enable TLS encryption
#   --data-dir DIR      Set data directory (default: ./data)
#   --config-dir DIR    Set config directory (default: ./config)
#   --logs-dir DIR      Set logs directory (default: ./logs)
#   --port PORT         Set DICOM port (default: 11112)
#   --help              Show this help message
```

### TLS-Enabled Deployment

To deploy with TLS enabled:

```bash
./deploy/deploy.sh --tls
```

This will generate self-signed certificates and configure the PACS System to use TLS encryption.

## Directory Structure

The deployment creates the following directory structure:

```
/data/
  ├── storage/       # DICOM storage files
  ├── worklist/      # Worklist files
  ├── db/            # Database files
  ├── security/      # Security files (user database)
  └── certs/         # TLS certificates (if TLS is enabled)
/config/
  └── pacs_config.json  # Main configuration file
/logs/
  └── ...               # Log files
```

## Configuration Files

### Main Configuration

The main configuration file is `pacs_config.json`, located in the config directory. It contains all settings for the PACS System, including:

- DICOM settings (AE Title, port, etc.)
- Directory paths
- TLS configuration
- Security settings
- Logging configuration

### TLS Configuration

When deploying with TLS, the following files are used:

- `server.crt`: Server certificate
- `server.key`: Server private key
- `ca.crt`: Certificate authority certificate

## Docker Compose Commands

### Start the System

```bash
cd deploy
docker-compose up -d
```

### View Logs

```bash
cd deploy
docker-compose logs -f
```

### Stop the System

```bash
cd deploy
docker-compose down
```

### Rebuild the Container

```bash
cd deploy
docker-compose build --no-cache
docker-compose up -d
```

## Manual Deployment

For manual deployment without Docker, follow these steps:

1. Clone the repository
2. Build the PACS System:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build .
   ```
3. Create necessary directories:
   ```bash
   mkdir -p data/storage data/worklist data/db data/security
   mkdir -p config
   mkdir -p logs
   ```
4. Copy the configuration file:
   ```bash
   cp deploy/pacs_config.json config/
   ```
5. Start the PACS Server:
   ```bash
   ./bin/pacs_server config/pacs_config.json
   ```

## Environment Variables

The PACS System supports the following environment variables:

- `PACS_AE_TITLE`: AE Title (default: PACS)
- `PACS_LOCAL_PORT`: DICOM port (default: 11112)
- `PACS_DATA_DIRECTORY`: Data directory path
- `PACS_LOG_DIRECTORY`: Log directory path
- `PACS_DATABASE_DIRECTORY`: Database directory path
- `PACS_CONFIG_FILE_PATH`: Configuration file path

## Troubleshooting

### Connection Issues

If clients cannot connect to the PACS System:

1. Check that the port is correct and accessible
2. Verify firewall settings allow the DICOM port
3. If using TLS, ensure certificates are valid
4. Check that the AE Title matches the expected value

### TLS Issues

If TLS connections fail:

1. Verify that certificates are generated correctly
2. Check that the certificate paths in the configuration are correct
3. Ensure the client trusts the server's certificate authority
4. Verify that TLS protocol versions are compatible

### Database Issues

If database errors occur:

1. Check that the database directory exists and is writable
2. Verify that SQLite is installed and working
3. Check the database file permissions

### Log Files

Check the log files in the logs directory for detailed error information.

## Production Recommendations

For production deployments:

1. Use proper TLS certificates from a trusted certificate authority
2. Configure appropriate authentication and authorization
3. Set up regular backups of the data directory
4. Monitor disk space for storage growth
5. Set up health monitoring for the PACS System
6. Configure proper user accounts with strong passwords
7. Consider using a reverse proxy for additional security
8. Implement network security measures (firewalls, VLANs)
9. Follow healthcare privacy regulations (e.g., HIPAA, GDPR)