# DEVIATIONS

This document records intentional, time-bounded deviations from the project's
target (1.0.0) architecture, together with the criteria that close each
deviation. It is documentation only — it does not change code or the build.

Tracking issue: **#1180**.

Related, authoritative sources in this repository:

- `CMakeLists.txt` — `PACS_BUILD_MODULES` / `PACS_USE_COMPAT` options
- `CMakePresets.json` — `default` vs `modules-experimental` presets
- `src/modules/` — experimental module interface units (`pacs.core`)
- `src/compat/` and `src/compat/README.md` — compatibility shims
- `docs/MODULES_GUIDE.md` — module toolchain requirements and limitations
- `docs/COMPILER_COMPATIBILITY.md` — default vs modules compiler floors
- `docs/VERSIONING.md`, `cmake/version.cmake`, root `VERSION` — version SSOT

---

## Deviation 1 — C++20 modules ship as an experimental target

### Current state (factual, from the repository)

- The modules target is **off by default**:
  `option(PACS_BUILD_MODULES "Build experimental C++20 modules target" OFF)`
  in `CMakeLists.txt`. The `modules-experimental` preset opts in
  (`PACS_BUILD_MODULES=ON`, `PACS_USE_COMPAT=OFF`).
- Only a **minimal surface** is exported: `src/modules/pacs.core.cppm` exports
  module `pacs.core` with a placeholder `pacs::Version` struct. The full public
  API export is marked TBD in-source and in `src/modules/README.md`.
- The target is **excluded from CI gating** while experimental
  (`src/modules/CMakeLists.txt`, `docs/MODULES_GUIDE.md`).
- `docs/MODULES_GUIDE.md` labels the target EXPERIMENTAL and "may change or be
  removed without notice".

### Toolchain gates (declared in the repository)

From `docs/MODULES_GUIDE.md`, `docs/COMPILER_COMPATIBILITY.md`, and the in-tree
CMake note (`CMakeLists.txt`, `src/modules/CMakeLists.txt`). These are the
floors known to **configure** the modules target — not a certification that the
target is stable on these versions.

| Tool  | Declared minimum (modules) | Source / note |
|-------|----------------------------|---------------|
| GCC   | 14                         | named modules; `import std` not used |
| Clang | 17                         | use libc++ or recent libstdc++ |
| MSVC  | 19.36 (VS 2022 17.6)       | `/std:c++20` |
| CMake | 3.28                       | required for `FILE_SET CXX_MODULES`; project-wide minimum is 3.20, so the modules target raises the floor for itself |

For contrast, the default (header + compat) build supports a wider range
(GCC 11+, Clang 14+, MSVC 19.29+ per `docs/COMPILER_COMPATIBILITY.md`).

### Definition of "stable" (criteria framework)

The modules target is promoted from *experimental* to *opt-in supported* only
when **all** of the following hold. Promotion is criteria-driven, not
date-driven; concrete dates are intentionally omitted.

- [ ] **CI matrix green.** A clean configure + build + test of
      `pacs_system_modules` passes on all three supported compilers
      (GCC, Clang, MSVC) at or above the declared minimum versions, on the CI
      runners. Today the target is explicitly excluded from CI gating.
- [ ] **CMake floor enforced for the target.** The modules build requires
      CMake 3.28+ (rather than relying only on the project-wide 3.20 minimum),
      and the CI toolchain matrix provides it.
- [ ] **API surface parity defined.** The exported module surface
      (`src/modules/pacs.core.cppm`) covers the agreed public API rather than
      the current minimal `pacs::Version` placeholder. The exact target surface
      is **TBD** and must be enumerated before promotion.
- [ ] **compat consumers migrated.** Internal consumers that currently rely on
      `src/compat/` can build against module imports (see Deviation 2).
- [ ] **No silent breakage.** The "may change or be removed without notice"
      caveat is removed from `docs/MODULES_GUIDE.md` and `src/modules/README.md`
      only once the above are met.

### Specific values still to be finalized (TBD — do not invent)

- [ ] **TBD** — exact CI runner OS / image versions per compiler.
- [ ] **TBD** — minimum *supported* (not just *configurable*) compiler versions,
      if they differ from the configure floors above.
