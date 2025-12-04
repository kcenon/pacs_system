# SDS - 데이터베이스 설계

> **버전:** 1.1.0
> **상위 문서:** [SDS_KO.md](SDS_KO.md)
> **최종 수정일:** 2025-12-04

---

## 목차

- [1. 개요](#1-개요)
- [2. 엔티티-관계 다이어그램](#2-엔티티-관계-다이어그램)
- [3. 테이블 정의](#3-테이블-정의)
- [4. 인덱스 설계](#4-인덱스-설계)
- [5. 조회 패턴](#5-조회-패턴)
- [6. 데이터 마이그레이션](#6-데이터-마이그레이션)
- [7. 성능 고려사항](#7-성능-고려사항)

---

## 1. 개요

### 1.1 데이터베이스 선택

**기술:** SQLite 3.36+

**선택 근거:**

| 기준 | SQLite 장점 |
|------|------------|
| 배포 | 무설정, 임베디드 |
| ACID | 완전한 트랜잭션 지원 |
| 동시성 | WAL 모드로 동시 읽기 |
| 성능 | 인덱싱된 조회에 1ms 미만 |
| 이식성 | 단일 파일, 크로스 플랫폼 |

**추적성:** DD-002, SRS-STOR-003

### 1.2 설계 원칙

1. **DICOM 계층 구조:** 테이블은 Patient → Study → Series → Instance 계층을 반영
2. **비정규화:** 성능을 위해 자주 조회되는 속성 중복 저장
3. **희소 저장:** 인덱싱된 속성만 저장, 전체 데이터는 DICOM 파일에 보관
4. **유니코드 지원:** 모든 텍스트 컬럼은 UTF-8 인코딩

---

## 2. 엔티티-관계 다이어그램

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        PACS 인덱스 데이터베이스 스키마                         │
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

## 3. 테이블 정의

### DES-DB-001: patients 테이블

**추적성:** SRS-STOR-003, FR-4.2

```sql
CREATE TABLE patients (
    -- 기본 키
    patient_pk      INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 고유 비즈니스 키
    patient_id      TEXT NOT NULL UNIQUE,  -- (0010,0020) 환자 ID

    -- 환자 인구통계
    patient_name    TEXT,                  -- (0010,0010) 환자 이름
    birth_date      TEXT,                  -- (0010,0030) 환자 생년월일 (YYYYMMDD)
    sex             TEXT,                  -- (0010,0040) 환자 성별 (M/F/O)

    -- 추가 속성 (조회용)
    issuer_of_pid   TEXT,                  -- (0010,0021) 환자 ID 발급기관
    other_patient_ids TEXT,                -- (0010,1000) 기타 환자 ID

    -- 메타데이터
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- 제약조건
    CHECK (sex IN ('M', 'F', 'O', NULL)),
    CHECK (length(patient_id) <= 64)
);

-- 인덱스
CREATE INDEX idx_patients_name ON patients(patient_name);
CREATE INDEX idx_patients_birth ON patients(birth_date);
```

**컬럼 명세:**

| 컬럼 | DICOM 태그 | 타입 | 최대 길이 | Nullable | 비고 |
|------|-----------|------|----------|----------|------|
| patient_pk | N/A | INTEGER | N/A | NO | 자동 증가 |
| patient_id | (0010,0020) | TEXT | 64 | NO | 고유, 인덱싱 |
| patient_name | (0010,0010) | TEXT | 324 | YES | PN 형식 |
| birth_date | (0010,0030) | TEXT | 8 | YES | YYYYMMDD |
| sex | (0010,0040) | TEXT | 1 | YES | M/F/O |

---

### DES-DB-002: studies 테이블

**추적성:** SRS-STOR-003, FR-4.2

```sql
CREATE TABLE studies (
    -- 기본 키
    study_pk        INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 외래 키
    patient_pk      INTEGER NOT NULL REFERENCES patients(patient_pk)
                    ON DELETE CASCADE,

    -- 고유 비즈니스 키
    study_uid       TEXT NOT NULL UNIQUE,  -- (0020,000D) Study Instance UID

    -- 검사 속성
    study_date      TEXT,                  -- (0008,0020) 검사 날짜
    study_time      TEXT,                  -- (0008,0030) 검사 시간
    accession_no    TEXT,                  -- (0008,0050) Accession Number
    study_id        TEXT,                  -- (0020,0010) Study ID
    study_desc      TEXT,                  -- (0008,1030) 검사 설명
    referring_phys  TEXT,                  -- (0008,0090) 의뢰의사 이름

    -- 집계 정보 (조회 성능을 위한 비정규화)
    modalities_in_study TEXT,              -- (0008,0061) 검사 내 모달리티
    num_series      INTEGER DEFAULT 0,
    num_instances   INTEGER DEFAULT 0,

    -- 메타데이터
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- 제약조건
    CHECK (length(study_uid) <= 64)
);

-- 인덱스
CREATE INDEX idx_studies_patient ON studies(patient_pk);
CREATE INDEX idx_studies_date ON studies(study_date);
CREATE INDEX idx_studies_accession ON studies(accession_no);
CREATE INDEX idx_studies_modalities ON studies(modalities_in_study);
```

**컬럼 명세:**

| 컬럼 | DICOM 태그 | 타입 | 최대 길이 | Nullable | 비고 |
|------|-----------|------|----------|----------|------|
| study_pk | N/A | INTEGER | N/A | NO | 자동 증가 |
| patient_pk | N/A | INTEGER | N/A | NO | patients FK |
| study_uid | (0020,000D) | TEXT | 64 | NO | 고유, 인덱싱 |
| study_date | (0008,0020) | TEXT | 8 | YES | YYYYMMDD |
| study_time | (0008,0030) | TEXT | 14 | YES | HHMMSS.FFFFFF |
| accession_no | (0008,0050) | TEXT | 16 | YES | 인덱싱됨 |
| modalities_in_study | (0008,0061) | TEXT | 256 | YES | 예: "CT\\MR" |
| num_series | N/A | INTEGER | N/A | NO | 비정규화 카운트 |
| num_instances | N/A | INTEGER | N/A | NO | 비정규화 카운트 |

---

### DES-DB-003: series 테이블

**추적성:** SRS-STOR-003

```sql
CREATE TABLE series (
    -- 기본 키
    series_pk       INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 외래 키
    study_pk        INTEGER NOT NULL REFERENCES studies(study_pk)
                    ON DELETE CASCADE,

    -- 고유 비즈니스 키
    series_uid      TEXT NOT NULL UNIQUE,  -- (0020,000E) Series Instance UID

    -- 시리즈 속성
    modality        TEXT,                  -- (0008,0060) 모달리티
    series_number   INTEGER,               -- (0020,0011) 시리즈 번호
    series_desc     TEXT,                  -- (0008,103E) 시리즈 설명
    body_part       TEXT,                  -- (0018,0015) 검사 부위
    laterality      TEXT,                  -- (0020,0060) 좌우구분
    series_date     TEXT,                  -- (0008,0021) 시리즈 날짜
    series_time     TEXT,                  -- (0008,0031) 시리즈 시간
    protocol_name   TEXT,                  -- (0018,1030) 프로토콜 이름
    station_name    TEXT,                  -- (0008,1010) 스테이션 이름
    institution     TEXT,                  -- (0008,0080) 기관 이름

    -- 집계 정보
    num_instances   INTEGER DEFAULT 0,

    -- 메타데이터
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- 제약조건
    CHECK (length(series_uid) <= 64),
    CHECK (length(modality) <= 16)
);

-- 인덱스
CREATE INDEX idx_series_study ON series(study_pk);
CREATE INDEX idx_series_modality ON series(modality);
CREATE INDEX idx_series_number ON series(series_number);
CREATE INDEX idx_series_date ON series(series_date);
```

---

### DES-DB-004: instances 테이블

**추적성:** SRS-STOR-003

```sql
CREATE TABLE instances (
    -- 기본 키
    instance_pk     INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 외래 키
    series_pk       INTEGER NOT NULL REFERENCES series(series_pk)
                    ON DELETE CASCADE,

    -- 고유 비즈니스 키
    sop_uid         TEXT NOT NULL UNIQUE,  -- (0008,0018) SOP Instance UID
    sop_class_uid   TEXT NOT NULL,         -- (0008,0016) SOP Class UID

    -- 인스턴스 속성
    instance_number INTEGER,               -- (0020,0013) 인스턴스 번호
    transfer_syntax TEXT,                  -- (0002,0010) 전송 구문 UID
    content_date    TEXT,                  -- (0008,0023) 콘텐츠 날짜
    content_time    TEXT,                  -- (0008,0033) 콘텐츠 시간

    -- 이미지 속성 (해당하는 경우)
    rows            INTEGER,               -- (0028,0010) 행
    columns         INTEGER,               -- (0028,0011) 열
    bits_allocated  INTEGER,               -- (0028,0100) 할당 비트
    number_of_frames INTEGER,              -- (0028,0008) 프레임 수

    -- 저장 정보
    file_path       TEXT NOT NULL,         -- 저장소 루트 기준 상대 경로
    file_size       INTEGER NOT NULL,      -- 파일 크기 (바이트)
    file_hash       TEXT,                  -- MD5 또는 SHA-256 해시

    -- 메타데이터
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- 제약조건
    CHECK (length(sop_uid) <= 64),
    CHECK (file_size >= 0)
);

-- 인덱스
CREATE INDEX idx_instances_series ON instances(series_pk);
CREATE INDEX idx_instances_sop_class ON instances(sop_class_uid);
CREATE INDEX idx_instances_number ON instances(instance_number);
CREATE INDEX idx_instances_created ON instances(created_at);
```

---

### DES-DB-005: mpps 테이블 (Modality Performed Procedure Step)

**추적성:** SRS-SVC-007, FR-3.4

```sql
CREATE TABLE mpps (
    -- 기본 키
    mpps_pk         INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 고유 비즈니스 키
    mpps_uid        TEXT NOT NULL UNIQUE,  -- SOP Instance UID

    -- 상태
    status          TEXT NOT NULL,         -- IN PROGRESS, COMPLETED, DISCONTINUED

    -- 시간
    start_datetime  TEXT,                  -- 수행 절차 단계 시작
    end_datetime    TEXT,                  -- 수행 절차 단계 종료

    -- 스테이션 정보
    station_ae      TEXT,                  -- 수행 스테이션 AE 타이틀
    station_name    TEXT,                  -- 수행 스테이션 이름
    modality        TEXT,                  -- 모달리티

    -- 참조
    study_uid       TEXT,                  -- Study Instance UID
    accession_no    TEXT,                  -- Accession Number

    -- 예약 단계 참조 (워크리스트에서)
    scheduled_step_id TEXT,                -- 예약 절차 단계 ID
    requested_proc_id TEXT,                -- 요청 절차 ID

    -- 수행 시리즈 (JSON 배열)
    performed_series TEXT,                 -- JSON: [{series_uid, protocol, images}]

    -- 메타데이터
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- 제약조건
    CHECK (status IN ('IN PROGRESS', 'COMPLETED', 'DISCONTINUED'))
);

-- 인덱스
CREATE INDEX idx_mpps_status ON mpps(status);
CREATE INDEX idx_mpps_station ON mpps(station_ae);
CREATE INDEX idx_mpps_study ON mpps(study_uid);
CREATE INDEX idx_mpps_date ON mpps(start_datetime);
```

---

### DES-DB-006: worklist 테이블

**추적성:** SRS-SVC-006, FR-3.3

```sql
CREATE TABLE worklist (
    -- 기본 키
    worklist_pk     INTEGER PRIMARY KEY AUTOINCREMENT,

    -- 예약 절차 단계
    step_id         TEXT NOT NULL,         -- 예약 절차 단계 ID
    step_status     TEXT DEFAULT 'SCHEDULED', -- SCHEDULED, STARTED, COMPLETED

    -- 환자 정보
    patient_id      TEXT NOT NULL,         -- 환자 ID
    patient_name    TEXT,                  -- 환자 이름
    birth_date      TEXT,                  -- 환자 생년월일
    sex             TEXT,                  -- 환자 성별

    -- 요청 절차
    accession_no    TEXT,                  -- Accession Number
    requested_proc_id TEXT,                -- 요청 절차 ID
    study_uid       TEXT,                  -- Study Instance UID (사전 할당)

    -- 예약 정보
    scheduled_datetime TEXT NOT NULL,      -- 예약 절차 단계 시작
    station_ae      TEXT,                  -- 예약 스테이션 AE 타이틀
    station_name    TEXT,                  -- 예약 스테이션 이름
    modality        TEXT NOT NULL,         -- 모달리티

    -- 절차 정보
    procedure_desc  TEXT,                  -- 예약 절차 단계 설명
    protocol_code   TEXT,                  -- 프로토콜 코드 시퀀스 (JSON)

    -- 의뢰의사
    referring_phys  TEXT,                  -- 의뢰의사 이름
    referring_phys_id TEXT,                -- 의뢰의사 ID

    -- 메타데이터
    created_at      TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT NOT NULL DEFAULT (datetime('now')),

    -- 제약조건
    UNIQUE (step_id, accession_no)
);

-- 인덱스
CREATE INDEX idx_worklist_station ON worklist(station_ae);
CREATE INDEX idx_worklist_modality ON worklist(modality);
CREATE INDEX idx_worklist_scheduled ON worklist(scheduled_datetime);
CREATE INDEX idx_worklist_patient ON worklist(patient_id);
CREATE INDEX idx_worklist_accession ON worklist(accession_no);
CREATE INDEX idx_worklist_status ON worklist(step_status);
```

---

### DES-DB-007: schema_version 테이블

**목적:** 마이그레이션을 위한 데이터베이스 스키마 버전 추적.

```sql
CREATE TABLE schema_version (
    version         INTEGER PRIMARY KEY,
    description     TEXT NOT NULL,
    applied_at      TEXT NOT NULL DEFAULT (datetime('now'))
);

-- 초기 버전
INSERT INTO schema_version (version, description)
VALUES (1, '초기 스키마 생성');
```

---

## 4. 인덱스 설계

### 4.1 주요 조회 패턴

| 조회 패턴 | 테이블 | 인덱스 전략 |
|----------|--------|------------|
| 환자 ID 조회 | patients | patient_id UNIQUE |
| 환자 이름 검색 | patients | patient_name B-tree |
| Accession 검사 조회 | studies | accession_no B-tree |
| 날짜 범위 검사 조회 | studies | study_date B-tree |
| 모달리티별 검사 조회 | studies | modalities_in_study B-tree |
| 모달리티별 시리즈 조회 | series | modality B-tree |
| SOP UID 인스턴스 조회 | instances | sop_uid UNIQUE |

### 4.2 복합 인덱스

```sql
-- 일반적인 조회를 위한 다중 컬럼 인덱스

-- 환자 이름 및 생년월일 검색
CREATE INDEX idx_patients_name_birth
ON patients(patient_name, birth_date);

-- 날짜 및 모달리티별 검사 검색
CREATE INDEX idx_studies_date_modality
ON studies(study_date, modalities_in_study);

-- 스테이션, 날짜, 모달리티별 워크리스트 조회
CREATE INDEX idx_worklist_station_date_mod
ON worklist(station_ae, scheduled_datetime, modality);
```

### 4.3 인덱스 크기 추정

| 테이블 | 예상 행 수 | 인덱스 크기 |
|--------|-----------|------------|
| patients | 10,000 | ~500 KB |
| studies | 50,000 | ~3 MB |
| series | 200,000 | ~12 MB |
| instances | 2,000,000 | ~150 MB |

---

## 5. 조회 패턴

### 5.1 환자 레벨 조회 (C-FIND)

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
    (? IS NULL OR p.patient_name LIKE ?)   -- 환자이름 매칭
    AND (? IS NULL OR p.patient_id = ?)    -- 환자ID 매칭
    AND (? IS NULL OR p.birth_date = ?)    -- 생년월일 매칭
ORDER BY p.patient_name
LIMIT ?;
```

### 5.2 검사 레벨 조회 (C-FIND)

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
    AND (? IS NULL OR s.study_date >= ?)   -- 날짜 범위 시작
    AND (? IS NULL OR s.study_date <= ?)   -- 날짜 범위 끝
    AND (? IS NULL OR s.accession_no = ?)
    AND (? IS NULL OR s.modalities_in_study LIKE ?)
ORDER BY s.study_date DESC, s.study_time DESC
LIMIT ?;
```

### 5.3 시리즈 레벨 조회 (C-FIND)

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

### 5.4 인스턴스 레벨 조회 (C-FIND)

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

### 5.5 파일 경로 조회 (C-MOVE/C-GET용)

```sql
-- DES-DB-Q005: 검사의 파일 경로 조회
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

### 5.6 워크리스트 조회 (MWL C-FIND)

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

## 6. 데이터 마이그레이션

### 6.1 마이그레이션 스크립트

```sql
-- 마이그레이션 v1 → v2: file_hash 컬럼 추가
BEGIN TRANSACTION;

ALTER TABLE instances ADD COLUMN file_hash TEXT;

UPDATE schema_version SET version = 2
WHERE version = (SELECT MAX(version) FROM schema_version);

INSERT INTO schema_version (version, description)
VALUES (2, 'instances 테이블에 file_hash 컬럼 추가');

COMMIT;
```

### 6.2 마이그레이션 전략

```cpp
// DES-DB-MIG: 마이그레이션 실행기
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

## 7. 성능 고려사항

### 7.1 SQLite 설정

```cpp
// DES-DB-CFG: 최적 SQLite 설정
void configure_database(sqlite3* db) {
    // 동시 읽기를 위한 WAL 모드 활성화
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    // 캐시 크기 설정 (64MB)
    sqlite3_exec(db, "PRAGMA cache_size=-65536;", nullptr, nullptr, nullptr);

    // 읽기 위주 워크로드 최적화
    sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);

    // 외래 키 활성화
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    // 메모리 매핑 I/O (256MB)
    sqlite3_exec(db, "PRAGMA mmap_size=268435456;", nullptr, nullptr, nullptr);

    // 임시 저장소를 메모리에
    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
}
```

### 7.2 조회 성능 목표

| 조회 유형 | 목표 지연시간 | 예상 데이터셋 |
|----------|-------------|-------------|
| 환자 조회 (ID별) | <1ms | 1만 환자 |
| 검사 검색 (날짜별) | <10ms | 5만 검사 |
| 시리즈 목록 | <5ms | 20만 시리즈 |
| 인스턴스 목록 | <20ms | 200만 인스턴스 |
| 워크리스트 조회 | <5ms | 1천 항목 |

### 7.3 유지보수 작업

```sql
-- DES-DB-MAINT: 정기 유지보수 쿼리

-- 쿼리 옵티마이저를 위한 테이블 분석
ANALYZE patients;
ANALYZE studies;
ANALYZE series;
ANALYZE instances;

-- 인덱스 재구축 (단편화된 경우)
REINDEX idx_studies_date;
REINDEX idx_instances_series;

-- 공간 회수를 위한 VACUUM (많은 삭제 후)
VACUUM;

-- 데이터베이스 무결성 검사
PRAGMA integrity_check;
```

### 7.4 용량 계획

| 지표 | 계산식 | 예시 (100만 이미지) |
|-----|-------|-------------------|
| 데이터베이스 크기 | ~200 bytes/인스턴스 | ~200 MB |
| 인덱스 크기 | ~100 bytes/인스턴스 | ~100 MB |
| 메모리 (캐시) | DB 크기의 10% | ~30 MB |
| 삽입 속도 | 지속 ~100/초 | - |
| 조회 속도 | ~1000/초 | - |

---

*문서 버전: 1.0.0*
*작성일: 2025-11-30*
*작성자: kcenon@naver.com*
