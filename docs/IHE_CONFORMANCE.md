---
doc_id: "PAC-INTR-003"
doc_title: "IHE XDS.b Conformance Statement"
doc_version: "0.1.0"
doc_date: "2026-04-24"
doc_status: "Released"
project: "pacs_system"
category: "INTR"
---

# IHE XDS.b Conformance Statement

> **SSOT**: This document is the single source of truth for **IHE ITI XDS.b
> Integration Statement / Conformance Statement** at the text-document layer
> (ITI-18/41/43).

This document is structured according to the
[IHE Integration Statement template](https://wiki.ihe.net/index.php/IHE_Integration_Statement)
and covers only the **IT Infrastructure (ITI)** domain transactions implemented
at `src/ihe/xds/`. For the **RAD** domain imaging profiles (XDS-I.b, AIRA,
PIR), see the separate [IHE Integration Statement](IHE_INTEGRATION_STATEMENT.md).
Those two documents are intentionally non-overlapping: ITI XDS.b here, RAD
XDS-I.b there.

For DICOM-level conformance details see the
[DICOM Conformance Statement](DICOM_CONFORMANCE_STATEMENT.md). For the
tracking of the XDS.b implementation gaps called out in
[#1101](https://github.com/kcenon/pacs_system/issues/1101), see the
[IHE XDS.b Completion Status](IHE_XDSB_COMPLETION_STATUS.md).

---

## 1. Vendor / Product Identification

| Field | Value |
|-------|-------|
| Vendor Name | kcenon |
| Product Name | PACS System |
| Product Version | 0.1.0 |
| Release Date | 2026-04-24 |
| Statement Date | 2026-04-24 |
| Statement Version | 0.1.0 |
| Contact | see repository `README.md` |

---

## 2. Actors Implemented

The PACS System implements the following IHE IT Infrastructure (ITI) actors at
the text-document / registry layer. All three are **initiator** (client) side
actors.

| Actor | Role | Implementation |
|-------|------|----------------|
| Document Source | Publishes documents + submission metadata to an XDS.b Repository | `include/kcenon/pacs/ihe/xds/document_source.h`, `src/ihe/xds/document_source.cpp` |
| Document Consumer | Retrieves documents from an XDS.b Repository by `{repositoryUniqueId, documentUniqueId}` | `include/kcenon/pacs/ihe/xds/document_consumer.h`, `src/ihe/xds/document_consumer.cpp` |
| Document Consumer (Query side) | Executes stored queries against an XDS.b Document Registry | `include/kcenon/pacs/ihe/xds/registry_query.h`, `src/ihe/xds/registry_query.cpp` |

The Repository and Registry actors are **not** implemented in this release;
the PACS System is exclusively a client of conformant third-party Repository
and Registry infrastructure.

---

## 3. Transactions Supported

| Transaction | Actor (this side) | Direction | Options / Profile Pins |
|-------------|-------------------|-----------|------------------------|
| ITI-41 Provide and Register Document Set-b | Document Source | Outbound | SOAP 1.2; MTOM/XOP request body; WS-Security X.509 Token Profile signing on `<soap:Body>` + timestamp; `wsa:Action = urn:ihe:iti:2007:ProvideAndRegisterDocumentSet-b`. |
| ITI-43 Retrieve Document Set | Document Consumer | Outbound | SOAP 1.2; MTOM/XOP response body parsed; WS-Security signature on response verified; `wsa:Action = urn:ihe:iti:2007:RetrieveDocumentSet`. |
| ITI-18 Registry Stored Query | Document Consumer (Query side) | Outbound | SOAP 1.2; `wsa:Action = urn:ihe:iti:2007:RegistryStoredQuery`; stored queries `FindDocuments` and `GetDocuments`; response signature verification same as ITI-43. |

Transactions are emitted over HTTP/1.1 TLS (see Section 5). No plain-HTTP
fallback is available.

---

## 4. Integration Profiles

| Profile | Profile Option | Support |
|---------|----------------|---------|
| ITI XDS.b (Cross-Enterprise Document Sharing) | Document Source actor | Yes (ITI-41 outbound) |
| ITI XDS.b | Document Consumer actor | Yes (ITI-43 + ITI-18 outbound) |
| ITI XDS.b | Document Registry / Repository | No (external infrastructure) |
| ITI ATNA (Audit Trail and Node Authentication) | Secure Node, audit emission only | Partial — audit messages are emitted per RFC 3881 / IHE ATNA for all three transactions; the secure-node TLS side inherits the product's BCP-195 profile (see Section 5) |

XDS.b here refers to the plain-document profile. For the imaging profile
XDS-I.b (RAD-68 / RAD-69), see
[IHE_INTEGRATION_STATEMENT.md §2.1](IHE_INTEGRATION_STATEMENT.md#21-xds-ib----cross-enterprise-document-sharing-for-imaging).

---

## 5. Configuration Parameters

### 5.1 Endpoints

Each actor is constructed with the URL of its remote counterpart; there is no
single global registry. Endpoints are plain HTTPS URLs and must be configured
by the deployment.

| Actor | Configured Endpoint |
|-------|---------------------|
| Document Source | Repository ITI-41 endpoint |
| Document Consumer | Repository ITI-43 endpoint |
| Registry Query (Consumer) | Registry ITI-18 endpoint |

### 5.2 WS-Security Signing

All three actors sign their outbound request `<soap:Body>` (plus the
`wsu:Timestamp` reference) using RSA-SHA256 with a BinarySecurityToken
(X.509v3). The emitted `<ds:CanonicalizationMethod>` and reference `Transform`
advertise a **project-local canonicalization URI** rather than the W3C
Exclusive XML Canonicalization URI. See Section 7 for the interoperability
implications.

| Parameter | Value |
|-----------|-------|
| Signature Algorithm | `http://www.w3.org/2001/04/xmldsig-more#rsa-sha256` |
| Digest Algorithm | `http://www.w3.org/2001/04/xmlenc#sha256` |
| CanonicalizationMethod URI | `urn:kcenon:xds:c14n:pugixml-format-raw-v1` |
| Reference Transform URI | `urn:kcenon:xds:c14n:pugixml-format-raw-v1` |
| Signer Key Binding | PEM X.509 certificate + private key pair passed at actor construction; public key carried as `wsse:BinarySecurityToken` |

The project-local URI is declared exactly once in
`src/ihe/xds/common/wss_signer.cpp` (constant `kKcenonC14nUri`) so the
signer and verifier cannot drift.

### 5.3 TLS

TLS transport is governed by the PACS System BCP-195 profile (`non_downgrading
= true`, TLS 1.3 preferred). See
[DICOM Conformance Statement §7](DICOM_CONFORMANCE_STATEMENT.md#7-security).
No XDS.b-specific TLS settings are introduced by these actors; the transport
stack (libcurl + OpenSSL) is shared with the rest of the product.

### 5.4 ATNA Audit Destination

Each actor exposes `set_audit_sink(std::shared_ptr<xds_audit_sink>)`. Sink
factories are published in
`include/kcenon/pacs/security/xds_audit_events.h`:

| Factory | Emission Behaviour |
|---------|--------------------|
| `make_null_xds_audit_sink()` | Installed by default. No events emitted. Preserves pre-audit call semantics (single null-check per transaction, zero allocations). |
| `make_atna_xds_audit_sink(atna_service_auditor&, ...)` | Forwards each event through `atna_service_auditor::submit_audit_message()`, which serialises to syslog over the auditor's configured `atna_syslog_transport` (UDP / TCP / TLS, per `syslog_transport_config`). |
| `recording_xds_audit_sink` (class) | Captures events in-process for tests. Used by the integration suite under the `pacs_xds_integration` CTest label. |

Audit emission fires on both success and failure paths of every transaction.
On failure, the triggering `Result<T>` error message is attached as an
`atna_object_detail` of type `FailureDescription` on the terminal participant
object, per IHE ATNA.

---

## 6. Audit Trail and Node Authentication (ATNA)

Each ITI transaction emits one audit message constructed by the builder in
`src/security/xds_audit_events.cpp`. The vocabulary below is the **pinned
final value** after the RFC 3881 alignment pass on this branch; it is locked
in unit and integration tests under the `pacs_xds_integration` CTest label.

| Transaction | Builder | EventID (RFC 3881) | EventActionCode | EventTypeCode | Local Participant Role |
|-------------|---------|--------------------|-----------------|---------------|------------------------|
| ITI-41 Provide and Register | `build_iti41_audit_message` | DCM 110106 `Export` | `C` (Create) | `ITI-41` (code system `IHETransactions`), display `Provide and Register Document Set-b` | Source (requestor) |
| ITI-43 Retrieve Document Set | `build_iti43_audit_message` | DCM 110107 `Import` | `R` (Read) | `ITI-43` (code system `IHETransactions`), display `Retrieve Document Set` | Destination (requestor) — local consumer pulls from remote repository |
| ITI-18 Registry Stored Query | `build_iti18_audit_message` | DCM 110112 `Query` | `E` (Execute) | `urn:ihe:iti:2007:RegistryStoredQuery` (code system `IHETransactions`), display `Registry Stored Query` | Source (requestor) |

Each message carries:

- Active participants: the two endpoints of the transaction, with RFC 3881
  role-ID codes (`kcenon::pacs::security::atna_role_ids::source`,
  `::destination`) and a single `user_is_requestor = true` flag on the local
  actor.
- Participant objects, where applicable:
  - ITI-41: `XDSSubmissionSet.uniqueId`, one
    `XDSDocumentEntry.uniqueId` per document, and patient number.
  - ITI-43: `XDSDocumentEntry.uniqueId` plus the remote repository's node ID.
  - ITI-18: patient number (for `FindDocuments`) and/or one
    `XDSDocumentEntry.entryUUID` per queried UUID (for `GetDocuments`).
- EventOutcomeIndicator: `success` (0) or `serious_failure` (8), matching
  `atna_event_outcome`.

References: RFC 3881 §5 (event vocabulary), IHE ITI TF-2a §3.20 (Record Audit
Event), IHE ITI TF-1 §9 (ATNA profile), IHE ITI TF-2a §3.18 / §3.41 / §3.43.

---

## 7. Known Interoperability Limitations

### 7.1 WS-Security Canonicalization

The WS-Security signature produced by `src/ihe/xds/common/wss_signer.cpp`
advertises

```
urn:kcenon:xds:c14n:pugixml-format-raw-v1
```

as both its `<ds:CanonicalizationMethod>` and each reference `Transform`,
**not** the W3C Exclusive XML Canonicalization URI
(`http://www.w3.org/2001/10/xml-exc-c14n#`). The byte stream is produced by
pugixml's `format_raw` serialization against an envelope whose namespace
prefixes are all declared on `<soap:Envelope>`, so it is deterministic and
bit-reproducible under the paired verifier in the same file.

Interoperability consequences:

- **Self-to-self**: fully interoperable. Signatures produced and verified by
  this codebase round-trip exactly.
- **Against real IHE Gazelle / Apache CXF / .NET WIF peers**: the peer will
  reject the signature at the Algorithm check, because it does not recognise
  the project-local URI. This applies to both the ITI-41 request signature
  (which a real Repository would reject) and the ITI-43 / ITI-18 response
  signature verification (which will reject responses from a real Repository /
  Registry).
- **Fix**: a follow-up issue tracks implementing full namespace-rewriting
  `exc-c14n`. Production deployments against real XDS.b infrastructure must
  wait for that work.

See the `@brief` on
`include/kcenon/pacs/ihe/xds/document_source.h`,
`document_consumer.h`, and `registry_query.h` for the caveat at the actor
level, and the header comment of `wss_signer.cpp` for the rationale.

### 7.2 Registry / Repository Actors Not Implemented

The PACS System does not host an XDS.b Document Registry or Document
Repository. Deployments must pair it with conformant third-party
infrastructure.

### 7.3 ATNA Secure Application / Secure Node

The ATNA audit emission side (Section 6) is implemented for all three
transactions. The full ATNA **Secure Application / Secure Node**
conformance (node authentication via bidirectional TLS, consolidated audit
record for every security-relevant event across the product) is tracked
separately against the product-wide BCP-195 + audit pipeline; this XDS.b
actor set contributes the XDS-specific audit events but does not, by itself,
certify the whole node.

### 7.4 Connectathon Testing

IHE Connectathon testing has not yet been performed. Transaction conformance
is validated by the Gazelle-lite integration harness registered under the
`pacs_xds_integration` CTest label (see `cmake/testing.cmake` lines 660-669).

---

## 8. References

| Document | Title |
|----------|-------|
| IHE ITI TF-1 §9 | IT Infrastructure Technical Framework Volume 1: Audit Trail and Node Authentication (ATNA) Integration Profile |
| IHE ITI TF-1 §10 | IT Infrastructure Technical Framework Volume 1: Cross-Enterprise Document Sharing (XDS.b) Integration Profile |
| IHE ITI TF-2a §3.18 | Registry Stored Query `[ITI-18]` |
| IHE ITI TF-2a §3.20 | Record Audit Event `[ITI-20]` |
| IHE ITI TF-2b §3.41 | Provide and Register Document Set-b `[ITI-41]` |
| IHE ITI TF-2b §3.43 | Retrieve Document Set `[ITI-43]` |
| RFC 3881 | Security Audit and Access Accountability Message XML Data Definitions for Healthcare Applications |
| RFC 5246 / RFC 8446 | TLS 1.2 / TLS 1.3 |
| BCP 195 | Recommendations for Secure Use of Transport Layer Security (TLS) |
| W3C XML-DSig | XML Signature Syntax and Processing (Second Edition) |
| W3C exc-c14n | Exclusive XML Canonicalization Version 1.0 |
| OASIS WS-Security 1.1 | Web Services Security: SOAP Message Security (including X.509 Token Profile 1.1) |

---

*IHE is a registered trademark of IHE International.*
