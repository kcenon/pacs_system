# API Reference - PACS System

> **Version:** 1.0.0
> **Last Updated:** 2025-11-30
> **Language:** **English** | [한국어](API_REFERENCE_KO.md)

---

## Table of Contents

- [Core Module](#core-module)
- [Encoding Module](#encoding-module)
- [Network Module](#network-module)
- [Services Module](#services-module)
- [Storage Module](#storage-module)
- [Integration Module](#integration-module)
- [Common Module](#common-module)

---

## Core Module

### `pacs::core::dicom_tag`

Represents a DICOM Tag (Group, Element pair).

```cpp
#include <pacs/core/dicom_tags.h>

namespace pacs::core {

class dicom_tag {
public:
    // Constructors
    constexpr dicom_tag(uint16_t group, uint16_t element);
    constexpr dicom_tag(uint32_t tag);  // Combined tag value

    // Accessors
    constexpr uint16_t group() const noexcept;
    constexpr uint16_t element() const noexcept;
    constexpr uint32_t value() const noexcept;

    // Comparison
    constexpr bool operator==(const dicom_tag& other) const noexcept;
    constexpr bool operator<(const dicom_tag& other) const noexcept;

    // String representation
    std::string to_string() const;  // "(GGGG,EEEE)"
};

// Standard tag constants
namespace tags {
    // Patient Module
    constexpr dicom_tag PatientName{0x0010, 0x0010};
    constexpr dicom_tag PatientID{0x0010, 0x0020};
    constexpr dicom_tag PatientBirthDate{0x0010, 0x0030};
    constexpr dicom_tag PatientSex{0x0010, 0x0040};

    // Study Module
    constexpr dicom_tag StudyInstanceUID{0x0020, 0x000D};
    constexpr dicom_tag StudyDate{0x0008, 0x0020};
    constexpr dicom_tag StudyTime{0x0008, 0x0030};
    constexpr dicom_tag AccessionNumber{0x0008, 0x0050};

    // Series Module
    constexpr dicom_tag SeriesInstanceUID{0x0020, 0x000E};
    constexpr dicom_tag Modality{0x0008, 0x0060};
    constexpr dicom_tag SeriesNumber{0x0020, 0x0011};

    // Instance Module
    constexpr dicom_tag SOPInstanceUID{0x0008, 0x0018};
    constexpr dicom_tag SOPClassUID{0x0008, 0x0016};
    constexpr dicom_tag InstanceNumber{0x0020, 0x0013};

    // Image Module
    constexpr dicom_tag Rows{0x0028, 0x0010};
    constexpr dicom_tag Columns{0x0028, 0x0011};
    constexpr dicom_tag BitsAllocated{0x0028, 0x0100};
    constexpr dicom_tag PixelData{0x7FE0, 0x0010};

    // ... (complete list in dicom_tags.h)
}

} // namespace pacs::core
```

---

### `pacs::core::dicom_element`

Represents a DICOM Data Element.

```cpp
#include <pacs/core/dicom_element.h>

namespace pacs::core {

class dicom_element {
public:
    // Factory methods
    static dicom_element create(dicom_tag tag, vr_type vr);
    static dicom_element create(dicom_tag tag, vr_type vr, const std::string& value);
    static dicom_element create(dicom_tag tag, vr_type vr, const std::vector<uint8_t>& bytes);

    template<typename T>
    static dicom_element create_numeric(dicom_tag tag, vr_type vr, T value);

    // Accessors
    dicom_tag tag() const noexcept;
    vr_type vr() const noexcept;
    uint32_t length() const noexcept;
    bool is_empty() const noexcept;

    // Value access
    std::string as_string() const;
    std::vector<std::string> as_strings() const;  // Multi-valued

    template<typename T>
    T as_numeric() const;

    std::vector<uint8_t> as_bytes() const;

    // For SQ (Sequence)
    bool is_sequence() const noexcept;
    std::vector<dicom_dataset>& items();
    const std::vector<dicom_dataset>& items() const;

    // Value modification
    void set_string(const std::string& value);
    void set_bytes(const std::vector<uint8_t>& bytes);

    template<typename T>
    void set_numeric(T value);

    // Serialization
    std::vector<uint8_t> serialize(const transfer_syntax& ts) const;
    static common::Result<dicom_element> deserialize(
        const std::vector<uint8_t>& data,
        const transfer_syntax& ts
    );
};

} // namespace pacs::core
```

**Example:**
```cpp
using namespace pacs::core;

// Create string element
auto patient_name = dicom_element::create(
    tags::PatientName,
    vr_type::PN,
    "Doe^John"
);

// Create numeric element
auto rows = dicom_element::create_numeric(
    tags::Rows,
    vr_type::US,
    uint16_t{512}
);

// Access values
std::cout << patient_name.as_string() << std::endl;  // "Doe^John"
std::cout << rows.as_numeric<uint16_t>() << std::endl;  // 512
```

---

### `pacs::core::dicom_dataset`

Ordered collection of DICOM Data Elements.

```cpp
#include <pacs/core/dicom_dataset.h>

namespace pacs::core {

class dicom_dataset {
public:
    // Construction
    dicom_dataset() = default;
    dicom_dataset(const dicom_dataset& other);
    dicom_dataset(dicom_dataset&& other) noexcept;

    // Element access
    bool contains(dicom_tag tag) const noexcept;
    const dicom_element* get(dicom_tag tag) const;
    dicom_element* get(dicom_tag tag);

    // Typed getters (return default if not found)
    std::string get_string(dicom_tag tag, const std::string& default_value = "") const;
    std::vector<std::string> get_strings(dicom_tag tag) const;

    template<typename T>
    T get_numeric(dicom_tag tag, T default_value = T{}) const;

    std::vector<uint8_t> get_bytes(dicom_tag tag) const;

    // Typed setters
    void set_string(dicom_tag tag, const std::string& value);
    void set_strings(dicom_tag tag, const std::vector<std::string>& values);

    template<typename T>
    void set_numeric(dicom_tag tag, T value);

    void set_bytes(dicom_tag tag, const std::vector<uint8_t>& bytes);

    // Element management
    void add(dicom_element element);
    void remove(dicom_tag tag);
    void clear();

    // Iteration (tag order)
    using iterator = /* implementation-defined */;
    using const_iterator = /* implementation-defined */;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;
    const_iterator cbegin() const;
    const_iterator cend() const;

    // Properties
    size_t size() const noexcept;
    bool empty() const noexcept;

    // Serialization
    std::vector<uint8_t> serialize(const transfer_syntax& ts) const;
    static common::Result<dicom_dataset> deserialize(
        const std::vector<uint8_t>& data,
        const transfer_syntax& ts
    );

    // Copy subset
    dicom_dataset subset(const std::vector<dicom_tag>& tags) const;

    // Merge
    void merge(const dicom_dataset& other, bool overwrite = false);
};

} // namespace pacs::core
```

**Example:**
```cpp
using namespace pacs::core;

dicom_dataset dataset;

// Set values
dataset.set_string(tags::PatientName, "Doe^John");
dataset.set_string(tags::PatientID, "12345");
dataset.set_numeric(tags::Rows, uint16_t{512});
dataset.set_numeric(tags::Columns, uint16_t{512});

// Get values
auto name = dataset.get_string(tags::PatientName);
auto rows = dataset.get_numeric<uint16_t>(tags::Rows);

// Check existence
if (dataset.contains(tags::PixelData)) {
    auto pixels = dataset.get_bytes(tags::PixelData);
}

// Iterate
for (const auto& element : dataset) {
    std::cout << element.tag().to_string() << ": "
              << element.as_string() << std::endl;
}
```

---

### `pacs::core::dicom_file`

DICOM Part 10 file handling.

```cpp
#include <pacs/core/dicom_file.h>

namespace pacs::core {

class dicom_file {
public:
    // Factory methods
    static common::Result<dicom_file> read(const std::filesystem::path& path);
    static common::Result<dicom_file> read(std::istream& stream);

    // Construction
    dicom_file() = default;

    // File Meta Information
    const dicom_dataset& file_meta_info() const noexcept;
    dicom_dataset& file_meta_info() noexcept;

    // Data Set
    const dicom_dataset& dataset() const noexcept;
    dicom_dataset& dataset() noexcept;
    void set_dataset(dicom_dataset dataset);

    // Transfer Syntax
    transfer_syntax transfer_syntax() const;
    void set_transfer_syntax(transfer_syntax ts);

    // Media Storage SOP
    std::string media_storage_sop_class_uid() const;
    std::string media_storage_sop_instance_uid() const;

    // Write
    common::Result<void> write(const std::filesystem::path& path) const;
    common::Result<void> write(std::ostream& stream) const;

    // Validation
    common::Result<void> validate() const;
};

} // namespace pacs::core
```

**Example:**
```cpp
using namespace pacs::core;

// Read file
auto result = dicom_file::read("/path/to/image.dcm");
if (result.is_err()) {
    std::cerr << "Read failed: " << result.error().message << std::endl;
    return;
}

auto& file = result.value();

// Access data
std::cout << "Patient: " << file.dataset().get_string(tags::PatientName) << std::endl;
std::cout << "Transfer Syntax: " << file.transfer_syntax().uid() << std::endl;

// Modify and write
file.dataset().set_string(tags::InstitutionName, "My Hospital");
auto write_result = file.write("/path/to/modified.dcm");
```

---

## Encoding Module

### `pacs::encoding::vr_type`

Enumeration of DICOM Value Representations.

```cpp
#include <pacs/encoding/vr_types.h>

namespace pacs::encoding {

enum class vr_type : uint16_t {
    AE = 0x4145,  // Application Entity
    AS = 0x4153,  // Age String
    AT = 0x4154,  // Attribute Tag
    CS = 0x4353,  // Code String
    DA = 0x4441,  // Date
    DS = 0x4453,  // Decimal String
    DT = 0x4454,  // Date Time
    FL = 0x464C,  // Floating Point Single
    FD = 0x4644,  // Floating Point Double
    IS = 0x4953,  // Integer String
    LO = 0x4C4F,  // Long String
    LT = 0x4C54,  // Long Text
    OB = 0x4F42,  // Other Byte
    OD = 0x4F44,  // Other Double
    OF = 0x4F46,  // Other Float
    OL = 0x4F4C,  // Other Long
    OW = 0x4F57,  // Other Word
    PN = 0x504E,  // Person Name
    SH = 0x5348,  // Short String
    SL = 0x534C,  // Signed Long
    SQ = 0x5351,  // Sequence
    SS = 0x5353,  // Signed Short
    ST = 0x5354,  // Short Text
    TM = 0x544D,  // Time
    UI = 0x5549,  // Unique Identifier
    UL = 0x554C,  // Unsigned Long
    UN = 0x554E,  // Unknown
    US = 0x5553,  // Unsigned Short
    UT = 0x5554   // Unlimited Text
};

// VR utilities
class vr_info {
public:
    static bool is_string_vr(vr_type vr) noexcept;
    static bool is_numeric_vr(vr_type vr) noexcept;
    static bool is_binary_vr(vr_type vr) noexcept;
    static bool is_sequence_vr(vr_type vr) noexcept;

    static uint32_t max_length(vr_type vr) noexcept;
    static char padding_character(vr_type vr) noexcept;
    static bool uses_extended_length(vr_type vr) noexcept;

    static std::string to_string(vr_type vr);
    static vr_type from_string(const std::string& str);
};

} // namespace pacs::encoding
```

---

### `pacs::encoding::transfer_syntax`

Transfer Syntax management.

```cpp
#include <pacs/encoding/transfer_syntax.h>

namespace pacs::encoding {

class transfer_syntax {
public:
    // Predefined transfer syntaxes
    static transfer_syntax implicit_vr_little_endian();
    static transfer_syntax explicit_vr_little_endian();
    static transfer_syntax explicit_vr_big_endian();

    // Construction
    explicit transfer_syntax(const std::string& uid);

    // Properties
    const std::string& uid() const noexcept;
    bool is_implicit_vr() const noexcept;
    bool is_little_endian() const noexcept;
    bool is_encapsulated() const noexcept;

    // Comparison
    bool operator==(const transfer_syntax& other) const noexcept;

    // Name lookup
    std::string name() const;

    // Validation
    static bool is_valid_uid(const std::string& uid);
};

} // namespace pacs::encoding
```

---

## Network Module

### `pacs::network::association`

DICOM Association management.

```cpp
#include <pacs/network/association.h>

namespace pacs::network {

// Association configuration
struct association_config {
    std::string calling_ae_title;    // SCU AE Title
    std::string called_ae_title;     // SCP AE Title
    uint32_t max_pdu_size = 16384;   // Maximum PDU size
    std::chrono::seconds timeout{30}; // Association timeout

    // Add presentation context
    void add_context(
        const std::string& abstract_syntax,
        const std::vector<transfer_syntax>& transfer_syntaxes
    );
};

// Association state
enum class association_state {
    idle,
    awaiting_transport,
    awaiting_associate_response,
    awaiting_release_response,
    established,
    released,
    aborted
};

class association {
public:
    // Factory (SCU)
    static common::Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config
    );

    // Factory (SCP) - created by server
    // association is move-only
    association(association&&) noexcept;
    association& operator=(association&&) noexcept;
    ~association();

    // State
    association_state state() const noexcept;
    bool is_established() const noexcept;

    // Properties
    const std::string& calling_ae_title() const noexcept;
    const std::string& called_ae_title() const noexcept;
    uint32_t max_pdu_size() const noexcept;

    // Presentation context
    bool has_accepted_context(const std::string& abstract_syntax) const;
    std::optional<uint8_t> accepted_context_id(
        const std::string& abstract_syntax
    ) const;
    transfer_syntax accepted_transfer_syntax(uint8_t context_id) const;

    // DIMSE operations
    common::Result<void> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& message
    );

    common::Result<dimse::dimse_message> receive_dimse();

    // Lifecycle
    common::Result<void> release();
    void abort();
};

} // namespace pacs::network
```

**Example:**
```cpp
using namespace pacs::network;

// Configure association
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.add_context(
    sop_class::verification,
    {transfer_syntax::implicit_vr_little_endian()}
);

// Connect
auto result = association::connect("192.168.1.100", 11112, config);
if (result.is_err()) {
    std::cerr << "Connect failed: " << result.error().message << std::endl;
    return;
}

auto assoc = std::move(result.value());

// Perform C-ECHO
// ...

// Release
assoc.release();
```

---

## Services Module

### `pacs::services::verification_scu`

Verification SCU (C-ECHO).

```cpp
#include <pacs/services/verification_scu.h>

namespace pacs::services {

class verification_scu {
public:
    explicit verification_scu(const std::string& ae_title);

    // Simple echo
    common::Result<void> echo(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port
    );

    // Echo with existing association
    common::Result<void> echo(network::association& assoc);
};

} // namespace pacs::services
```

---

### `pacs::services::storage_scp`

Storage SCP (C-STORE receiver).

```cpp
#include <pacs/services/storage_scp.h>

namespace pacs::services {

// Storage status codes
enum class storage_status : uint16_t {
    success = 0x0000,
    warning_coercion = 0xB000,
    warning_elements_discarded = 0xB007,
    error_cannot_understand = 0xC000,
    error_out_of_resources = 0xA700
};

// Storage handler callback
using storage_handler = std::function<storage_status(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae
)>;

// Configuration
struct storage_scp_config {
    std::string ae_title;
    uint16_t port = 11112;
    std::filesystem::path storage_path;
    size_t max_associations = 10;
    std::vector<std::string> accepted_sop_classes;
};

class storage_scp {
public:
    explicit storage_scp(const storage_scp_config& config);
    ~storage_scp();

    // Handler
    void set_handler(storage_handler handler);

    // Lifecycle
    common::Result<void> start();
    void stop();
    void wait_for_shutdown();

    // Status
    bool is_running() const noexcept;
    size_t active_associations() const noexcept;
};

} // namespace pacs::services
```

**Example:**
```cpp
using namespace pacs::services;

storage_scp_config config;
config.ae_title = "PACS_SCP";
config.port = 11112;
config.storage_path = "/data/dicom";

storage_scp server(config);

server.set_handler([](const core::dicom_dataset& dataset,
                      const std::string& calling_ae) {
    std::cout << "Received from " << calling_ae << ": "
              << dataset.get_string(tags::SOPInstanceUID) << std::endl;
    return storage_status::success;
});

auto result = server.start();
if (result.is_ok()) {
    server.wait_for_shutdown();
}
```

---

### `pacs::services::storage_scu`

Storage SCU (C-STORE sender).

```cpp
#include <pacs/services/storage_scu.h>

namespace pacs::services {

class storage_scu {
public:
    explicit storage_scu(const std::string& ae_title);

    // Send single image
    common::Result<void> store(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port,
        const core::dicom_file& file
    );

    // Send multiple images
    common::Result<void> store_batch(
        const std::string& remote_ae,
        const std::string& host,
        uint16_t port,
        const std::vector<std::filesystem::path>& files,
        std::function<void(size_t, size_t)> progress_callback = nullptr
    );

    // Send with existing association
    common::Result<void> store(
        network::association& assoc,
        const core::dicom_file& file
    );
};

} // namespace pacs::services
```

---

## Storage Module

### `pacs::storage::storage_interface`

Abstract storage backend interface.

```cpp
#include <pacs/storage/storage_interface.h>

namespace pacs::storage {

class storage_interface {
public:
    virtual ~storage_interface() = default;

    // Store
    virtual common::Result<void> store(const core::dicom_dataset& dataset) = 0;

    // Retrieve
    virtual common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid
    ) = 0;

    // Delete
    virtual common::Result<void> remove(const std::string& sop_instance_uid) = 0;

    // Query
    virtual common::Result<std::vector<core::dicom_dataset>> find(
        const core::dicom_dataset& query
    ) = 0;

    // Check existence
    virtual bool exists(const std::string& sop_instance_uid) = 0;
};

} // namespace pacs::storage
```

---

### `pacs::storage::file_storage`

Filesystem-based storage implementation.

```cpp
#include <pacs/storage/file_storage.h>

namespace pacs::storage {

struct file_storage_config {
    std::filesystem::path root_path;

    enum class naming_scheme {
        uid_based,       // {StudyUID}/{SeriesUID}/{SOPUID}.dcm
        date_based,      // {YYYY}/{MM}/{DD}/{SOPUID}.dcm
        flat             // {SOPUID}.dcm
    } scheme = naming_scheme::uid_based;

    bool create_directories = true;
    bool overwrite_duplicates = false;
};

class file_storage : public storage_interface {
public:
    explicit file_storage(const file_storage_config& config);

    // storage_interface implementation
    common::Result<void> store(const core::dicom_dataset& dataset) override;
    common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid
    ) override;
    common::Result<void> remove(const std::string& sop_instance_uid) override;
    common::Result<std::vector<core::dicom_dataset>> find(
        const core::dicom_dataset& query
    ) override;
    bool exists(const std::string& sop_instance_uid) override;

    // File path
    std::filesystem::path file_path(const std::string& sop_instance_uid) const;
};

} // namespace pacs::storage
```

---

## Common Module

### Error Codes

```cpp
#include <pacs/common/error_codes.h>

namespace pacs::error_codes {

// DICOM Parsing Errors (-800 to -819)
constexpr int INVALID_DICOM_FILE = -800;
constexpr int INVALID_VR = -801;
constexpr int MISSING_REQUIRED_TAG = -802;
constexpr int INVALID_TRANSFER_SYNTAX = -803;
constexpr int INVALID_DATA_ELEMENT = -804;
constexpr int SEQUENCE_ENCODING_ERROR = -805;

// Association Errors (-820 to -839)
constexpr int ASSOCIATION_REJECTED = -820;
constexpr int ASSOCIATION_ABORTED = -821;
constexpr int NO_PRESENTATION_CONTEXT = -822;
constexpr int INVALID_PDU = -823;
constexpr int PDU_TOO_LARGE = -824;
constexpr int ASSOCIATION_TIMEOUT = -825;

// DIMSE Errors (-840 to -859)
constexpr int DIMSE_FAILURE = -840;
constexpr int DIMSE_TIMEOUT = -841;
constexpr int DIMSE_INVALID_RESPONSE = -842;
constexpr int DIMSE_CANCELLED = -843;
constexpr int DIMSE_STATUS_ERROR = -844;

// Storage Errors (-860 to -879)
constexpr int STORAGE_FAILED = -860;
constexpr int DUPLICATE_SOP_INSTANCE = -861;
constexpr int INVALID_SOP_CLASS = -862;
constexpr int STORAGE_FULL = -863;
constexpr int FILE_WRITE_ERROR = -864;

// Query Errors (-880 to -899)
constexpr int QUERY_FAILED = -880;
constexpr int NO_MATCHES_FOUND = -881;
constexpr int TOO_MANY_MATCHES = -882;
constexpr int INVALID_QUERY_LEVEL = -883;
constexpr int DATABASE_ERROR = -884;

} // namespace pacs::error_codes
```

---

### UID Generator

```cpp
#include <pacs/common/uid_generator.h>

namespace pacs::common {

class uid_generator {
public:
    // Generate new UID
    static std::string generate();

    // Generate with prefix (organization root)
    static std::string generate(const std::string& root);

    // Validate UID format
    static bool is_valid(const std::string& uid);
};

} // namespace pacs::common
```

---

*Document Version: 1.0.0*
*Created: 2025-11-30*
*Author: kcenon@naver.com*
