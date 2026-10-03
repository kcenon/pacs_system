---
doc_id: PAC-QUAL-010
doc_title: "IEC 62304 software lifecycle checklist"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
---

# IEC 62304 software lifecycle checklist

Candidate basis: IEC 62304:2006 with Amendment 1:2015, Edition 1.1, Clauses 4
through 9. Determine the system and item safety classes before selecting
class-dependent activities. Product validation and final device release also
need the [product checklist](iec-82304-1-checklist.md).

Use the [assessment rules](README.md#how-to-record-a-review) and
[review record](review-record-template.md). Every row is initially unassessed.
Clause groups identify where to consult the adopted standard; grouped questions
need separate findings if their parts have different outcomes. See the
[reference register](regulatory-references.md) for edition control.

## Planning and general requirements

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62304-01 | 4.1, 4.2 | Place software development and maintenance under the manufacturer's QMS and risk process. | Approved process scope, responsibilities and risk management plan. |
| PAC-62304-02 | 4.3 | Justify A, B or C for the complete system and any separately classified items. Consider wrong-patient data, corrupted images, lost studies, delayed access and failure of external controls. | Safety classification decision, hazardous situations, severity rationale and segregation evidence; reconcile the existing SOUP class labels. |
| PAC-62304-03 | 4.4 | Determine whether the legacy software provisions apply. Repository age, a prior package release or missing records alone does not establish eligibility. | Applicability decision; if applicable, feedback review, gap analysis, remediation and continued-use rationale. |
| PAC-62304-04 | 5.1 | Define lifecycle activities, deliverables, review authorities and release scope for the PACS libraries, services, web UI, tools and downstream integrations. | Approved development plan identifying manufacturer and component-supplier responsibilities. |
| PAC-62304-05 | 5.1 | Plan integration order, verification methods, acceptance criteria and links to system validation. | Integration and verification plans, review schedule and traceability approach. |
| PAC-62304-06 | 5.1 | Control development tools, coding rules, test tools, documentation, configuration, change requests and defect handling before verification. | Tool inventory, tool validation decisions, coding rules and configuration/problem-resolution procedures. |

## Requirements and design

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62304-07 | 5.2 | Reconcile PRD/SRS/SDS and historical verification reports with the selected source or published package and its enabled features. | Reviewed requirements baseline, stable identifiers, change history and resolution of inconsistent completion claims. |
| PAC-62304-08 | 5.2 | Specify DICOM inputs, identity, geometry, pixel interpretation, supported transfer syntaxes and invalid-input behavior. | Interface and data requirements, supported-format matrix and acceptance criteria. |
| PAC-62304-09 | 5.2 | Specify pixel preservation, lossy compression limits, rendered-image behavior, measurement units and AI result provenance for included functions. | Accuracy and integrity requirements for codecs, window/level rendering, stored measurements and supplied analysis results. |
| PAC-62304-10 | 5.2 | Include latency, capacity, interruption, security, installation, storage and hospital network requirements. Incorporate software risk controls. | Reviewed operational and security requirements linked to risks and tests. |
| PAC-62304-11 | 5.3 | Describe DIMSE/DICOMweb, web UI, routing, worklist, AI, file/object storage and index boundaries, including consistency and failure recovery. | Architecture and interface review, deployment diagrams, state ownership and risk-control allocation. |
| PAC-62304-12 | 5.3, 7.1 | Identify SOUP and other supplied components; define required behavior, platform assumptions and handling of known anomalies. | Assessment of kcenon libraries, SQLite, Crow, Asio, OpenSSL, codecs, cloud SDKs and frontend packages; reconcile SOUP, manifest and resolved artifacts. |
| PAC-62304-13 | 5.3 | Justify separation used to reduce an item safety class, including shared executors, caches, databases and process privileges. | Segregation design and verification; common dependency, concurrency and resource-exhaustion analysis. |
| PAC-62304-14 | 5.4 | Define units and detailed interfaces for concurrent associations, store/index transactions, asynchronous jobs, retries, cancellation and shutdown. | Detailed design, state/lifetime rules, durability contracts and design verification records. |

## Implementation and verification

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62304-15 | 5.5 | Verify units against predefined criteria, including bounds, numerical precision, initialization, memory ownership and error handling. | Unit verification results, code reviews and static/dynamic analysis appropriate to risk. |
| PAC-62304-16 | 5.6 | Test the complete modality-to-storage-to-index-to-retrieval path and included UI/API workflows. Separate mock, partial and production implementations. | Executed integration results for ingestion, query/retrieve, routing, worklist, measurements and AI result handling where supplied. |
| PAC-62304-17 | 5.6 | Test included DIMSE, DICOMweb, Storage Commitment, MWL/MPPS, UPS, PIR and IHE XDS interfaces with the supported peer configurations. | DICOM/IHE statements, interoperability matrix, protocol traces, identity checks, partial failures, timeouts and recovery results. |
| PAC-62304-18 | 5.6 | Verify imported package targets, transitive libraries, feature definitions and ABI consistency after ecosystem updates. | Clean configure/build/link results using the frozen dependency set; runtime smoke results. |
| PAC-62304-19 | 5.6, 5.7 | Run regression and system tests against all requirements on each claimed deployment configuration. Evaluate test adequacy and resolve anomalies. | Traceability matrix, executed reports, coverage rationale and problem records. |
| PAC-62304-20 | 5.7 | Preserve repeatable test inputs, expected outputs, tolerances, artifact identifiers, tools, dates and executors. | Dataset identifiers and hashes, ground truth, environment capture and signed test reports. |
| PAC-62304-33 | 5.6, 5.7 | Verify claimed de-identification profiles across tags, private data, burned-in pixels, overlays, structured content and consistent UID/date mappings. | Profile-specific tests, representative modalities, documented unsupported content and downstream re-identification/identity controls. |

## Release and maintenance

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62304-21 | 5.8 | Confirm planned activities and verification are complete. Evaluate every residual anomaly before software release. | Software release review, anomaly list, safety impact and authorized disposition. |
| PAC-62304-22 | 5.8 | Identify the release and preserve its sources, dependencies, build procedure, configuration, artifacts and delivery controls. | Product/component version mapping, SBOM, build provenance, artifact hashes and retention/distribution records. |
| PAC-62304-23 | 6.1 | Establish support responsibility for the supplied component or product, including downstream users, supplier updates, patches and end of support. | Approved maintenance plan, service contacts, responsibility agreements and review cadence. |
| PAC-62304-24 | 6.2 | Assess reported problems and proposed changes for effects on patients, users, connected systems and deployed versions. | Problem/change records, risk analysis, approval and notification decisions. |
| PAC-62304-25 | 6.3 | Reapply affected development, verification and release activities when changing PACS code, schemas, codecs or dependencies. | Update impact assessment, regression selection, migration/recovery results and redistribution approval. |

## Risk, configuration and problem resolution

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62304-26 | 7.1 | Identify software contributions to hazardous situations, including patient/UID mismatch, false storage acknowledgement, stale cache entries and SOUP failures. | Risk file linking causes, items, affected workflows and known supplier anomalies. |
| PAC-62304-27 | 7.2, 7.3 | Specify, implement and verify each software risk control; assess new risks introduced by the control. | Bidirectional risk-to-requirement-to-implementation-to-test traceability. |
| PAC-62304-28 | 7.4 | Reassess safety after dependency, codec, routing, AI service, authentication, database or cloud-storage changes. | Before/after behavior assessment and updated risk-control verification, including restore and migration. |
| PAC-62304-29 | 8.1 | Uniquely identify all configuration items and SOUP versions. Freeze all kcenon SHAs and third-party package versions. | Controlled lock/provenance records; distinguish released packages from unreleased source snapshots. |
| PAC-62304-30 | 8.2, 8.3 | Use approved, traceable changes and recoverable configuration history. Reconcile CMake, vcpkg, dependency manifest, SOUP and CI resolution. | Change approvals, build records, cache invalidation rules and configuration audits; closure evidence for the PR-001 dependency substitution. |
| PAC-62304-31 | 9.1 through 9.5 | Record, investigate, communicate and resolve problems through controlled changes. Capture reasons for taking no action. | Problem reports with severity, affected versions, root cause, risk review and resolution history. |
| PAC-62304-32 | 9.6 through 9.8 | Review defect trends and verify resolution and regression results before closure. | Trend reviews and repeatable closure tests with version, configuration, executor and date. |

## Evidence starting points

- [Requirements](../SRS.md), [design](../SDS.md), [traceability](../SDS_TRACEABILITY.md), [feature/test mapping](../TRACEABILITY.md).
- [SOUP](../SOUP.md), [dependency manifest](../../dependency-manifest.json), [CMake](../../CMakeLists.txt), [CI](../../.github/workflows/ci.yml), [tests](../../tests).
- [Domain release gates](../RELEASE_GATES.md), [PR-001](../compliance/problem-resolution/PR-001-soup-dangling-sha.md), [risk review](iso-14971-checklist.md).
