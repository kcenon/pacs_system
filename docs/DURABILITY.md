# Storage durability and recovery

What a successful store promises, what an interrupted one leaves behind, and what is and is not tested. It
covers `pacs_server`'s C-STORE and STOW-RS, which both store through `storage::archive_instance` into
`file_storage` and the SQLite index.

## What a success response means

Before `pacs_server` answers a C-STORE with success, or STOW-RS lists an instance as stored:

1. The object's file is complete under its final name. It is written to `<name>.tmp.<n>` beside it, closed with
   the write checked, and renamed over the final name.
2. Its patient, study, series and instance rows are committed to the index (SQLite, WAL journal,
   `synchronous=NORMAL`).

The object therefore survives the end of the `pacs_server` process at any later moment: a crash, SIGKILL or a
stopped container.

It is **not** promised to survive an operating system crash or a power loss. Neither the file nor the WAL is
flushed to the device before the response (no `fsync`; with `synchronous=NORMAL` SQLite syncs the WAL at
checkpoints only), so after such an event an acknowledged object can be missing, empty or unindexed. No test
here models that: the fault probe kills processes, and the page cache outlives them.

## When the process ends before the response

| The process ends | Left behind | At the next start | The same object sent again |
|---|---|---|---|
| while the object is received | nothing | | stored |
| after the temporary file, before the rename | `<name>.tmp.<n>` | the file is removed | stored |
| after the rename, before the index | a complete file without index rows; no query finds it | the file stays | the file is replaced and indexed |
| after the index, before the response | the object, filed and indexed | | refused as a duplicate (0x0111, the default policy): it is held once |

In the first three rows the sender never saw success and should send again. In the last it may, safely.
Before #1326 the third row ended with the object refused as a duplicate of its own file for ever, and
temporary files stayed in the archive.

## Refusals

- **A write that fails** (full disk, I/O error): the store fails (0xC001), nothing is filed or indexed and the
  temporary file is removed. Before #1326 a small object's write failed only when the stream was destroyed,
  unchecked, and the server answered success for an empty file.
- **An index that cannot take the rows** (locked by another writer, full): the store fails after SQLite's 5 s
  busy timeout and the new file is removed.
- **A sender that disappears** in the middle of an object: the partial object is discarded; acknowledged ones stay.
- **A duplicate** of an object held (filed and indexed): refused with 0x0111 under `duplicate_policy::reject`,
  `pacs_server`'s only policy, as its command line has no option for it.

Both resource failures are answered 0xC001, which PS3.4 classes as "Error: Cannot Understand"; 0xA7xx
("Refused: Out of Resources") would tell the sender to try later. The archive cannot yet tell a resource
failure from an object it cannot encode, so the status stays as it is.

## Verified by

`validation/reliability/fault_probe.py`, run as `component.reliability_faults`, injects each fault with
`validation/reliability/faultshim.c` and reads the result without PACS code: index rows from the SQLite file,
objects and pixel digests from the files, leftovers from the directory. Cases: a kill just before and just
after the rename, a kill of the server and of the client in the middle of a 40-object batch, a full disk and a
locked index. Unit tests: `dicom_file::save fails when the device is full` (core_tests),
`archive_instance stores an object whose file was never indexed` and `file_storage removes the temporary files
an interrupted store left` (storage_tests).

## Not verified

- Power loss and block-device failure, as above.
- Endurance: `validation/reliability/endurance.json` declares 24 hours; a shorter run of
  `validation/reliability/endurance.py` reports itself as diagnostic.
- A container restart as such: what it does to the processes is what the process kills do, and the
  container's files persist in both.
