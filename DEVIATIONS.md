# DEVIATIONS

This document records intentional, time-bounded deviations from the project's
target architecture, together with the criteria that close each deviation. It is
documentation only — it does not change code or the build.

Tracking issue: **#1180**.

Scope of this revision: define the stabilization criteria and toolchain gates
for the experimental C++20 modules target (`pacs_system_modules`), and document
a staged sunset plan for the `include/kcenon/pacs/compat/` shims.

Authoritative in-repo sources cited below:

- `cmake/options.cmake` — `PACS_BUILD_MODULES` option
- `cmake/targets.cmake` — `pacs_system_modules` target and its toolchain note
- `src/modules/*.cppm` — module interface units (primary module `kcenon.pacs`)
- `include/kcenon/pacs/compat/{format.h,time.h,namespace_compat.h}` — shims
- `CMakeLists.txt` — `project(pacs_system VERSION 1.0.0 ...)`
- `README.md` — build options and "C++20 Module Support" section
- `cmake/VERSION`, `cmake/DEVIATIONS.md` — ecosystem cmake-template version (#1140)

> Note on naming: this file (`DEVIATIONS.md`, repo root) is distinct from the
> pre-existing `cmake/DEVIATIONS.md`, which tracks cmake-module divergences from
> the ecosystem template (#1140). They do not overlap in scope.

---

## Deviation 1 — C++20 modules ship as an experimental target

### Current state (factual, from the repository)

- The modules target is **off by default**:
  `option(PACS_BUILD_MODULES "Build C++20 module version of pacs_system" OFF)`
  in `cmake/options.cmake`. Enable with `-DPACS_BUILD_MODULES=ON`
  (`README.md`, "Building with Modules").
- When enabled, `cmake/targets.cmake` builds `pacs_system_modules`
  (alias `kcenon::pacs_modules`) via a `FILE_SET CXX_MODULES`, and the build
  **self-disables** the target with a warning if CMake is older than 3.28.
- The module is real, not a placeholder: `src/modules/pacs.cppm` is the primary
  interface unit `kcenon.pacs`, aggregating 12 partitions
  (`core`, `encoding`, `storage`, `security`, `network`, `services`,
  `workflow`, `web`, `integration`, `ai` [guarded by `KCENON_WITH_AI`],
  `monitoring`, `di`). The partitions re-export the existing header library API
  (e.g. `pacs-core.cppm` re-exports `pacs::core` and the `Result<T>` pattern);
  the header-based interface remains the primary, fully supported API.
- **Experimental status is declared in the repository**: the `cmake/targets.cmake`
  module note and `README.md` ("C++20 modules are experimental. The header-based
  interface remains the primary API.").

### Toolchain gates (declared in the repository)

Both the in-source note and the README state the same floors. These are floors
known to **configure** the modules target — not a certification of stability.

| Tool  | `cmake/targets.cmake` note | `README.md` (Module Support) | Enforcement |
|-------|----------------------------|------------------------------|-------------|
| CMake | 3.28                       | 3.28                         | enforced at configure time — the target self-disables below 3.28 |
| GCC   | 14                         | 14                           | documentation only |
| Clang | 16                         | 16                           | documentation only |
| MSVC  | 2022 17.4                  | 2022 17.4                    | documentation only |

(Note: this *modules* floor differs from the wider *header-build* compiler floor
in `README.md` "Requirements" — GCC 10+ / Clang 10+ / MSVC 2022 19.30+, with
GCC 13+/Clang 14+ recommended.)

### Definition of "stable" (criteria framework)

The modules target is promoted from *experimental* to *supported* only when
**all** of the following hold. Promotion is criteria-driven, not date-driven;
concrete dates are intentionally omitted.

- [ ] **CI matrix green.** A clean configure + build + test of
      `pacs_system_modules` passes on all three named compilers (GCC, Clang,
      MSVC) at or above the declared minimum versions, on the CI runners, with
      CMake 3.28+. (Today the modules target is opt-in and not part of the
      default CI build matrix.)
- [ ] **CMake floor exercised.** The CI toolchain provides CMake 3.28+ for the
      modules job so the target is actually built rather than self-disabled.
- [ ] **Exported surface frozen.** The partition layout and exported surface of
      `kcenon.pacs` (`src/modules/*.cppm`) are declared stable, matching the
      header library's public API. The exact frozen surface is **TBD** and must
      be enumerated before promotion.
- [ ] **No silent breakage.** The "experimental" caveat is removed from
      `cmake/targets.cmake` and `README.md` only once the above are met.

### Specific values still to be finalized (TBD — do not invent)

- [ ] **TBD** — exact CI runner OS / image versions per compiler.
- [ ] **TBD** — minimum *supported* (not just *configurable*) compiler versions,
      if they differ from the configure floors above.
- [ ] **TBD** — whether GCC, Clang, and MSVC are all gating, or a subset gates
      initial support with the rest "best effort".
- [ ] **TBD** — the enumerated, frozen `kcenon.pacs` export surface, and whether
      `import std;` is adopted (the units currently use header includes in the
      global-module fragment, not `import std;`).
- [ ] **TBD** — performance / IDE-tooling acceptance bar, if any.
- [ ] **TBD** — reconcile the `module_version` constant in `src/modules/pacs.cppm`
      (declares `0.1.0.0`) with the product version `1.0.0` in `CMakeLists.txt`.

### Closure

This deviation closes when the modules target is promoted to supported and the
experimental caveats are removed from `cmake/targets.cmake` and `README.md`.

---

## Deviation 2 — `include/kcenon/pacs/compat/` shims are active

### Current state (factual, from the repository)

- There is **no build option** gating the compat layer. `cmake/options.cmake`
  declares no `PACS_USE_COMPAT`; the compat headers under
  `include/kcenon/pacs/compat/` are always available and included where partial
  std support or legacy paths require it (`README.md` lists "Compat" as
  "Compatibility shims for cross-platform and legacy include paths"). Any
  reference to a `PACS_USE_COMPAT` toggle is incorrect.
- Three shims exist, each carrying its own in-source rationale:
  - `compat/format.h` — aliases `std::format` into `kcenon::pacs::compat`
    (defines `PACS_HAS_STD_FORMAT 1`). Its header documents that all supported
    compilers already provide C++20 `std::format`; the shim exists so existing
    call sites compile unchanged.
  - `compat/time.h` — cross-platform thread-safe `localtime_safe` / `gmtime_safe`
    wrappers over POSIX `localtime_r`/`gmtime_r` vs Windows `localtime_s`/`gmtime_s`.
  - `compat/namespace_compat.h` — provides `namespace pacs = kcenon::pacs;` so
    downstream code using the old `pacs::` namespace still compiles after the
    migration to `kcenon::pacs::`.

### Version SSOT status (prerequisite context)

- The product version SSOT is `project(pacs_system VERSION 1.0.0 ...)` in
  `CMakeLists.txt`.
- `cmake/VERSION` (currently `1.0.0`) is a **different** axis: it records the
  ecosystem cmake-template version aligned under `cmake/DEVIATIONS.md` (#1140),
  not the product version.
- Issue #1180 ties the compat sunset to "cmake/VERSION SSOT alignment". The
  precise meaning of that alignment is **TBD** and must be confirmed before
  Stage 1; the staged plan below is gated on it either way.

### Sunset roadmap (staged plan — documentation, not code changes)

The compat shims are retired in stages. The **prerequisite** for any shrink step
is that the "cmake/VERSION SSOT alignment" from #1180 lands first.

Stage 0 — Prerequisite (gates all later stages)

- [ ] The cmake/VERSION SSOT alignment referenced by #1180 lands (exact scope to
      be confirmed — see TBD above).

Stage 1 — Deprecate

- [ ] Mark the shims deprecated (e.g. deprecation pragma/comment) signalling
      planned removal, once the prerequisite lands.

Stage 2 — Shrink (criteria-driven)

- [ ] Remove `compat/format.h` once the supported toolchain floor guarantees
      full `<format>` everywhere and all call sites use `std::format` directly —
      tie to the Deviation 1 supported-compiler decision.
- [ ] Remove `compat/time.h` if/when the supported toolchains provide a portable
      thread-safe time API that makes the POSIX/Windows wrappers unnecessary.
- [ ] Remove `compat/namespace_compat.h` once all downstream consumers have
      migrated from `pacs::` to `kcenon::pacs`.

Stage 3 — Remove

- [ ] Delete `include/kcenon/pacs/compat/` once no shim remains in use.

### Specific values still to be finalized (TBD — do not invent)

- [ ] **TBD** — exact meaning/scope of the #1180 "cmake/VERSION SSOT alignment".
- [ ] **TBD** — the toolchain floor that makes `format.h` removable (tie to
      Deviation 1).
- [ ] **TBD** — whether `time.h` is ever removable or is permanently required for
      POSIX/Windows portability (it papers over a platform API difference, not a
      missing std feature).
- [ ] **TBD** — inventory of remaining `pacs::`-namespace downstream consumers to
      migrate before removing `namespace_compat.h`.
- [ ] **TBD** — whether Stage 1 deprecation is warning-only or hard error.

### Closure

This deviation closes when `include/kcenon/pacs/compat/` is removed.

---

## Notes

- Promotion (Deviation 1) and removal (Deviation 2) are **criteria-driven**, not
  date-driven; concrete dates are intentionally omitted.
- Items marked **TBD** are open decisions under issue #1180 and must be resolved
  (not guessed) before the corresponding checklist item can be ticked.
