# PACS System Database Configuration Guide

## Overview

The PACS system supports multiple database backends for storing metadata about DICOM studies, series, instances, worklists, and MPPS information. The actual DICOM files are stored on the filesystem with optional encryption.

## Supported Databases

1. **SQLite** (Default)
   - Embedded database
   - No server required
   - Good for small to medium installations
   - Single file storage

2. **PostgreSQL** (Recommended for production)
   - Enterprise-grade database
   - High performance and scalability
   - Advanced features (replication, partitioning)
   - Connection pooling support

3. **MySQL** (Planned)
   - Popular open-source database
   - Good performance
   - Wide hosting support

4. **MongoDB** (Planned)
   - NoSQL document database
   - Flexible schema
   - Horizontal scaling

## Configuration

### Database Type Selection

Set the database type in configuration:
```json
{
  "database": {
    "type": "postgresql"  // Options: sqlite, postgresql, mysql, mongodb
  }
}
```

### SQLite Configuration

```json
{
  "database": {
    "type": "sqlite",
    "sqlite": {
      "path": "./data/db/pacs.db",
      "journal_mode": "WAL",
      "synchronous": "NORMAL",
      "cache_size": 10000,
      "busy_timeout": 5000
    }
  }
}
```

### PostgreSQL Configuration

```json
{
  "database": {
    "type": "postgresql",
    "postgresql": {
      "host": "localhost",
      "port": 5432,
      "database": "pacs",
      "username": "pacs",
      "password": "secure_password",
      "ssl_mode": "require",
      "connection_timeout": 10,
      "command_timeout": 30,
      "min_pool_size": 2,
      "max_pool_size": 10
    }
  }
}
```

#### SSL Modes
- `disable`: No SSL
- `allow`: Try non-SSL first, then SSL
- `prefer`: Try SSL first, then non-SSL (default)
- `require`: Only SSL connections
- `verify-ca`: SSL with CA verification
- `verify-full`: SSL with full verification

## Database Schema

### Studies Table
```sql
CREATE TABLE studies (
    study_instance_uid VARCHAR(64) PRIMARY KEY,
    patient_id VARCHAR(64),
    patient_name VARCHAR(256),
    study_date DATE,
    study_time TIME,
    study_description TEXT,
    accession_number VARCHAR(64),
    referring_physician VARCHAR(256),
    created_at TIMESTAMP,
    updated_at TIMESTAMP
);
```

### Series Table
```sql
CREATE TABLE series (
    series_instance_uid VARCHAR(64) PRIMARY KEY,
    study_instance_uid VARCHAR(64) REFERENCES studies,
    modality VARCHAR(16),
    series_number INTEGER,
    series_date DATE,
    series_time TIME,
    series_description TEXT,
    body_part_examined VARCHAR(64),
    patient_position VARCHAR(16),
    created_at TIMESTAMP,
    updated_at TIMESTAMP
);
```

### Instances Table
```sql
CREATE TABLE instances (
    sop_instance_uid VARCHAR(64) PRIMARY KEY,
    series_instance_uid VARCHAR(64) REFERENCES series,
    sop_class_uid VARCHAR(64),
    instance_number INTEGER,
    storage_path TEXT,
    file_size BIGINT,
    transfer_syntax_uid VARCHAR(64),
    created_at TIMESTAMP,
    updated_at TIMESTAMP
);
```

## PostgreSQL Setup

### 1. Install PostgreSQL

Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install postgresql postgresql-contrib
```

CentOS/RHEL:
```bash
sudo yum install postgresql-server postgresql-contrib
sudo postgresql-setup initdb
```

macOS:
```bash
brew install postgresql
brew services start postgresql
```

### 2. Create Database and User

```sql
-- Connect as postgres user
sudo -u postgres psql

-- Create database
CREATE DATABASE pacs;

-- Create user
CREATE USER pacs WITH ENCRYPTED PASSWORD 'secure_password';

-- Grant privileges
GRANT ALL PRIVILEGES ON DATABASE pacs TO pacs;

-- Enable required extensions
\c pacs
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";
CREATE EXTENSION IF NOT EXISTS "pgcrypto";
```

### 3. Configure PostgreSQL

Edit `postgresql.conf`:
```conf
# Connection settings
listen_addresses = 'localhost'
port = 5432
max_connections = 200

# Memory settings
shared_buffers = 256MB
effective_cache_size = 1GB
work_mem = 4MB
maintenance_work_mem = 64MB

