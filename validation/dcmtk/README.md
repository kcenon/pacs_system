# PACS functional validation using dcmtk-docker

The system under test is **pacs_system**. A pinned **dcmtk-docker** build supplies
independent PACS peers, a receiver, a modality worklist server, fixtures, codecs,
and reference SCUs. Passing the peer's self-tests does not certify PACS.

Run from a checkout with Python 3.10+, Git, Docker and Docker Compose v2:

```sh
python3 scripts/run_dcmtk_validation.py --output /tmp/pacs-validation-run-1
```

The output directory must be new and outside the source tree. Docker's ordinary
`DOCKER_HOST` and `DOCKER_CONFIG` settings work, including Colima. The driver uses
image builds, named volumes and `docker cp`; it needs no host bind mount or exposed
host port. It does not modify sibling ecosystem repositories or change production
configuration. Each profile gets a fresh Compose project and synthetic volumes.
Cleanup removes only that project and its volumes after evidence is copied.
`--keep` retains those resources for investigation; the generated `compose.json`
and startup log identify the project for later `docker compose -p ... -f ... down --volumes`.

## What runs

1. Build the DCMTK revision in `lock.json`, including its concurrent storage patch.
2. Build the current PACS working-tree contents, including uncommitted source edits,
   against the seven dependency SHAs in the existing checkout action. All component
   tests and CLI tools are built; JPEG, OpenJPEG, CharLS and OpenJPH are required.
3. Run the 88-case DCMTK baseline in the native profile. Each compressed profile
   runs its 16-case peer suite, including negotiated compressed storage, C-MOVE and
   decoded pixel integrity. All profiles run independent TLS self-tests and then
   exercise PACS with new-UID CT/MR/CR objects. Run **uncompressed, JPEG lossless,
   JPEG-LS lossless and RLE lossless sequentially**.
4. Check actual PACS C-ECHO/C-STORE/C-FIND/C-MOVE/C-GET/MWL client behavior and
   echo, storage, query/retrieve and TLS server behavior with independent DCMTK tools.
   Check exact UID sets, counts, metadata and whole decoded PixelData hashes.
   Compressed cases also require the received file's transfer syntax; silently
   falling back to native encoding fails. Check MWL nested scheduled-step fields.
   Separate diagnostic chains use distinct run/profile/role UID namespaces.
   DCMTK `storescu` seeds reference data, then independent `getscu` plus decoded
   pixel and metadata comparisons verify every instance before PACS query/retrieve
   clients run. PACS SCP diagnostics use the documented `qr_scp --scan-only`
   import/index contract and external C-ECHO readiness. A zero-file import fails
   preparation. No test writes database rows directly. The original store-dependent
   chain keeps its case IDs, prerequisites and failures.
   Plaintext CLI SCPs use the v2 TCP transport. Per-CLI C-ECHO checks cover QR,
   worklist, MPPS and the complete PACS server, including repeat associations and
   clean SIGTERM exit. Echo and storage CLI checks also enforce clean shutdown;
   storage compares received objects with the independent fixtures. See
   [ADR-004](../../docs/adr/ADR-004-cli-scp-tcp-transport.md) for the retained v1 API
   and the separately tracked TLS migration.
5. In the uncompressed profile, discover individual CTest and Catch2 cases before
   running every component suite listed in `features.json`,
   the existing integration suite, v2 TLS cases and bounded stress cases. JUnit
   failures, unexpected skips, missing binaries, empty results and timeouts fail.
   Compare every job's selected case identities with its actual execution results.
6. Evaluate the entire feature manifest. A subset cannot claim complete validation.

Native tests include core/encoding, network/services, storage, security, client,
workflow, AI, monitoring, DI, web, ecosystem integration, XDS, CLI roundtrip and
concurrency. In-memory/mock tests remain component evidence, not proof of external
provider interoperability. Long-duration `[.slow]` tests, module/toolchain/platform
matrices and additional external services are explicit outstanding requirements.

## Results and outstanding work

`summary.txt`, `summary.json` and `summary.xml` distinguish:

- actual failed/missing/skipped test evidence;
- blocked scenarios whose prerequisite failed (zero tests, never a pass);
- feature groups with passing implemented tests;
- missing independent coverage, such as S3/Azure providers, AI service, XDS registry,
  browser workflows, cross-protocol DICOMweb flows, and all advertised SOP classes.

`features.json` maps **20 feature groups** to executable evidence and lists these
remaining coverage requirements. They deliberately prevent an overall `passed`
result. Remove a gap only when its test and feature mapping are implemented, not
because related component tests happen to pass. `known_issues` are references,
not an allowlist or permission to ignore failures.

