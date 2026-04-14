# Document Review Report — pacs_system

**Generated**: 2026-04-14
**Mode**: Report-only (no source `.md` files were modified)
**Files analyzed**: 104 markdown files (excluding `build/`, `.git/`)
**Timezone**: Asia/Seoul (KST)

---

## Scope

- **Phase 1 — Anchors & Links**: GitHub-style slug registry with duplicate-suffix handling; fenced code blocks skipped; intra-file `](#anchor)` and inter-file `](./file.md#anchor)` links validated; external URLs containing `:` excluded.
- **Phase 2 — Accuracy & Consistency**: DICOM terminology (SOP classes, AE Title, C-STORE/C-FIND/C-MOVE, Modality Worklist, PACS/RIS/HIS), factual checks (years, C++ std, DCMTK refs, DICOM Standard year, HL7 version, SQLite3, Catch2), URL validity.
- **Phase 3 — SSOT & Redundancy**: SSOT declarations across PRD / ARCHITECTURE / SDS_* / DICOM_CONFORMANCE_STATEMENT / PACS_IMPLEMENTATION_ANALYSIS; bidirectional cross-references (SDS ↔ PRD traceability); orphans and duplication; Korean ↔ English sibling consistency.

Total links scanned (non-external): **1,223**. Total docs with YAML frontmatter: **79**.

---

## Findings Summary

| Severity | Phase 1 | Phase 2 | Phase 3 | Total |
|----------|---------|---------|---------|-------|
| Must-Fix      | 4 | 2 | 2 | **8**  |
| Should-Fix    | 0 | 3 | 4 | **7**  |
| Nice-to-Have  | 0 | 2 | 3 | **5**  |
| **Total**     | 4 | 7 | 9 | **20** |

---

## Must-Fix Items

### Phase 1 — Broken Links (4)

1. `examples/integration_tests/README.md:946` — Link `../../docs/architecture.md` uses lowercase filename. The canonical file is `docs/ARCHITECTURE.md`. macOS APFS resolves this on the local volume, but the link will break on case-sensitive filesystems (Linux CI, Docker, git hosting). (Phase 1)
2. `examples/integration_tests/README.md:947` — Link `../../docs/dicom-protocol.md` points to a file that does not exist. No `dicom-protocol.md` exists in the repo. (Phase 1)
3. `examples/integration_tests/README.md:948` — Link `../tests/README.md` points to a directory that does not exist at `examples/tests/`. (Phase 1)
4. `examples/integration_tests/scripts/README.md:63` — Link `../test_fixtures.hpp` uses `.hpp` extension, but the actual header is `test_fixtures.h`. (Phase 1)

### Phase 2 — Factual / DICOM Conformance (2)

5. `docs/PRD.md:1094` (Appendix C "Cross-Document Version References" table) — States `SRS | 0.1.3.1`, but `docs/SRS.md:15` declares `Version: 0.1.3.2`. PRD is the authority on this table, so the table is stale. Blocks correct cross-document version citation. (Phase 2)
6. `docs/PRD.md:1095` (same Appendix C table) — States `SDS | 0.2.0.0` (4-digit form), but `docs/SDS.md:15` declares `Version: 0.2.0` (3-digit form). Creates ambiguity on whether SDS has been versioned to 0.2.0.0 (patch bump) or the citation is wrong. Per `docs/SDS.md` document history, SDS is `2.2.0` (doc revision) with product version `0.2.0`. (Phase 2)

### Phase 3 — SSOT Contradictions (2)