# Write performance
checkpoint_completion_target = 0.9
wal_buffers = 16MB
default_statistics_target = 100
```

Edit `pg_hba.conf`:
```conf
# TYPE  DATABASE  USER  ADDRESS     METHOD
local   all       all               md5
host    all       all   127.0.0.1/32    md5
host    all       all   ::1/128         md5
```

### 4. Performance Tuning

For optimal performance with PACS workload:

```sql
-- Create indexes
CREATE INDEX idx_studies_patient_date ON studies(patient_id, study_date);
CREATE INDEX idx_series_study_modality ON series(study_instance_uid, modality);
CREATE INDEX idx_instances_series ON instances(series_instance_uid);

-- Analyze tables regularly
ANALYZE studies;
ANALYZE series;
ANALYZE instances;

-- Configure autovacuum
ALTER TABLE studies SET (autovacuum_vacuum_scale_factor = 0.1);
ALTER TABLE series SET (autovacuum_vacuum_scale_factor = 0.1);
ALTER TABLE instances SET (autovacuum_vacuum_scale_factor = 0.1);
```

## Connection Pooling

The PostgreSQL adapter includes built-in connection pooling:

```json
{
  "database": {
    "postgresql": {
      "min_pool_size": 2,    // Minimum connections
      "max_pool_size": 10,   // Maximum connections
      "idle_timeout": 300    // Seconds before closing idle connections
    }
  }
}
```

Benefits:
- Reduced connection overhead
- Better resource utilization
- Improved response times
- Automatic reconnection handling

## Migration

### From SQLite to PostgreSQL

1. Export data from SQLite:
```bash
sqlite3 pacs.db .dump > pacs_dump.sql
```

2. Convert SQL syntax:
```bash
# Use migration script
./tools/sqlite_to_postgresql.py pacs_dump.sql > pacs_postgresql.sql
```

3. Import to PostgreSQL:
```bash
psql -U pacs -d pacs -f pacs_postgresql.sql
```

### Backup and Recovery

PostgreSQL backup:
```bash
# Full backup
pg_dump -U pacs -d pacs -F custom -f pacs_backup.dump

# Restore
pg_restore -U pacs -d pacs -F custom pacs_backup.dump
```

Automated backup script:
```bash
#!/bin/bash
# backup_pacs.sh

BACKUP_DIR="/backup/pacs"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
DB_NAME="pacs"
DB_USER="pacs"

# Create backup
pg_dump -U $DB_USER -d $DB_NAME -F custom -f "$BACKUP_DIR/pacs_$TIMESTAMP.dump"

# Keep only last 7 days
find $BACKUP_DIR -name "pacs_*.dump" -mtime +7 -delete
```

## High Availability

### PostgreSQL Replication

1. Configure master server:
```conf
# postgresql.conf
wal_level = replica
max_wal_senders = 3
wal_keep_segments = 64
```

2. Configure replica:
```bash
pg_basebackup -h master_host -D /var/lib/postgresql/data -U replicator -v -P -W
```

3. Update PACS configuration for read replicas:
```json
{
  "database": {
    "postgresql": {
      "master": {
        "host": "master.example.com"
      },
      "replicas": [
        {"host": "replica1.example.com"},
        {"host": "replica2.example.com"}
      ]
    }
  }
}
```

## Monitoring

### Database Metrics

Monitor these key metrics:
- Connection count
- Query execution time
- Table sizes
- Index usage
- Cache hit ratio
- Replication lag

### Useful Queries

```sql
-- Database size
SELECT pg_database_size('pacs') / 1024 / 1024 AS size_mb;

-- Table sizes
SELECT schemaname, tablename, 
       pg_size_pretty(pg_total_relation_size(schemaname||'.'||tablename)) AS size
FROM pg_tables 
WHERE schemaname = 'public'
ORDER BY pg_total_relation_size(schemaname||'.'||tablename) DESC;

-- Active connections
SELECT count(*) FROM pg_stat_activity;

-- Slow queries
SELECT query, mean_time, calls 
FROM pg_stat_statements 
ORDER BY mean_time DESC 
LIMIT 10;
```

## Troubleshooting

### Common Issues

1. **Connection refused**
   - Check PostgreSQL is running
   - Verify connection parameters
   - Check firewall settings
   - Review pg_hba.conf

2. **Performance degradation**
   - Run ANALYZE on tables
   - Check for missing indexes
   - Review slow query log
   - Increase connection pool size

3. **Disk space issues**
   - Monitor table and index sizes
   - Configure table partitioning
   - Archive old data
   - Run VACUUM FULL if needed

4. **Lock contention**
   - Use pg_locks view
   - Review long-running transactions
   - Optimize queries
   - Consider read replicas