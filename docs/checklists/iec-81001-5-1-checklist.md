---
doc_id: PAC-QUAL-014
doc_title: "IEC 81001-5-1 cybersecurity lifecycle checklist"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
---

# IEC 81001-5-1 cybersecurity lifecycle checklist

Candidate basis: IEC 81001-5-1:2021, including Interpretation Sheet 1:2025 and
the corrected December 2025 publication, Clauses 4 through 9 and conditional
Annex F. Consult the adopted interpretation when evaluating supplier software
categories, disclosure, accompanying documentation and maintenance activities.

Assess security across the management UI, REST/DICOMweb, DIMSE, IHE and AI
interfaces, storage, cloud services, deployment and update pipeline. Connect
security failures to safety consequences. Use the
[assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Governance and secure development

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-81001-01 | 4.1 | Assign security responsibilities and competence, determine applicability, review the process and control supplier contributions. | Security plan, role/competence records, supplier assessments and periodic reviews. |
| PAC-81001-02 | 4.1 | Establish vulnerability intake, coordinated disclosure and review of security defects and customer guidance. | Published contact/policy, controlled disclosure procedure and review records. |
| PAC-81001-03 | 4.2, 7 | Connect threat assessment and security controls to safety risk management. | Threat model, risk criteria and cross-references to the device risk file. |
| PAC-81001-04 | 4.3 | Classify maintained, supported and required software and document overlapping responsibilities where applicable. | Classification of PACS code, kcenon libraries, codecs, browser/OS, DB, cloud SDKs, AI services and downstream integrations. |
| PAC-81001-05 | 5.1 | Protect source, reviews, CI, build runners, signing material and release publication; define secure coding practices. | Access reviews, branch/release controls, secret handling and secure coding rules. |
| PAC-81001-06 | 5.2 | Define testable security requirements and evaluate risks from required software before design approval. | Approved requirements, supplier risk assessments and review records. |
| PAC-81001-07 | 5.3, 5.4 | Document trust boundaries, attack surfaces, least privilege and defense in depth. Review architecture and detailed interfaces. | Data-flow diagrams, threat scenarios, design reviews and security-control allocation. |
| PAC-81001-08 | 5.4 through 5.7 | Verify the actual authentication mechanisms and credential/session lifecycle for web/API access and configured remote services. | Positive and negative tests of credential validation, expiry/revocation, service identity and failure behavior; identify controls supplied by a proxy or integrator. |
| PAC-81001-09 | 5.4 through 5.7 | Verify authorization per patient/study, session and operation, including direct API calls and guessed identifiers. | RBAC/object-access tests and isolation results across concurrent users. |
| PAC-81001-10 | 5.4 through 5.7 | Verify browser origin, CORS, CSRF and session/cookie controls where applicable, and protect the management UI against script injection. | Browser/API tests, deployed configuration review and penetration findings; justify each non-applicable control. |
| PAC-81001-11 | 5.4 through 5.7 | Verify TLS and certificate validation for DIMSE, DICOMweb, AI, cloud, XDS and audit connections where included. | Connection inventory, trust/key settings and negative tests; document any plaintext interface and compensating controls. |
| PAC-81001-12 | 5.4 through 5.7 | Validate hostile DICOM/PDU, JSON, XML/MTOM, URL and path inputs; bound decompression, memory, association counts and request sizes. | Parser fuzzing, injection/path-traversal, SSRF and resource-exhaustion tests for enabled services. |
| PAC-81001-13 | 5.4 through 5.7 | Protect stored images, indexes, measurements, AI results, credentials and audit events, including replicas and backups. | Data inventory, encryption/key decisions, authorization, retention/deletion and recovery evidence. |
| PAC-81001-14 | 5.4 through 5.7 | Preserve audit attribution, ordering/integrity and usable timestamps without leaking unnecessary patient data into logs. | Audit tests, failure handling, clock assumptions and log access/retention policy. |

## Verification and release

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-81001-15 | 5.5, 5.6 | Review implementation and integration against secure coding and interface requirements. | Code review, static analysis, sanitizer and integration results with disposition of findings. |
| PAC-81001-16 | 5.7 | Test requirements, threat mitigations, known vulnerabilities, attack surfaces, malformed inputs and penetration scenarios. | Security verification plan and executed reports; a dependency scan alone is insufficient. |
| PAC-81001-17 | 5.7 | Establish appropriate independence and competence for security evaluation and review all findings before release. | Evaluator scope/competence, independence rationale, remediation and retest results. |
| PAC-81001-18 | 5.8 | Freeze and identify source, dependencies, assets and artifacts; verify delivery integrity and remove development credentials or services. | Release SBOM, dependency hashes, build provenance, artifact integrity checks and configuration audit. |
| PAC-81001-19 | 5.8 | Supply installation, hardening, network, account, backup, update and secure-decommissioning instructions. | Reviewed customer security guidance, shared-responsibility model and known limitations. |

## Maintenance and response

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-81001-20 | 6 | Monitor upstream advisories and field reports for every supported release, including ecosystem libraries, codecs and optional cloud SDKs. | Component ownership, monitoring cadence, vulnerability register and affected-version analysis. |
| PAC-81001-21 | 6 | Assess patches for exploitability, clinical consequences, compatibility and effects on existing controls. | Update impact and prioritization records with safety and security review. |
| PAC-81001-22 | 6 | Validate and distribute security updates with rollback, customer communication and post-update monitoring. | Patch verification, deployment/recovery exercise and notification records. |
| PAC-81001-23 | 7 | Review threats, controls and residual risks throughout deployment and change, including cloud and on-premise differences. | Updated threat model, residual-risk decisions and control effectiveness evidence. |
| PAC-81001-24 | 8 | Preserve security-relevant configuration history and reproduce the exact affected and corrected release. | Controlled configurations, release inventories and traceable change approvals. |
| PAC-81001-25 | 9 | Receive, investigate, prioritize, communicate and verify resolution of security problems. Coordinate with clinical incident handling. | Incident/vulnerability cases, risk decisions, disclosure and closure records. |
| PAC-81001-26 | 6, 9, Annex F | Define support retirement and assess Annex F eligibility separately. A prior release or missing lifecycle evidence does not establish an exception. | Support/EOL plan; if applicable, Annex F gap assessment, remediation and customer transition evidence. |

## Evidence starting points

- [Security policy](../../SECURITY.md), [security implementation](../../src/security), [security design](../SDS_SECURITY.md), [audit limitations](../SECURITY_AUDIT.md).
- [Web endpoints](../../src/web/endpoints), [AI connector](../../src/ai/ai_service_connector.cpp), [storage](../../src/storage), [security tests](../../tests/security).
- [MFDS cybersecurity](mfds-digital-cybersecurity-checklist.md) adds jurisdiction-specific review and reporting decisions.