7. **Frontmatter vs. inline version drift — 31 docs affected** (systemic). Every SSOT document has YAML frontmatter `doc_version: "1.0.0"` / `doc_date: "2026-04-04"` while the body `> **Version:** X.Y.Z` field reports the real revision. Representative examples:
   - `docs/PRD.md` — frontmatter `1.0.0`, inline `0.2.0.0`
   - `docs/ARCHITECTURE.md` — frontmatter `1.0.0`, inline `0.1.4.0`
   - `docs/SDS.md` — frontmatter `1.0.0`, inline `0.2.0`
   - `docs/SDS_TRACEABILITY.md` — frontmatter `1.0.0`, inline `3.0.0`
   - `docs/SRS.md` — frontmatter `1.0.0`, inline `0.1.3.2`
   - `docs/DICOM_CONFORMANCE_STATEMENT.md` — frontmatter `1.0.0`, inline `0.1.0`
   - Full list of 31 mismatched files in the "Details" section below.
   
   This is an SSOT violation because the repo has two conflicting "source of truth" version fields on the same document. Either the frontmatter must be treated as canonical (and mass-updated), or it must be removed / relabeled (e.g., "schema_version") so that `**Version:**` remains authoritative. (Phase 3)

8. `docs/SDS_TRACEABILITY.md` (1,613 lines, inline `Version: 3.0.0`, updated `2026-02-08`) vs `docs/SDS_TRACEABILITY.kr.md` (763 lines, inline `버전: 0.1.3`, updated `2026-02-07`). The Korean translation is ~850 lines / ~53% shorter and tracks a pre-2.0.0 schema. Because SDS_TRACEABILITY is the requirements-traceability SSOT, divergence between the two language versions constitutes an SSOT contradiction for Korean readers. The KR file is missing the major 2.0.0 overhaul (7 new modules: Security, Web, Workflow, AI, DI, Monitoring, Compat; file counts 81 → 257; 76 new DES-xxx IDs). (Phase 3)

---

## Should-Fix Items

### Phase 2 — Accuracy (3)

9. `samples/01_hello_dicom/README.md:127-129`, `samples/02_echo_server/README.md:171-173`, `samples/03_storage_server/README.md:253-254` — 8 DICOM NEMA spec links use insecure `http://dicom.nema.org/...`. `dicomstandard.org` and `dicom.nema.org` both serve HTTPS; links should be upgraded. The two `docs/CLI_REFERENCE*.md:365` occurrences are XML namespace URIs (`xmlns="http://dicom.nema.org/PS3.19/models/NativeDICOM"`) and are NOT clickable links — leave those unchanged (namespace identity, not a URL). (Phase 2)
10. `docs/SDS_COMPRESSION.md:759` — States "This transfer syntax is retired in DICOM 2024". `docs/SRS.md:82` declares conformance to "DICOM Standard 2024" (`DICOM PS3.1-PS3.20`). The current published DICOM Standard is the 2024 edition, so the wording is correct today, but the two sites should be cross-referenced or consolidated so that a single DICOM-edition constant governs the repo (to ease the 2025 bump). (Phase 2)
11. `docs/PRD.kr.md:135` / `docs/PRD.md:135` / `docs/FEATURES*.md` — "DICOM 2019b" is the historical edition that added OV/SV/UV VRs. Technically correct, but the phrasing "including DICOM 2019b" (without noting it is a prior edition name, not a VR tag) can mislead readers. Should annotate as "DICOM 2019b additions (OV, SV, UV)" consistently everywhere. Already done correctly in `docs/SRS.md:234`; PRD / FEATURES variants should mirror the SRS phrasing. (Phase 2)

### Phase 3 — Cross-reference Gaps (4)

12. `docs/DICOM_CONFORMANCE_STATEMENT.md` has **0 outgoing markdown links**. A conformance statement is required reading for DICOM integrators and should explicitly link back to `PRD.md` (FR-1, FR-3, FR-8 conformance requirements), `SDS_NETWORK_V2.md` (DIMSE implementation), `SDS_SEQUENCES.md` (C-STORE/C-FIND/C-MOVE flows), and `IHE_INTEGRATION_STATEMENT.md`. (Phase 3)
13. `docs/SDS_NETWORK_V2.md` outgoing links count: **1** (only `SDS.md`). Should link back to `PRD.md` (FR-2 Network Protocol), `SRS.md` (R-2.x Network requirements), and `DICOM_CONFORMANCE_STATEMENT.md`. (Phase 3)
14. `docs/SDS_SEQUENCES.md` outgoing links count: **1** (only `SDS.md`). Should link back to `SDS_NETWORK_V2.md` (protocol layer) and `DICOM_CONFORMANCE_STATEMENT.md` (conformance). (Phase 3)
15. `docs/SDS_TRACEABILITY.md` does NOT list `PRD.md` or `SRS.md` or `FEATURES.md` among its outgoing links. The 13 links it has go to sibling SDS modules and `VALIDATION_REPORT.md`, but a traceability matrix must explicitly reference the upstream requirement documents it traces from. (Phase 3)

