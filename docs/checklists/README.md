---
doc_id: PAC-QUAL-009
doc_title: "SaMD certification readiness checklists"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
baseline_commit: ddea5fe4cbd58b8165a4fffe719851812e26076d
---

# SaMD certification readiness checklists

Use this collection to assess the evidence needed when `pacs_system` is supplied
as a medical imaging product or as a component of one. The nine checklists cover
software development, risk, usability, cybersecurity, product validation,
manufacturer quality controls and a potential South Korean MFDS submission.

No medical device qualification, regulatory class, software safety class, QMS
certification, market authorization or conformity finding is established here.
The standards are candidates for a recorded adoption decision. All checklist
rows start unreviewed; existing document status labels and CI results do not
automatically satisfy them. The questions are grouped review prompts, not a
complete transcription of normative requirements. Reconcile clause coverage
with the adopted controlled texts before making a conformity claim.

## Product boundary and applicability

Approve the supplied product and intended purpose before selecting obligations.
Record included, excluded and undecided functions for the actual release, with
the build options and deployment controls that enforce that scope.

| Supply scenario | Review focus | Decision still required |
|---|---|---|
| DICOM library or storage/transfer component | Component requirements, hazards, configuration, verification, known anomalies, integration instructions and support | Whether the supplied function qualifies as a device; allocation of responsibilities to the final manufacturer and component supplier |
| Clinical PACS product or hosted service | Complete deployed product, users, data integrity, availability, security and manufacturer processes | Intended use, market, product code/class and authorization route |
| PACS integrated with a viewer or AI service | End-to-end patient/result identity, processing, clinical claims and changes across product boundaries | Who owns each claim, model, interface, validation activity and regulatory obligation |

The repository contains C++ libraries/services, a React management UI, CLI tools,
DICOM/DICOMweb interfaces, local and optional cloud storage, codecs, rendered
images, workflow services and external AI integration. Code availability does
not establish a clinical claim or inclusion in the released package. The
[package state](../PACKAGE_STATE.md) distinguishes published v0.1.0 from later
source and the unreleased v1.0 milestone.

Identify all supplied and required dependencies: OS, browser, database, codecs,
storage, network, certificates, reverse proxy, cloud services, external AI and
connected modalities/viewers. Record manufacturer, supplier and hospital
responsibilities. For `dicom_viewer` integration, exchange controlled evidence
with its owner and validate the actual combined versions and configuration.

The name PACS, open-source licensing, an AI connector or a storage-only deployment
does not settle qualification in every market. Use the
[reference register](regulatory-references.md) to document the jurisdictional
decision. Manufacturer QMS and MFDS reviews are conditional on the chosen role
and product; supplier evidence can still be needed for the final product.

## Checklist index

| Checklist | Review scope | PACS emphasis |
|---|---|---|
| [IEC 62304](iec-62304-checklist.md) | Software lifecycle, Clauses 4 through 9 | Requirements, interfaces, SOUP, verification, release and changes |
| [ISO 13485](iso-13485-checklist.md) | Manufacturer QMS, Clauses 4 through 8 | Organizational records, suppliers, installation and support |
| [ISO 14971](iso-14971-checklist.md) | Risk management, Clauses 4 through 10 | Patient identity, durable storage, fidelity, availability and recovery |
| [IEC 62366-1](iec-62366-1-checklist.md) | Usability, Clauses 4 and 5; conditional Annex C | Patient/worklist edits, routing, deletion and recovery interfaces |
| [IEC 81001-5-1](iec-81001-5-1-checklist.md) | Security lifecycle, Clauses 4 through 9; conditional Annex F | DIMSE, web, IHE, AI, cloud and update trust boundaries |
| [IEC 82304-1](iec-82304-1-checklist.md) | Product safety, Clauses 4 through 8 | Validation of the complete supplied product and operating environment |
| [MFDS digital device approval](mfds-digital-device-approval-checklist.md) | Qualification, application evidence, labeling and changes | Function-level purpose, product boundary and evidence consistency |
| [MFDS digital GMP](mfds-digital-gmp-checklist.md) | Annexes 2 and 3; conditional Annex 4 | QMS, software and role-specific AI controls |
| [MFDS digital cybersecurity](mfds-digital-cybersecurity-checklist.md) | Protection, lifecycle, response and submission evidence | Product/provider responsibilities and verified safeguards |

## Decisions required before assessment

