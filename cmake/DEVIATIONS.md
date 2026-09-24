# pacs_system cmake/ Deviations from Ecosystem Template

This document records every place where `pacs_system/cmake/*.cmake` diverges
from the canonical ecosystem template at
[`common_system/cmake/template/`](https://github.com/kcenon/common_system/tree/main/cmake/template).
The aligned template version is recorded in [`cmake/VERSION`](VERSION).

The template is the Single Source of Truth (SSOT) for the **conventions**
shared across the kcenon ecosystem (canonical option names, target naming,
export-bridge pattern, install/export shape, warning flag list). pacs_system
adopts those conventions but in several places implements them directly
inside its own `cmake/*.cmake` files instead of including the template's
helper functions. The reasons are documented below.

This file is required by issue #1140 (acceptance criteria: "Each cmake module
is either bit-aligned with the canonical template or has a documented
deviation"). It is part of EPIC #1137 (kcenon ecosystem directory
standardization).

## Deviation Categories

| Category | Meaning |
|----------|---------|
| **Pattern** | pacs implements the same conventions but inline (no `include(template-name)`) |
| **Content** | pacs file holds project-specific definitions the template does not (cannot) carry |
| **Framework** | pacs uses a different test/build framework than the ecosystem default |
| **Role** | pacs assigns the file a different responsibility than the template |

## Per-File Deviations

### `compiler.cmake` -- Role + Content

* **Template role**: C++ standard baseline, single-config build-type default,
  `compile_commands.json` export. Provides three helpers
  (`kcenon_template_set_cpp_baseline`,
  `kcenon_template_set_default_build_type`,
  `kcenon_template_enable_compile_commands`).
* **pacs role**: defines the `PACS_WARNING_FLAGS` list and the
  `pacs_apply_warnings()` helper used by `warnings.cmake`. C++ standard,
  build-type default, and `CMAKE_EXPORT_COMPILE_COMMANDS` are set in the
  top-level `CMakeLists.txt` instead.
* **Why**: this file pre-dates the template and was historically the place
  where pacs collected its compiler flag wiring. Splitting it now would
  require changing every `include(...)` line and rewriting the warnings
  pipeline. See "Migration plan" below.
* **Functional contract**: equivalent to the template's warnings module
  (same `-Wall -Wextra -Wpedantic` list on Clang/GNU/AppleClang, `/W4` on
  MSVC, plus `-Werror`/`/WX` when `PACS_WARNINGS_AS_ERRORS` is ON).

### `dependencies.cmake` -- Content

* **Template content**: three generic helpers
  (`kcenon_template_homebrew_prefix`, `kcenon_template_find_threads`,
  `kcenon_template_find_package_with_fallback`). The template explicitly
  delegates the actual `find_package` and `FetchContent_Declare` calls to
  each consumer.
* **pacs content**: the actual dependency-discovery logic (ICU, SQLite3,
  libjpeg-turbo, OpenJPEG, CharLS, libpng, OpenJPH, OpenSSL, AWS SDK, Azure
  SDK, Crow, ASIO, Catch2). 36 KB of project-specific `find_package` /
  `FetchContent` blocks.
* **Why**: this is exactly the role the template assigns to the consumer.
  pacs does not omit any helper -- it inlines the equivalent logic
  (Homebrew prefix probing, CONFIG-then-MODULE fallback) directly into
  each `find_package` call so the discovery sequence is readable in one
  place.

### `examples.cmake` -- Content

* **Template content**: a generic helper `kcenon_template_add_example_dirs`
  that wraps `add_subdirectory()` per a list passed by the consumer.
* **pacs content**: the concrete CLI tool list (32 binaries under `tools/`,
  all gated by `PACS_BUILD_EXAMPLES`).
* **Why**: same as `dependencies.cmake`. The template is generic by design;
  the actual subdirectory list lives at the consumer. pacs's listing also
  carries individual `message(STATUS)` lines per tool -- this is louder
  than `kcenon_template_add_example_dirs` but matches pacs's "verbose
  configure log" style and provides per-tool descriptions that the helper
  cannot.

### `install.cmake` -- Pattern

* **Template pattern**: a single high-level helper
  `kcenon_template_install_package(PACKAGE_NAME ... NAMESPACE ... TARGETS
  ... CONFIG_TEMPLATE ...)` that performs `install(TARGETS EXPORT)`,
  `configure_package_config_file`, `write_basic_package_version_file`,
  `install(EXPORT)`, `install(FILES)`, and `export(EXPORT)` in one call.
* **pacs pattern**: the same six steps inline, plus extensive feature-flag
  variable computation (`PACS_SYSTEM_WITH_SQLITE`,
  `PACS_SYSTEM_WITH_OPENSSL`, ...) that the
  `pacs_system-config.cmake.in` template consumes via `@VAR@` substitution.
* **Why**: pacs's per-feature export variables (12 of them) are part of the
  install contract. They cannot be computed from inside
  `kcenon_template_install_package` because they depend on which optional
  targets actually exist after `dependencies.cmake` ran. Refactoring to
  call the helper would still require the same pre-call block, making the
  helper invocation no shorter than the inline version.

### `options.cmake` -- Pattern

* **Template pattern**: `kcenon_template_define_standard_options(<PREFIX>)`
  defines six options (`<PREFIX>_BUILD_TESTS`, `_BUILD_EXAMPLES`,
  `_BUILD_BENCHMARKS`, `_BUILD_DOCS`, `_BUILD_MODULES`,
  `_WARNINGS_AS_ERRORS`). Plus the four export helpers
  (`kcenon_template_get_export_bridge_target`,
  `kcenon_template_link_external_dependency`,
  `kcenon_template_register_export_target`).
