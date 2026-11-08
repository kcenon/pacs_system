---
doc_id: "PAC-ADR-004"
doc_title: "ADR-004: TCP transport for CLI SCPs"
doc_version: "1.1.0"
doc_date: "2026-11-08"
doc_status: "Accepted"
project: "pacs_system"
category: "ADR"
---

# ADR-004: TCP transport for CLI SCPs

Status: accepted on 2026-11-08. Implemented by #1303 and merged into develop with
#1308; the TLS and functional acceptance work it deferred is done under #1292,
#1304, #1305, #1306, #1314, #1315, #1316 and #1317.

## Context and decision

The legacy `network::dicom_server` registers an in-process endpoint and opens a
listening socket, but its accept worker closes external connections. Consequently,
successful same-process associations did not demonstrate CLI interoperability.

Use `network::v2::dicom_server_v2` for externally exposed CLI SCPs. It delegates
TCP sessions to the pinned network_system implementation and DICOM protocol state
to `dicom_association_handler`. Do not introduce a second socket implementation
inside v1 or silently change the public v1 callback/registry contract.

| CLI | Services/configuration preserved | Transport state |
|---|---|---|
| `echo_scp` | Verification, limits, timeouts | v2 TCP |
| `store_scp` | Storage callback, file layout, storage options | v2 TCP |
| `qr_scp` | Public file scan/index, query/retrieve handlers, destination map | v2 TCP |
| `worklist_scp` | JSON source, query handler, result limit | v2 TCP |
| `mpps_scp` | N-CREATE/N-SET handlers and JSON persistence | v2 TCP |
| `pacs_server` | Storage/index, query/retrieve, MWL/MPPS, AE whitelist | v2 TCP |
| `secure_echo_scp` | Certificate/key/CA, peer verification and TLS minimum | v2 encrypted TCP; external TLS/mTLS matrix in `validation/security` |

TLS is applied to the v2 listener with the requested verification policy. The
minimum is TLS 1.2 by default or TLS 1.3 when configured. Independent peers test
DIMSE success, certificate failures, plaintext rejection and version policy.

## Compatibility and lifecycle

`server_config` and `scp_service` registration remain unchanged. CLI established
callbacks now receive session ID, calling AE and called AE; closed callbacks
receive session ID and a graceful-release flag. They no longer expose a borrowed
v1 association object. Session IDs are used in close logs.

The public v1 class and `network_adapter::create_server` return type remain
available for in-process consumers. They do not provide external TCP DICOM
service. Existing v1 unit/workflow fixtures remain to check this retained API;
the shared external DCMTK fixture uses v2 and no longer has
`v1_server_accepts_tcp()` skip gates.

The retained factory preserves TLS settings and v1 rejects them at `start()`;
it cannot silently downgrade a TLS request. Historical `[tls]~[v2]` fixture
names now execute the v2 encrypted listener. They do not claim v1 TLS support.

CLI signal handlers only set a lock-free atomic flag. The main thread performs
shutdown and joins transport threads, avoiding logging, server locks and I/O
thread joins inside a signal handler. V2 stop closes active sessions immediately;
it does not promise to drain outstanding DIMSE work. Resource cleanup and restart
must be tested independently of successful association establishment.

## Validation boundaries

Run DCMTK tools as separate processes against the actual CLI listeners. Check
negotiated contexts, stored instance identities/metadata/pixels, subsequent
associations and clean SIGTERM exit. Run the Binary Integration and DCMTK shell
suites as well; #1258 adds explicit CLI contracts, bounded processes and exact
object checks. Their success is separate from the #1240 C++ fixtures.

C-MOVE delivery/counters (#1304), C-GET roles/sub-operations (#1306), compressed
representation (#1292/#1314), indexing (#1315), MWL/MPPS semantics (#1316) and TLS
(#1305/#1317) retain their own assertions. TCP readiness or C-ECHO alone establishes
none of those service contracts.
