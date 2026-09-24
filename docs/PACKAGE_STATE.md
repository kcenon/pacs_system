---
doc_id: "PAC-PROJ-008"
doc_title: "Package State - PACS System"
doc_version: "1.0.0"
doc_date: "2026-09-24"
doc_status: "Accepted"
project: "pacs_system"
category: "PROJ"
---

# Package State

> **Language:** **English** | [한국어](PACKAGE_STATE.kr.md)

This page records the published package state of `pacs_system`, how the repository's package
metadata relates to the canonical vcpkg registry, and how that state is verified. Decision and
evidence are tracked in [#1175](https://github.com/kcenon/pacs_system/issues/1175).

## Decision

| Item | State |
|------|-------|
| Latest published package | **0.1.0** — tag `v0.1.0`, GitHub release `v0.1.0` |
| Canonical vcpkg port | `kcenon-pacs-system` 0.1.0, port-version 11 |
| Canonical registry | [kcenon/vcpkg-registry](https://github.com/kcenon/vcpkg-registry) at `40632164c62b2256579a27eda228c48b057cbee9` |
| v0.1.0 archive SHA512 | `0db1f80c979f726efc6db5836e626d717297e6e4ead74005973d655ab63e67d093ed89afa402579d53e823a7c1bf5ac350ee199faf2f5b5e2cb70976ed4c282a` |
| v1.0 | Unreleased API-contract milestone. No v1.0.0 tag, GitHub release, or registry version exists. Readiness is governed by [#1095](https://github.com/kcenon/pacs_system/issues/1095); the dependency release gate by [#1164](https://github.com/kcenon/pacs_system/issues/1164). |

Every repository version source stays at 0.1.0 until a release is actually prepared:
`CMakeLists.txt` `project(... VERSION)`, `Doxyfile` `PROJECT_NUMBER`, root `vcpkg.json`
`version-semver`, and `vcpkg-ports/kcenon-pacs-system/vcpkg.json`. Check them together with:

```bash
python3 scripts/verify_release_version.py 0.1.0
```

The release workflow runs the same verifier against its `version` input before tagging.

## Local overlay port

`vcpkg-ports/kcenon-pacs-system/` is a byte-identical copy of the canonical registry port
(`vcpkg.json`, `portfile.cmake`, `usage`). Change the canonical port first, then copy it here.

Known canonical defect: the port's `usage` text lists `pacs_system::*` targets, but the 0.1.0
package exports `kcenon::pacs::<component>` targets and installs headers under `include/pacs/`.
The `pacs_system::*` names belong to the planned v1.0 contract. The overlay keeps the canonical
text until the registry is corrected in
[kcenon/vcpkg-registry#104](https://github.com/kcenon/vcpkg-registry/issues/104).

## Root manifest and overlay dependency sets

The root `vcpkg.json` builds the current source tree; the overlay builds the published `v0.1.0`
archive. Their default (core) dependency sets match: common, container, logger, network and
thread ecosystem ports, ICU, and `libjpeg-turbo >= 3.0.2`. The remaining differences are
intentional:

| Difference | Root `vcpkg.json` | Overlay port | Reason |
|------------|-------------------|--------------|--------|
| `vcpkg-cmake`, `vcpkg-cmake-config` | absent | host dependencies | Needed only by a portfile build |
| `overrides` | exact third-party pins | absent | vcpkg ignores `overrides` in ports; the root pins feed version-drift checks |
| `rest-api` feature Crow floor | `>= 1.3.1` | `>= 1.2.1` | The overlay mirrors the canonical port; the root build pins Crow 1.3.1 |
| `pugixml`, `curl[openssl]` | core dependencies | absent | IHE XDS.b actors (#1128-#1131) exist only in the current source tree, not in `v0.1.0` |
| `testing` feature | `benchmark` only (Catch2 via FetchContent) | `gtest` + `benchmark` | The overlay mirrors the canonical port for the `v0.1.0` test setup |
| `KCENON_WITH_*` options | n/a | legacy `PACS_WITH_*` only | `v0.1.0` predates the canonical option shim |

The default root build resolves published registry ports, which install legacy package names
(`ContainerSystem`, `NetworkSystem`, `LoggerSystem`). `cmake/dependencies.cmake` accepts those
names after the current `container_system`/`network_system`/`logger_system` names, the export
bridges link whichever package was found, and the installed `pacs_system-config.cmake` accepts
the same legacy names. `cmake --preset vcpkg` therefore works with the reviewed registry
baseline, and a source install passes `tests/cmake_consumer`.

## Dependency provenance

`dependency-manifest.json` records the current FetchContent pins (SQLite
`sqlite-amalgamation-3450300` / 3.45.3, OpenJPH 0.21.0, Crow v1.3.1, ASIO `asio-1-30-2`) and two
sets of ecosystem SHAs:

- `internal_ecosystem[].version`: the SHAs CI actually checks out through
  `.github/actions/checkout-kcenon-deps`, also listed in `docs/SOUP.md`. `scripts/check_manifest_drift.py`
  keeps the three in agreement.
- `internal_ecosystem_provenance.validated_source_tuple`: the tuple validated by
  [common_system coherence run 35861689435](https://github.com/kcenon/common_system/actions/runs/35861689435):

| Repository | Source SHA |
|------------|------------|
| common_system | `180a8155a96d9ef2794151b59e3fedff529c20d0` |
| thread_system | `ec5fdf3351e1bddd852f8ae4f51501a1d8dac4f1` |
| container_system | `605a0afd76ad78de97fc28ff6d416d5d08430b78` |
| logger_system | `02eafd95640bc24799a36ed043c3e6a16c688832` |
| network_system | `47cf31923ad2c883a96577fb52c7261ec191980f` |
| monitoring_system | `e8c4b2e333f9766511120a9cab063f73c7b970bb` |
| database_system | `0c0ad764a2c80ae3be5757b72f12299a1a2a7fd7` |

All of these Git SHAs are build-provenance pins, not published version constraints. Moving the
CI checkout pins to the validated tuple, and replacing SHAs with published v1.x constraints, are
owned by #1164 and happen only after every Tier 0-4 dependency ships v1.0.

## Verification

1. Pin vcpkg to the root builtin baseline (`d90a9b159c08169f39adcd1b0f1ac0ca12c4b96c`); older
   vcpkg checkouts lack the port versions that baseline selects.
2. Install through the overlay with a fresh cache: `VCPKG_BINARY_SOURCES=clear`, empty
   `VCPKG_DOWNLOADS` and `X_VCPKG_REGISTRIES_CACHE`, and a consumer manifest that depends on
   `kcenon-pacs-system` with `overlay-ports` pointing at `vcpkg-ports/`. vcpkg verifies the
   archive against the port SHA512.
3. Build an external consumer with `-DVCPKG_MANIFEST_INSTALL=OFF` against that install tree,
   using `find_package(pacs_system 0.1 CONFIG REQUIRED)` and `kcenon::pacs::core`, and confirm
   `pacs_system_DIR` points inside the install tree.
4. Run `cmake --preset vcpkg` and `cmake --build --preset vcpkg` from the repository root.
5. Install that build (`cmake --install build/vcpkg --prefix <prefix>`) and configure, build and
   run `tests/cmake_consumer` against it.