---

## Nice-to-Have Items

### Phase 2 — Style (2)

16. `docs/PRD.kr.md:135` — Uses "DCMTK" for benchmark baselines but `docs/PRD.md:70` uses the same. Acceptable but should clarify that `pacs_system` is DCMTK-independent at implementation level while using `storescu`/`findscu`/`movescu`/`echoscu` (DCMTK CLI tools) as interop test harness. Scattered notes in `GETTING_STARTED.md:45-46`, `DICOM_CONFORMANCE_STATEMENT.md:52`, and `SRS.md:174` say this individually but there is no single canonical paragraph. (Phase 2)
17. `docs/PRD.md` History table versions (1.0.0 → 2.3.0) use the "doc revision" scheme while the header / footer use "product version" (0.2.0.0). The `docs/PRD.md:1085` explanatory note is correct but buried inside Appendix C. Consider promoting this paragraph to Appendix C's opening line or to the top of the Document History section. (Phase 2)

### Phase 3 — Redundancy / Orphans (3)

18. **Orphan docs** — files not linked inbound by any other `.md` (excluding legitimate entry points CLAUDE.md, CHANGELOG.md, SECURITY.md, CONTRIBUTING.md, CODE_OF_CONDUCT.md, `.github/*`):
   - `docs/GETTING_STARTED.md` (468 lines) — substantial onboarding content that only `README.md` heading-anchors, never a direct file link.
   - `docs/ECOSYSTEM.md` (31 lines) — not referenced; content appears duplicated inside `ARCHITECTURE.md` "Ecosystem Dependencies" section.
   - `samples/01_hello_dicom/README.md` … `samples/05_production_pacs/README.md` + `samples/README.md` + `samples/common/README.md` — no inbound links from `docs/` tree. Root `README.md:34` "CLI Tools & Examples" section should link to `samples/README.md`.
   - `web/README.md` — no inbound links (documented in `SDS_WEB_API.md` textually, but not linked).
   - `examples/integration_tests/scripts/README.md` and `examples/integration_tests/test_data/README.md` — no inbound links.
   (Phase 3)
19. `docs/ECOSYSTEM.md` (31 lines) appears to be a stub that duplicates `docs/ARCHITECTURE.md#ecosystem-dependencies`. Either fold it into ARCHITECTURE or expand it with a clear authoritative-for scope. (Phase 3)
20. Korean sibling drift — beyond #8 (SDS_TRACEABILITY), minor version/date drift is visible across several `*.kr.md` files (e.g., `SDS_TRACEABILITY` 3.0.0 → 0.1.3). Not all were audited file-by-file; a standing "sync KR to EN on merge" CI gate would help. (Phase 3)

---

## Details — 31 Files with Frontmatter/Inline Version Drift (Must-Fix #7)

