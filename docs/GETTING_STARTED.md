# Getting Started with pacs_system

A practical introduction to the PACS System library. This guide walks through
reading DICOM files, inspecting tags, performing network queries (C-FIND),
sending images (C-STORE), and accessing DICOMweb endpoints.

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Reading a DICOM File](#reading-a-dicom-file)
- [Inspecting DICOM Tags](#inspecting-dicom-tags)
- [Querying a Remote PACS (C-FIND)](#querying-a-remote-pacs-c-find)
- [Sending Images to a PACS (C-STORE)](#sending-images-to-a-pacs-c-store)
- [DICOMweb WADO-RS Retrieval](#dicomweb-wado-rs-retrieval)
- [Running the Examples](#running-the-examples)
- [Next Steps](#next-steps)

---

## Prerequisites

| Requirement | Notes |
|---|---|
| **C++20 compiler** | GCC 12+, Clang 14+, or MSVC 2022 17.4+ |
| **CMake 3.21+** | Build system |
| **kcenon ecosystem** | `common_system`, `container_system`, `network_system` (fetched automatically) |
| **ICU** | Required for DICOM character set conversion |
| **SQLite3 >= 3.45.1** | Required for storage/indexing |

### Optional Dependencies

| Dependency | Purpose |
|---|---|
| OpenSSL >= 3.0 | TLS-secured DICOM transport |
| libjpeg-turbo | JPEG Baseline/Lossless codec |
| OpenJPEG | JPEG 2000 codec |
| CharLS | JPEG-LS codec |
| OpenJPH | High-Throughput JPEG 2000 (HTJ2K) codec |
| libpng | PNG pixel data export |
| Crow | REST API / DICOMweb endpoints |

> **Note on DCMTK:** pacs_system implements the DICOM standard from scratch and
> does **not** depend on DCMTK or GDCM. However, the CLI tools follow
> dcmtk-compatible interfaces where possible (e.g., `store_scu`, `find_scu`).

---

## Installation

### Clone and Build

```bash
git clone --recursive https://github.com/kcenon/pacs_system.git
cd pacs_system

# Configure and build
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON -DPACS_BUILD_TESTS=ON
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure --timeout 120
```

### CMake Integration (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
    pacs_system
    GIT_REPOSITORY https://github.com/kcenon/pacs_system.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(pacs_system)

target_link_libraries(my_app PRIVATE pacs_core pacs_encoding)
```

### Available CMake Options

| Option | Default | Description |
|---|---|---|
| `PACS_BUILD_TESTS` | ON | Build Catch2 unit tests |
| `PACS_BUILD_EXAMPLES` | OFF | Build 32 example programs |
| `PACS_BUILD_STORAGE` | ON | Build storage module (requires SQLite3) |
| `PACS_BUILD_CODECS` | ON | Build compression codecs |
| `PACS_WITH_OPENSSL` | ON | Enable TLS support |
| `PACS_WITH_REST_API` | ON | Enable DICOMweb REST API |

---

## Reading a DICOM File

Open a DICOM Part 10 file, read patient metadata, and access pixel data.

```cpp
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"

#include <iostream>

int main() {
    using namespace kcenon::pacs::core;

    // Open a DICOM file
    auto result = dicom_file::open("image.dcm");
    if (result.is_err()) {
        std::cerr << "Failed to open: " << result.error().message << "\n";
        return 1;
    }

    auto& file = result.value();
    const auto& dataset = file.dataset();

    // Read patient information
    std::cout << "Patient Name: "
              << dataset.get_string(tags::patient_name) << "\n";
    std::cout << "Patient ID:   "
              << dataset.get_string(tags::patient_id) << "\n";
    std::cout << "Study Date:   "
              << dataset.get_string(tags::study_date) << "\n";
    std::cout << "Modality:     "
              << dataset.get_string(tags::modality) << "\n";

    // Read image dimensions
    if (auto rows = dataset.get_numeric<uint16_t>(tags::rows)) {
        auto cols = dataset.get_numeric<uint16_t>(tags::columns);
        std::cout << "Dimensions:   "
                  << *cols << " x " << *rows << " pixels\n";
    }

    // Transfer syntax information
    std::cout << "Transfer Syntax: " << file.transfer_syntax().name() << "\n";

    return 0;
}
```

Link against: `pacs_core`, `pacs_encoding`

> **See also:** [dcm_info](../examples/dcm_info/) and
> [dcm_dump](../examples/dcm_dump/) for complete CLI implementations.

---

## Inspecting DICOM Tags

Iterate over all elements in a dataset and display their tag, VR, and value.

```cpp
#include "kcenon/pacs/core/dicom_dataset.h"
#include "kcenon/pacs/core/dicom_dictionary.h"
#include "kcenon/pacs/core/dicom_file.h"

#include <iomanip>
#include <iostream>

int main() {
    using namespace kcenon::pacs::core;

    auto result = dicom_file::open("image.dcm");
    if (result.is_err()) return 1;

    const auto& dataset = result.value().dataset();

    // Iterate all elements
    for (const auto& element : dataset) {
        auto tag = element.tag();
        auto& dict = dicom_dictionary::instance();
        auto name = dict.name_of(tag);

        std::cout << tag.to_string() << " "
                  << std::setw(30) << std::left
                  << (name.empty() ? "Unknown" : name) << " "
                  << to_string(element.vr()) << " ";

        if (!element.is_sequence() && element.length() < 256) {
            std::cout << dataset.get_string(tag);
        } else if (element.is_sequence()) {
            std::cout << "[Sequence: "
                      << element.sequence_item_count() << " items]";
        } else {
            std::cout << "[" << element.length() << " bytes]";
        }
        std::cout << "\n";
    }

    return 0;
}
```

> **See also:** [dcm_dump](../examples/dcm_dump/) for a full tag browser with
> filtering and JSON output.

---

## Querying a Remote PACS (C-FIND)

Search for studies on a remote PACS using the Study Root C-FIND service.

```cpp
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"
#include "kcenon/pacs/services/query_scp.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"

#include <iostream>

int main() {
    using namespace kcenon::pacs::network;
    using namespace kcenon::pacs::network::dimse;
    using namespace kcenon::pacs::services;
    using namespace kcenon::pacs::core;

    // Configure association
    association_config config;
    config.calling_ae_title = "MY_SCU";
    config.called_ae_title  = "PACS_SCP";
    config.proposed_contexts.push_back({
        1,
        std::string(study_root_find_sop_class_uid),
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });

    // Connect
    auto conn = association::connect(
        "pacs.example.com", 11112, config,
        std::chrono::milliseconds{30000});

    if (conn.is_err()) {
        std::cerr << "Connection failed: " << conn.error().message << "\n";
        return 1;
    }
    auto& assoc = conn.value();

    // Build query: all CT studies from today
    dicom_dataset query;
    query.set_string(dicom_tag{0x0008, 0x0052}, vr_type::CS, "STUDY");
    query.set_string(tags::modality,            vr_type::CS, "CT");
    query.set_string(tags::study_date,          vr_type::DA, "20260405");
    query.set_string(tags::patient_name,        vr_type::PN, "");  // return key
    query.set_string(tags::study_description,   vr_type::LO, "");  // return key

    // Send C-FIND
    auto find_rq = make_c_find_rq(1, study_root_find_sop_class_uid);
    find_rq.set_dataset(std::move(query));

    auto ctx_id = *assoc.accepted_context_id(study_root_find_sop_class_uid);
    assoc.send_dimse(ctx_id, find_rq);

    // Receive results
    while (true) {
        auto rsp = assoc.receive_dimse(std::chrono::milliseconds{30000});
        if (rsp.is_err()) break;

        auto& [id, msg] = rsp.value();
        if (msg.status() == status_success) break;  // done

        if (msg.has_dataset()) {
            auto ds = msg.dataset();
            if (ds.is_ok()) {
                auto& d = ds.value().get();
                std::cout << d.get_string(tags::patient_name)
                          << " | " << d.get_string(tags::study_description)
                          << "\n";
            }
        }
    }

    assoc.release(std::chrono::milliseconds{5000});
    return 0;
}
```

Link against: `pacs_core`, `pacs_encoding`, `pacs_network`, `pacs_services`

> **See also:** [query_scu](../examples/query_scu/) and
> [find_scu](../examples/find_scu/) for full CLI implementations with
> table/JSON/CSV output.

---

## Sending Images to a PACS (C-STORE)

Send a DICOM file to a remote SCP using the C-STORE service.

```cpp
#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/network/association.h"
#include "kcenon/pacs/network/dimse/dimse_message.h"

#include <iostream>

int main() {
    using namespace kcenon::pacs::core;
    using namespace kcenon::pacs::network;
    using namespace kcenon::pacs::network::dimse;

    // Open the DICOM file to send
    auto file_result = dicom_file::open("image.dcm");
    if (file_result.is_err()) {
        std::cerr << "Failed to open file\n";
        return 1;
    }
    auto& file = file_result.value();

    auto sop_class = file.sop_class_uid();
    auto sop_instance = file.sop_instance_uid();

    // Configure association with Storage SOP Class
    association_config config;
    config.calling_ae_title = "MY_SCU";
    config.called_ae_title  = "PACS_SCP";
    config.proposed_contexts.push_back({
        1, sop_class,
        {"1.2.840.10008.1.2.1", "1.2.840.10008.1.2"}
    });

    auto conn = association::connect(
        "pacs.example.com", 11112, config,
        std::chrono::milliseconds{30000});
    if (conn.is_err()) {
        std::cerr << "Connection failed: " << conn.error().message << "\n";
        return 1;
    }
    auto& assoc = conn.value();

    // Build and send C-STORE request
    auto store_rq = make_c_store_rq(1, sop_class, sop_instance);
    store_rq.set_dataset(std::move(file.dataset()));

    auto ctx_id = *assoc.accepted_context_id(sop_class);
    auto send = assoc.send_dimse(ctx_id, store_rq);
    if (send.is_err()) {
        std::cerr << "Send failed: " << send.error().message << "\n";
        assoc.abort();
        return 1;
    }

    // Receive response
    auto rsp = assoc.receive_dimse(std::chrono::milliseconds{30000});
    if (rsp.is_ok()) {
        auto& [id, msg] = rsp.value();
        if (msg.status() == status_success) {
            std::cout << "C-STORE succeeded\n";
        } else {
            std::cerr << "C-STORE failed, status: 0x"
                      << std::hex << msg.status() << "\n";
        }
    }

    assoc.release(std::chrono::milliseconds{5000});
    return 0;
}
```

Link against: `pacs_core`, `pacs_encoding`, `pacs_network`

> **See also:** [store_scu](../examples/store_scu/) for batch sending with
> progress display and [store_scp](../examples/store_scp/) for the receiver side.

---

## DICOMweb WADO-RS Retrieval

If the REST API module is enabled (`PACS_WITH_REST_API=ON`), pacs_system exposes
DICOMweb endpoints. This section shows how to configure and use them.

### Server-Side Setup

```cpp
#include "kcenon/pacs/web/dicomweb_server.h"

// DICOMweb endpoints are served by the pacs_server example.
// Start the server with REST API enabled:
//   pacs_server --port 11112 --rest-port 8080 --storage-dir ./archive
```

### Client-Side: WADO-RS with curl

```bash
# Retrieve a study (all instances as multipart DICOM)
curl -H "Accept: multipart/related; type=application/dicom" \
     "http://localhost:8080/wado-rs/studies/1.2.840.113619.2.55.3.604..."

# Retrieve study metadata as JSON
curl -H "Accept: application/dicom+json" \
     "http://localhost:8080/wado-rs/studies/1.2.840.113619.2.55.3.604.../metadata"

# Search for studies (QIDO-RS)
curl "http://localhost:8080/qido-rs/studies?PatientName=Smith*&Modality=CT"

# Store instances (STOW-RS)
curl -X POST \
     -H "Content-Type: multipart/related; type=application/dicom" \
     -F "file=@image.dcm;type=application/dicom" \
     "http://localhost:8080/stow-rs/studies"
```

> **See also:** The [pacs_server](../examples/pacs_server/) example for a
> complete server with DICOMweb support.

---

## Running the Examples

Build the examples with:

```bash
cmake -S . -B build -DPACS_BUILD_EXAMPLES=ON
cmake --build build
```

### Example Programs by Category

#### File Utilities
| Example | Description |
|---|---|
| `dcm_dump` | Inspect DICOM file tags |
| `dcm_info` | Display file summary |
| `dcm_extract` | Extract pixel data to images |
| `dcm_conv` | Convert transfer syntax |
| `dcm_modify` | Modify DICOM tags |
| `dcm_anonymize` | De-identify patient data |
| `dcm_to_json` / `json_to_dcm` | DICOM-JSON conversion |
| `dcm_to_xml` / `xml_to_dcm` | DICOM-XML conversion |
| `dcm_dir` | Create/manage DICOMDIR |
| `img_to_dcm` | Convert images to DICOM |

#### Networking
| Example | Description |
|---|---|
| `echo_scu` / `echo_scp` | Connectivity test (C-ECHO) |
| `store_scu` / `store_scp` | Image transfer (C-STORE) |
| `find_scu` | Query (C-FIND) |
| `move_scu` | Retrieve via move (C-MOVE) |
| `get_scu` | Retrieve directly (C-GET) |
| `query_scu` / `retrieve_scu` | Query and retrieve with output formatting |
| `qr_scp` | Query/Retrieve server |

#### Workflow
| Example | Description |
|---|---|
| `worklist_scu` / `worklist_scp` | Modality Worklist |
| `mpps_scu` / `mpps_scp` | Modality Performed Procedure Step |
| `print_scu` | DICOM Print Management |
| `pacs_server` | Full PACS archive server |

#### Advanced
| Example | Description |
|---|---|
| `secure_dicom/` | TLS-secured DICOM communication |
| `db_browser` | PACS index database viewer |
| `module_example` | C++20 module usage |

---

## Next Steps

- **[API Reference](API_REFERENCE.md)** -- Full class and method documentation.
- **[API Quick Reference](API_QUICK_REFERENCE.md)** -- Cheat sheet for common operations.
- **[Architecture Guide](ARCHITECTURE.md)** -- System design and module dependencies.
- **[CLI Reference](CLI_REFERENCE.md)** -- Complete command-line tool documentation.
- **[DICOM Conformance Statement](DICOM_CONFORMANCE_STATEMENT.md)** -- Supported SOP classes and transfer syntaxes.
- **[Doxygen Documentation](https://kcenon.github.io/pacs_system/)** -- Generated API docs with cross-referenced examples.
