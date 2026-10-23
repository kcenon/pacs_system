---
doc_id: PAC-QUAL-012
doc_title: "ISO 14971 risk management checklist"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
---

# ISO 14971 risk management checklist

Candidate basis: ISO 14971:2019, Clauses 4 through 10. Assess the device across
its lifecycle and intended environment. ISO/TR 24971:2020 is supporting guidance,
not an additional set of normative requirements. The examples below are prompts
for hazard analysis; no severity, probability, acceptability or safety class has
been assigned.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Risk process and analysis

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-14971-01 | 4.1 through 4.3 | Establish the risk process, management policy, resources and competent reviewers for imaging and clinical use. | Approved policy, responsibilities, competence and process review records. |
| PAC-14971-02 | 4.4 | Plan scope, responsibilities, review points, verification, acceptability criteria and overall residual-risk evaluation before judging results. | Approved risk management plan, including cases where probability cannot be estimated. |
| PAC-14971-03 | 4.5 | Maintain a risk file that connects each hazard and hazardous situation to evaluation, controls, verification and residual risk. | Risk-file index and complete traceability. |
| PAC-14971-04 | 5.1, 5.2 | Define intended medical use, patient population, user qualifications and foreseeable misuse, including use of research or incomplete functions in diagnosis. | Approved intended use, exclusions, misuse analysis and clinical review. |
| PAC-14971-05 | 5.3, 5.4 | Analyze wrong patient, study, series, instance or frame association, including UID collisions, patient reconciliation, routing, caching and retrieval. | Hazard sequences and controls for identity, identifier changes and result provenance. |
| PAC-14971-06 | 5.3, 5.4 | Analyze loss or alteration of orientation, pixel spacing, frame order, rescale values and other metadata during ingestion, conversion and export. | Geometry and metadata hazards, reference datasets and preservation checks. |
| PAC-14971-07 | 5.3, 5.4 | Analyze pixel signedness, rescale/HU conversion, photometric interpretation, lossy encoding and incorrect window/level. | Image-fidelity analysis and input/processing limitations. |
| PAC-14971-08 | 5.3, 5.4 | Analyze incorrect stored measurements, units, annotation ownership, source references and stale edits presented to a downstream viewer. | Data contracts, concurrent-edit controls, provenance and end-to-end display/export checks; identify who computes the measurement. |
| PAC-14971-09 | 5.3, 5.4 | Analyze false C-STORE success or Storage Commitment success before durable storage, including disk-full, crash, power loss and object-store failure. | Durability assumptions, acknowledgement rules, fault-injection evidence and reconciliation/recovery controls. |
| PAC-14971-10 | 5.3, 5.4 | Analyze AI results linked to the wrong patient, model or source image, incomplete SR/SEG validation, duplicate callbacks and delayed or failed inference. | AI interface hazards, model/version provenance, supported-template limits and responsibility for clinical interpretation. |
| PAC-14971-11 | 5.3, 5.4 | Analyze stale query/prefetch caches, missing instances, queue overload, interrupted retrieval, retry storms and concurrent shutdown. | Availability and timing hazards, resource limits, completeness detection and recovery controls. |
| PAC-14971-12 | 5.3, 5.4 | Analyze unauthorized access, image/result modification, audit loss and availability attacks for their possible clinical consequences. | Linked threat and safety analyses; failure sequences across trust boundaries. |
| PAC-14971-13 | 5.3, 5.4 | Analyze file/index divergence, failed migrations, incomplete replicas, destructive deletion, retention expiry and backups that cannot restore a complete study. | Persistence and recovery hazards, consistency checks, restore evidence and justified recovery objectives. |
| PAC-14971-14 | 5.5, 6 | Estimate and evaluate risks using the approved method, addressing uncertainty and systematic software faults. | Documented estimates, assumptions and decisions; rationale beyond a CVSS score or passing test count. |
| PAC-14971-25 | 5.3, 5.4 | Analyze incorrect worklist/order updates, MPPS/UPS transitions, study-lock overrides and routing rules that assign the wrong examination or conceal incomplete work. | Workflow hazard sequences, ownership/state-transition controls and supported RIS/modality integration evidence. |
| PAC-14971-26 | 5.3, 5.4 | Analyze incomplete de-identification and identifier remapping that leaks patient information or breaks source/result relationships when data is reused. | Confidentiality-profile scope, private-tag/pixel/SR handling, consistent mappings and linked safety/privacy controls. |

## Controls and residual risk

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-14971-15 | 7.1 | Consider safer design and protective measures before relying on warnings or user training. | Risk-control option analysis with selection rationale. |
| PAC-14971-16 | 7.2 | Verify implementation and effectiveness of controls for patient identity, pixel integrity, durable storage, completeness and recovery. | Control-specific verification and failure-injection results with predefined acceptance criteria. |
| PAC-14971-17 | 7.3 | Evaluate individual residual risks and determine what must be disclosed to users. | Residual-risk decisions and traceable safety information. |
| PAC-14971-18 | 7.4 | Where further reduction is impracticable and criteria are not met, assess benefit against residual risk using relevant clinical evidence. | Documented benefit-risk analysis and authorized conclusion. |
| PAC-14971-19 | 7.5, 7.6 | Check for new risks from controls and confirm all hazardous situations have been addressed. | Completeness review and analysis of control interactions. |
| PAC-14971-20 | 8 | Evaluate overall residual risk across the complete deployed product, including storage, network, UI and external AI/viewer interactions. | Overall evaluation against the plan, clinical input, shared responsibilities and required disclosures. |
| PAC-14971-21 | 9 | Review execution of the plan, acceptability and readiness to collect production/post-production information before release. | Approved risk management report and release linkage. |

## Production and post-production

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-14971-22 | 10.1, 10.2 | Collect complaints, clinical incidents, near misses, supplier defects, interoperability failures and new vulnerability information. | Information sources, owners, review frequency and field-data records. |
| PAC-14971-23 | 10.3 | Evaluate new information for unidentified hazards, changed acceptability or changes to the benefit-risk conclusion. | Periodic and event-triggered risk review records. |
| PAC-14971-24 | 10.4 | Take and verify actions affecting installed systems, technical documentation, users and future releases. | Field action/change records, notification decisions, effectiveness evidence and updated risk file. |

## Evidence starting points

- [Storage](../../src/storage), [services](../../src/services), [routing and retrieval](../../src/client), [workflow design](../SDS_WORKFLOW.md).
- [Codecs](../../src/encoding/compression), [DICOMweb rendering](../../src/web/endpoints/dicomweb_endpoints.cpp), [AI results](../../src/ai/ai_result_handler.cpp).
- [Usability](iec-62366-1-checklist.md) and [security](iec-81001-5-1-checklist.md) findings feed the same safety risk decisions.