These decisions are **undecided in this checklist package**. Role names propose
accountability; they neither appoint an individual nor record an approval.

| ID | Decision to record | Suggested accountable role |
|---|---|---|
| PAC-DEC-01 | Legal manufacturer or component supplier, product/model, release authority and support owner | Management and quality |
| PAC-DEC-02 | Intended purpose, clinical claims, users, patient population, care setting, contraindications and exclusions | Product, clinical and regulatory |
| PAC-DEC-03 | Markets, device qualification, code, regulatory class and authorization route | Regulatory |
| PAC-DEC-04 | IEC 62304 system/item safety classes and segregation rationale; reconcile SOUP class labels | Risk and software leads |
| PAC-DEC-05 | Included libraries, services, UI, tools, codecs, rendered output, workflows and external AI functions | Product and software |
| PAC-DEC-06 | Supported OS/browser, schema, storage, network, certificates, peers and deployment configurations | Software and operations |
| PAC-DEC-07 | Adopted standards, editions, national versions, amendments, interpretations and justified exclusions | Regulatory and quality |
| PAC-DEC-08 | AI role: result storage, inference orchestration or model supplier; model ownership, learning behavior and additional controls | Product, software and regulatory |
| PAC-DEC-09 | Support period, supplier monitoring, update commitments, complaint intake, incident response and retirement | Management and operations |
| PAC-DEC-10 | Responsibilities and evidence exchanged with downstream viewers, AI providers, integrators and hospitals | Product, quality and integration owners |

## How to record a review

1. Copy the [review record template](review-record-template.md) into a controlled
   review location. Identify the full product SHA, resolved dependencies, build
   features, artifacts, deployment, checklist revision and adopted source texts.
2. Create a row for every selected checklist ID. List omitted IDs as
   `Not reviewed`; record why a checklist is deferred or excluded.
3. Keep applicability, finding and evidence availability separate. Evidence
   requested from a supplier or the QMS is `Pending evidence`, not proof that a
   process or record is absent.
4. Use `Satisfied` only after reviewing evidence for the stated configuration.
   Split grouped prompts into child rows when needed; a parent remains open
   while an applicable child is open. Non-applicability needs an approved reason.
5. Assign open actions an owner, acceptance criteria and due date. Trace risks
   through controls, requirements, design, implementation and executed results.
   Preserve identifiers and revisions for evidence held outside the repository.
6. Keep patient data, credentials, licensed standards and confidential QMS
   records in approved locations. Public records should reference controlled
   identifiers or de-identified evidence.

A merged documentation PR authorizes a repository change. Product release,
risk acceptance and regulatory decisions require their own controlled records.
These blank checklists and baseline observations are not an executed assessment.

## Repository evidence starting points

| Area | Existing material | Evidence to establish for the selected release |
|---|---|---|
| Product and design | [PRD](../PRD.md), [SRS](../SRS.md), [SDS](../SDS.md), [architecture](../ARCHITECTURE.md) | Approved scope, consistent claims and current design |
| Configuration and supply | [Package state](../PACKAGE_STATE.md), [SOUP](../SOUP.md), [manifest](../../dependency-manifest.json), [CMake](../../CMakeLists.txt), [SBOM workflow](../../.github/workflows/sbom.yml) | Resolved versions, supplier assessment, build provenance and artifact retention |
| Interoperability | [DICOM statement](../DICOM_CONFORMANCE_STATEMENT.md), [IHE statement](../IHE_INTEGRATION_STATEMENT.md), [IHE scope](../IHE_CONFORMANCE.md), [integration tools](../../tools/integration_tests) | Supported-peer results and behavior under partial failure; protocol conformance alone does not establish medical-device conformity |
| Storage and workflow | [Storage](../../src/storage), [services](../../src/services), [workflow design](../SDS_WORKFLOW.md), [database design](../SDS_DATABASE.md) | Patient/UID integrity, commitment, consistency, concurrency, migrations and restore |
| Images and results | [Codecs](../../src/encoding/compression), [rendering](../../src/web/endpoints/dicomweb_endpoints.cpp), [measurements](../../src/web/endpoints/measurement_endpoints.cpp), [AI](../../src/ai) | Pixel fidelity, unit/source provenance, model/service boundaries and any clinical claim validation |
| Security and operations | [Security design](../SDS_SECURITY.md), [audit](../SECURITY_AUDIT.md), [policy](../../SECURITY.md), [ISO 27799 mapping](../compliance/iso-27799.md) | Complete threat model, security verification, shared responsibilities and incident/recovery exercises |
| Verification and release | [Feature/test matrix](../TRACEABILITY.md), [design traceability](../SDS_TRACEABILITY.md), [verification report](../VERIFICATION_REPORT.md), [validation report](../VALIDATION_REPORT.md), [domain gates](../RELEASE_GATES.md) | Current executed evidence, risk-control traceability, anomalies and authorized conclusions |