* **pacs pattern**: the canonical six options are declared directly with
  `option(PACS_BUILD_TESTS ...)` etc. (matching the template's prefix
  convention). The four export helpers are defined inline as `pacs_*`
  rather than `kcenon_template_*` (functionally identical).
* **Why**: pacs has 9 additional project-specific options
  (`PACS_BUILD_SAMPLES`, `PACS_BUILD_FUZZ_TARGETS`,
  `PACS_WITH_COMMON_SYSTEM`, `PACS_WITH_CONTAINER_SYSTEM`,
  `PACS_WITH_NETWORK_SYSTEM`, `PACS_BUILD_STORAGE`, `PACS_WITH_AWS_SDK`,
  `PACS_WITH_AZURE_SDK`, `PACS_USE_MOCK_S3`, `PACS_BUILD_CODECS`,
  `PACS_WITH_OPENSSL`, `PACS_WITH_REST_API`) that must coexist with the
  six standard options. Mixing helper-call + explicit `option()` for the
  rest creates a confusing two-step list; pacs flattens both into a single
  declarative block. The export-bridge helpers are functionally identical
  to the template's; only the prefix differs.

### `targets.cmake` -- Content

* **Template content**: three generic helpers
  (`kcenon_template_setup_target_includes`,
  `kcenon_template_set_cxx_feature_std`,
  `kcenon_template_create_aliases`).
* **pacs content**: 12 library targets (`pacs_core`, `pacs_encoding`,
  `pacs_security`, `pacs_network`, `pacs_client`, `pacs_services`,
  `pacs_storage`, `pacs_ai`, `pacs_monitoring`, `pacs_workflow`,
  `pacs_web`, `pacs_integration`) with their full source lists, link
  rules, and per-target feature flags.
* **Why**: same as `dependencies.cmake`. The template is generic; the
  target inventory belongs to the consumer.

### `testing.cmake` -- Framework

* **Template framework**: Google Test 1.14.0 (helper:
  `kcenon_template_setup_testing` calls `find_package(GTest)`; helper:
  `kcenon_template_add_gtest`).
* **pacs framework**: **Catch2 v3.4.0** via `FetchContent`. Eight test
  executables registered with CTest directly.
* **Why**: pacs predates the ecosystem decision to converge on Google
  Test. Migration is tracked separately and is out of scope for the
  cmake-template alignment work (it would change ~700 test file imports).
  Documented in the project-level CLAUDE.md as a known constraint.

### `warnings.cmake` -- Role

* **Template role**: helper definitions
  (`kcenon_template_init_warnings(<PREFIX>)`,
  `kcenon_template_apply_warnings(<target>)`).
* **pacs role**: helper *application* -- the file is a list of
  `pacs_apply_warnings(<target>)` calls plus per-target deprecation
  suppressions (e.g. `-Wno-deprecated-declarations` for the database
  bridge). The helper itself is defined in `compiler.cmake` (see above).
* **Why**: pacs has 16 distinct warning-application sites with
  per-target conditional logic (existence checks, framework guards,
  deprecation overrides). Inlining them in `compiler.cmake` would mix
  flag definition with target wiring; splitting them into
  `warnings.cmake` keeps the application surface visible in one file.

## pacs-Specific Files (NOT deviations from template)

### `pacs_system-config.cmake.in`

* **Purpose**: install/export config consumed by `find_package(pacs_system)`.
* **Why pacs-specific**: every system has its own `<name>-config.cmake.in`.
  The template intentionally does not ship one because the install-time
  config is by definition per-system. This file is the canonical pattern
  the template's `kcenon_template_install_package` helper expects via
  `CONFIG_TEMPLATE <path>`. Not a deviation.

### `summary.cmake`

* **Purpose**: pretty-print the build configuration summary at configure
  time.
* **Why pacs-specific**: pacs is the only ecosystem system that ships a
  build summary today. Could be promoted to the template later (filed as
  upstream candidate; see PR description for the issue link).

## Migration Plan (Future Work)

The deviations above are explicit, not accidental. A future ecosystem-wide
sweep could converge pacs_system onto the template's helper-call style:

1. Move C++ standard / build-type / `compile_commands` from the top-level
   `CMakeLists.txt` into a renamed `cmake/compiler.cmake` that
   `include()`s the template module.
2. Move pacs's warning flag pipeline into a new `cmake/warnings.cmake`
   that calls `kcenon_template_init_warnings(PACS)` and reuses
   `kcenon_template_apply_warnings()`.
3. Replace the four inline `pacs_*` export helpers in `options.cmake`
   with `include(template-name)` of the template's `options.cmake`.
4. Replace direct `install(...)` calls in `install.cmake` with a call to
   `kcenon_template_install_package(...)`.
5. Migrate the test framework to Google Test 1.14.0 (large change, tracked
   separately).

Each step is independently shippable and bisectable. They are not in
scope for #1140 -- this file is the documentation of the current state.

## Cross-References

* Layout standard: [`common_system/docs/kcenon-system-layout.md`](https://github.com/kcenon/common_system/blob/main/docs/kcenon-system-layout.md)
* Template README: [`common_system/cmake/template/README.md`](https://github.com/kcenon/common_system/blob/main/cmake/template/README.md)
* Template version: [`cmake/VERSION`](VERSION)
* EPIC: kcenon/pacs_system#1137
* Sub-issue: kcenon/pacs_system#1140
