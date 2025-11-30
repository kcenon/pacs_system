# SDS - Sequence Diagrams

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2025-11-30

---

## Table of Contents

- [1. Association Establishment](#1-association-establishment)
- [2. C-STORE Operation](#2-c-store-operation)
- [3. C-FIND Operation](#3-c-find-operation)
- [4. C-MOVE Operation](#4-c-move-operation)
- [5. Worklist Query](#5-worklist-query)
- [6. MPPS N-CREATE/N-SET](#6-mpps-n-createn-set)
- [7. Error Handling Scenarios](#7-error-handling-scenarios)

---

## 1. Association Establishment

### SEQ-001: SCU Initiated Association

**Traces to:** SRS-NET-003, FR-2.2

```
┌─────────┐                                    ┌─────────┐
│   SCU   │                                    │   SCP   │
│(Client) │                                    │(Server) │
└────┬────┘                                    └────┬────┘
     │                                              │
     │  1. TCP Connect                              │
     │─────────────────────────────────────────────►│
     │                                              │
     │  2. A-ASSOCIATE-RQ                           │
     │  ┌────────────────────────────────────┐      │
     │  │ Calling AE: MY_SCU                 │      │
     │  │ Called AE: PACS_SCP                │      │
     │  │ Application Context: 1.2.840...    │      │
     │  │ Presentation Contexts:             │      │
     │  │   PC1: CT Image Storage            │      │
     │  │        - Implicit VR LE            │      │
     │  │        - Explicit VR LE            │      │
     │  │   PC2: Verification                │      │
     │  │        - Implicit VR LE            │      │
     │  │ Max PDU Size: 16384                │      │
     │  └────────────────────────────────────┘      │
     │─────────────────────────────────────────────►│
     │                                              │
     │                                    ┌─────────┴─────────┐
     │                                    │ Validate:         │
     │                                    │ - Called AE Title │
     │                                    │ - SOP Classes     │
     │                                    │ - Transfer Syntax │
     │                                    └─────────┬─────────┘
     │                                              │
     │  3. A-ASSOCIATE-AC                           │
     │  ┌────────────────────────────────────┐      │
     │  │ Accepted Contexts:                 │      │
     │  │   PC1: Accepted (Explicit VR LE)   │      │
     │  │   PC2: Accepted (Implicit VR LE)   │      │
     │  │ Max PDU Size: 16384                │      │
     │  └────────────────────────────────────┘      │
     │◄─────────────────────────────────────────────│
     │                                              │
     │            [Association Established]         │
     │                                              │
```

### SEQ-002: Association Rejection

**Traces to:** SRS-NET-003

```
┌─────────┐                                    ┌─────────┐
│   SCU   │                                    │   SCP   │
└────┬────┘                                    └────┬────┘
     │                                              │
     │  1. TCP Connect                              │
     │─────────────────────────────────────────────►│
     │                                              │
     │  2. A-ASSOCIATE-RQ                           │
     │  ┌────────────────────────────────────┐      │
     │  │ Called AE: UNKNOWN_AE              │      │
     │  └────────────────────────────────────┘      │
     │─────────────────────────────────────────────►│
     │                                              │
     │                                    ┌─────────┴─────────┐
     │                                    │ AE Title not in   │
     │                                    │ whitelist         │
     │                                    └─────────┬─────────┘
     │                                              │
     │  3. A-ASSOCIATE-RJ                           │
     │  ┌────────────────────────────────────┐      │
     │  │ Result: Rejected (Permanent)       │      │
     │  │ Source: Service Provider (ACSE)    │      │
     │  │ Reason: Called AE Title Not Recog. │      │
     │  └────────────────────────────────────┘      │
     │◄─────────────────────────────────────────────│
     │                                              │
     │  4. TCP Close                                │
     │◄─────────────────────────────────────────────│
     │                                              │
```

### SEQ-003: Association Release

**Traces to:** SRS-NET-003

```
┌─────────┐                                    ┌─────────┐
│   SCU   │                                    │   SCP   │
└────┬────┘                                    └────┬────┘
     │                                              │
     │         [Association Established]            │
     │                                              │
     │  1. A-RELEASE-RQ                             │
     │─────────────────────────────────────────────►│
     │                                              │
     │                                    ┌─────────┴─────────┐
     │                                    │ Cleanup session   │
     │                                    │ resources         │
     │                                    └─────────┬─────────┘
     │                                              │
     │  2. A-RELEASE-RP                             │
     │◄─────────────────────────────────────────────│
     │                                              │
     │  3. TCP Close                                │
     │─────────────────────────────────────────────►│
     │                                              │
```

---

## 2. C-STORE Operation

### SEQ-004: Successful C-STORE

**Traces to:** SRS-SVC-002, FR-3.1

```
┌──────────┐          ┌──────────┐          ┌──────────┐          ┌──────────┐
│ Modality │          │StorageSCP│          │FileStorage│         │IndexDB   │
│  (SCU)   │          │          │          │          │          │          │
└────┬─────┘          └────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │                     │
     │  1. C-STORE-RQ      │                     │                     │
     │  (P-DATA-TF)        │                     │                     │
     │  ┌─────────────────────────────────┐      │                     │
     │  │ Command Set:                     │     │                     │
     │  │   CommandField: C-STORE-RQ       │     │                     │
     │  │   MessageID: 1                   │     │                     │
     │  │   AffectedSOPClass: CT Image     │     │                     │
     │  │   AffectedSOPInstance: 1.2.3...  │     │                     │
     │  │   Priority: MEDIUM               │     │                     │
     │  └─────────────────────────────────┘      │                     │
     │────────────────────►│                     │                     │
     │                     │                     │                     │
     │  2. Data Set        │                     │                     │
     │  (P-DATA-TF)        │                     │                     │
     │  ┌─────────────────────────────────┐      │                     │
     │  │ Patient/Study/Series info       │      │                     │
     │  │ Image attributes                │      │                     │
     │  │ Pixel Data                      │      │                     │
     │  └─────────────────────────────────┘      │                     │
     │────────────────────►│                     │                     │
     │                     │                     │                     │
     │                     │ 3. Validate dataset │                     │
     │                     │    - Check SOP Class│                     │
     │                     │    - Verify attrs   │                     │
     │                     │                     │                     │
     │                     │ 4. Check duplicate  │                     │
     │                     │─────────────────────────────────────────►│
     │                     │                     │                     │
     │                     │◄─────────────────────────────────────────│
     │                     │    (not exists)     │                     │
     │                     │                     │                     │
     │                     │ 5. Store file       │                     │
     │                     │────────────────────►│                     │
     │                     │                     │                     │
     │                     │                     │ 6. Write DICOM file │
     │                     │                     │    to filesystem    │
     │                     │                     │                     │
     │                     │◄────────────────────│                     │
     │                     │    (success)        │                     │
     │                     │                     │                     │
     │                     │ 7. Update index     │                     │
     │                     │─────────────────────────────────────────►│
     │                     │                     │                     │
     │                     │                     │    8. INSERT into   │
     │                     │                     │    patient/study/   │
     │                     │                     │    series/instance  │
     │                     │                     │    tables           │
     │                     │                     │                     │
     │                     │◄─────────────────────────────────────────│
     │                     │    (success)        │                     │
     │                     │                     │                     │
     │  9. C-STORE-RSP     │                     │                     │
     │  ┌─────────────────────────────────┐      │                     │
     │  │ CommandField: C-STORE-RSP        │     │                     │
     │  │ MessageIDRespondedTo: 1          │     │                     │
     │  │ Status: 0x0000 (Success)         │     │                     │
     │  └─────────────────────────────────┘      │                     │
     │◄────────────────────│                     │                     │
     │                     │                     │                     │
```

### SEQ-005: C-STORE with Duplicate Handling

**Traces to:** SRS-STOR-002

```
┌──────────┐          ┌──────────┐          ┌──────────┐
│ Modality │          │StorageSCP│          │IndexDB   │
└────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │
     │  C-STORE-RQ         │                     │
     │────────────────────►│                     │
     │                     │                     │
     │                     │ Check duplicate     │
     │                     │────────────────────►│
     │                     │                     │
     │                     │◄────────────────────│
     │                     │ (EXISTS: dup found) │
     │                     │                     │
     │                     │                     │
     │           ┌─────────┴─────────┐           │
     │           │ Duplicate Policy: │           │
     │           │ ┌───────────────┐ │           │
     │           │ │ REJECT        │ │           │
     │           │ │ → Return error│ │           │
     │           │ └───────────────┘ │           │
     │           │ ┌───────────────┐ │           │
     │           │ │ REPLACE       │ │           │
     │           │ │ → Overwrite   │ │           │
     │           │ └───────────────┘ │           │
     │           │ ┌───────────────┐ │           │
     │           │ │ IGNORE        │ │           │
     │           │ │ → Skip, OK    │ │           │
     │           │ └───────────────┘ │           │
     │           └─────────┬─────────┘           │
     │                     │                     │
     │  C-STORE-RSP        │                     │
     │  (Based on policy)  │                     │
     │◄────────────────────│                     │
     │                     │                     │
```

---

## 3. C-FIND Operation

### SEQ-006: Patient Root Query

**Traces to:** SRS-SVC-004, FR-3.2

```
┌──────────┐          ┌──────────┐          ┌──────────┐
│  Viewer  │          │ QuerySCP │          │IndexDB   │
│  (SCU)   │          │          │          │          │
└────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │
     │  1. C-FIND-RQ       │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ AffectedSOPClass: PatientRoot   │      │
     │  │ Query Level: STUDY              │      │
     │  │ Matching Keys:                  │      │
     │  │   PatientName: "DOE^J*"         │      │
     │  │   StudyDate: "20250101-"        │      │
     │  │ Return Keys:                    │      │
     │  │   StudyInstanceUID              │      │
     │  │   StudyDescription              │      │
     │  │   Modality                      │      │
     │  └─────────────────────────────────┘      │
     │────────────────────►│                     │
     │                     │                     │
     │                     │ 2. Build SQL Query  │
     │                     │────────────────────►│
     │                     │                     │
     │                     │   SELECT * FROM studies s │
     │                     │   JOIN patients p ON ...  │
     │                     │   WHERE p.name LIKE 'DOE^J%' │
     │                     │   AND s.study_date >= '20250101' │
     │                     │                     │
     │                     │◄────────────────────│
     │                     │   (Result Set: 3 rows) │
     │                     │                     │
     │  3. C-FIND-RSP #1   │                     │
     │  (Status: Pending)  │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ PatientName: "DOE^JOHN"         │      │
     │  │ StudyInstanceUID: 1.2.3...      │      │
     │  │ StudyDescription: "CT CHEST"    │      │
     │  │ Modality: "CT"                  │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
     │  4. C-FIND-RSP #2   │                     │
     │  (Status: Pending)  │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ PatientName: "DOE^JANE"         │      │
     │  │ StudyInstanceUID: 1.2.4...      │      │
     │  │ StudyDescription: "MR BRAIN"    │      │
     │  │ Modality: "MR"                  │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
     │  5. C-FIND-RSP #3   │                     │
     │  (Status: Pending)  │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ PatientName: "DOE^JOHN"         │      │
     │  │ StudyInstanceUID: 1.2.5...      │      │
     │  │ StudyDescription: "XR CHEST"    │      │
     │  │ Modality: "CR"                  │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
     │  6. C-FIND-RSP      │                     │
     │  (Status: Success)  │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ No Dataset                      │      │
     │  │ Status: 0x0000 (Success)        │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
```

### SEQ-007: C-FIND Cancel

**Traces to:** SRS-SVC-004

```
┌──────────┐          ┌──────────┐          ┌──────────┐
│  Viewer  │          │ QuerySCP │          │IndexDB   │
└────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │
     │  C-FIND-RQ          │                     │
     │────────────────────►│                     │
     │                     │                     │
     │                     │ Query (many results)│
     │                     │────────────────────►│
     │                     │                     │
     │  C-FIND-RSP (Pending) │◄──────────────────│
     │◄────────────────────│   (1000+ matches)  │
     │                     │                     │
     │  C-FIND-RSP (Pending) │                   │
     │◄────────────────────│                     │
     │                     │                     │
     │  C-CANCEL-RQ        │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ MessageIDToCancel: 1            │      │
     │  └─────────────────────────────────┘      │
     │────────────────────►│                     │
     │                     │                     │
     │                     │ Cancel query       │
     │                     │────────────────────►│
     │                     │                     │
     │  C-FIND-RSP         │                     │
     │  (Status: Canceled) │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ Status: 0xFE00 (Cancel)         │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
```

---

## 4. C-MOVE Operation

### SEQ-008: C-MOVE Retrieve

**Traces to:** SRS-SVC-005, FR-3.2

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Viewer  │     │RetrieveSCP│    │FileStorage│    │DestSCP  │
│  (SCU)   │     │          │     │          │     │(Viewer) │
└────┬─────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
     │                │                │                │
     │  1. C-MOVE-RQ  │                │                │
     │  ┌───────────────────────────────────────┐      │
     │  │ MoveDestination: VIEWER_SCP           │      │
     │  │ QueryLevel: STUDY                     │      │
     │  │ StudyInstanceUID: 1.2.3...            │      │
     │  └───────────────────────────────────────┘      │
     │───────────────►│                │                │
     │                │                │                │
     │                │ 2. Lookup images │              │
     │                │───────────────►│                │
     │                │                │                │
     │                │◄───────────────│                │
     │                │ (50 instances) │                │
     │                │                │                │
     │                │ 3. Connect to Destination       │
     │                │────────────────────────────────►│
     │                │                │                │
     │                │◄────────────────────────────────│
     │                │ (Association established)       │
     │                │                │                │
     │  4. C-MOVE-RSP │                │                │
     │  (Pending: 50) │                │                │
     │◄───────────────│                │                │
     │                │                │                │
     │                │ 5. C-STORE-RQ (image 1)        │
     │                │────────────────────────────────►│
     │                │                │                │
     │                │◄────────────────────────────────│
     │                │ C-STORE-RSP (Success)           │
     │                │                │                │
     │  6. C-MOVE-RSP │                │                │
     │  (Pending: 49) │                │                │
     │◄───────────────│                │                │
     │                │                │                │
     │                │     ... (repeat for 48 more images) ...
     │                │                │                │
     │                │ C-STORE-RQ (image 50)          │
     │                │────────────────────────────────►│
     │                │                │                │
     │                │◄────────────────────────────────│
     │                │ C-STORE-RSP (Success)           │
     │                │                │                │
     │  7. C-MOVE-RSP │                │                │
     │  (Success)     │                │                │
     │  ┌───────────────────────────────────────┐      │
     │  │ Status: 0x0000 (Success)              │      │
     │  │ NumberOfCompletedSuboperations: 50    │      │
     │  │ NumberOfFailedSuboperations: 0        │      │
     │  │ NumberOfWarningSuboperations: 0       │      │
     │  └───────────────────────────────────────┘      │
     │◄───────────────│                │                │
     │                │                │                │
     │                │ 8. Release to Destination      │
     │                │────────────────────────────────►│
     │                │                │                │
```

---

## 5. Worklist Query

### SEQ-009: Modality Worklist Query

**Traces to:** SRS-SVC-006, FR-3.3

```
┌──────────┐          ┌──────────┐          ┌──────────┐
│ Modality │          │WorklistSCP│         │ RIS/HIS  │
│  (SCU)   │          │          │          │ Interface│
└────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │
     │  1. C-FIND-RQ       │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ AffectedSOPClass: MWL           │      │
     │  │ Matching Keys:                  │      │
     │  │   ScheduledStationAETitle: CT01 │      │
     │  │   ScheduledProcedureDate: TODAY │      │
     │  │ Return Keys:                    │      │
     │  │   PatientName                   │      │
     │  │   PatientID                     │      │
     │  │   AccessionNumber               │      │
     │  │   ScheduledProcedureStepSeq     │      │
     │  │     - ScheduledProcedureStepID  │      │
     │  │     - Modality                  │      │
     │  │     - ScheduledProcedureDesc    │      │
     │  └─────────────────────────────────┘      │
     │────────────────────►│                     │
     │                     │                     │
     │                     │ 2. Query RIS       │
     │                     │────────────────────►│
     │                     │                     │
     │                     │◄────────────────────│
     │                     │ (Scheduled exams)  │
     │                     │                     │
     │  3. C-FIND-RSP      │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ PatientName: "DOE^JOHN"         │      │
     │  │ PatientID: "12345"              │      │
     │  │ AccessionNumber: "ACC001"       │      │
     │  │ ScheduledProcedureStepSequence: │      │
     │  │   Item:                         │      │
     │  │     StepID: "STEP001"           │      │
     │  │     Modality: "CT"              │      │
     │  │     Description: "CT Chest"     │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
     │  ... (more results) │                     │
     │                     │                     │
     │  4. C-FIND-RSP (Success - no data)       │
     │◄────────────────────│                     │
     │                     │                     │
```

---

## 6. MPPS N-CREATE/N-SET

### SEQ-010: MPPS In Progress

**Traces to:** SRS-SVC-007, FR-3.4

```
┌──────────┐          ┌──────────┐          ┌──────────┐
│ Modality │          │ MPPS_SCP │          │ RIS/HIS  │
│  (SCU)   │          │          │          │ Interface│
└────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │
     │  [Exam Started]     │                     │
     │                     │                     │
     │  1. N-CREATE-RQ     │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ AffectedSOPClass: MPPS          │      │
     │  │ AffectedSOPInstance: 1.2.3...   │      │
     │  │ Dataset:                        │      │
     │  │   PerformedStationAETitle: CT01 │      │
     │  │   PerformedProcedureStepStatus: │      │
     │  │       "IN PROGRESS"             │      │
     │  │   PerformedProcedureStepStartDT:│      │
     │  │       "20250601120000"          │      │
     │  │   ScheduledStepAttributesSeq:   │      │
     │  │     - StudyInstanceUID          │      │
     │  │     - AccessionNumber           │      │
     │  │     - RequestedProcedureID      │      │
     │  └─────────────────────────────────┘      │
     │────────────────────►│                     │
     │                     │                     │
     │                     │ 2. Store MPPS      │
     │                     │ 3. Notify RIS      │
     │                     │────────────────────►│
     │                     │                     │
     │                     │◄────────────────────│
     │                     │                     │
     │  4. N-CREATE-RSP    │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ Status: 0x0000 (Success)        │      │
     │  │ AffectedSOPInstance: 1.2.3...   │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
```

### SEQ-011: MPPS Completed

**Traces to:** SRS-SVC-007

```
┌──────────┐          ┌──────────┐          ┌──────────┐
│ Modality │          │ MPPS_SCP │          │ RIS/HIS  │
└────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │
     │  [Exam Completed]   │                     │
     │                     │                     │
     │  1. N-SET-RQ        │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ RequestedSOPClass: MPPS         │      │
     │  │ RequestedSOPInstance: 1.2.3...  │      │
     │  │ ModificationList:               │      │
     │  │   PerformedProcedureStepStatus: │      │
     │  │       "COMPLETED"               │      │
     │  │   PerformedProcedureStepEndDT:  │      │
     │  │       "20250601123000"          │      │
     │  │   PerformedSeriesSequence:      │      │
     │  │     - SeriesInstanceUID         │      │
     │  │     - PerformedProtocolName     │      │
     │  │     - ReferencedImageSequence:  │      │
     │  │         (list of SOP instances) │      │
     │  └─────────────────────────────────┘      │
     │────────────────────►│                     │
     │                     │                     │
     │                     │ 2. Update MPPS     │
     │                     │ 3. Notify RIS      │
     │                     │────────────────────►│
     │                     │                     │
     │                     │   [Procedure done] │
     │                     │                     │
     │                     │◄────────────────────│
     │                     │                     │
     │  4. N-SET-RSP       │                     │
     │  ┌─────────────────────────────────┐      │
     │  │ Status: 0x0000 (Success)        │      │
     │  └─────────────────────────────────┘      │
     │◄────────────────────│                     │
     │                     │                     │
```

---

## 7. Error Handling Scenarios

### SEQ-012: Network Timeout

**Traces to:** SRS-NET-004, NFR-2

```
┌──────────┐          ┌──────────┐
│   SCU    │          │   SCP    │
└────┬─────┘          └────┬─────┘
     │                     │
     │  C-STORE-RQ         │
     │────────────────────►│
     │                     │
     │                     │ [Processing...]
     │                     │
     │  [Timeout: 30s]     │
     │                     │
     │  A-ABORT            │
     │  ┌─────────────────────────────────┐
     │  │ Source: Service User           │
     │  │ Reason: Reason Not Specified   │
     │  └─────────────────────────────────┘
     │────────────────────►│
     │                     │
     │                     │ [Cleanup resources]
     │                     │
     │  TCP Close          │
     │────────────────────►│
     │                     │
     │  [Retry with exponential backoff?] │
     │                     │
```

### SEQ-013: Storage Error Recovery

**Traces to:** SRS-STOR-002, NFR-2

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│ Modality │     │StorageSCP│     │FileStorage│    │IndexDB   │
└────┬─────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
     │                │                │                │
     │  C-STORE-RQ    │                │                │
     │───────────────►│                │                │
     │                │                │                │
     │                │ Store file     │                │
     │                │───────────────►│                │
     │                │                │                │
     │                │                │ [DISK FULL ERROR]
     │                │◄───────────────│                │
     │                │ (Error)        │                │
     │                │                │                │
     │                │ [Transaction rollback - no index update]
     │                │                │                │
     │  C-STORE-RSP   │                │                │
     │  (Error)       │                │                │
     │  ┌─────────────────────────────────┐             │
     │  │ Status: 0xA700 (Out of Resources)│            │
     │  │ ErrorComment: "Storage full"    │             │
     │  └─────────────────────────────────┘             │
     │◄───────────────│                │                │
     │                │                │                │
```

### SEQ-014: Association Abort During Operation

**Traces to:** SRS-NET-003

```
┌──────────┐          ┌──────────┐          ┌──────────┐
│ Modality │          │StorageSCP│          │FileStorage│
└────┬─────┘          └────┬─────┘          └────┬─────┘
     │                     │                     │
     │  C-STORE-RQ (large) │                     │
     │────────────────────►│                     │
     │                     │                     │
     │  [Network interruption]                   │
     │                     │                     │
     │  [TCP RST received] │                     │
     │                     │                     │
     │                     │ [Detected disconnect]│
     │                     │                     │
     │                     │ Rollback partial write
     │                     │────────────────────►│
     │                     │                     │
     │                     │◄────────────────────│
     │                     │ (File deleted)      │
     │                     │                     │
     │                     │ Log error           │
     │                     │ Cleanup association │
     │                     │ Release resources   │
     │                     │                     │
```

---

*Document Version: 1.0.0*
*Created: 2025-11-30*
*Author: kcenon@naver.com*
