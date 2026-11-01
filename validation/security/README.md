# External security validation

Run from the repository with an OpenSSL-enabled build, Python 3.10+, pydicom,
OpenSSL CLI and TLS-enabled DCMTK tools on PATH:

```sh
cmake --build build --target pacs_security_probe secure_echo_scp secure_echo_scu dcm_anonymize
python3 validation/security/run.py --bin-dir build/bin \
  --output /tmp/security-public-run1 --private-output /tmp/security-private-run1
```

Both directories must be new and separate. The private directory has mode 0700;
raw synthetic DICOM objects, received audit payloads and exact private command
arguments stay there. Disposable CA/client/server keys are deleted on exit.
Only the public directory is suitable for upload after `logs.public-clean`
passes. Failed runs preserve raw exits, hashes and named failed/blocked results;
missing tools, expired fixtures or cases never become skips.

`cases.json` declares every case and its prerequisites. `run.py` checks this
inventory against its execution selection. The DCMTK validation driver imports
each named result and requires the aggregate process result. The required binary
integration CI job runs the same matrix, including all seven #1300 anonymization
cases. Its private corpus is deliberately excluded from artifact upload.

## Evidence and policy

- DCMTK C-ECHO tests both PACS server and client roles. A controlled independent
  Python TLS/DIMSE peer verifies exact TLS 1.2 and 1.3 negotiation, response
  command/message/context identity, success status and association release.
- Default DICOM minimum TLS is 1.2; configuring 1.3 rejects 1.2. Plaintext,
  TLS 1.1, untrusted/expired/missing client certificates, and untrusted/expired/
  wrong-host server certificates require specific TLS errors. Handshake logs
  retain protocol, cipher and certificate identity. Recovery requires a fresh
  successful DCMTK exchange after negative cases.
- The production ATNA transport verifies both trust and the receiver's DNS/IP
  identity, defaults to the system trust store when no CA file is supplied,
  requires matching client certificate/key configuration, and serializes sends
  and close on a shared transport. Moving or destroying it requires exclusive
  ownership. TLS socket I/O and the close-notify exchange use `io_timeout`
  (default five seconds); this is not a general DNS/connect deadline.
- Independent UDP and TLS receivers check RFC 5424 headers, RFC 5425 octet
  framing, DICOM audit event codes, action/outcome, timestamps and identities.
  Sixteen concurrent sends must arrive exactly once. A successful transport
  write alone never establishes external receipt.
- `pacs_security_probe serve` wires actual v2 TLS, `storage_scp`, `file_storage`
  and its native audit hook. DCMTK stores one object, then receives A700 on a
  duplicate. Independent file inspection must find exactly one unchanged
  object, and the receiver must see matching success/failure events (110104,
  action C, outcomes 0/8). The harness does not fabricate service responses or
  write receiver evidence.
- `send` exercises the production transport with synthetic authentication
  messages. These are transport probes, not native DICOM authentication audits.
- The existing recursive anonymization suite independently inspects nested,
  private, retained/cleaned and repeated UID content through DCMTK. Its raw
  identifying corpus remains private. The disclosure scanner checks public
  artifacts and designated anonymizer diagnostics for source identifiers,
  private-key content and credential canaries; mutation checks prove the scanner
  and truncated-frame oracle reject bad samples.

## Limits

The local receiver is an independent Python socket/TLS/XML implementation, not
a qualified commercial Audit Record Repository. Tests inspect the documented
event fields; they do not claim full DICOM audit XML schema or IHE conformance.
Native TLS authentication audit hooks, early service-rejection audits, and
broader authorization policy remain explicit #1325 coverage requirements.
Transport-level receipt does not establish durable storage in a remote audit
repository. Pixel OCR, every confidentiality-profile option, hosted CI and a
complete four-profile validation gate require separate evidence.

Protocol references: [DICOM PS3.15 audit schema](https://dicom.nema.org/medical/dicom/current/output/chtml/part15/sect_A.5.html),
[RFC 5425 TLS syslog transport](https://www.rfc-editor.org/rfc/rfc5425.html), and
[OpenSSL endpoint verification](https://docs.openssl.org/3.5/man3/SSL_set1_host/).
