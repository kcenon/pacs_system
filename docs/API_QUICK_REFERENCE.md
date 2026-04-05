# API Quick Reference

Cheat sheet for common pacs_system operations. For full details see the
[API Reference](API_REFERENCE.md).

---

## Table of Contents

- [Dataset Operations](#dataset-operations)
- [Element Access](#element-access)
- [File I/O](#file-io)
- [SCU Operations (C-FIND / C-STORE / C-MOVE)](#scu-operations)
- [SCP Setup](#scp-setup)
- [DICOMweb Endpoints](#dicomweb-endpoints)
- [Storage Backends](#storage-backends)
- [Transfer Syntax and Codecs](#transfer-syntax-and-codecs)

---

## Dataset Operations

```cpp
#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/encoding/vr_type.h"

using namespace kcenon::pacs::core;
using namespace kcenon::pacs::encoding;

dicom_dataset ds;

// --- Set values ---
ds.set_string(tags::patient_name, vr_type::PN, "Doe^John");
ds.set_string(tags::patient_id,   vr_type::LO, "12345");
ds.set_numeric<uint16_t>(tags::rows,    vr_type::US, 512);
ds.set_numeric<uint16_t>(tags::columns, vr_type::US, 512);

// --- Get values ---
std::string name = ds.get_string(tags::patient_name);      // "Doe^John"
auto rows = ds.get_numeric<uint16_t>(tags::rows);           // std::optional<uint16_t>

// --- Check existence ---
if (ds.contains(tags::pixel_data)) { /* ... */ }

// --- Remove ---
ds.remove(tags::patient_name);

// --- Iterate ---
for (const auto& elem : ds) {
    std::cout << elem.tag().to_string() << "\n";
}

// --- Sequence ---
auto& seq = ds.get_or_create_sequence(dicom_tag{0x0040, 0x0340});
dicom_dataset item;
item.set_string(tags::series_instance_uid, vr_type::UI, "1.2.3.4");
seq.push_back(std::move(item));

// --- Subset / Merge ---
auto sub = ds.subset({tags::patient_name, tags::patient_id});
ds.merge(other_ds, /*overwrite=*/true);
```

---

## Element Access

```cpp
#include "kcenon/pacs/core/dicom_element.h"

using namespace kcenon::pacs::core;

// Create elements
auto el = dicom_element(tags::patient_name, vr_type::PN, "Doe^John");
auto px = dicom_element(tags::pixel_data,   vr_type::OW, pixel_bytes);

// Read values
el.tag();                   // dicom_tag{0x0010, 0x0010}
el.vr();                    // vr_type::PN
el.length();                // byte count
el.raw_data();              // std::span<const uint8_t>
el.is_sequence();           // false
el.sequence_item_count();   // 0

// From dataset pointer
const dicom_element* e = ds.get(tags::patient_name);
if (e) { /* use *e */ }
```

---

## File I/O

```cpp
#include "kcenon/pacs/core/dicom_file.h"

using namespace kcenon::pacs::core;

// --- Read ---
auto result = dicom_file::open("image.dcm");
if (result.is_err()) {
    std::cerr << result.error().message << "\n";
}
auto& file = result.value();

file.dataset();              // const dicom_dataset&
file.transfer_syntax();      // transfer_syntax
file.sop_class_uid();        // std::string
file.sop_instance_uid();     // std::string

// --- Create and Write ---
auto new_file = dicom_file::create(
    std::move(dataset),
    transfer_syntax::explicit_vr_little_endian
);
auto save = new_file.save("output.dcm");
if (save.is_err()) { /* handle error */ }
```

---

## SCU Operations

### C-FIND (Query)

```cpp
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/services/query_scp.h"

using namespace kcenon::pacs::network;
using namespace kcenon::pacs::network::dimse;
using namespace kcenon::pacs::services;

association_config cfg;
cfg.calling_ae_title = "MY_SCU";
cfg.called_ae_title  = "PACS_SCP";
cfg.proposed_contexts.push_back({
    1, std::string(study_root_find_sop_class_uid),
    {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
});

auto conn = association::connect("host", 11112, cfg, 30s);
auto& assoc = conn.value();

dicom_dataset query;
query.set_string(dicom_tag{0x0008, 0x0052}, vr_type::CS, "STUDY");
query.set_string(tags::patient_name, vr_type::PN, "Smith*");

auto rq = make_c_find_rq(1, study_root_find_sop_class_uid);
rq.set_dataset(std::move(query));

auto ctx = *assoc.accepted_context_id(study_root_find_sop_class_uid);
assoc.send_dimse(ctx, rq);

// Loop: receive_dimse() until status == status_success
assoc.release(5s);
```

### C-STORE (Send)

```cpp
auto file = dicom_file::open("image.dcm").value();
auto sop = file.sop_class_uid();

// ... establish association proposing the SOP class ...

auto rq = make_c_store_rq(1, sop, file.sop_instance_uid());
rq.set_dataset(std::move(file.dataset()));

assoc.send_dimse(ctx_id, rq);
auto rsp = assoc.receive_dimse(30s);
// Check rsp.value().status() == status_success
```

### C-MOVE (Retrieve)

```cpp
auto rq = make_c_move_rq(1, study_root_move_sop_class_uid, "DEST_AE");
rq.set_dataset(std::move(query));  // query with StudyInstanceUID

assoc.send_dimse(ctx_id, rq);
// Loop: receive_dimse() until status == status_success
// Sub-operations reported via pending status fields
```

---

## SCP Setup

### Verification SCP (C-ECHO)

```cpp
#include "kcenon/pacs/network/dicom_server.h"
#include "kcenon/pacs/network/server_config.h"
#include "kcenon/pacs/services/verification_scp.h"

server_config cfg;
cfg.ae_title = "MY_SCP";
cfg.port     = 11112;

dicom_server server(cfg);
server.register_service(std::make_shared<verification_scp>());
server.start();       // Non-blocking
// ...
server.stop();
```

### Storage SCP (C-STORE receiver)

```cpp
#include "kcenon/pacs/services/storage_scp.h"

auto storage = std::make_shared<storage_scp>("./received");
server.register_service(storage);
server.start();
```

### Query/Retrieve SCP

```cpp
#include "kcenon/pacs/services/query_scp.h"

auto qr = std::make_shared<query_scp>(index_db, storage_dir);
server.register_service(qr);
```

---

## DICOMweb Endpoints

When built with `PACS_WITH_REST_API=ON`, pacs_server exposes:

| Endpoint | Method | Description |
|---|---|---|
| `/wado-rs/studies/{study}` | GET | Retrieve study instances |
| `/wado-rs/studies/{study}/metadata` | GET | Study metadata (JSON) |
| `/wado-rs/studies/{study}/series/{series}` | GET | Retrieve series |
| `/wado-rs/studies/{study}/series/{series}/instances/{instance}` | GET | Single instance |
| `/qido-rs/studies` | GET | Search studies |
| `/qido-rs/studies/{study}/series` | GET | Search series |
| `/qido-rs/studies/{study}/series/{series}/instances` | GET | Search instances |
| `/stow-rs/studies` | POST | Store instances |
| `/stow-rs/studies/{study}` | POST | Store into study |

Query parameters follow DICOM keyword names:
`PatientName`, `PatientID`, `StudyDate`, `Modality`, `AccessionNumber`, etc.

```bash
# QIDO-RS: search
curl "http://localhost:8080/qido-rs/studies?PatientName=Doe*&Modality=CT"

# WADO-RS: retrieve metadata
curl -H "Accept: application/dicom+json" \
     "http://localhost:8080/wado-rs/studies/1.2.840.../metadata"

# STOW-RS: store
curl -X POST \
     -H "Content-Type: multipart/related; type=application/dicom" \
     -F "file=@image.dcm;type=application/dicom" \
     "http://localhost:8080/stow-rs/studies"
```

---

## Storage Backends

```cpp
#include "kcenon/pacs/storage/index_database.h"

using namespace kcenon::pacs::storage;

// SQLite index database
auto db = index_database::open("pacs.db");

// Index a stored file
db.index_file("./archive/1.2.3.4.5.dcm", dataset);

// Query the index
auto results = db.find_studies({
    {"PatientName", "Doe*"},
    {"Modality", "CT"}
});

// Hierarchical file storage
// Files stored as: <root>/<StudyUID>/<SeriesUID>/<SOPInstanceUID>.dcm
```

Cloud storage (optional, requires `PACS_WITH_AWS_SDK` or `PACS_WITH_AZURE_SDK`):

```cpp
// S3 backend
auto s3 = s3_storage_backend("my-pacs-bucket", "us-east-1");

// Azure Blob backend
auto azure = azure_storage_backend("my-container", connection_string);
```

---

## Transfer Syntax and Codecs

```cpp
#include "kcenon/pacs/encoding/transfer_syntax.h"
#include "kcenon/pacs/encoding/compression/codec_factory.h"

using namespace kcenon::pacs::encoding;

// Predefined transfer syntaxes
auto ts = transfer_syntax::explicit_vr_little_endian;
ts.uid();                // "1.2.840.10008.1.2.1"
ts.name();               // "Explicit VR Little Endian"
ts.is_implicit_vr();     // false
ts.is_encapsulated();    // false

// Compression codecs
auto codec_uids = compression::codec_factory::supported_transfer_syntaxes();
// Returns UIDs for: JPEG Baseline, JPEG Lossless, JPEG 2000,
//                    JPEG-LS, RLE, HTJ2K, etc.

// Supported transfer syntaxes
auto all_ts = supported_transfer_syntaxes();

// Lookup by UID
auto found = find_transfer_syntax("1.2.840.10008.1.2.4.50");
if (found) {
    std::cout << found->name() << "\n";  // "JPEG Baseline (Process 1)"
}
```

---

## Common Include Paths

| Header | Purpose |
|---|---|
| `kcenon/pacs/core/dicom_dataset.h` | Dataset class |
| `kcenon/pacs/core/dicom_element.h` | Element class |
| `kcenon/pacs/core/dicom_file.h` | Part 10 file I/O |
| `kcenon/pacs/core/dicom_tag.h` | Tag class |
| `kcenon/pacs/core/dicom_tag_constants.h` | Standard tag constants (`tags::`) |
| `kcenon/pacs/core/dicom_dictionary.h` | Tag name lookup |
| `kcenon/pacs/encoding/transfer_syntax.h` | Transfer syntax definitions |
| `kcenon/pacs/encoding/vr_type.h` | VR type enumeration |
| `kcenon/pacs/network/association.h` | DICOM association management |
| `kcenon/pacs/network/dimse/dimse_message.h` | DIMSE message construction |
| `kcenon/pacs/network/dicom_server.h` | SCP server framework |
| `kcenon/pacs/services/query_scp.h` | Query/Retrieve service + SOP UIDs |
| `kcenon/pacs/services/storage_scp.h` | Storage SCP service |
| `kcenon/pacs/services/verification_scp.h` | Verification (C-ECHO) service |
| `kcenon/pacs/storage/index_database.h` | SQLite index database |

---

## CMake Link Targets

| Target | Module |
|---|---|
| `pacs_core` | Tags, elements, datasets, file I/O, dictionary |
| `pacs_encoding` | VR types, transfer syntaxes, compression codecs |
| `pacs_network` | PDU, association, DIMSE, server framework |
| `pacs_services` | SCP/SCU implementations, SOP class registry |
| `pacs_storage` | File storage, SQLite indexing |
| `pacs_security` | Anonymization, RBAC, TLS profiles |
| `pacs_web` | REST API, DICOMweb (WADO/STOW/QIDO) |
| `pacs_client` | Job management, routing, prefetch |
| `pacs_ai` | AI service connector |
| `pacs_monitoring` | Health checks, Prometheus metrics |
| `pacs_workflow` | Auto prefetch, task scheduler |
| `pacs_integration` | kcenon ecosystem adapters |