- [ ] **TBD** — whether GCC, Clang, and MSVC are all gating, or a subset gates
      initial opt-in support with the rest "best effort".
- [ ] **TBD** — the enumerated public module export surface for `pacs.core`
      (and whether additional module units are introduced).
- [ ] **TBD** — performance / IDE-tooling acceptance bar, if any
      (`docs/MODULES_GUIDE.md` notes build performance is not yet benchmarked
      and IDE/tooling support is still maturing).
- [ ] **TBD** — whether `import std;` is adopted (currently not used).

### Closure

This deviation closes when the modules target is promoted to supported (and
ultimately default) at 1.0.0, with the caveats removed from the module docs.

---

## Deviation 2 — `src/compat/` compatibility layer is active

### Current state (factual, from the repository)

- `option(PACS_USE_COMPAT "Use compatibility shims for partial std support" ON)`
  in `CMakeLists.txt`; the compat layer is the supported path while modules are
  experimental. The `modules-experimental` preset disables it
  (`PACS_USE_COMPAT=OFF`).
- Components (`src/compat/`, per `src/compat/README.md`):
  - `format_compat.h` — papers over partial `<format>`; removable when the
    floor guarantees full `std::format`.
  - `time_compat.h` — papers over partial `<chrono>` calendar; removable when
    the floor guarantees `<chrono>` calendar support.
  - `namespace_compat.h` — legacy namespace aliases; removable when all
    consumers are migrated off the aliases.

### Version SSOT status (prerequisite context)

- The single source of truth for the version is the root `VERSION` file
  (currently `0.9.0`); `cmake/version.cmake` reads it and configures
  `include/common/pacs_version.h`, and `project(... VERSION ...)` reads the same
  file (`docs/VERSIONING.md`).
- The historical hardcoded `0.8.5` in `cmake/version.cmake` has already been
  removed. `docs/VERSIONING.md` notes "remaining SSOT cleanup in the compat
  layer" is still tracked under #1180 — that cleanup is the prerequisite below.

### Sunset roadmap (staged plan — documentation, not code changes)

The compat layer is retired in stages. The **prerequisite** for any shrink step
is that the VERSION SSOT alignment fully lands — i.e. no version definitions
remain duplicated anywhere, including any remnants reachable through the compat
layer — before compat headers begin to be removed.

Stage 0 — Prerequisite (gates all later stages)

- [ ] VERSION SSOT alignment fully lands: every consumer derives the version
      from the root `VERSION` file; no duplicate/hardcoded version remains
      (the "remaining SSOT cleanup in the compat layer" from
      `docs/VERSIONING.md` is complete).

Stage 1 — Deprecate

- [ ] Emit deprecation warnings from the compat headers once the prerequisite
      lands, signalling planned removal.

Stage 2 — Shrink (criteria-driven)

- [ ] Remove `format_compat.h` once the supported toolchain floor guarantees
      full `std::format` (re-evaluate against the modules compiler matrix in
      Deviation 1).
- [ ] Remove `time_compat.h` once `<chrono>` calendar support is guaranteed on
      the supported toolchains.
- [ ] Migrate `namespace_compat.h` consumers off the legacy aliases, then
      remove the header.

Stage 3 — Remove (1.0.0)

- [ ] `PACS_USE_COMPAT` default flips and/or the option is removed; `src/compat/`
      is deleted once no consumer depends on the layer
      (`docs/COMPILER_COMPATIBILITY.md`: the compat layer is removed once the
      default-build compiler floor is raised to guarantee the std features).

### Specific values still to be finalized (TBD — do not invent)

- [ ] **TBD** — the exact toolchain floor that makes `format_compat.h` and
      `time_compat.h` unnecessary (tie to the Deviation 1 supported-compiler
      decision).
- [ ] **TBD** — inventory of remaining `namespace_compat.h` consumers to migrate.
- [ ] **TBD** — whether Stage 1 deprecation is warning-only or a hard error.

### Closure

This deviation closes at 1.0.0 when `src/compat/` is removed.

---

## Notes

- Promotion (Deviation 1) and removal (Deviation 2) are **criteria-driven**, not
  date-driven; concrete dates are intentionally omitted.
- Items marked **TBD** are open decisions under issue #1180 and must be resolved
  (not guessed) before the corresponding checklist item can be ticked.
