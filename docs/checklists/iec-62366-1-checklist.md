---
doc_id: PAC-QUAL-013
doc_title: "IEC 62366-1 usability engineering checklist"
doc_version: 0.1.0
doc_date: 2026-10-03
doc_status: Draft
project: pacs_system
category: QUAL
---

# IEC 62366-1 usability engineering checklist

Candidate basis: IEC 62366-1:2015 with Amendment 1:2020, Edition 1.1, Clauses 4
and 5 and conditional Annex C. Review safety-related use of the supplied web,
CLI and service interfaces with their intended users and environments. Demonstrations and
automated UI tests are inputs; they do not replace evaluation with representative
users when that evaluation is required.

Use the [assessment rules](README.md#how-to-record-a-review),
[review record](review-record-template.md) and
[reference register](regulatory-references.md). Every row is initially unassessed.

## Process and use specification

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62366-01 | 4.1, 4.2 | Establish the usability process and file, linking UI risk controls to the device risk file. | Approved plan, competence, responsibilities and usability-file index. |
| PAC-62366-02 | 4.1 | Prefer safer interface design over warnings alone; evaluate whether users perceive and understand safety information. | Design rationale, warning requirements and evaluation results. |
| PAC-62366-03 | 4.3 | Tailor effort to the significance of the UI and its risks without omitting necessary safety evaluation. | Documented scope and resource rationale. |
| PAC-62366-04 | 5.1 | Define clinical, technical and administrative users, tasks, patient population, care settings, language and training needs for supplied interfaces. | Approved use specification for the web UI, CLI, installation and service workflows; downstream UI responsibility boundaries. |
| PAC-62366-05 | 5.2, 5.3 | Identify safety-related UI characteristics and known use errors, including similar-product experience. | Task analysis, complaint/literature review and linked hazards. |

## Hazard-related scenarios and interface design

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62366-06 | 5.4, 5.5 | Define and select hazard-related use scenarios for summative evaluation using a documented rationale. | Scenario inventory, risk linkage and selection decision. |
| PAC-62366-07 | 5.4, 5.6 | Make patient/study identifiers, accession numbers, destination AE/node and worklist selection unambiguous during routing and reconciliation. | UI requirements and scenarios for wrong-patient, wrong-destination and incorrect worklist edits. |
| PAC-62366-08 | 5.4, 5.6 | Distinguish queued, transferring, stored, committed, partial, failed and stale states. Show what action a user must take after an interruption. | State/feedback requirements and slow-network, retry, reconnection and session-expiry scenarios. |
| PAC-62366-09 | 5.4, 5.6 | Make deletion, retention changes, patient reconciliation, study-lock override and restore consequences clear and prevent unintended actions. | Destructive-operation and recovery scenarios with role restrictions, confirmations and acceptance criteria. |
| PAC-62366-10 | 5.4, 5.6 | Present lossy or rendered-image limitations, measurement provenance and AI result source/status without implying validated diagnostic performance. | Label/warning requirements, result interpretation scenarios and downstream viewer responsibilities. |
| PAC-62366-11 | 5.4, 5.6 | Differentiate successful file transfer from durable commitment and complete study availability, including partial C-MOVE or STOW responses. | Store/query/retrieve and commitment scenarios with completion and failure feedback. |
| PAC-62366-12 | 5.6 | Specify web form behavior, validation, browser scaling, keyboard navigation, CLI options and configuration defaults used in safety-related tasks. | Interface specification tied to supported browsers, terminals, roles and deployment configurations. |

## Evaluation and existing interfaces

| ID | Clause group | Review check for pacs_system | Evidence required |
|---|---|---|---|
| PAC-62366-13 | 5.7 | Plan formative and summative methods, users, environments, training, scenarios, data collection and success criteria before execution. | Approved evaluation plans with recruitment and sampling rationale. |
| PAC-62366-14 | 5.8 | Conduct formative evaluations during design and track the effect of resulting changes. | Observation records, design changes and follow-up evaluation. |
| PAC-62366-15 | 5.9 | Conduct summative evaluation on the representative final UI and included clinical workflows. | Executed protocol, participant characteristics, results and deviations. |
| PAC-62366-16 | 5.9 | Analyze observed use errors, close calls and difficulties; assess root causes and remaining risks. | Usability conclusions, corrective actions, risk updates and justified retesting. |
| PAC-62366-17 | 5.10, Annex C | Decide whether any existing interface qualifies for the interface-of-unknown-provenance route; a prior open-source release alone does not establish eligibility. | Applicability rationale and identification of affected UI/CLI versions. |
| PAC-62366-18 | Annex C | If applicable, evaluate use specification, post-production information, hazards, controls and residual risks for the existing UI. | Annex C assessment and any additional evaluation needed to close gaps. |

## Evidence starting points

- [Management UI](../../web/src), [CLI tools](../../tools), [web endpoints](../../src/web/endpoints), [PRD](../PRD.md).
- Scope patient/worklist edits, routing configuration, deletion, recovery and service tasks to the supplied interfaces.
- Link [risk management](iso-14971-checklist.md) and [product validation](iec-82304-1-checklist.md); obtain downstream viewer evidence from its owner.
