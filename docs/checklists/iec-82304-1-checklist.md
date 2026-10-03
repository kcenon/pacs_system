---
doc_id: PAC-QUAL-015
doc_title: "IEC 82304-1 health software product checklist"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
---

# IEC 82304-1 health software product checklist

Candidate basis: IEC 82304-1:2016, Clauses 4 through 8. Review the complete
health software product and its intended computing environment. A C++ library
test, successful CI run or browser demonstration does not by itself establish
validation of the product for its intended use.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Product requirements and lifecycle

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-82304-01 | 4 | Approve intended purpose, users, clinical context and the supplied product boundary: component, PACS appliance/service or integrated imaging product. | Product requirements, manufacturer/supplier responsibilities and claims matrix with clinical/regulatory review. |
| PAC-82304-02 | 4 | Define functional, performance, safety, security, usability and interoperability requirements at product level. | Reviewed requirements with measurable acceptance criteria. |
| PAC-82304-03 | 4 | Define supported server OS, CPU, browser, database, filesystem/object store, network, modalities, viewers and AI endpoints. | Compatibility matrix, enabled build options and installation/environment requirements. |
| PAC-82304-04 | 4 | Define dependencies on external systems and behavior when a prerequisite becomes unavailable or incompatible. | External interface contracts, shared responsibilities and safe failure requirements. |
| PAC-82304-05 | 4 | Evaluate product risk, requirement completeness and consistency; maintain traceability through changes. | Requirements reviews and linked risk/verification records. |
| PAC-82304-06 | 5 | Apply the selected software lifecycle activities to every supplied component and support the product requirements. | Completed IEC 62304 assessment and product-to-software traceability. |

## Product validation

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-82304-07 | 6.1 | Plan validation before execution, defining methods, intended users, environments, datasets, acceptance criteria and responsibilities. | Approved product validation plan and independence/competence rationale. |
| PAC-82304-08 | 6.2 | Validate ingestion, query/retrieve and DICOMweb with supported modalities, transfer syntaxes, multiframe data, character sets and malformed inputs. | Representative datasets, reference peers, identity/completeness checks and executed results. |
| PAC-82304-09 | 6.2 | Validate compression, transcoding and rendered-image output, including bit depth, signedness, photometric interpretation, rescale and window/level. | Reference pixels, lossless comparisons or justified lossy tolerances, unsupported-input handling and claim-specific limits. |
| PAC-82304-10 | 6.2 | Validate included AI and measurement workflows, identifying whether PACS stores external results or supplies a clinical interpretation. | Source/model/unit provenance, representative end-to-end results and analytical/clinical evidence for any supplied clinical claim. |
| PAC-82304-11 | 6.2 | Validate durable storage, Storage Commitment, file/index consistency, patient reconciliation and supported workflow/IHE interfaces. | End-to-end results with supported peers, fault injection, result matching and recovery evidence. |
| PAC-82304-12 | 6.2 | Validate load, network/storage outages, failover, backup/restore, schema migration and rollback without falsely reporting complete studies. | Capacity and interruption results, recovery objectives and verified deployment configurations. |
| PAC-82304-13 | 6.2 | Evaluate safety-related use with representative users and final user instructions. | Usability evaluation and product validation linkage. |
| PAC-82304-14 | 6.3 | Report the tested product and environment, methods, results, deviations, anomalies, conclusions and responsible personnel. | Approved validation report with traceability to requirements and retained source evidence. |

## Identification and accompanying documents

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-82304-15 | 7.1 | Identify manufacturer, product, model, source/package version and enabled features consistently in UI, APIs, artifacts and support records. | Product/component version mapping, build provenance and identification checks; distinguish released v0.1.0 from source snapshots. |
| PAC-82304-16 | 7.2.1, 7.2.2 | Provide intended purpose, users, limitations, warnings, operating instructions, interpretation of outputs and support contacts. | Reviewed instructions for use and language/readability/usability evidence. |
| PAC-82304-17 | 7.2.3 | Provide technical requirements, installation, network ports, identity/storage setup, hardening, backup, updates and decommissioning instructions. | Technical description and tested deployment/service procedures. |
| PAC-82304-18 | 7.2 | Reconcile all required documentation items with the adopted standard and applicable labeling rules, including any omitted conditional items. | Documentation coverage matrix, approved exclusions and release-document index. |

## Post-release activities

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-82304-19 | 8 | Maintain complaint, defect, security and performance monitoring with defined support and customer communications. | Maintenance plan, field-feedback reviews and escalation records. |
| PAC-82304-20 | 8 | Reassess and revalidate affected product claims after changes; plan migration and safe retirement. | Change impact, verification/validation results, update instructions and retirement/data-transfer records. |

## Evidence starting points

- [PRD](../PRD.md), [SRS](../SRS.md), [package state](../PACKAGE_STATE.md), [production tutorial](../../examples/tutorials/05_production_pacs).
- [Tests](../../tests), [integration tools](../../tools/integration_tests), [DICOM statement](../DICOM_CONFORMANCE_STATEMENT.md), [IHE statement](../IHE_INTEGRATION_STATEMENT.md).
- Review [baseline gaps](README.md#baseline-gaps-to-resolve) and retain executed evidence for the selected release candidate.