The current PACS baseline has known failures: the v1 CLI server TCP path (#1303),
retrieval SCP behavior (#1304/#1306), compressed storage (#1292) and v2 TLS (#1305).
The prior audit also found C-GET SCU role negotiation and a utility test using a
nonexistent `echoscp` executable. This validation workflow is expected to report
those defects; adding the workflow is not a claim that they were fixed.

The primary outputs are the actual results, not the workflow's existence. The
[CI workflow](../../.github/workflows/dcmtk-validation.yml) runs the same command
and uploads logs and JUnit even when the gate fails. No `continue-on-error`,
all-skipped success option or artifact-existence success rule is used by this gate.

Each profile directory contains:

- `evidence/results.json` and `results.xml`: scenario/component outcomes;
- `evidence/*.log` and component JUnit files: command and assertion details;
- `commands.jsonl` (driver and profile evidence): expected exit codes, actual exit
  or signal, timeout versus spawn failure, and the associated command/case;
- `evidence/fixture-manifest.json`: expected identifiers and complete pixel hashes;
- `evidence/originals`, `inputs` and received objects: synthetic reproduction data;
- `peers.log`, `startup.log`, `cleanup.log`, and `compose.json`: environment evidence.

Top-level `provenance.json` records the source revision **and content digest**, peer
revision and manifest digest. `images.json` records the actual locally built image
IDs. Dependency versions are pinned; apt security revisions can change the image
ID, so an image tag alone is not treated as immutable evidence.

## Focused diagnosis

### Individual case inventory

`test-inventory.json` records every discovered case, its feature owners, build/OS
configuration, fixture/oracle, expected outcome, execution jobs and evidence format.
Catch2 discovery uses an explicit `*` filter to include hidden tests. Each execution
filter is evaluated by Catch2 or CTest itself. Filtered-out cases remain explicitly
unverified and fail completeness; the presence of their binary cannot cover them.

Catch2 identities include the binary, full test name and tags. Native Catch2 XML
provides those same identities after execution. JUnit remains available to CI, but
its flattened section names are not used to guess which test ran. Allowed assertion
failures (`!mayfail`) remain failures. CTest registrations and aliases are retained,
including duplicate names; ambiguous registrations are reported rather than merged.

`test-inventory-assessment.json` compares discovery, assignment and execution sets
after all component jobs, including legacy TLS. Missing, duplicate, unknown,
mismatched and skipped results cannot pass. The top-level report independently
repeats this comparison rather than trusting the discovery component's pass count.

`features.json` records positive/negative evaluation requirements for all feature
groups. The inventory also expands the conformance statement's SOP roles and syntax
boundaries into individually identified requirements. These are declarations,
explicitly unverified until independent cases supply evidence. File-only Big Endian
support is distinct from DIMSE rejection; unsupported codecs require rejection
tests, while forwarding and codec availability remain separate capabilities.
Registry, policy, build-option and web-script source digests accompany this scope.
Required browser, modules, installed-consumer and SDK build discovery remains
explicitly unverified until its owning jobs contribute inventories (#1322/#1324/#1327).

The original broad gap text is retained in the feature manifest; scope history
records the correction to unsupported-codec expectations. Assigning an owner or
recording a declaration never removes a coverage gap. #1313 owns fixture/filter
defects exposed by inventory, and #1328 integrates the later execution jobs.

### Commands

```sh
python3 scripts/run_dcmtk_validation.py \
  --output /tmp/pacs-validation-uncompressed \
  --profiles uncompressed --interop-only --keep
python3 -m unittest discover -s validation/dcmtk/tests -v
```

The diagnostic run still fails the **full** gate because profiles/components are
missing. Inspect individual outcomes to assess the selected scope. A prebuilt
`--runner-image` is accepted only when its labels match the current source-content
digest and pinned peer revision; the default builds the runner from source.

The baseline peer aggregate intentionally omits its optional ad-hoc destination
and TLS-overlay suites. Its nonzero aggregate count is checked, and its independent
TLS suite runs separately. These baseline tests validate the reference environment;
PACS-specific TLS and retrieval tests have their own mandatory result IDs.
The native-fixture baseline must not run with forced compressed outgoing C-MOVE:
DCMTK's archive forwards precompressed data but does not encode native data on
demand. The compressed peer suite verifies this supported path before testing PACS.

Process-launch failures remain explicit failed cases. POSIX test utilities use
`posix_spawnp` with reset signal handlers and an owned process group, report missing
executables immediately, and reap terminated children. The integration executable
lets Catch2 own stdout so listing/reporting never includes a decorative banner.
Malformed XML is rejected in place; the validator never repairs it into a pass.

`test-inventory.json` includes a per-case deferral audit: allowed-failure tags,
hidden cases, potential `SKIP` branches, and exact CTest sanitizer quarantine names.
Each entry records its cause, issue owner and exit condition. Conditional branches
are distinguished from actual skipped execution results. Existing cross-system
allowances still need native/sanitizer matrix verification; passing one Linux run
does not establish their historical root cause. The four direct worker tests now
register workers explicitly and assert work delivery and shutdown without `!mayfail`.
The hour-long store/query endurance test remains assigned to #1326; a shorter
component run cannot stand in for its execution. Unresolved quarantine names remain
visible as absent from the current build. None of these audit entries waives a
failed, missing or skipped result.

### Independent diagnostic fault probe

Inside a running isolated topology, execute the following twice with new output
directories; add `--fail-reference-seed` to the second invocation:

```sh
python3 /workspace/pacs_system/validation/dcmtk/tests/diagnostic_probe.py \
  --profile rle-lossless --output /results/diagnostic-store-refusal \
  --source-revision ACTUAL_SOURCE_REVISION --source-digest ACTUAL_SOURCE_DIGEST
```

This invokes real compressed PACS C-STORE with an intentionally refused called AE.
The independent chain must still invoke PACS C-FIND/C-GET/C-MOVE. In the second
probe, DCMTK reference seeding also uses a refused AE: every dependent diagnostic
must remain blocked with `executed: false` and `tests: 0`, even if older synthetic
objects exist in the peer. `probe-assertions.json` measures this scheduling contract;
it does not certify product behavior. `results.json` retains actual product failures,
preparation failures, environment failures, prerequisites and blocked case IDs.
The reference readiness check rejects missing, duplicate, corrupt or stale output.
If public PACS import/readiness fails, SCP diagnostics remain explicitly unexecuted
until the owning product defects (#1315/#1303) are repaired.
