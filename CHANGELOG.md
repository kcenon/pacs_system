# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

> **Release state:** The latest published release is still [0.1.0](#010---2026-03-13)
> (`v0.1.0`; vcpkg `kcenon-pacs-system` 0.1.0, port-version 11). Entries below are untagged.
> Many of them prepare the planned v1.0 API contract, which remains unreleased: no v1.0.0 tag,
> GitHub release, or registry version exists, and publication is gated by
> [#1095](https://github.com/kcenon/pacs_system/issues/1095) and
> [#1164](https://github.com/kcenon/pacs_system/issues/1164). They were previously listed under a
> placeholder `[1.0.0] - TBD` heading; they move to a versioned section only when a release is tagged.

### Added

- Formalize the four PACS medical-domain release gates (DICOM conformance, TLS/ATNA audit logging, anonymization, storage/index migration) in `docs/RELEASE_GATES.md`: a gate matrix mapping each gate to its exact Catch2 tag / CTest selector, test source, authoritative conformance doc, and CI-enforcing workflow, plus a pre-release checklist that is followable without reading test source. Linked from `CONTRIBUTING.md` and surfaced as a "Release Gates" section in the generated release notes (`release.yml`) ([#1176](https://github.com/kcenon/pacs_system/issues/1176))
- IHE XDS.b Document Source actor (ITI-41) at `src/ihe/xds/` with pugixml + libcurl transport stack ([#1128](https://github.com/kcenon/pacs_system/issues/1128))
- IHE XDS.b Document Consumer actor (ITI-43) at `src/ihe/xds/` with retrieve envelope builder and MTOM/XOP response parser ([#1129](https://github.com/kcenon/pacs_system/issues/1129))
- IHE XDS.b Registry Query actor (ITI-18) at `src/ihe/xds/` with FindDocuments and GetDocuments stored queries against a conformant XDS.b Document Registry ([#1130](https://github.com/kcenon/pacs_system/issues/1130))
- ATNA audit emission for IHE XDS.b transactions (ITI-18/41/43) with Gazelle-lite integration test harness, and published `docs/IHE_CONFORMANCE.md` ([#1131](https://github.com/kcenon/pacs_system/issues/1131))
- v1.0.0 migration guide at [`docs/migration/0.x-to-1.0.md`](docs/migration/0.x-to-1.0.md): every breaking change between the 0.1.0 baseline and the 1.0.0 freeze documented with before/after snippets, an end-to-end `Result<T>` migration walkthrough, and an upgrade self-audit checklist. Linked from README "Upgrading" ([#1162](https://github.com/kcenon/pacs_system/issues/1162))

### Changed

- Keep `project(pacs_system VERSION ...)` at the published `0.1.0` (reverting the unreleased `1.0.0` bump from #1158 while keeping the `pacs_system::pacs_system` contract), synchronize the local `kcenon-pacs-system` overlay with canonical registry port 0.1.0#11, move the registry baseline to `40632164`, and record the validated ecosystem source tuple and current FetchContent pins in `dependency-manifest.json`, the dependency checkout action and `docs/SOUP.md` ([#1175](https://github.com/kcenon/pacs_system/issues/1175))
- API freeze for v1.0: enumerate the 290-header public surface under `include/kcenon/pacs/` in `docs/v1.0-api-surface.md`; confirm zero `[[deprecated]]` symbols and no experimental staging area; mark `network/detail/accept_worker.h` as the only implementation-detail header outside the v1.0 stability promise ([#1156](https://github.com/kcenon/pacs_system/issues/1156))
- Migrate `decode_rle_segment` (file-scope helper in `src/encoding/compression/rle_codec.cpp`) from `throw std::runtime_error` to `Result<std::vector<uint8_t>>` so malformed RLE segments surface through the public `rle_codec::decode()` `Result<T>` contract instead of escaping as exceptions. Document the remaining 10 throws in `src/integration/` and `src/storage/hsm_storage.cpp` as constructor-invariant or `std::future`-boundary ABI escapes in the new `docs/v1.0-throw-policy.md`. Public headers under `include/kcenon/pacs/` continue to contain zero non-comment `throw` statements ([#1157](https://github.com/kcenon/pacs_system/issues/1157))
- Enhance code coverage workflow with v1.0 70% line-coverage threshold gate, line/branch summary computation, and per-PR coverage comment with hit/total counts; threshold is advisory until v1.0 release (toggle via `PACS_COVERAGE_ENFORCE`) ([#1159](https://github.com/kcenon/pacs_system/issues/1159))
- Remove committed `.key` files from version control; regenerate via `generate_test_certs.sh` in CI and local development ([#1092](https://github.com/kcenon/pacs_system/issues/1092))
- **BREAKING**: Consolidate directory layout: `samples/` renamed to `examples/` (5 progressive tutorials) and `examples/` renamed to `tools/` (32 CLI utility binaries) so that the role split matches the ecosystem standard. CMake option names (`PACS_BUILD_EXAMPLES`, `PACS_BUILD_SAMPLES`) are preserved for backward compatibility. Downstream consumers that referenced the old paths must update to the new locations: tutorials are now under `examples/`, and CLI utility sources live under `tools/`. Build outputs for tutorials moved from `${CMAKE_BINARY_DIR}/samples` to `${CMAKE_BINARY_DIR}/examples` ([#1139](https://github.com/kcenon/pacs_system/issues/1139))
- Align `cmake/*.cmake` modules with the canonical ecosystem template at `common_system/cmake/template`; pacs-specific modules (`pacs_system-config.cmake.in`, `summary.cmake`) and intentional design divergences are documented in `cmake/DEVIATIONS.md`, and the aligned template version is recorded in `cmake/VERSION` ([#1140](https://github.com/kcenon/pacs_system/issues/1140))
- Stabilize the v1.0 CMake export contract: bump `project(pacs_system VERSION ...)` to `1.0.0`, add a canonical `pacs_system::pacs_system` aggregate INTERFACE target that links every required and built optional component, expose a `kcenon::pacs_system` alias for the legacy README spelling, and document the contract in the README "CMake Integration" section so downstream consumers can write `find_package(pacs_system 1.0 REQUIRED)` followed by `target_link_libraries(... PRIVATE pacs_system::pacs_system)`. Adds a regression consumer project at `tests/cmake_consumer/` ([#1158](https://github.com/kcenon/pacs_system/issues/1158))
- Replace POSIX iconv with ICU (ucnv_convert) for DICOM character set encoding/decoding ([#1012](https://github.com/kcenon/pacs_system/issues/1012))
- Implement memory-mapped I/O for DICOM file loading ([#989](https://github.com/kcenon/pacs_system/issues/989))
- Replace global singleton `pool_manager` with thread-local instances to eliminate cross-thread contention ([#992](https://github.com/kcenon/pacs_system/issues/992))
- Modernize Doxygen with doxygen-awesome-css theme, dark mode toggle, and standardized mainpage ([#1066](https://github.com/kcenon/pacs_system/issues/1066))
- Confirm canonical namespace is `kcenon::pacs::` across all source, with no remaining `pacs_system::` references; record ecosystem alignment ([#1138](https://github.com/kcenon/pacs_system/issues/1138))
- Document Catch2 test framework retention as an explicit ecosystem exception in README, `docs/ECOSYSTEM.md`, and the master EPIC ([#1141](https://github.com/kcenon/pacs_system/issues/1141))

### Removed

- Audited public and internal source for `[[deprecated]]` markers ahead of v1.0 freeze; no deprecated APIs were found, so no symbols were removed in this audit. Inventory recorded in [`docs/v1.0-deprecation-inventory.md`](docs/v1.0-deprecation-inventory.md) ([#1160](https://github.com/kcenon/pacs_system/issues/1160))

### Fixed

- DICOMweb: `pacs_server --http-port` serves it, on the database and storage of its DICOM services. Retrieve answers multipart for a study of one object, returns every object or a failure, honors `Accept` (406 without one, 400 for mixed families), converts Implicit VR, Big Endian and lossless compressed objects to Explicit VR Little Endian by default as PS3.18 8.7.3 requires, and checks that a series or instance is where its URL says. Corrected behavior: `/studies/{study}` no longer answers metadata for `Accept: application/dicom+json`; use `/metadata`.
- DICOM JSON now follows PS3.18 Annex F: uppercase tags, split values, numbers as numbers, all three person name groups, sequences, attribute tags, binary values and UTF-8. Before, a multi-valued DS produced invalid JSON and sequences were dropped. Metadata and search also speak the Native DICOM Model, and Bulk Data URIs answer: they used to name a route that did not exist.
- QIDO-RS answers from the same evaluation as C-FIND. The first query parameter used to be read as part of the path, so a search with one key returned every study. `limit` and `offset` work on every route and state what remains in a Warning, instead of stopping silently at 100. Bad parameters are 400, and search results carry what Tables 10.6.3-3 to 10.6.3-5 require, including Retrieve URL.
- WADO-RS frames: a frame past the end used to be left out silently, and `99,1` answered 200 with no data. A wrong, descending or repeated list now fails. Compressed frames are decoded for `application/octet-stream` or handed over as held for their own media type, instead of raw bytes of the encapsulated stream being cut by a native frame size.
- Rendering: render the frame asked for rather than always the first, write PNG, apply the window by the formulas of PS3.3 with `window=center,width,function`, decode compressed objects first, honor Pixel Representation, High Bit and MONOCHROME1, and scale to `viewport`.
- STOW-RS: store objects in the Transfer Syntax they came in, so compressed ones are no longer refused; fail a truncated body rather than keep part of it; list a part it did not store instead of skipping it; and enforce `max_body_size`, documented and until now not read. An instance is refused if its UIDs are not UIDs.
- C-STORE and STOW-RS store through `storage::archive_instance`. C-STORE used to answer success when the index refused an object, or when its Patient ID was empty, leaving a file no query could reach. Under simultaneous stores, two writers creating one patient, study or series row made one of them lose its object that way. Now the rows are written one at a time, a failure removes the file and reports a refusal, the index records the Transfer Syntax and image attributes, and `duplicate_policy::replace` no longer leaves the old file behind when the copy belongs to another study.
- The data dictionary now holds all of PS3.6, so no standard Keyword is refused in a search and no standard attribute reads as UN from Implicit VR.
- `pacs_server` refuses to start when it cannot open its HTTP port, instead of running without it.
- `pacs_server` logs timestamps through `localtime_r`. `std::localtime` shares one static buffer, and handlers log from several association threads: ThreadSanitizer reported the race.
- `pacs_server` now provides Storage Commitment, Print and Unified Procedure Step, and reports N-EVENT-REPORTs on associations it opens to the peers named with `--peer`. `tools/dimse_n_scu` hosts the SCU classes that had no tool.
- UPS: answer every status of PS3.4 Tables CC.1.1-2 and CC.2.6, keep the whole instance, never return the Transaction UID, read Receiving AE and Deletion Lock from the request, and carry each operation on the SOP Class CC.2 gives it. `ups_action_request_cancel` is now 2 and `ups_query_find_sop_class_uid` is now `...34.6.5`: the old values were those of other operations, and no other implementation could interoperate with them.
- Storage Commitment: commit an instance only when the index holds it under that SOP Class and its file exists, and report on the requesting association only after waiting for the N-EVENT-REPORT-RSP.
- Print: read Requested SOP identity on N-SET, N-GET, N-ACTION and N-DELETE, tie each Film Session to its association and each Film Box to the session it names, and answer invalid requests with their Annex H status instead of ending the association. `print_scu` can now send an image.
- MPPS: answer duplicates, unknown instances and final states with PS3.4 F.7.2 statuses, and `mpps_scu` sends every Type 1 and Type 2 attribute. `pacs_server` matches Worklist keys and returns the scheduled step.
- Implicit VR: add the Storage Commitment, UPS, MPPS, Worklist and Print attributes to the data dictionary, and read an undefined length on an unknown tag as a sequence. Implicit VR requests of these services failed to decode or lost their values.
- `dicom_server_v2`: answer an association with no acceptable presentation context with A-ASSOCIATE-RJ instead of an A-ASSOCIATE-AC refusing them all.
- Release workflow version check: replace grep parsing that missed the multi-line `project()` and `version-semver` with `scripts/verify_release_version.py`, which also checks Doxygen `PROJECT_NUMBER` and the overlay manifest ([#1175](https://github.com/kcenon/pacs_system/issues/1175))
- `cmake --preset vcpkg`: resolve the ecosystem packages installed by the published registry ports, and add `kcenon-logger-system` and `libjpeg-turbo` to the root manifest to match the overlay's default dependency set ([#1175](https://github.com/kcenon/pacs_system/issues/1175))
- Replace thread-unsafe `std::localtime` with `localtime_r`/`localtime_s` ([#990](https://github.com/kcenon/pacs_system/issues/990))

### Security

- `rest_server_config::enable_rest_api`: most routes under `/api` check no credentials, and OAuth 2.0 tokens do not cover them. `pacs_server` serves DICOMweb and WADO-URI only, and allows no web origin unless `--http-cors-origin` names one.
- DICOMweb: nine read routes skipped authorization even with OAuth 2.0 enabled, including instance, frames and metadata retrieval and `/dicomweb/series` and `/dicomweb/instances`: anyone could read objects without a token. Every route now passes through one check.
- OAuth 2.0: an RSA key from a JWKS was built with its modulus and exponent in the wrong byte order on little-endian hosts, so every RS256 token failed verification.
- `file_storage` refuses a UID of only dots, which `sanitize_uid` would have left able to name a directory above the archive.
- Implement AES-256-GCM encryption for anonymizer `encrypt_value()` ([#987](https://github.com/kcenon/pacs_system/issues/987))
- Replace non-cryptographic `std::hash` with SHA-256 in anonymizer `hash_value()` ([#988](https://github.com/kcenon/pacs_system/issues/988))
- Set `non_downgrading=true` in BCP195 basic TLS profile to prevent version downgrade attacks ([#991](https://github.com/kcenon/pacs_system/issues/991))
- Handle patient age in basic anonymization profile per HIPAA Safe Harbor requirements ([#993](https://github.com/kcenon/pacs_system/issues/993))

## [0.1.0] - 2026-03-13

### Added

- DICOM protocol implementation (C-ECHO, C-STORE, C-FIND, C-MOVE, C-GET)
- Transfer syntax support (Implicit VR Little Endian, Explicit VR, JPEG, JPEG2000, HTJ2K, JPEG-LS, RLE)
- PACS storage with SQLite backend
- DICOMweb REST API via Crow HTTP framework
- Image codec support (JPEG, JPEG2000, HTJ2K, JPEG-LS, PNG)
- TLS/SSL support for secure DICOM associations
- AWS S3 and Azure Blob storage integration
- AI inference pipeline integration
- CLI tools (pacs_echo, pacs_store, pacs_find, pacs_move, pacs_get, pacs_server)
- Network layer migration to public tcp_facade API ([#934](https://github.com/kcenon/pacs_system/issues/934))
- vcpkg overlay port with feature-based dependency declaration ([#921](https://github.com/kcenon/pacs_system/issues/921))
- CMake install/export infrastructure ([#920](https://github.com/kcenon/pacs_system/issues/920))
- Release workflow for version tagging ([#925](https://github.com/kcenon/pacs_system/issues/925))
- GitHub Actions CI/CD with sanitizer testing
- Integration test workflow
- SBOM generation workflow
- Doxygen documentation with GitHub Pages deployment
- vcpkg manifest with feature-based codecs, storage, and cloud features
- Cross-platform support (Linux, macOS, Windows)

[Unreleased]: https://github.com/kcenon/pacs_system/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/kcenon/pacs_system/releases/tag/v0.1.0
