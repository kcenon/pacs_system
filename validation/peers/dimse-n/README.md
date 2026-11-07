# Independent DIMSE-N peers

Peers for the normalized services, run as separate processes that share no PACS code.
`validation/dcmtk/tests/test_dimse_n.py` fails if a script here imports or names PACS code.

## What runs where

| Peer | Version | License | Pinned by | Roles it plays |
|------|---------|---------|-----------|----------------|
| dcm4che tools | 5.35.2 | MPL-1.1, GPL-2.0 or LGPL-2.1 | `install.py` (SHA-256) | `mppsscu`, `mppsscp` with IOD validation, `stgcmtscu`, `dcmqrscp` as Storage Commitment SCP, `upsscu` including `-O receive`, `findscu -M MWL` and `-M UPSPull` |
| pynetdicom | 2.1.1 (its wheel reports 2.1.0) | MIT | `install.py` (SHA-256) | The transport of the scripted peers below |
| DCMTK | 3.6.9 from the validation image | BSD-style | `validation/dcmtk/lock.json` | `dcmprscu`, `dcmprscp`, `dcmpsprt` for Print |
| OpenJDK | 21 JRE, for dcm4che | GPL-2.0 with Classpath Exception | the image's package | |

Scripted peers, all from the standard's text and none of them accepting everything:

| Script | What it is | What it was qualified against |
|--------|------------|-------------------------------|
| `scripted_scu.py` | Sends listed requests, reports what came back, judges nothing | `mppsscp`, `dcmqrscp`, `dcmprscp` |
| `event_receiver.py` | Receives N-EVENT-REPORT on associations others open; refuses a requestor that lacks the SCP role | `dcmqrscp`, `ups_reference_scp.py` |
| `ups_reference_scp.py` | UPS SCP from Tables CC.1.1-2, CC.2.3-2, CC.2.5-3 and CC.2.6-2, because no tool implements one | `upsscu`, including events to `upsscu -O receive` |
| `mwl_scp.py` | Worklist SCP that records the identifiers it receives | `findscu -M MWL` |
| `print_scp.py` | Print SCP that records the jobs it prints, because `dcmprscp` takes no color | `dcmprscu` |

## How a result is trusted

`dimse_n_probe.py` runs a `peers` group first, with no PACS process. Each pair of tools, and each
expected table, must work between independent implementations; a PACS case whose prerequisite failed is
reported as blocked, so a broken peer cannot be mistaken for a broken product. dcm4che's tools exit 0
whatever status they get, so the probe reads the status from their log and the object from what they stored.

Where a reference answers otherwise than PACS must, the difference is listed in the probe with the reason,
and the run fails if a new one appears:

- `dcmprscp` never reads the Action Type ID, so it answers an undefined action with B603H; PS3.7 makes it 0123H.
- `dcmprscp` answers a second Film Session on an association with 0111H. H.4.1.2.1 allows one and names no
  status; PACS answers 0213H, because the UID is not a duplicate.
- `dcmprscp` accepts an Image Box naming the position of another. PACS answers 0106H rather than print an
  image in the wrong place.
- `dcm4che dcmqrscp` ends the association on a commitment request with no Referenced SOP Sequence or
  Transaction UID, so only the undefined action is compared there. PACS is held to PS3.7 for the others.

## Running

```bash
python3 validation/peers/dimse-n/install.py /opt/pacs-peers
python3 validation/dcmtk/tests/dimse_n_probe.py --output /tmp/dimse-n --build-dir /path/to/build \
    --source-revision "$(git rev-parse HEAD)" --source-digest working-tree
# --groups takes mpps, mwl, stgcmt, ups, print and nget; `none` runs only the peer qualification.
```

`compose.yaml` runs each profile in its own container from the validation image.

## What this does not cover

These are recorded as gaps of `normalized_services` in `validation/dcmtk/features.json`:

- Patient Information Reconciliation is a local library call with no DIMSE exchange, so no peer can reach it.
- The generic `n_get_scp` has no product host. N-GET is exercised through UPS and the Printer, and `n_get_scu`
  against `dcmprscp`.
- Filtered global UPS subscriptions are refused with C307H, which CC.2.3.3 allows.
- The library has no C-FIND client for UPS Pull and UPS Query, and no product listens for UPS events as an SCU.
- Commitment reports waiting for a retry do not survive a restart.
- No peer ran over TLS.
