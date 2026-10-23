---
doc_id: PAC-QUAL-018
doc_title: "MFDS digital medical device cybersecurity checklist"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
---

# MFDS digital medical device cybersecurity checklist

Candidate basis: MFDS Notice No. 2025-30 on protection against electronic
intrusion, Articles 2 through 22, and cybersecurity evidence under the approval
notice. Confirm the adopted text and applicability using the
[reference register](regulatory-references.md). Distinguish manufacturer duties,
healthcare-provider responsibilities and optional measures in the source text.

Use the [assessment rules](README.md#how-to-record-a-review) and
[review record](review-record-template.md). Every row is initially unassessed.
The project should retain a release SBOM; this project practice is not a claim
that every SBOM-related provision in Article 16 is an unconditional duty.

## Physical and technical safeguards

| ID | Reference area | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-MC-01 | Article 2 | Define security activities across the product, interfaces, components and operational environment. | Security scope and responsibilities, linked to IEC 81001-5-1. |
| PAC-MC-02 | Article 3 | Address physical protection and authorized access to servers, displays, network equipment, backups and support workstations. | Hospital/cloud shared-responsibility records and site access controls. |
| PAC-MC-03 | Article 4 | Protect communications across REST, DIMSE, DICOMweb, AI, XDS, cloud/object storage and audit connections where included. | Interface/network inventory, secure configuration, responsibility boundaries and connection verification. |
| PAC-MC-04 | Article 5 | Establish identity, authentication, authorization and credential/session lifecycle controls. | Role/access matrix and tests of direct API access, token expiry, revocation and isolation. |
| PAC-MC-05 | Article 6 | Analyze technical threats and protect patient information throughout acquisition, processing, storage and support. | Threat assessment, data inventory and verified privacy/access measures. |
| PAC-MC-06 | Article 7 | Validate critical files and inputs and protect software integrity, including hostile DICOM/PDU, XML/MTOM and configured remote URLs. | Integrity checks, parser/fuzz results, path/decompression/SSRF controls and release verification. |
| PAC-MC-07 | Article 8 | Protect important data against unauthorized disclosure, alteration and loss. | Storage, encryption, backup/restore and deletion controls with verification results. |
| PAC-MC-08 | Article 9 | Control cryptographic keys and certificates through generation, storage, access, rotation, revocation and disposal. | Key-management procedure and operational tests; no keys in source or public evidence. |
| PAC-MC-09 | Article 10 | Control maintenance access and updates across the deployed environment. | Maintenance authorization, update integrity, rollback and compatibility records. |
| PAC-MC-10 | Article 11 | Monitor relevant security events and react to suspected compromise without creating misleading clinical output. | Audit/monitoring design, event coverage and incident detection exercises. |
| PAC-MC-11 | Article 12 | If AI is included, assess data/model attacks, extraction/evasion and safe operation when problems are detected. | AI applicability and threat records, tests and operational response controls. |

## Risk, development and lifecycle

| ID | Reference area | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-MC-12 | Article 13 | Perform security risk assessment and control verification throughout development and post-market use. | Threat/risk records, residual-risk report and monitoring arrangements. |
| PAC-MC-13 | Article 14 | Apply secure coding, design and security/vulnerability testing to the actual release paths. | Executed verification and penetration reports with remediation and retest evidence. |
| PAC-MC-14 | Article 14 | Validate security test tools for their intended use and record decisions on documentation supplied to providers. | Tool qualification/validation and approved customer-documentation scope. |
| PAC-MC-15 | Article 15 | Address support retirement, remaining vulnerabilities, transition support and secure removal of patient data. | EOL schedule, provider guidance, migration/deletion verification and support responsibilities. |
| PAC-MC-16 | Article 16 | Decide how to produce, protect, use and share the SBOM for vulnerability management and provider needs. | Release-specific SBOM, access/sharing policy and optional-versus-required applicability rationale. |

## Incident response and vulnerability handling

| ID | Reference area | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-MC-17 | Article 17 | Plan incident reporting, risk assessment, containment, team roles, clinical continuity and exercises. | Approved incident plan, contact list, recovery plan and exercise results. |
| PAC-MC-18 | Article 18 | Implement the required incident-notification triggers, recipients and timing, including MFDS, providers and users where required. | Regulatory reporting procedure and drill records; distinguish incidents from unexploited vulnerabilities. |
| PAC-MC-19 | Article 19 | Investigate incidents, implement response and recurrence prevention, and provide required follow-up information. | Investigation, action and communication records with effectiveness checks. |
| PAC-MC-20 | Article 20 | Monitor vulnerabilities and receive provider/user reports. Distinguish discretionary notification decisions from required report content. | Intake/monitoring records, severity/clinical-impact assessment and notification rationale. |
| PAC-MC-21 | Article 21 | Define vulnerability disclosure content and timing, including any required authority consultation when delaying disclosure. | Coordinated-disclosure procedure and dated decision/communication records. |
| PAC-MC-22 | Article 22 | Provide verified corrective measures and monitor for recurrence after deployment. | Patch/update records, regression and safety evaluation, rollout and follow-up results. |

## Submission linkage

| ID | Reference area | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-MC-23 | Approval notice, Article 26 | Describe communication technologies, public/private networks, operational environments, protective measures and evaluation results. | Product-specific security dossier matching the submitted model, version and representative configuration. |
| PAC-MC-24 | Approval notice, Articles 11 and 17 | Reflect protection measures and customer precautions consistently in architecture, screens, technical documentation and instructions. | Dossier/labeling consistency review and verified deployment guidance. |

## Evidence starting points

- [IEC 81001-5-1 review](iec-81001-5-1-checklist.md), [approval review](mfds-digital-device-approval-checklist.md).
- [Security implementation](../../src/security), [audit](../SECURITY_AUDIT.md), [storage](../../src/storage), [web endpoints](../../src/web/endpoints), [security policy](../../SECURITY.md).
- Obtain hosting, provider, support and reporting records from the responsible organization.
