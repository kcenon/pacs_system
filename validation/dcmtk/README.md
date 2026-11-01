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

The initial audit recorded defects in the v1 CLI TCP path (#1303), retrieval
SCP behavior (#1304/#1306), compressed storage (#1292), v2 TLS (#1305), C-GET
role negotiation and a utility invoking nonexistent `echoscp`. Subsequent fixes
have dedicated regression evidence; run this validator against the current source
and rebuilt images to establish the integrated result. Cross-platform TLS and the
broader external security/audit matrix (#1317) still require their own evidence.

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

## Focused incoming storage regression (#1292)

Within the built validation runner, execute the following with a new output path:

```sh
python3 /workspace/pacs_system/validation/dcmtk/tests/storage_probe.py \
  --profile rle-lossless --output /results/incoming-rle \
  --source-revision <base-commit> --source-digest <tested-source-digest>
```

Repeat with `--profile uncompressed` and a different output path. The probe uses
independent DCMTK `storescu` for all 13 CT/MR/CR instances, compares saved Pixel Data
bytes and length form, and decodes compressed files with DCMTK before comparing
all metadata and full pixel hashes. It checks clean receiver shutdown and preserves
command/status/server logs. A passing focused probe does not certify the complete
feature matrix, outbound storage, or real cloud providers.

## Compressed wire and restart checks (#1314)

`tests/compressed_roundtrip_probe.py` accepts the same profile, output, source
revision/digest and fixture arguments as the incoming storage probe. Run each of
`uncompressed`, `jpeg-lossless`, `jpeg-ls-lossless`, and `rle-lossless` separately.
It sends all 13 objects from PACS to an independent DCMTK receiver, receives 13
objects from DCMTK into PACS, stops and restarts the PACS receiver, stores a new
readiness object, then uses `store_scu` to read and forward the original archived
files. A second receiver checks a separate study. Negotiated contexts, exact item
bytes, metadata and full decoded pixel hashes must match. Restart forwarding here
uses the stored files; it does not substitute for C-GET/C-MOVE SCP acceptance.

`tests/peer_storage_probe.py` runs within the isolated Compose topology. It runs
the pinned peer's native/compressed prerequisite suite, the original PACS primary
store of 13 instances, independent DCMTK C-GET integrity for 5/6/2 CT/MR/CR objects,
and the separate archive destination. Run all four profiles in fresh projects.
Reusing a runner image with copied binaries is a local diagnostic; record the
binary/source hashes and do not claim the image's original digest proves the edits.

The explicit VR encoder emits marked encapsulated pixels with undefined length,
unchanged BOT/fragments and exactly one delimiter. DIMSE validates the dataset
against the negotiated syntax before encoding and after decoding. It rejects a
native/encapsulated mismatch, defined-length compressed wire pixels and unsupported
network byte order/encoding. It does not compress or decompress pixels implicitly.

## C-GET requestor roles (#1315)

`tests/get_scu_peer_probe.py` runs in a fresh pinned Compose topology for each
profile. Independent DCMTK stores and retrieves 13 fixture objects first. PACS
`retrieve_scu` then receives exactly 5 CT, 6 MR and 2 CR objects on the original
association. Accepted Storage SCP roles, completed/received counters, file syntax,
metadata and full decoded pixel hashes must match. This isolates C-GET SCU behavior
from PACS C-STORE SCU and PACS retrieve SCP defects.

`tests/get_scu_protocol_probe.py --fixture <native-CT.dcm> --output <new-directory>`
executes the actual CLI against a controlled socket peer. It retains raw association
PDUs and checks role denial/omission, partial contexts, multiple sub-operations,
wrong counters, warning/cancellation responses, bounded timeout, wrong message ID,
unnegotiated or missing-dataset stores, optional omitted final counters, stale
existing files and escaped paths. This is protocol fault-injection evidence;
independent archive interoperability comes from the peer probe above.

### C-MOVE destination delivery (#1304)

`retrieve_scp` resolves `--peer AE:host:port` (or the application's configured AE
resolver), opens a separate association, and reuses it for matching instances.
It proposes each stored SOP class/transfer syntax pair, with separate batches
when more than 128 contexts are needed. Encapsulated syntax must match exactly;
native Little Endian can use either native VR encoding. No transcoding is implied.
The default destination connect/write, C-STORE response and release deadlines are
3 seconds, 30 seconds and 1 second. Applications can configure positive durations
with `set_move_timeouts()` before starting their server.

Each completed/warning count requires a correlated C-STORE response. Refused
contexts, connection failures, invalid responses and unanswered stores count as
failed, with their SOP Instance UIDs in the final identifier. Unknown destinations
return A801. Final Success/Warning/Failure omits Remaining; cancellation returns
FE00 with the unattempted count. An in-flight store without an acknowledgement is
failed and the destination association is aborted. The optional custom store
callback receives the **destination** association/context for C-MOVE; its own
I/O/deadline behavior remains the application's responsibility.

The v2 association handler executes services on one joined worker per association.
The network receive thread continues processing C-CANCEL, responses and disconnects.
Cancellation is matched by association, presentation context and original MessageID;
shutdown aborts active waits and joins the worker. Closed handlers are retired
on the session closer thread; their workers finish before network resources are
released. Service writes and protocol-control writes are serialized. This dispatch foundation does
not establish C-GET SCP acceptance, which is tracked separately in #1306.

Run these scoped probes from a prepared validation runner, using a new output
folder and recording the source revision/digest:

```sh
python3 validation/dcmtk/tests/move_scp_peer_probe.py \
  --output /results/move-native --profile uncompressed \
  --source-revision "$PACS_SOURCE_REVISION" --source-digest "$PACS_SOURCE_DIGEST"
python3 validation/dcmtk/tests/move_scp_protocol_probe.py \
  --binary /workspace/build/bin/qr_scp --fixture /dicom/testdata/ct/ct-1.dcm \
  --output /results/move-protocol
```

Repeat the peer probe for `jpeg-lossless`, `jpeg-ls-lossless`, and `rle-lossless`.
It imports all 13 fixtures through the public `qr_scp --scan-only` command, then
runs independent DCMTK movescu/storescp exchanges for CT/MR/CR (5/6/2 objects).
It checks exact UID sets, metadata, decoded full Pixel Data hashes, preserved item
bytes and final sub-operation counts. The controlled protocol probe covers
unknown/refused destinations, partial context rejection, mixed storage outcomes,
malformed responses, cancellation during a blocked store, transfer/release timeout,
and shutdown. Controlled wire peers are not independent interoperability evidence.
These focused probes do not replace the complete validation gate.

### C-GET SCP response routing (#1306)

The v2 server negotiates requestor Storage SCP roles from the retrieve service's
outbound capabilities. A Storage context without this role cannot be used for
C-GET. The service selects a context for the stored SOP class and exact compressed
transfer syntax; native Little Endian may use either VR encoding. It sends each
C-STORE on the requesting association, preserves the requested priority, and does
not add C-MOVE originator fields.

`association::send_request()` registers a single C-STORE/C-ECHO response wait
before sending. The receive thread calls `deliver_response()` while the service
worker waits in `receive_response()`. Context, command, MessageID, status and any
returned SOP identities must match. Known late/duplicate responses are discarded;
a mismatched response fails the active exchange. Replies with no owner are logged
and discarded, including acknowledgements from legacy fire-and-forget notification
services. They cannot accumulate in the request queue. New requests remain on the
bounded service queue and C-CANCEL retains its association/context/MessageID scope.

The default C-GET acknowledgement deadline is 30 seconds, configurable with
`set_get_response_timeout()` before serving. Cancellation is checked at most every
50 ms during a wait. Release, abort and server stop wake the response mailbox.
An unacknowledged in-flight store fails; cancellation leaves unstarted matches in
Remaining. On timeout or invalid acknowledgement, the remaining matches fail
without another timeout per object. Final counters and the Failed SOP Instance UID
List reflect these outcomes. Normal terminal responses omit Remaining.

MessageIDs allocated by `next_request_message_id()` are not reused on an
association, preventing old acknowledgements from matching later operations.
After 65535 allocated IDs the requester must open a new association. The mailbox
holds one response and bounded identifier history. The API is for writer/in-memory
associations with an external receive dispatcher; TCP SCUs retain their existing
`send_dimse()`/`receive_dimse()` interface. Custom store callbacks receive the actual
Storage context and empty/zero Move Originator values for C-GET, and remain
responsible for their own acknowledgement and deadline handling.

```sh
python3 validation/dcmtk/tests/get_scp_peer_probe.py \
  --output /results/get-native --profile uncompressed \
  --source-revision "$PACS_SOURCE_REVISION" --source-digest "$PACS_SOURCE_DIGEST"
python3 validation/dcmtk/tests/get_scp_protocol_probe.py \
  --binary /workspace/build/bin/qr_scp --fixture /dicom/testdata/ct/ct-1.dcm \
  --output /results/get-protocol
./build/bin/network_tests '[response-routing]'
```

Repeat the independent peer probe for `jpeg-lossless`, `jpeg-ls-lossless`, and
`rle-lossless`. It imports 13 objects through the public CLI and verifies independent
DCMTK getscu delivery of exact 5/6/2 CT/MR/CR objects, metadata, decoded full pixel
hashes, raw pixel/item bytes, and final counters. The controlled probe exercises
three stores per normal retrieval, delayed/interleaved messages, duplicate/late
replies, four simultaneous associations, invalid replies, missing roles/contexts,
cancellation, timeout, release/abort and server shutdown. Use sanitizer-built
`qr_scp` with `--binary` for the same lifetime probes. These scoped runs do not
replace the complete validation gate.

Shutdown also fences late handler registration. A callback reaching the closer
after it stops leaves session closure to the underlying server, avoiding re-entry
into the session callback lock. Rejected/shutdown-time connections clear the
adapter's self-capturing receive/error callbacks while preserving disconnection
cleanup. The `shutdown-race` protocol case overlaps a blocked C-GET with connection
churn and SIGTERM; run it with leak detection enabled.

### Nested worklists and normalized request identity (#1316)

`mwl.sequence` compares all six scheduled-step fields for the three pinned MWL
patients with DICOM response files independently extracted by DCMTK `findscu`.
Eleven queries cover modality, station, no matches, date/range, time, patient and
custom step ID. `mwl.scp` sends nine DCMTK queries to the public `worklist_scp`
using a separate declared fixture, including a continuous date/time range crossing
midnight. Scheduled-step matching and return keys live inside the single
Scheduled Procedure Step Sequence item (PS3.4 K.6.1.2.2). Both DIMSE VR decoders
parse defined-length sequences as well as undefined-length sequences.

`mpps.protocol` runs the public `pacs_server` and `mpps_scu` in separate processes.
Fixed odd/even UIDs cover completion and discontinuation, immutable terminal
states and restart persistence. The oracle opens SQLite read-only; only network
operations create/update records. Seven malformed wire requests test wrong SOP,
wrong/unknown context, conflicting or substituted Affected fields and absent
Requested Instance UID, followed by a successful C-ECHO. Run it directly with:

```sh
python3 validation/dcmtk/tests/mpps_scp_protocol_probe.py \
  --bin-dir /path/to/build/bin --output /tmp/mpps-protocol-new-run
```

The output directory must be new. Python requires pydicom. The probe preserves
command arguments, exits, output, command bytes, association responses, server
logs and before/after database rows. Required binary CI runs this probe before the
shell aggregate. Its controlled malformed peer is not an independent MPPS
reference implementation or a full MPPS IOD conformance test.

Catch2 `[dispatch]` runs actual retained v1 dispatch with file-backed database
state and v2 dispatch over TCP, including database reopen, fixed UID parity,
terminal rejection, N-GET routing and wrong-context rejection. The v1 transport
is explicitly an in-process API; it is not external network evidence. Dispatch
uses Requested SOP Class for N-GET/N-SET/N-ACTION/N-DELETE and Affected SOP Class
for commands that require it. Command identity must agree with the negotiated
context; Print Meta SOP contexts accept only their declared member SOP Classes.
Other normalized service builders and full service validation remain in #1320.
These focused cases do not establish the complete multi-profile validation gate.

The shell port allocator checks a wildcard bind without address reuse, so bound
but non-listening ports and TCP TIME_WAIT are excluded. Startup remains an actual
required operation: discovery cannot reserve a port across unrelated processes.
The #1258 aggregate CI allowance was removed after the deterministic MPPS probe,
both aggregate modes and all four DCMTK shell suites passed locally. Hosted CI
still supplies its own evidence; these local runs do not stand in for it.
