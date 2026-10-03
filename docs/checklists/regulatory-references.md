---
doc_id: PAC-QUAL-019
doc_title: "SaMD checklist sources and applicability"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
---

# SaMD checklist sources and applicability

This register records the sources used to organize the checklists and the
candidate editions to verify before a formal assessment. Catalogue and official
notice metadata were checked on 2026-10-03. This authoring check does not establish
market recognition, product applicability or a complete clause-level assessment
against licensed standards.

## Adaptation source

The direct adaptation source is
[`dicom_viewer` PR #572](https://github.com/kcenon/dicom_viewer/pull/572), using
[its checklist files](https://github.com/kcenon/dicom_viewer/tree/8520a74d608286a190eb8ac08884f150c26ba759/docs/checklists)
at commit `8520a74d608286a190eb8ac08884f150c26ba759`. Its reference register
records the earlier `flonics-dev/streamliner_viewer_prototype` source. This
adaptation uses the public viewer collection and does not claim to have assessed
the upstream private project's evidence or approvals.

This collection keeps its nine review areas and its separation of applicability,
finding and evidence availability. The prompts are rewritten in English for the
`pacs_system` component/product boundary, storage, workflow, image conversion,
AI interfaces and operations. They do not claim a one-to-one mapping of every
normative subclause. Source project personnel, approval status, product
decisions and evidence paths have not been adopted as facts about this product.

| Source file in the referenced directory | Local checklist | Coverage retained |
|---|---|---|
| `iec-62304-checklist.md` | [Software lifecycle](iec-62304-checklist.md) | General requirements, planning, requirements/design, verification, release, maintenance, risk, configuration and problems |
| `iso-13485-checklist.md` | [QMS](iso-13485-checklist.md) | QMS, management, resources, realization, suppliers, delivery, monitoring and improvement |
| `iso-14971-checklist.md` | [Risk management](iso-14971-checklist.md) | Risk process, analysis, evaluation, controls, residual risk and production/post-production |
| `iec-62366-1-checklist.md` | [Usability](iec-62366-1-checklist.md) | Process, use specification, scenarios, UI, evaluation and conditional Annex C |
| `iec-81001-5-1-checklist.md` | [Security lifecycle](iec-81001-5-1-checklist.md) | Governance, development, release, maintenance, security risk, configuration, problems and conditional Annex F |
| `iec-82304-1-checklist.md` | [Health software product](iec-82304-1-checklist.md) | Product requirements, lifecycle, validation, identification, documentation and post-release activities |
| `mfds-digital-device-approval-checklist.md` | [MFDS approval](mfds-digital-device-approval-checklist.md) | Qualification/classification, application content, evidence, conformity report, changes and labeling |
| `mfds-digital-gmp-checklist.md` | [MFDS GMP](mfds-digital-gmp-checklist.md) | Annex 2 QMS, Annex 3 software and conditional Annex 4 AI controls |
| `mfds-digital-cybersecurity-checklist.md` | [MFDS cybersecurity](mfds-digital-cybersecurity-checklist.md) | Physical/technical safeguards, lifecycle, response, monitoring and submission linkage |

Before making a conformity statement, expand grouped prompts as needed and
record a coverage crosswalk against every applicable requirement in the adopted
source. The responsible reviewer must identify missing or conditional items and
retain exclusions with their rationale. The public catalogues identify standards;
they do not replace access to the full controlled text.

## Candidate international standards

| ID | Candidate edition and scope | Official source |
|---|---|---|
| REF-62304 | IEC 62304:2006 plus AMD1:2015, consolidated Edition 1.1; medical device software lifecycle | [IEC catalogue](https://webstore.iec.ch/en/publication/22794) |
| REF-13485 | ISO 13485:2016; medical device quality management systems | [ISO catalogue](https://www.iso.org/standard/59752.html) |
| REF-14971 | ISO 14971:2019; medical device risk management | [ISO catalogue](https://www.iso.org/standard/72704.html) |
| REF-62366 | IEC 62366-1:2015 plus AMD1:2020, consolidated Edition 1.1; usability engineering | [IEC catalogue](https://webstore.iec.ch/en/publication/67220) |
| REF-81001 | IEC 81001-5-1:2021, corrected version 2025-12 including ISH1:2025; cybersecurity lifecycle | [IEC catalogue](https://webstore.iec.ch/en/publication/63293), [Interpretation Sheet 1](https://webstore.iec.ch/en/publication/108664) |
| REF-82304 | IEC 82304-1:2016; health software product safety | [IEC catalogue](https://webstore.iec.ch/en/publication/26120) |

The IEC catalogue dates Interpretation Sheet 1 for IEC 81001-5-1 to 2025-12-04
and identifies its incorporation in the corrected December 2025 base publication.
Record the actual controlled copy used in an assessment. If ISO/TR 24971:2020
or other guidance is used, identify it separately from normative requirements.

For each target market, record the recognized national/regional adoption,
amendments, corrigenda, interpretations and transition dates. Do not infer
recognition from a catalogue entry or replace the adopted edition merely because
a newer publication exists.

## Candidate South Korean regulatory sources

English descriptions below are navigation labels. The official Korean text
controls the regulatory interpretation.

| ID | Source identified for review | Official source |
|---|---|---|
| REF-MFDS-FRAMEWORK | Digital Medical Products Act and implementing framework; use the official portal to identify applicable legislation and procedures | [MFDS digital products portal](https://emedi.mfds.go.kr/msismext/emd/bif/digitInfoIntrcnView.do) |
| REF-MFDS-CLASS | Classification and grading notice, amended by No. 2026-4 on 2026-01-23 | [MFDS consolidated notice](https://www.mfds.go.kr/brd/m_211/view.do?seq=14944) |
| REF-MFDS-APPROVAL | Approval, certification, notification, review and evaluation notice, amended by No. 2026-54 on 2026-07-27 | [MFDS consolidated notice](https://www.mfds.go.kr/brd/m_211/view.do?seq=14982) |
| REF-MFDS-GMP | Digital medical device manufacturing and quality management, No. 2025-28, dated 2025-04-21 | [National Law Information Center](https://www.law.go.kr/LSW/admRulLsInfoP.do?admRulSeq=2100000258066), [MFDS GMP procedures](https://emedi.mfds.go.kr/msismext/emd/bif/digitInfoGmpView.do) |
| REF-MFDS-CYBER | Digital medical device protection against electronic intrusion, No. 2025-30 | [MFDS notice](https://www.mfds.go.kr/brd/m_211/view.do?seq=14894) |

The cybersecurity notice page shows 2025-04-21 in its notice-date field and
2025-04-29 in its title and registration date. Preserve the adopted attachment
and verify its effective-date provision rather than treating those metadata
fields as equivalent. Similarly, distinguish an amendment's publication date
from its effective date and any transition period.

The source checklist also uses MFDS explanatory guidance. This adaptation does
not inherit its page-number references or treat guidance as an independent legal
obligation. Record the guidance edition if it is used to support an assessment.

## Source control before an assessment

- [ ] **PAC-REF-01** Confirm the intended market, applicable legal framework, manufacturer obligations and product scope.
- [ ] **PAC-REF-02** Retrieve current consolidated rules and applicable forms; check amendments, effective dates and transitions.
- [ ] **PAC-REF-03** Identify the adopted standards and authorized copies, including national versions and interpretations.
- [ ] **PAC-REF-04** Retain source identifiers, revision/date, retrieval date and a checksum for downloaded regulatory attachments in the controlled record location.
- [ ] **PAC-REF-05** Resolve contradictory definitions, uncertain exemptions or ambiguous classification with the responsible regulatory authority or qualified regulatory reviewer before closing affected items.
- [ ] **PAC-REF-06** Update the coverage crosswalk and affected checklist assessments when sources, claims or release configurations change.

## Other markets

For the United States, the FDA's
[medical image storage and communications guidance](https://www.fda.gov/regulatory-information/search-fda-guidance-documents/medical-device-data-systems-medical-image-storage-devices-and-medical-image-communications-devices)
describes software functions solely for transfer, storage, format conversion or
display as non-device functions. The
[FDA function assessment](https://www.fda.gov/medical-devices/digital-health-center-excellence/step-5-software-function-intended-transferring-storing-converting-formats-or-displaying-data-and)
also addresses analysis/interpretation and control of connected devices. Record
the actual supplied functions before using this rationale; a PACS repository
name does not establish an exemption. This US policy does not establish Korean
qualification. The Korean classification notice considers intended purpose,
technology, form and potential harm, including medical-image information
management categories.

Selecting another market requires a separate assessment of its classification,
authorization, clinical evidence, labeling, cybersecurity and post-market rules.
The common IEC/ISO reviews may contribute evidence, but this MFDS-oriented
collection does not establish FDA clearance, EU MDR conformity or authorization
in another jurisdiction.
