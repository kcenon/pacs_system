# HTTP peers for DICOMweb

`validation/dcmtk/tests/dicomweb_probe.py` reads one archive over HTTP and DIMSE through tools that share no PACS code.
`http_peer.py` holds what those tools leave to the caller, and it imports nothing of PACS: `test_dicomweb_cross.py` fails
if it starts to.

| Peer | Version | License | Used for |
|------|---------|---------|----------|
| Orthanc with its DICOMweb plugin | Debian 1.12.7+dfsg-4 and 1.20+dfsg-1 | GPL-3.0+ and AGPL-3.0+ | Reference server: the tables must hold on it, with no PACS process, before a PACS case counts |
| curl | 8.14.1 from the distribution | curl | Every raw HTTP request, so status, headers and body are what the wire carried |
| dcm4che `stowrs`, `wadors`, `json2dcm`, `xml2dcm` | 5.35.2, installed by `validation/peers/dimse-n/install.py` | MPL-1.1, GPL-2.0 or LGPL-2.1 | Clients of another make, and second readers of DICOM JSON and of the Native DICOM Model |
| DCMTK `storescu`, `findscu`, `getscu`, `movescu`, `storescp`, `img2dcm`, `dcmdjpeg`, `dcmdrle`, `dcmdjpls` | 3.6.9 from the distribution | BSD-style | DIMSE side, and decoding |
| pydicom | 2.4.2 from the distribution | MIT | Reading Part 10 objects and DICOM JSON |
| openssl | 3.5.7 from the distribution | Apache-2.0 | Making keys and signing tokens, and verifying them against the JWKS alone |

`http_peer.py` provides a strict `multipart/related` reader, which treats a missing type or closing delimiter as an error
rather than as a shorter list, PNG and JPEG readers, and `TokenKit`, which builds RS256 and ES256 tokens and a JWKS from
keys made by the openssl command. Keys are made for each run in a private temporary directory and never reach the results.

## What the reference is held to

Orthanc passes the tables except where `REFERENCE_DIFFERENCES` and `REFERENCE_LENIENCY` in `validation/dcmtk/dicomweb_cross.py`
say otherwise, each with the sentence of PS3.18 PACS follows instead. A listed case that passes on the reference fails, so
the list cannot go stale, and PACS is never given the leniency: `test_dicomweb_cross.py` checks both. Orthanc's authorization
is not used, so the token cases are PACS only, and are qualified by `web.peer.tokens` instead: the tokens verify under
openssl with a key rebuilt from the JWKS, and a token from an unpublished key does not.
