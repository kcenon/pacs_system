# Audit Log Security

This document describes the at-rest protection applied to PACS audit log
records and maps the implementation to the ISO 27799 (health informatics
information security management) and related regulatory controls.

## Scope

The controls described here apply to the RFC 3881 / DICOM PS3.15 audit
messages produced by the ATNA audit pipeline when they are persisted to
local storage. Records in transit to a centralised Audit Record Repository
are protected separately by the existing TLS transport (RFC 5425).

## Control Mapping

| Standard | Clause | Control | Implementation |
|---|---|---|---|
| ISO 27799:2016 | Sec. 7.9 | Confidentiality of audit records | `audit_log_cipher` AES-256-GCM envelope |
| ISO 27799:2016 | Sec. 7.9 | Integrity of audit records | Independent HMAC-SHA256 per record |
| ISO 27799:2016 | Sec. 7.9.2 | Key management and rotation | `audit_key_manager`, keyed by `key_id` |
| ISO 27002:2022 | Sec. 8.15 | Logging | Format-versioned records tolerate evolution |
| HIPAA | 45 CFR 164.312(b) | Audit controls | Encrypted storage of audit events |
| HIPAA | 45 CFR 164.312(a)(2)(iv) | Encryption and decryption | AES-256-GCM, authenticated |
| PIPA (Korea) | Art. 29, Notice 2020-7 | Safeguards for medical PHI | AES-256 symmetric encryption |

## Cryptographic Design

Every audit record produced by the writer is serialised as a single ASCII
line with the following fields separated by `|`:

```
PACSAUDIT|v1|<key_id>|<iv_b64>|<ciphertext_b64>|<gcm_tag_b64>|<hmac_b64>
```

| Field | Contents | Purpose |
|---|---|---|
| `PACSAUDIT` | Literal tag | Identifies encrypted audit records |
| `v1` | Format version | Allows non-breaking format evolution |
| `key_id` | Active key identifier | Enables rotation (see below) |
| `iv_b64` | 96-bit random IV | Unique per record (NIST SP 800-38D) |
| `ciphertext_b64` | AES-256-GCM ciphertext | Confidentiality |
| `gcm_tag_b64` | 128-bit GCM tag | Inner AEAD integrity |
| `hmac_b64` | HMAC-SHA256 of all preceding fields | Outer integrity, bound to an independent MAC key |

The HMAC is computed with a key that is distinct from the data encryption
key. This defence-in-depth step means that compromising either the GCM
tag alone or the MAC alone is not sufficient to forge an acceptable
record; both must verify against keys held by the relying party.

Decryption is performed in two stages. First the outer HMAC is verified
in constant time; only on success does AES-GCM decryption run. A failed
HMAC never exposes plaintext.

## Key Management

Keys are managed by `audit_key_manager` in
`include/kcenon/pacs/security/audit_log_cipher.h`.

### Key material

A key is the pair (`data_key`, `mac_key`), each a 32-byte (256-bit)
uniformly random buffer. Both halves are required; generate them with a
cryptographically secure RNG, for example:

```bash
openssl rand -hex 32   # data key for key id "k2026-04"
openssl rand -hex 32   # mac key for key id "k2026-04"
```

### Environment-variable sourcing (default)

At boot, call `audit_key_manager::load_from_env()`. It reads:

| Variable | Required | Meaning |
|---|---|---|
| `PACS_AUDIT_LOG_ACTIVE_KEY_ID` | yes | Identifier of the key used for all new writes |
| `PACS_AUDIT_LOG_KEYS` | no | Comma-separated list of all key ids to register for read access; defaults to the active id |
| `PACS_AUDIT_LOG_KEY_<id>` | yes | Data encryption key for `<id>`, hex or base64 |
| `PACS_AUDIT_LOG_MAC_<id>` | yes | MAC key for `<id>`, hex or base64 |

All keys are parsed as either exact-length hex (64 hex chars for 32
bytes) or standard base64. Strings of the wrong length are rejected.

### Key rotation procedure

1. Generate a new (data, mac) key pair. Choose a monotonic identifier
   (for example `k2026-04`).
2. Populate `PACS_AUDIT_LOG_KEY_<new>` and `PACS_AUDIT_LOG_MAC_<new>`.
3. Append the new id to `PACS_AUDIT_LOG_KEYS` (the old id stays present so
   historical records remain readable).
4. Update `PACS_AUDIT_LOG_ACTIVE_KEY_ID` to the new id and restart the
   process group. New writes use the new key immediately; old records
   still decrypt with the retained previous key.
5. After the regulatory retention window elapses and old records have
   been archived or purged, remove the retired id from
   `PACS_AUDIT_LOG_KEYS`, `PACS_AUDIT_LOG_KEY_<old>` and
   `PACS_AUDIT_LOG_MAC_<old>`.

### Production hardening — key storage backends

The env-var path exists for operational simplicity and compatibility with
container orchestration secret stores (Kubernetes Secrets, HashiCorp
Vault Agent, AWS Secrets Manager env-var injection, Azure Key Vault
integration). Direct KMS integration (envelope encryption where the data
key itself is wrapped by a KMS-held master key) is a planned follow-up;
the header-level `register_key` API accepts any externally-sourced key
material, so a future `kms_audit_key_manager` implementation can be added
without changing the record format.

## Operator Actions

Before deploying this change to production:

1. Provision a key pair and distribute the keying material to the secret
   store of the deployment environment.
2. Set `PACS_AUDIT_LOG_ACTIVE_KEY_ID`, `PACS_AUDIT_LOG_KEY_<id>` and
   `PACS_AUDIT_LOG_MAC_<id>`.
3. Confirm encryption is in effect by inspecting the audit log directory:
   every stored record should start with the literal string
   `PACSAUDIT|v1|`.

## Verification

The implementation is covered by `tests/security/audit_log_cipher_test.cpp`,
which exercises:

- Encryption/decryption round trip for varied plaintext sizes
- Uniqueness of IVs across repeated encryptions of identical plaintext
- Tamper detection on ciphertext, GCM tag and HMAC
- Rejection of records whose key id is not registered
- Key rotation — old records remain decryptable while new writes use the
  new key, and retiring the old key correctly loses access to historical
  records
- Malformed record detection (wrong field count, bad version tag)
- Tolerance of trailing newline characters when reading log files

## Threat Model Notes

- **Confidentiality**. A passive attacker with read access to audit log
  files learns at most the record length. Plaintext is never present
  on disk.
- **Integrity**. A tampered record fails the HMAC step before any
  cryptographic operation that could leak plaintext. Both silent bit
  flips and coordinated modifications of `ciphertext` together with
  `gcm_tag` are caught by the outer HMAC.
- **Replay / reorder**. This layer does not bind records to a total
  order on disk. If regulatory context requires append-only integrity of
  the log as a whole, pair this implementation with the existing ATNA
  syslog forwarder to a write-once repository, or add a per-segment
  Merkle chain as a future enhancement.

## References

- ISO 27799:2016 — Health informatics — Information security management
  in health using ISO/IEC 27002
- NIST SP 800-38D — Recommendation for Block Cipher Modes of Operation:
  Galois/Counter Mode (GCM) and GMAC
- RFC 2104 — HMAC: Keyed-Hashing for Message Authentication
- RFC 3881 — Security Audit and Access Accountability Message XML Data
  Definitions for Healthcare Applications