## Baseline gaps to resolve

These observations refer to commit
`ddea5fe4cbd58b8165a4fffe719851812e26076d`, reviewed on 2026-10-03. Recheck each
against the actual release and any external records. They are scoped evidence
requests, not conclusions that the whole product is unsafe or nonconforming.

| ID | Observation | Required follow-up |
|---|---|---|
| PAC-GAP-01 | The reviewed public documents describe functions and personas but do not establish an approved medical purpose, legal manufacturer, regulatory class or complete product risk file. | Obtain controlled decisions and risk/QMS records; distinguish unavailable evidence from confirmed absence. |
| PAC-GAP-02 | [Verification](../VERIFICATION_REPORT.md) reports 85% SRS coverage. [Validation](../VALIDATION_REPORT.md) lists planned requirements while concluding all requirements are met and production deployment is ready. | Reconcile scope, counts and conclusions against executed evidence for the selected release; record excluded functions. |
| PAC-GAP-03 | [PR-001](../compliance/problem-resolution/PR-001-soup-dangling-sha.md) and [SOUP](../SOUP.md) still record re-verification/validation of the substituted thread dependency as a release action. | Locate the actual closure evidence, confirm its baseline and approval, or retain the action as open. |
| PAC-GAP-04 | [SOUP](../SOUP.md) labels libraries A/B/C but does not provide a product-level classification and segregation rationale there. Some versions are system-provided or ranges. | Justify system/item classes from harm and architecture; link exact resolved versions and known-anomaly reviews for each build. |
| PAC-GAP-05 | [Domain release gates](../RELEASE_GATES.md) cover DICOM, TLS/ATNA, anonymization and storage/index migration. | Add applicable product, risk, usability, QMS and regulatory dispositions; retain actual run evidence rather than treating the matrix as proof of completion. |
| PAC-GAP-06 | [AI design](../SDS_AI.md) says HTTP transport is pending, while the [connector](../../src/ai/ai_service_connector.cpp) contains HTTP transport. [SR validation](../../src/ai/ai_result_handler.cpp) notes incomplete content-sequence checking. | Reconcile implementation and claims, specify supported SR/SEG constraints and verify result/source/model integrity for included workflows. |
| PAC-GAP-07 | [Package state](../PACKAGE_STATE.md) distinguishes published v0.1.0, current source and unreleased v1.0; historical reports describe other versions and feature sets. | Map the assessed source, package, resolved dependencies, enabled features and binary hashes to every cited test report. |
| PAC-GAP-08 | [Traceability](../TRACEABILITY.md) maps features to test files/modules; that table does not record patient hazards, control effectiveness or release-specific test executions. | Extend the controlled traceability record through risk controls, test procedures, executed results and residual-risk decisions. |

## Release readiness gates

These are proposed gates for the selected medical product or supplier scope,
subject to adoption by its release authority. They supplement the existing
[technical release gates](../RELEASE_GATES.md); no CI or release policy is changed
by adding this checklist collection.

- [ ] **PAC-GATE-01** Approve applicable product, role, market, classification and standard decisions.
- [ ] **PAC-GATE-02** Reconcile baseline gaps and freeze source, dependencies, features, artifacts and deployment configuration.
- [ ] **PAC-GATE-03** Complete traceability and executed verification, product validation and clinical evidence appropriate to every included claim.
- [ ] **PAC-GATE-04** Review residual safety, usability and security risks, unresolved anomalies and supplier limitations.
- [ ] **PAC-GATE-05** Complete applicable manufacturer QMS, GMP, labeling, submission and authorization activities, or approve the component-supplier allocation.
- [ ] **PAC-GATE-06** Approve distribution, installation, support, incident/field action, recovery and retirement arrangements with downstream owners.

Review scope and applicability first, then planning/QMS, software and risk,
usability and security, product validation, and submission/release. Reopen the
affected reviews after code, dependency, schema, model, configuration or claim
changes; the impact assessment determines necessary regression and revalidation.