```
docs/API_REFERENCE.md                   fm=1.0.0  inline=0.1.5.0
docs/API_REFERENCE_CORE.md              fm=1.0.0  inline=0.1.5.0
docs/API_REFERENCE_NETWORK.md           fm=1.0.0  inline=0.1.5.0
docs/API_REFERENCE_SECURITY.md          fm=1.0.0  inline=0.1.5.0
docs/API_REFERENCE_SERVICES.md          fm=1.0.0  inline=0.1.5.0
docs/API_REFERENCE_WEB.md               fm=1.0.0  inline=0.1.5.0
docs/ARCHITECTURE.md                    fm=1.0.0  inline=0.1.4.0
docs/DICOM_CONFORMANCE_STATEMENT.md     fm=1.0.0  inline=0.1.0
docs/FEATURES.md                        fm=1.0.0  inline=0.2.0.0
docs/IHE_INTEGRATION_STATEMENT.md       fm=1.0.0  inline=0.1.0
docs/MIGRATION_COMPLETE.md              fm=1.0.0  inline=0.1.0.0
docs/PERFORMANCE_BASELINE.md            fm=1.0.0  inline=0.1.0.0
docs/PERFORMANCE_RESULTS.md             fm=1.0.0  inline=0.1.0.0
docs/PRD.md                             fm=1.0.0  inline=0.2.0.0
docs/PROJECT_STRUCTURE.md               fm=1.0.0  inline=0.1.3.0
docs/SDS.md                             fm=1.0.0  inline=0.2.0
docs/SDS_AI.md                          fm=1.0.0  inline=2.0.0
docs/SDS_COMPONENTS.md                  fm=1.0.0  inline=0.2.0
docs/SDS_DATABASE.md                    fm=1.0.0  inline=0.2.0
docs/SDS_DI.md                          fm=1.0.0  inline=1.1.0
docs/SDS_INTERFACES.md                  fm=1.0.0  inline=0.1.3
docs/SDS_MONITORING_COLLECTORS.md       fm=1.0.0  inline=2.1.0
docs/SDS_NETWORK_V2.md                  fm=1.0.0  inline=2.0.0
docs/SDS_SEQUENCES.md                   fm=1.0.0  inline=0.1.1
docs/SDS_SERVICES_CACHE.md              fm=1.0.0  inline=1.1.0
docs/SDS_TRACEABILITY.md                fm=1.0.0  inline=3.0.0
docs/SDS_WEB_API.md                     fm=1.0.0  inline=2.0.0
docs/SECURITY.md                        fm=1.0.0  inline=0.1.1.0
docs/SRS.md                             fm=1.0.0  inline=0.1.3.2
docs/contributing/CONTRIBUTING.md       fm=1.0.0  inline=0.1.0.0
docs/database/MIGRATION_GUIDE.md        fm=1.0.0  inline=1.0.1
```

---

## Details — Positive Findings (no issues)

