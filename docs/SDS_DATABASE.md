# SDS - Database Design

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2025-11-30

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Entity-Relationship Diagram](#2-entity-relationship-diagram)
- [3. Table Definitions](#3-table-definitions)
- [4. Index Design](#4-index-design)
- [5. Query Patterns](#5-query-patterns)
- [6. Data Migration](#6-data-migration)
- [7. Performance Considerations](#7-performance-considerations)

---

## 1. Overview

### 1.1 Database Selection

**Technology:** SQLite 3.36+

**Rationale:**

| Criterion | SQLite Advantage |
|-----------|-----------------|
| Deployment | Zero configuration, embedded |
| ACID | Full transaction support |
| Concurrency | WAL mode for concurrent reads |
| Performance | <1ms query for indexed lookups |
| Portability | Single file, cross-platform |

**Traces to:** DD-002, SRS-STOR-003

### 1.2 Design Principles

1. **DICOM Hierarchy:** Tables mirror Patient → Study → Series → Instance hierarchy
2. **Denormalization:** Common query attributes duplicated for performance
3. **Sparse Storage:** Only indexed attributes stored, full data in DICOM files
4. **Unicode Support:** All text columns UTF-8 encoded

---

## 2. Entity-Relationship Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        PACS Index Database Schema                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   ┌──────────────────┐                                                       │
│   │     patients     │                                                       │
│   ├──────────────────┤                                                       │
│   │ PK: patient_pk   │                                                       │
│   │ UK: patient_id   │                                                       │
│   │    patient_name  │                                                       │
│   │    birth_date    │                                                       │
│   │    sex           │                                                       │
│   └────────┬─────────┘                                                       │
│            │ 1                                                               │
│            │                                                                 │
│            │ *                                                               │
│   ┌────────▼─────────┐                                                       │
│   │     studies      │                                                       │
│   ├──────────────────┤                                                       │
│   │ PK: study_pk     │                                                       │
│   │ FK: patient_pk   │──────────────┐                                        │
│   │ UK: study_uid    │              │                                        │
│   │    study_date    │              │                                        │
│   │    study_time    │              │                                        │
│   │    accession_no  │              │                                        │
│   │    study_desc    │              │                                        │
│   │    modalities    │              │                                        │
│   │    num_series    │              │                                        │
│   │    num_instances │              │                                        │
│   └────────┬─────────┘              │                                        │
│            │ 1                      │                                        │
│            │                        │                                        │
│            │ *                      │                                        │
│   ┌────────▼─────────┐              │                                        │
│   │     series       │              │                                        │
│   ├──────────────────┤              │                                        │
│   │ PK: series_pk    │              │                                        │
│   │ FK: study_pk     │──────────────┤                                        │
│   │ UK: series_uid   │              │                                        │
│   │    modality      │              │                                        │
│   │    series_number │              │                                        │
│   │    series_desc   │              │                                        │
│   │    body_part     │              │                                        │
│   │    num_instances │              │                                        │
│   └────────┬─────────┘              │                                        │
│            │ 1                      │                                        │
│            │                        │                                        │
│            │ *                      │                                        │
│   ┌────────▼─────────┐              │                                        │
│   │    instances     │              │                                        │
│   ├──────────────────┤              │                                        │
│   │ PK: instance_pk  │              │                                        │
│   │ FK: series_pk    │──────────────┘                                        │
│   │ UK: sop_uid      │                                                       │
│   │    sop_class_uid │                                                       │
│   │    instance_no   │                                                       │
│   │    transfer_syn  │                                                       │
│   │    file_path     │                                                       │
│   │    file_size     │                                                       │
│   │    created_at    │                                                       │
│   └──────────────────┘                                                       │
│                                                                              │
│                                                                              │
│   ┌──────────────────┐      ┌──────────────────┐                            │
│   │      mpps        │      │   worklist       │                            │
│   ├──────────────────┤      ├──────────────────┤                            │
│   │ PK: mpps_pk      │      │ PK: worklist_pk  │                            │
│   │ UK: mpps_uid     │      │    accession_no  │                            │
│   │    status        │      │    patient_id    │                            │
│   │    start_dt      │      │    patient_name  │                            │
│   │    end_dt        │      │    scheduled_dt  │                            │
│   │    station_ae    │      │    station_ae    │                            │
│   │    study_uid     │      │    modality      │                            │
│   └──────────────────┘      │    procedure_id  │                            │
│                             │    step_id       │                            │
│                             └──────────────────┘                            │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Table Definitions

### DES-DB-001: patients Table

**Traces to:** SRS-STOR-003, FR-4.2

```sql
CREATE TABLE patients (
    -- Primary Key
    patient_pk      INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Unique Business Key
    patient_id      TEXT NOT NULL UNIQUE,  -- (0010,0020) Patient ID

    -- Patient Demographics
    patient_name    TEXT,                  -- (0010,0010) Patient's Name
    birth_date      TEXT,                  -- (0010,0030) Patient's Birth Date (YYYYMMDD)
    sex             TEXT,                  -- (0010,0040) Patient's Sex (M/F/O)

    -- Additional Attributes (for queries)
    issuer_of_pid   TEXT,                  -- (0010,0021) Issuer of Patient ID
    other_patient_ids TEXT,                -- (0010,1000) Other Patient IDs

    -- Metadata
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- Constraints
    CHECK (sex IN ('M', 'F', 'O', NULL)),
    CHECK (length(patient_id) <= 64)
);

-- Indexes
CREATE INDEX idx_patients_name ON patients(patient_name);
CREATE INDEX idx_patients_birth ON patients(birth_date);
```

**Column Specifications:**

| Column | DICOM Tag | Type | Max Length | Nullable | Notes |
|--------|-----------|------|------------|----------|-------|
| patient_pk | N/A | INTEGER | N/A | NO | Auto-increment |
| patient_id | (0010,0020) | TEXT | 64 | NO | Unique, indexed |
| patient_name | (0010,0010) | TEXT | 324 | YES | PN format |
| birth_date | (0010,0030) | TEXT | 8 | YES | YYYYMMDD |
| sex | (0010,0040) | TEXT | 1 | YES | M/F/O |

---

### DES-DB-002: studies Table

**Traces to:** SRS-STOR-003, FR-4.2

```sql
CREATE TABLE studies (
    -- Primary Key
    study_pk        INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Foreign Key
    patient_pk      INTEGER NOT NULL REFERENCES patients(patient_pk)
                    ON DELETE CASCADE,

    -- Unique Business Key
    study_uid       TEXT NOT NULL UNIQUE,  -- (0020,000D) Study Instance UID

    -- Study Attributes
    study_date      TEXT,                  -- (0008,0020) Study Date
    study_time      TEXT,                  -- (0008,0030) Study Time
    accession_no    TEXT,                  -- (0008,0050) Accession Number
    study_id        TEXT,                  -- (0020,0010) Study ID
    study_desc      TEXT,                  -- (0008,1030) Study Description
    referring_phys  TEXT,                  -- (0008,0090) Referring Physician's Name

    -- Aggregated Info (denormalized for query performance)
    modalities_in_study TEXT,              -- (0008,0061) Modalities in Study
    num_series      INTEGER DEFAULT 0,
    num_instances   INTEGER DEFAULT 0,

    -- Metadata
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- Constraints
    CHECK (length(study_uid) <= 64)
);

-- Indexes
CREATE INDEX idx_studies_patient ON studies(patient_pk);
CREATE INDEX idx_studies_date ON studies(study_date);
CREATE INDEX idx_studies_accession ON studies(accession_no);
CREATE INDEX idx_studies_modalities ON studies(modalities_in_study);
```

**Column Specifications:**

| Column | DICOM Tag | Type | Max Length | Nullable | Notes |
|--------|-----------|------|------------|----------|-------|
| study_pk | N/A | INTEGER | N/A | NO | Auto-increment |
| patient_pk | N/A | INTEGER | N/A | NO | FK to patients |
| study_uid | (0020,000D) | TEXT | 64 | NO | Unique, indexed |
| study_date | (0008,0020) | TEXT | 8 | YES | YYYYMMDD |
| study_time | (0008,0030) | TEXT | 14 | YES | HHMMSS.FFFFFF |
| accession_no | (0008,0050) | TEXT | 16 | YES | Indexed |
| modalities_in_study | (0008,0061) | TEXT | 256 | YES | e.g., "CT\\MR" |
| num_series | N/A | INTEGER | N/A | NO | Denormalized count |
| num_instances | N/A | INTEGER | N/A | NO | Denormalized count |

---

### DES-DB-003: series Table

**Traces to:** SRS-STOR-003

```sql
CREATE TABLE series (
    -- Primary Key
    series_pk       INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Foreign Key
    study_pk        INTEGER NOT NULL REFERENCES studies(study_pk)
                    ON DELETE CASCADE,

    -- Unique Business Key
    series_uid      TEXT NOT NULL UNIQUE,  -- (0020,000E) Series Instance UID

    -- Series Attributes
    modality        TEXT,                  -- (0008,0060) Modality
    series_number   INTEGER,               -- (0020,0011) Series Number
    series_desc     TEXT,                  -- (0008,103E) Series Description
    body_part       TEXT,                  -- (0018,0015) Body Part Examined
    laterality      TEXT,                  -- (0020,0060) Laterality
    series_date     TEXT,                  -- (0008,0021) Series Date
    series_time     TEXT,                  -- (0008,0031) Series Time
    protocol_name   TEXT,                  -- (0018,1030) Protocol Name
    station_name    TEXT,                  -- (0008,1010) Station Name
    institution     TEXT,                  -- (0008,0080) Institution Name

    -- Aggregated Info
    num_instances   INTEGER DEFAULT 0,

    -- Metadata
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- Constraints
    CHECK (length(series_uid) <= 64),
    CHECK (length(modality) <= 16)
);

-- Indexes
CREATE INDEX idx_series_study ON series(study_pk);
CREATE INDEX idx_series_modality ON series(modality);
CREATE INDEX idx_series_number ON series(series_number);
CREATE INDEX idx_series_date ON series(series_date);
```

---

### DES-DB-004: instances Table

**Traces to:** SRS-STOR-003

```sql
CREATE TABLE instances (
    -- Primary Key
    instance_pk     INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Foreign Key
    series_pk       INTEGER NOT NULL REFERENCES series(series_pk)
                    ON DELETE CASCADE,

    -- Unique Business Keys
    sop_uid         TEXT NOT NULL UNIQUE,  -- (0008,0018) SOP Instance UID
    sop_class_uid   TEXT NOT NULL,         -- (0008,0016) SOP Class UID

    -- Instance Attributes
    instance_number INTEGER,               -- (0020,0013) Instance Number
    transfer_syntax TEXT,                  -- (0002,0010) Transfer Syntax UID
    content_date    TEXT,                  -- (0008,0023) Content Date
    content_time    TEXT,                  -- (0008,0033) Content Time

    -- Image Attributes (if applicable)
    rows            INTEGER,               -- (0028,0010) Rows
    columns         INTEGER,               -- (0028,0011) Columns
    bits_allocated  INTEGER,               -- (0028,0100) Bits Allocated
    number_of_frames INTEGER,              -- (0028,0008) Number of Frames

    -- Storage Info
    file_path       TEXT NOT NULL,         -- Relative path from storage root
    file_size       INTEGER NOT NULL,      -- File size in bytes
    file_hash       TEXT,                  -- MD5 or SHA-256 hash

    -- Metadata
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- Constraints
    CHECK (length(sop_uid) <= 64),
    CHECK (file_size >= 0)
);

-- Indexes
CREATE INDEX idx_instances_series ON instances(series_pk);
CREATE INDEX idx_instances_sop_class ON instances(sop_class_uid);
CREATE INDEX idx_instances_number ON instances(instance_number);
CREATE INDEX idx_instances_created ON instances(created_at);
```

---

### DES-DB-005: mpps Table (Modality Performed Procedure Step)

**Traces to:** SRS-SVC-007, FR-3.4

```sql
CREATE TABLE mpps (
    -- Primary Key
    mpps_pk         INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Unique Business Key
    mpps_uid        TEXT NOT NULL UNIQUE,  -- SOP Instance UID

    -- Status
    status          TEXT NOT NULL,         -- IN PROGRESS, COMPLETED, DISCONTINUED

    -- Timing
    start_datetime  TEXT,                  -- Performed Procedure Step Start
    end_datetime    TEXT,                  -- Performed Procedure Step End

    -- Station Info
    station_ae      TEXT,                  -- Performed Station AE Title
    station_name    TEXT,                  -- Performed Station Name
    modality        TEXT,                  -- Modality

    -- References
    study_uid       TEXT,                  -- Study Instance UID
    accession_no    TEXT,                  -- Accession Number

    -- Scheduled Step Reference (from worklist)
    scheduled_step_id TEXT,                -- Scheduled Procedure Step ID
    requested_proc_id TEXT,                -- Requested Procedure ID

    -- Performed Series (JSON array)
    performed_series TEXT,                 -- JSON: [{series_uid, protocol, images}]

    -- Metadata
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- Constraints
    CHECK (status IN ('IN PROGRESS', 'COMPLETED', 'DISCONTINUED'))
);

-- Indexes
CREATE INDEX idx_mpps_status ON mpps(status);
CREATE INDEX idx_mpps_station ON mpps(station_ae);
CREATE INDEX idx_mpps_study ON mpps(study_uid);
CREATE INDEX idx_mpps_date ON mpps(start_datetime);
```

---

### DES-DB-006: worklist Table

**Traces to:** SRS-SVC-006, FR-3.3

```sql
CREATE TABLE worklist (
    -- Primary Key
    worklist_pk     INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Scheduled Procedure Step
    step_id         TEXT NOT NULL,         -- Scheduled Procedure Step ID
    step_status     TEXT DEFAULT 'SCHEDULED', -- SCHEDULED, STARTED, COMPLETED

    -- Patient Info
    patient_id      TEXT NOT NULL,         -- Patient ID
    patient_name    TEXT,                  -- Patient's Name
    birth_date      TEXT,                  -- Patient's Birth Date
    sex             TEXT,                  -- Patient's Sex

    -- Requested Procedure
    accession_no    TEXT,                  -- Accession Number
    requested_proc_id TEXT,                -- Requested Procedure ID
    study_uid       TEXT,                  -- Study Instance UID (pre-assigned)

    -- Scheduling Info
    scheduled_datetime TEXT NOT NULL,      -- Scheduled Procedure Step Start
    station_ae      TEXT,                  -- Scheduled Station AE Title
    station_name    TEXT,                  -- Scheduled Station Name
    modality        TEXT NOT NULL,         -- Modality

    -- Procedure Info
    procedure_desc  TEXT,                  -- Scheduled Procedure Step Description
    protocol_code   TEXT,                  -- Protocol Code Sequence (JSON)

    -- Referring Physician
    referring_phys  TEXT,                  -- Referring Physician's Name
    referring_phys_id TEXT,                -- Referring Physician ID

    -- Metadata
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- Constraints
    UNIQUE (step_id, accession_no)
);

-- Indexes
CREATE INDEX idx_worklist_station ON worklist(station_ae);
CREATE INDEX idx_worklist_modality ON worklist(modality);
CREATE INDEX idx_worklist_scheduled ON worklist(scheduled_datetime);
CREATE INDEX idx_worklist_patient ON worklist(patient_id);
CREATE INDEX idx_worklist_accession ON worklist(accession_no);
CREATE INDEX idx_worklist_status ON worklist(step_status);
```

---

### DES-DB-007: schema_version Table

**Purpose:** Track database schema version for migrations.

```sql
CREATE TABLE schema_version (
    version         INTEGER PRIMARY KEY,
    description     TEXT NOT NULL,
    applied_at      TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Initial version
INSERT INTO schema_version (version, description)
VALUES (1, 'Initial schema creation');
```

---

## 4. Index Design

### 4.1 Primary Query Patterns

| Query Pattern | Tables | Index Strategy |
|--------------|--------|----------------|
| Patient lookup by ID | patients | UNIQUE on patient_id |
| Patient search by name | patients | B-tree on patient_name |
| Study by accession | studies | B-tree on accession_no |
| Study by date range | studies | B-tree on study_date |
| Study by modality | studies | B-tree on modalities_in_study |
| Series by modality | series | B-tree on modality |
| Instance by SOP UID | instances | UNIQUE on sop_uid |

### 4.2 Composite Indexes

```sql
-- Multi-column indexes for common queries

-- Patient search by name and birth date
CREATE INDEX idx_patients_name_birth
ON patients(patient_name, birth_date);

-- Study search by date and modality
CREATE INDEX idx_studies_date_modality
ON studies(study_date, modalities_in_study);

-- Worklist by station, date, and modality
CREATE INDEX idx_worklist_station_date_mod
ON worklist(station_ae, scheduled_datetime, modality);
```

### 4.3 Index Sizing Estimates

| Table | Expected Rows | Index Size |
|-------|---------------|------------|
| patients | 10,000 | ~500 KB |
| studies | 50,000 | ~3 MB |
| series | 200,000 | ~12 MB |
| instances | 2,000,000 | ~150 MB |

---

## 5. Query Patterns

### 5.1 Patient-Level Query (C-FIND)

```sql
-- DES-DB-Q001: Patient Root C-FIND at Patient Level
SELECT
    p.patient_id,
    p.patient_name,
    p.birth_date,
    p.sex,
    (SELECT COUNT(*) FROM studies WHERE patient_pk = p.patient_pk) as num_studies
FROM patients p
WHERE
    (? IS NULL OR p.patient_name LIKE ?)   -- PatientName matching
    AND (? IS NULL OR p.patient_id = ?)    -- PatientID matching
    AND (? IS NULL OR p.birth_date = ?)    -- BirthDate matching
ORDER BY p.patient_name
LIMIT ?;
```

### 5.2 Study-Level Query (C-FIND)

```sql
-- DES-DB-Q002: Patient/Study Root C-FIND at Study Level
SELECT
    p.patient_id,
    p.patient_name,
    p.birth_date,
    p.sex,
    s.study_uid,
    s.study_date,
    s.study_time,
    s.accession_no,
    s.study_desc,
    s.referring_phys,
    s.modalities_in_study,
    s.num_series,
    s.num_instances
FROM studies s
JOIN patients p ON s.patient_pk = p.patient_pk
WHERE
    (? IS NULL OR p.patient_name LIKE ?)
    AND (? IS NULL OR p.patient_id = ?)
    AND (? IS NULL OR s.study_date >= ?)   -- Date range start
    AND (? IS NULL OR s.study_date <= ?)   -- Date range end
    AND (? IS NULL OR s.accession_no = ?)
    AND (? IS NULL OR s.modalities_in_study LIKE ?)
ORDER BY s.study_date DESC, s.study_time DESC
LIMIT ?;
```

### 5.3 Series-Level Query (C-FIND)

```sql
-- DES-DB-Q003: Series Level Query
SELECT
    p.patient_id,
    p.patient_name,
    s.study_uid,
    sr.series_uid,
    sr.modality,
    sr.series_number,
    sr.series_desc,
    sr.body_part,
    sr.num_instances
FROM series sr
JOIN studies s ON sr.study_pk = s.study_pk
JOIN patients p ON s.patient_pk = p.patient_pk
WHERE
    s.study_uid = ?
    AND (? IS NULL OR sr.modality = ?)
    AND (? IS NULL OR sr.series_number = ?)
ORDER BY sr.series_number;
```

### 5.4 Instance-Level Query (C-FIND)

```sql
-- DES-DB-Q004: Instance Level Query
SELECT
    p.patient_id,
    p.patient_name,
    s.study_uid,
    sr.series_uid,
    i.sop_uid,
    i.sop_class_uid,
    i.instance_number,
    i.rows,
    i.columns,
    i.number_of_frames
FROM instances i
JOIN series sr ON i.series_pk = sr.series_pk
JOIN studies s ON sr.study_pk = s.study_pk
JOIN patients p ON s.patient_pk = p.patient_pk
WHERE
    sr.series_uid = ?
ORDER BY i.instance_number;
```

### 5.5 File Path Lookup (for C-MOVE/C-GET)

```sql
-- DES-DB-Q005: Get file paths for a study
SELECT
    i.sop_uid,
    i.file_path,
    i.file_size,
    i.transfer_syntax
FROM instances i
JOIN series sr ON i.series_pk = sr.series_pk
JOIN studies s ON sr.study_pk = s.study_pk
WHERE s.study_uid = ?
ORDER BY sr.series_number, i.instance_number;
```

### 5.6 Worklist Query (MWL C-FIND)

```sql
-- DES-DB-Q006: Modality Worklist Query
SELECT
    w.step_id,
    w.patient_id,
    w.patient_name,
    w.birth_date,
    w.sex,
    w.accession_no,
    w.requested_proc_id,
    w.study_uid,
    w.scheduled_datetime,
    w.station_ae,
    w.modality,
    w.procedure_desc,
    w.referring_phys
FROM worklist w
WHERE
    w.step_status = 'SCHEDULED'
    AND (? IS NULL OR w.station_ae = ?)
    AND (? IS NULL OR w.modality = ?)
    AND (? IS NULL OR w.scheduled_datetime >= ?)
    AND (? IS NULL OR w.scheduled_datetime <= ?)
    AND (? IS NULL OR w.patient_id = ?)
ORDER BY w.scheduled_datetime;
```

---

## 6. Data Migration

### 6.1 Migration Scripts

```sql
-- Migration v1 → v2: Add file_hash column
BEGIN TRANSACTION;

ALTER TABLE instances ADD COLUMN file_hash TEXT;

UPDATE schema_version SET version = 2
WHERE version = (SELECT MAX(version) FROM schema_version);

INSERT INTO schema_version (version, description)
VALUES (2, 'Add file_hash column to instances');

COMMIT;
```

### 6.2 Migration Strategy

```cpp
// DES-DB-MIG: Migration Runner
class migration_runner {
public:
    common::Result<void> run_migrations(sqlite3* db) {
        auto current = get_current_version(db);

        while (current < LATEST_VERSION) {
            auto result = apply_migration(db, current + 1);
            if (result.is_err()) {
                return result;
            }
            current++;
        }

        return common::Result<void>::ok();
    }

private:
    static constexpr int LATEST_VERSION = 2;

    int get_current_version(sqlite3* db);
    common::Result<void> apply_migration(sqlite3* db, int version);
};
```

---

## 7. Performance Considerations

### 7.1 SQLite Configuration

```cpp
// DES-DB-CFG: Optimal SQLite configuration
void configure_database(sqlite3* db) {
    // Enable WAL mode for concurrent reads
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    // Set cache size (64MB)
    sqlite3_exec(db, "PRAGMA cache_size=-65536;", nullptr, nullptr, nullptr);

    // Optimize for read-heavy workload
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    // Enable foreign keys
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    // Memory-mapped I/O (256MB)
    sqlite3_exec(db, "PRAGMA mmap_size=268435456;", nullptr, nullptr, nullptr);

    // Temp store in memory
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
}
```

### 7.2 Query Performance Targets

| Query Type | Target Latency | Expected Dataset |
|------------|----------------|------------------|
| Patient lookup (by ID) | <1ms | 10K patients |
| Study search (by date) | <10ms | 50K studies |
| Series listing | <5ms | 200K series |
| Instance listing | <20ms | 2M instances |
| Worklist query | <5ms | 1K items |

### 7.3 Maintenance Operations

```sql
-- DES-DB-MAINT: Regular maintenance queries

-- Analyze tables for query optimizer
ANALYZE patients;
ANALYZE studies;
ANALYZE series;
ANALYZE instances;

-- Rebuild indexes (if fragmented)
REINDEX idx_studies_date;
REINDEX idx_instances_series;

-- Vacuum to reclaim space (after many deletes)
VACUUM;

-- Check database integrity
PRAGMA integrity_check;
```

### 7.4 Capacity Planning

| Metric | Calculation | Example (1M images) |
|--------|-------------|---------------------|
| Database size | ~200 bytes/instance | ~200 MB |
| Index size | ~100 bytes/instance | ~100 MB |
| Memory (cache) | 10% of DB size | ~30 MB |
| Insert rate | ~100/sec sustained | - |
| Query rate | ~1000/sec | - |

---

*Document Version: 1.0.0*
*Created: 2025-11-30*
*Author: kcenon@naver.com*
