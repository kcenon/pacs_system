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
5. In the uncompressed profile, run every component suite listed in `features.json`,
   the existing integration suite, v2 TLS cases and bounded stress cases. JUnit
   failures, unexpected skips, missing binaries, empty results and timeouts fail.
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
- `evidence/fixture-manifest.json`: expected identifiers and complete pixel hashes;
- `evidence/originals`, `inputs` and received objects: synthetic reproduction data;
- `peers.log`, `startup.log`, `cleanup.log`, and `compose.json`: environment evidence.

Top-level `provenance.json` records the source revision **and content digest**, peer
revision and manifest digest. `images.json` records the actual locally built image
IDs. Dependency versions are pinned; apt security revisions can change the image
ID, so an image tag alone is not treated as immutable evidence.

## Focused diagnosis

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
