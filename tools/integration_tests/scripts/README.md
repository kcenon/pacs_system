# Binary Integration Test Scripts

Run the network suites against a build with examples, storage and OpenSSL enabled:

```bash
bash tools/integration_tests/scripts/run_all_binary_tests.sh build --verbose
bash tools/integration_tests/scripts/test_dcmtk_echo.sh build
bash tools/integration_tests/scripts/test_dcmtk_store.sh build
bash tools/integration_tests/scripts/test_dcmtk_find.sh build
bash tools/integration_tests/scripts/test_dcmtk_move.sh build
python3 tools/integration_tests/scripts/test_shell_helpers.py
```

The scripts require Bash, Python 3, OpenSSL and DCMTK (`echoscu`, `storescu`,
`storescp`, `findscu`, `movescu`, `dcmdump`, `dcmodify`). GNU `timeout` or macOS
`gtimeout` is used when available; the Python fallback also enforces a deadline
and kills remaining child processes. Individual commands have deadlines and the
aggregate limits each suite to 240 seconds.

| Suite | Required executables | Checks |
|---|---|---|
| `test_connectivity.sh` | echo_scp, echo_scu | C-ECHO, repeated requests, calling AE, connection and argument errors |
| `test_store_retrieve.sh` | pacs_server, store_scu, query_scu, echo_scu | Storage/query, duplicate rejection, three concurrent distinct instances |
| `test_worklist_mpps.sh` | pacs_server, worklist_scu, mpps_scu, echo_scu | Empty MWL success and MPPS create/complete using the returned UID |
| `test_secure_dicom.sh` | secure_echo_scp, secure_echo_scu | Encrypted C-ECHO, client certificate requirement, wrong CA, independent OpenSSL handshake |
| `test_dcmtk_echo.sh` | echo_scp, echo_scu | Bidirectional C-ECHO and DCMTK baseline via storescp |
| `test_dcmtk_store.sh` | pacs_server, store_scu | Bidirectional C-STORE, DCMTK baseline, exact received SOP Instance UIDs |
| `test_dcmtk_find.sh` | pacs_server | Seeded query results, Patient/Study Root models, wildcard/date/empty matching |
| `test_dcmtk_move.sh` | qr_scp | Registered destination, actual received object, unknown destination and connection rejection |
| `test_cli_tools.sh` | File utility executables | Separate existing file utility suite; not part of the network aggregate |

`pacs_server` has no CLI destination registration option. The C-MOVE suite uses
`qr_scp --peer AE:host:port` with an isolated pre-indexed source directory.
Retrieval is covered there; `test_store_retrieve.sh` covers storage and query.
The default duplicate policy rejects resends, so concurrent storage uses distinct
SOP Instance UIDs and duplicate rejection is checked separately.

## CLI contracts and results

Every network suite checks its required executables' `--help` output before
starting servers. Some existing executables return 1 or 2 for help; the checks
pin the expected status and required usage text. Actual operations must return
their expected status. `worklist_scu` returns 1 for an empty successful query;
that case also requires a successful final DIMSE response in verbose output.

`echo_scu`, `store_scu` and `worklist_scu` use `-aec`/`-aet` before positional
host/port arguments. `query_scu`, `mpps_scu` and `secure_echo_scu` retain positional
called AE arguments. MPPS uses positional `create`/`set` commands. Secure tools
receive separate certificate and key files generated with `generate_certs.sh`.
The secure SCP uses the v2 TLS server; the SCU sets `association_config::tls`.

Received objects are recognized by Part 10 `DICM` bytes and parsed with DCMTK,
regardless of suffix. `dicom_inventory.py` rejects missing, extra, wrong or
duplicate SOP Instance UIDs. Find responses are parsed separately because query
datasets need not contain a SOP Instance UID. Logs and non-DICOM files are never
counted as stored objects.

## Selection, deadlines and evidence

```bash
# Select one or more suites; --parallel runs them concurrently.
bash tools/integration_tests/scripts/run_all_binary_tests.sh build --storage --worklist
PACS_TEST_LOG_DIR=/tmp/pacs-shell-evidence \
  bash tools/integration_tests/scripts/run_all_binary_tests.sh build --parallel --verbose
```

The default evidence directory is `<build>/test-results/shell/`. Each run gets a
unique directory containing full server logs, command output, quoted arguments
and raw exit statuses. The aggregate also writes `suite-exits.tsv`. Startup
failures print the server log and exit status; an occupied port is refused.
Cleanup stops only owned processes, removes temporary databases/data and private
keys, and retains logs. CI uploads the evidence even when a suite fails.

Exit 0 requires all selected suites to pass. Missing binaries, missing test data,
empty selections and skipped prerequisites cannot establish a pass. Suite exits
are 1 for test/startup failures and 2 for missing prerequisites. Expected command
failures are asserted explicitly; timeout exits (124, or 137 after forced GNU
timeout termination) are never accepted as successful negative tests.

The helper regression suite exercises deadlines, status/log preservation,
startup failures, port ownership, extensionless objects and UID mismatches.
It requires DCMTK and fails if that prerequisite is missing.

The four DCMTK shell steps are required in CI. The binary aggregate still has
an advisory allowance pending #1316: an odd-length MPPS UID retains a trailing
UI padding NUL during N-SET, causing status `0xc001` after a successful N-CREATE.
Both sequential and parallel execution use the returned UID and preserve this
failure. Remove that allowance only after the product fix and #1258 acceptance
rerun; a passing run with even-length generated UIDs does not settle it.

### Recursive anonymization round trip

Run `python3 tools/integration_tests/scripts/test_recursive_anonymization.py build`.
This required CI check uses DCMTK `dump2dcm`, `dcm2xml`, and `dcmdump` to create and
independently inspect Part 10 files. Missing tools fail the check. Seven cases
cover nested K/C/U actions, X/Z/D and unknown sequence handling, batch UID linkage,
UID retention, value-free diagnostics, and refusal to save failed recursive
processing. The nested case covers explicit/implicit VR and defined/undefined
sequence lengths. Synthetic source identifiers are checked in output bytes as
well as parsed structures. Commands, exits, output, and fixtures are retained
under `build/test-results/anonymization/`; an optional second argument changes the
evidence directory. These cases do not establish complete profile compliance or
pixel cleaning.