- **Intra-file anchors**: 0 broken out of 1,223 non-external links. All `](#anchor)` references resolve via the GitHub slug (duplicate-suffix aware).
- **Inter-file `.md#anchor` links**: 0 broken. Every section-anchored cross-file link resolves.
- **DICOM terminology is consistent**: `C-STORE` / `C-FIND` / `C-MOVE` / `C-ECHO` / `C-GET` are used uppercase-hyphenated everywhere (no `CSTORE`, `c-store`, or `CStore` mixed forms in prose). `AE Title` is consistent (65 occurrences, no `AETitle` / `AE_Title` in documentation).
- **DICOM PS3 references** are internally consistent across `DICOM_CONFORMANCE_STATEMENT.md`, `SDS_SECURITY.md`, `SDS_AI.md`, `SDS_NETWORK_V2.md`, and sample READMEs (PS3.2, PS3.4, PS3.5, PS3.6, PS3.7, PS3.8, PS3.10, PS3.15, PS3.18).
- **DICOM Standard year**: `2024` is used consistently (SRS.md:82, SDS_COMPRESSION.md:759, FEATURES.md).
- **C++ standard**: `C++20` is used uniformly (65 occurrences). The only non-C++20 mention is `bug_report.md:40` which is an issue-template prompt listing options ("C++17, C++20") — appropriate.
- **DCMTK version**: Not pinned anywhere (DCMTK is only used as an interop baseline / CLI tool harness). CLAUDE.md declares "Zero external DICOM libraries" and this is consistent across ARCHITECTURE.md:81, SDS.md:595, PACS_IMPLEMENTATION_ANALYSIS.md:23, SRS.md:174, DICOM_CONFORMANCE_STATEMENT.md:52.
- **HL7 version**: HL7 v2.x is the only version mentioned (`IHE_INTEGRATION_STATEMENT.md:139,244`). No contradiction with a hypothetical HL7 FHIR claim.
- **Library versions**: `SQLite3 >= 3.45.1`, `Catch2 v3.4.0`, `Crow v1.3.1`, `ASIO 1.30.2` — consistently stated between `CLAUDE.md`, `GETTING_STARTED.md`, `PERFORMANCE_BASELINE.md`, `VERIFICATION_REPORT.md`.
- **Ecosystem count**: The 6 → 7 update (addition of `database_system`, Issue #674) is reflected in `docs/PRD.md` document history (v2.2.0, 2026-02-09) and `ARCHITECTURE.md` diagrams.
- **SSOT declarations**: 25 documents carry the `> **SSOT**: This document is the single source of truth for …` banner. No two docs claim SSOT for the same topic.

---

## Score

- **Overall**: **7.8 / 10**
- **Anchors** (Phase 1): **9.5 / 10** — 4 broken inter-file refs (all in `examples/integration_tests/`), zero broken anchors. The anchor system (GitHub slugs) is well-maintained across 104 docs and 1,223 links.
- **Accuracy** (Phase 2): **8.5 / 10** — Terminology and DICOM PS3 references are excellent. Deductions: PRD Appendix C cross-version table is stale (SRS, SDS rows), 8 insecure HTTP spec links in samples, and phrasing around DICOM 2019b could be unified.
- **SSOT** (Phase 3): **5.5 / 10** — The 31-document frontmatter/inline version drift is a systemic SSOT contradiction. Korean traceability file is 53% shorter than English. Key modules (DICOM_CONFORMANCE_STATEMENT, SDS_NETWORK_V2, SDS_SEQUENCES) are under-linked to upstream requirement docs.

---

## Notes

- **Report-only mode**: No `.md` source files were modified. All 104 markdown files were read; none were written or deleted.
- **Filesystem caveat**: The local volume is macOS APFS (case-insensitive on this disk). Case-mismatch items (e.g., `architecture.md` vs `ARCHITECTURE.md`) were detected via a case-sensitive path walk (`os.listdir`) rather than `Path.exists()`, so the findings are valid for Linux / CI environments.
- **Fenced code exclusion**: All ` ``` ` and `~~~` fenced blocks are excluded from both heading and link extraction. Inline backtick code spans are also stripped from link extraction so that shell examples like `` `cp [src](target)` `` do not produce false positives.
- **External URL exclusion**: Any link target containing `:` and not starting with `#` was treated as an external URL and excluded from file-existence and anchor validation (per brief).
- **GitHub slug algorithm used**: lowercase → strip all punctuation except `-`, `_`, alphanumerics, whitespace → replace whitespace with `-` → preserve consecutive `-` (GitHub does NOT collapse; `CLI Tools & Examples` → `cli-tools--examples` with double hyphen). Duplicate headings add `-N` suffix (N starts at 1 for the second occurrence).
- **Recommended follow-ups (by ROI)**:
  1. Regenerate YAML frontmatter across the 31 docs to match inline `**Version:**` (or drop the frontmatter version field) — single scripted pass. (Must-Fix #7)
  2. Re-sync `docs/SDS_TRACEABILITY.kr.md` from the English v3.0.0 source. (Must-Fix #8)
  3. Fix the 4 broken links in `examples/integration_tests/`. (Must-Fix #1–4)
  4. Patch `docs/PRD.md:1094-1095` Appendix C version table (SRS 0.1.3.1 → 0.1.3.2; SDS 0.2.0.0 → 0.2.0). (Must-Fix #5, #6)
  5. Add outgoing cross-refs from `DICOM_CONFORMANCE_STATEMENT.md`, `SDS_NETWORK_V2.md`, `SDS_SEQUENCES.md`, `SDS_TRACEABILITY.md` back to PRD / SRS. (Should-Fix #12–15)
  6. Upgrade 8 `http://dicom.nema.org` clickable links in samples to `https://`. (Should-Fix #9)

---

*End of report.*
