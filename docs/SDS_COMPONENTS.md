# SDS - Component Design Specifications

> **Version:** 1.0.0
> **Parent Document:** [SDS.md](SDS.md)
> **Last Updated:** 2025-11-30

---

## Table of Contents

- [1. Core Module Components](#1-core-module-components)
- [2. Encoding Module Components](#2-encoding-module-components)
- [3. Network Module Components](#3-network-module-components)
- [4. Services Module Components](#4-services-module-components)
- [5. Storage Module Components](#5-storage-module-components)
- [6. Integration Module Components](#6-integration-module-components)

---

## 1. Core Module Components

### DES-CORE-001: dicom_tag

**Traces to:** SRS-CORE-001, FR-1.1

**Purpose:** Represents a DICOM Tag (Group, Element pair).

**Class Design:**

```cpp
namespace pacs::core {

class dicom_tag {
public:
    // ─────────────────────────────────────────────────────
    // Constants
    // ─────────────────────────────────────────────────────
    static constexpr uint16_t PRIVATE_GROUP_START = 0x0009;
    static constexpr uint16_t PRIVATE_GROUP_END = 0xFFFF;

    // ─────────────────────────────────────────────────────
    // Constructors
    // ─────────────────────────────────────────────────────
    constexpr dicom_tag() noexcept;
    constexpr dicom_tag(uint16_t group, uint16_t element) noexcept;
    constexpr explicit dicom_tag(uint32_t combined) noexcept;

    // ─────────────────────────────────────────────────────
    // Accessors
    // ─────────────────────────────────────────────────────
    [[nodiscard]] constexpr uint16_t group() const noexcept;
    [[nodiscard]] constexpr uint16_t element() const noexcept;
    [[nodiscard]] constexpr uint32_t value() const noexcept;

    // ─────────────────────────────────────────────────────
    // Classification
    // ─────────────────────────────────────────────────────
    [[nodiscard]] constexpr bool is_private() const noexcept;
    [[nodiscard]] constexpr bool is_group_length() const noexcept;
    [[nodiscard]] constexpr bool is_meta_info() const noexcept;

    // ─────────────────────────────────────────────────────
    // Comparison (for std::map ordering)
    // ─────────────────────────────────────────────────────
    constexpr auto operator<=>(const dicom_tag&) const noexcept = default;

    // ─────────────────────────────────────────────────────
    // String representation
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::string to_string() const;  // "(GGGG,EEEE)"

private:
    uint16_t group_{0};
    uint16_t element_{0};
};

} // namespace pacs::core
```

**Design Decisions:**

| Decision | Rationale |
|----------|-----------|
| `constexpr` construction | Compile-time tag constants |
| `operator<=>` | C++20 three-way comparison for ordering |
| Separate group/element | Direct mapping to DICOM binary format |

**Memory Layout:**

```
┌──────────────────────────────────────┐
│           dicom_tag (4 bytes)        │
├──────────────────┬───────────────────┤
│  group_ (2 bytes)│ element_ (2 bytes)│
│    0x0010        │      0x0010       │
│   (Patient)      │   (PatientName)   │
└──────────────────┴───────────────────┘
```

---

### DES-CORE-002: dicom_element

**Traces to:** SRS-CORE-002, FR-1.1

**Purpose:** Represents a DICOM Data Element with tag, VR, and value.

**Class Design:**

```cpp
namespace pacs::core {

class dicom_element {
public:
    // ─────────────────────────────────────────────────────
    // Factory Methods
    // ─────────────────────────────────────────────────────
    static dicom_element create(dicom_tag tag, vr_type vr);
    static dicom_element create(dicom_tag tag, vr_type vr,
                                 std::string_view value);
    static dicom_element create(dicom_tag tag, vr_type vr,
                                 std::span<const uint8_t> bytes);

    template<std::integral T>
    static dicom_element create_numeric(dicom_tag tag, vr_type vr, T value);

    template<std::floating_point T>
    static dicom_element create_numeric(dicom_tag tag, vr_type vr, T value);

    static dicom_element create_sequence(dicom_tag tag);

    // ─────────────────────────────────────────────────────
    // Accessors (const)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] dicom_tag tag() const noexcept;
    [[nodiscard]] vr_type vr() const noexcept;
    [[nodiscard]] uint32_t length() const noexcept;
    [[nodiscard]] bool is_empty() const noexcept;

    // ─────────────────────────────────────────────────────
    // Value Access
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::string as_string() const;
    [[nodiscard]] std::vector<std::string> as_strings() const;

    template<typename T>
    [[nodiscard]] T as_numeric() const;

    [[nodiscard]] std::span<const uint8_t> as_bytes() const;

    // ─────────────────────────────────────────────────────
    // Sequence Access
    // ─────────────────────────────────────────────────────
    [[nodiscard]] bool is_sequence() const noexcept;
    [[nodiscard]] std::vector<dicom_dataset>& items();
    [[nodiscard]] const std::vector<dicom_dataset>& items() const;

    // ─────────────────────────────────────────────────────
    // Value Modification
    // ─────────────────────────────────────────────────────
    void set_string(std::string_view value);
    void set_bytes(std::span<const uint8_t> bytes);

    template<typename T>
    void set_numeric(T value);

    void add_item(dicom_dataset item);

    // ─────────────────────────────────────────────────────
    // Serialization
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::vector<uint8_t> serialize(
        const transfer_syntax& ts) const;

    static common::Result<dicom_element> deserialize(
        std::span<const uint8_t> data,
        const transfer_syntax& ts,
        const dicom_dictionary& dict);

private:
    dicom_tag tag_;
    vr_type vr_;
    std::variant<
        std::string,                    // String VRs (AE, CS, LO, etc.)
        std::vector<uint8_t>,           // Binary VRs (OB, OW, UN, etc.)
        std::vector<dicom_dataset>      // Sequence VR (SQ)
    > value_;
};

} // namespace pacs::core
```

**Value Storage Strategy:**

```
┌─────────────────────────────────────────────────────────────────┐
│                     dicom_element Value Storage                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  std::variant<                                                   │
│      std::string,          ◄── String VRs: AE, AS, CS, DA, DS,  │
│                                DT, IS, LO, LT, PN, SH, ST, TM,  │
│                                UI, UT                            │
│                                                                  │
│      std::vector<uint8_t>, ◄── Binary VRs: AT, FL, FD, OB, OD,  │
│                                OF, OL, OW, SL, SS, UL, US, UN   │
│                                                                  │
│      std::vector<dicom_dataset>  ◄── Sequence VR: SQ            │
│  >                                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Serialization Format (Explicit VR Little Endian):**

```
┌────────────────────────────────────────────────────────────────────┐
│              Explicit VR Data Element Encoding                      │
├────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Standard VRs (2-byte length):                                      │
│  ┌──────────┬──────────┬─────────┬─────────┬────────────────────┐  │
│  │  Group   │ Element  │   VR    │ Length  │      Value         │  │
│  │ (2 bytes)│ (2 bytes)│(2 bytes)│(2 bytes)│   (variable)       │  │
│  └──────────┴──────────┴─────────┴─────────┴────────────────────┘  │
│                                                                     │
│  Extended VRs (OB, OD, OF, OL, OW, SQ, UN, UC, UR, UT):            │
│  ┌──────────┬──────────┬─────────┬─────────┬─────────┬──────────┐  │
│  │  Group   │ Element  │   VR    │Reserved │ Length  │  Value   │  │
│  │ (2 bytes)│ (2 bytes)│(2 bytes)│(2 bytes)│(4 bytes)│(variable)│  │
│  └──────────┴──────────┴─────────┴─────────┴─────────┴──────────┘  │
│                                                                     │
└────────────────────────────────────────────────────────────────────┘
```

---

### DES-CORE-003: dicom_dataset

**Traces to:** SRS-CORE-003, FR-1.1

**Purpose:** Ordered collection of DICOM Data Elements.

**Class Design:**

```cpp
namespace pacs::core {

class dicom_dataset {
public:
    // ─────────────────────────────────────────────────────
    // Type Aliases
    // ─────────────────────────────────────────────────────
    using container_type = std::map<dicom_tag, dicom_element>;
    using iterator = container_type::iterator;
    using const_iterator = container_type::const_iterator;

    // ─────────────────────────────────────────────────────
    // Construction / Copy / Move
    // ─────────────────────────────────────────────────────
    dicom_dataset() = default;
    dicom_dataset(const dicom_dataset&);
    dicom_dataset(dicom_dataset&&) noexcept;
    dicom_dataset& operator=(const dicom_dataset&);
    dicom_dataset& operator=(dicom_dataset&&) noexcept;

    // ─────────────────────────────────────────────────────
    // Element Access
    // ─────────────────────────────────────────────────────
    [[nodiscard]] bool contains(dicom_tag tag) const noexcept;
    [[nodiscard]] const dicom_element* get(dicom_tag tag) const;
    [[nodiscard]] dicom_element* get(dicom_tag tag);

    const dicom_element& operator[](dicom_tag tag) const;
    dicom_element& operator[](dicom_tag tag);

    // ─────────────────────────────────────────────────────
    // Typed Getters (with defaults)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::string get_string(
        dicom_tag tag,
        std::string_view default_value = "") const;

    [[nodiscard]] std::vector<std::string> get_strings(dicom_tag tag) const;

    template<typename T>
    [[nodiscard]] T get_numeric(dicom_tag tag, T default_value = T{}) const;

    [[nodiscard]] std::vector<uint8_t> get_bytes(dicom_tag tag) const;

    // ─────────────────────────────────────────────────────
    // Typed Setters (auto VR lookup)
    // ─────────────────────────────────────────────────────
    void set_string(dicom_tag tag, std::string_view value);
    void set_strings(dicom_tag tag, const std::vector<std::string>& values);

    template<typename T>
    void set_numeric(dicom_tag tag, T value);

    void set_bytes(dicom_tag tag, std::span<const uint8_t> bytes);

    // ─────────────────────────────────────────────────────
    // Element Management
    // ─────────────────────────────────────────────────────
    void add(dicom_element element);
    void remove(dicom_tag tag);
    void clear();

    // ─────────────────────────────────────────────────────
    // Iteration (in tag order)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] iterator end() noexcept;
    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator end() const noexcept;
    [[nodiscard]] const_iterator cbegin() const noexcept;
    [[nodiscard]] const_iterator cend() const noexcept;

    // ─────────────────────────────────────────────────────
    // Properties
    // ─────────────────────────────────────────────────────
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;

    // ─────────────────────────────────────────────────────
    // Operations
    // ─────────────────────────────────────────────────────
    [[nodiscard]] dicom_dataset subset(
        std::span<const dicom_tag> tags) const;

    void merge(const dicom_dataset& other, bool overwrite = false);

    // ─────────────────────────────────────────────────────
    // Serialization
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::vector<uint8_t> serialize(
        const transfer_syntax& ts) const;

    static common::Result<dicom_dataset> deserialize(
        std::span<const uint8_t> data,
        const transfer_syntax& ts);

private:
    container_type elements_;
    const dicom_dictionary* dictionary_ = &dicom_dictionary::instance();
};

} // namespace pacs::core
```

**Internal Structure:**

```
┌─────────────────────────────────────────────────────────────────┐
│                        dicom_dataset                             │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  elements_: std::map<dicom_tag, dicom_element>                  │
│                                                                  │
│  ┌─────────────┬────────────────────────────────────────────┐   │
│  │  (0008,0016)│ SOPClassUID = "1.2.840.10008.5.1.4.1.1.2" │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │  (0008,0018)│ SOPInstanceUID = "1.2.3.4.5..."           │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │  (0010,0010)│ PatientName = "Doe^John"                   │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │  (0010,0020)│ PatientID = "12345"                        │   │
│  ├─────────────┼────────────────────────────────────────────┤   │
│  │     ...     │        ...                                 │   │
│  └─────────────┴────────────────────────────────────────────┘   │
│                                                                  │
│  Note: Elements stored in ascending tag order (std::map)        │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### DES-CORE-004: dicom_file

**Traces to:** SRS-CORE-004, FR-1.3

**Purpose:** DICOM Part 10 file read/write operations.

**Class Design:**

```cpp
namespace pacs::core {

class dicom_file {
public:
    // ─────────────────────────────────────────────────────
    // Constants
    // ─────────────────────────────────────────────────────
    static constexpr size_t PREAMBLE_SIZE = 128;
    static constexpr std::string_view DICM_PREFIX = "DICM";

    // ─────────────────────────────────────────────────────
    // Factory Methods (File I/O)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static common::Result<dicom_file> read(
        const std::filesystem::path& path);

    [[nodiscard]] static common::Result<dicom_file> read(
        std::istream& stream);

    [[nodiscard]] static common::Result<dicom_file> read(
        std::span<const uint8_t> data);

    // ─────────────────────────────────────────────────────
    // Construction
    // ─────────────────────────────────────────────────────
    dicom_file() = default;
    explicit dicom_file(dicom_dataset dataset);

    // ─────────────────────────────────────────────────────
    // File Meta Information (Group 0002)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] const dicom_dataset& file_meta_info() const noexcept;
    [[nodiscard]] dicom_dataset& file_meta_info() noexcept;

    // ─────────────────────────────────────────────────────
    // Data Set
    // ─────────────────────────────────────────────────────
    [[nodiscard]] const dicom_dataset& dataset() const noexcept;
    [[nodiscard]] dicom_dataset& dataset() noexcept;
    void set_dataset(dicom_dataset dataset);

    // ─────────────────────────────────────────────────────
    // Transfer Syntax
    // ─────────────────────────────────────────────────────
    [[nodiscard]] transfer_syntax get_transfer_syntax() const;
    void set_transfer_syntax(const transfer_syntax& ts);

    // ─────────────────────────────────────────────────────
    // Media Storage SOP
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::string media_storage_sop_class_uid() const;
    [[nodiscard]] std::string media_storage_sop_instance_uid() const;

    // ─────────────────────────────────────────────────────
    // Write Operations
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> write(
        const std::filesystem::path& path) const;

    [[nodiscard]] common::Result<void> write(std::ostream& stream) const;

    [[nodiscard]] std::vector<uint8_t> to_bytes() const;

    // ─────────────────────────────────────────────────────
    // Validation
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> validate() const;

private:
    void generate_file_meta_info();

    std::array<uint8_t, PREAMBLE_SIZE> preamble_{};
    dicom_dataset file_meta_info_;
    dicom_dataset dataset_;
};

} // namespace pacs::core
```

**File Format Structure:**

```
┌──────────────────────────────────────────────────────────────────┐
│                    DICOM Part 10 File Format                      │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                     Preamble (128 bytes)                    │  │
│  │                     (Usually all zeros)                     │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                  DICM Prefix (4 bytes)                      │  │
│  │                     "DICM" literal                          │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │              File Meta Information (Group 0002)             │  │
│  │  ─────────────────────────────────────────────────────────  │  │
│  │  Always Explicit VR Little Endian                           │  │
│  │                                                              │  │
│  │  (0002,0000) FileMetaInformationGroupLength                 │  │
│  │  (0002,0001) FileMetaInformationVersion                     │  │
│  │  (0002,0002) MediaStorageSOPClassUID                        │  │
│  │  (0002,0003) MediaStorageSOPInstanceUID                     │  │
│  │  (0002,0010) TransferSyntaxUID                              │  │
│  │  (0002,0012) ImplementationClassUID                         │  │
│  │  (0002,0013) ImplementationVersionName                      │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                      Data Set                               │  │
│  │  ─────────────────────────────────────────────────────────  │  │
│  │  Encoded per Transfer Syntax from (0002,0010)               │  │
│  │                                                              │  │
│  │  (0008,0016) SOPClassUID                                    │  │
│  │  (0008,0018) SOPInstanceUID                                 │  │
│  │  (0010,0010) PatientName                                    │  │
│  │  (0010,0020) PatientID                                      │  │
│  │  ... (all other data elements)                              │  │
│  │  (7FE0,0010) PixelData                                      │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

### DES-CORE-005: dicom_dictionary

**Traces to:** SRS-CORE-005, FR-1.1

**Purpose:** Tag metadata lookup and registration.

**Class Design:**

```cpp
namespace pacs::core {

// Tag metadata entry
struct tag_info {
    vr_type vr;
    std::string_view keyword;
    std::string_view name;
    std::string_view vm;  // Value Multiplicity
    bool retired;
};

class dicom_dictionary {
public:
    // ─────────────────────────────────────────────────────
    // Singleton Access
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static const dicom_dictionary& instance();

    // ─────────────────────────────────────────────────────
    // Tag Lookup
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<tag_info> lookup(dicom_tag tag) const;
    [[nodiscard]] std::optional<vr_type> lookup_vr(dicom_tag tag) const;
    [[nodiscard]] std::optional<std::string_view> lookup_name(
        dicom_tag tag) const;

    // ─────────────────────────────────────────────────────
    // Keyword Lookup
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<dicom_tag> lookup_tag(
        std::string_view keyword) const;

    // ─────────────────────────────────────────────────────
    // Private Tag Registration
    // ─────────────────────────────────────────────────────
    void register_private_tag(
        dicom_tag tag,
        std::string_view creator,
        tag_info info);

    [[nodiscard]] std::optional<tag_info> lookup_private(
        dicom_tag tag,
        std::string_view creator) const;

    // ─────────────────────────────────────────────────────
    // SOP Class Registry
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<std::string_view> sop_class_name(
        std::string_view uid) const;

    [[nodiscard]] bool is_storage_sop_class(std::string_view uid) const;

private:
    dicom_dictionary();  // Initialize from PS3.6 data

    std::unordered_map<uint32_t, tag_info> standard_tags_;
    std::unordered_map<std::string_view, dicom_tag> keyword_map_;
    std::map<std::pair<dicom_tag, std::string>, tag_info> private_tags_;
    std::unordered_map<std::string_view, std::string_view> sop_classes_;
};

} // namespace pacs::core
```

---

## 2. Encoding Module Components

### DES-ENC-001: vr_type

**Traces to:** SRS-CORE-006, FR-1.2

**Purpose:** Enumeration of 27 DICOM Value Representations.

**Design:**

```cpp
namespace pacs::encoding {

// VR Type enumeration (2-byte character code)
enum class vr_type : uint16_t {
    // String VRs
    AE = 0x4145,  // Application Entity (16 chars max)
    AS = 0x4153,  // Age String (4 chars: nnnD/W/M/Y)
    CS = 0x4353,  // Code String (16 chars max)
    DA = 0x4441,  // Date (8 chars: YYYYMMDD)
    DS = 0x4453,  // Decimal String (16 chars max)
    DT = 0x4454,  // Date Time (26 chars max)
    IS = 0x4953,  // Integer String (12 chars max)
    LO = 0x4C4F,  // Long String (64 chars max)
    LT = 0x4C54,  // Long Text (10240 chars max)
    PN = 0x504E,  // Person Name (64 chars per component)
    SH = 0x5348,  // Short String (16 chars max)
    ST = 0x5354,  // Short Text (1024 chars max)
    TM = 0x544D,  // Time (14 chars max)
    UI = 0x5549,  // Unique Identifier (64 chars max)
    UT = 0x5554,  // Unlimited Text (2^32-2 max)

    // Binary Numeric VRs
    AT = 0x4154,  // Attribute Tag (4 bytes)
    FL = 0x464C,  // Floating Point Single (4 bytes)
    FD = 0x4644,  // Floating Point Double (8 bytes)
    SL = 0x534C,  // Signed Long (4 bytes)
    SS = 0x5353,  // Signed Short (2 bytes)
    UL = 0x554C,  // Unsigned Long (4 bytes)
    US = 0x5553,  // Unsigned Short (2 bytes)

    // Binary Data VRs
    OB = 0x4F42,  // Other Byte
    OD = 0x4F44,  // Other Double
    OF = 0x4F46,  // Other Float
    OL = 0x4F4C,  // Other Long
    OW = 0x4F57,  // Other Word

    // Special VRs
    SQ = 0x5351,  // Sequence
    UN = 0x554E,  // Unknown
};

} // namespace pacs::encoding
```

### DES-ENC-002: vr_info

**Traces to:** SRS-CORE-006

**Purpose:** VR metadata and utilities.

**Design:**

```cpp
namespace pacs::encoding {

class vr_info {
public:
    // ─────────────────────────────────────────────────────
    // Classification
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static constexpr bool is_string_vr(vr_type vr) noexcept;
    [[nodiscard]] static constexpr bool is_numeric_vr(vr_type vr) noexcept;
    [[nodiscard]] static constexpr bool is_binary_vr(vr_type vr) noexcept;
    [[nodiscard]] static constexpr bool is_sequence_vr(vr_type vr) noexcept;

    // ─────────────────────────────────────────────────────
    // Constraints
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static constexpr uint32_t max_length(vr_type vr) noexcept;
    [[nodiscard]] static constexpr char padding_char(vr_type vr) noexcept;
    [[nodiscard]] static constexpr bool uses_extended_length(
        vr_type vr) noexcept;

    // ─────────────────────────────────────────────────────
    // Conversion
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static std::string to_string(vr_type vr);
    [[nodiscard]] static std::optional<vr_type> from_string(
        std::string_view str);
    [[nodiscard]] static std::array<char, 2> to_chars(vr_type vr) noexcept;
};

} // namespace pacs::encoding
```

**VR Classification Table:**

| VR | Category | Max Length | Padding | Extended |
|----|----------|------------|---------|----------|
| AE | String | 16 | Space | No |
| AS | String | 4 | Space | No |
| CS | String | 16 | Space | No |
| DA | String | 8 | Space | No |
| DS | String | 16 | Space | No |
| FL | Numeric | 4 | N/A | No |
| FD | Numeric | 8 | N/A | No |
| OB | Binary | 2^32-2 | 0x00 | Yes |
| OW | Binary | 2^32-2 | N/A | Yes |
| SQ | Sequence | Undefined | N/A | Yes |
| ... | ... | ... | ... | ... |

---

### DES-ENC-003: transfer_syntax

**Traces to:** SRS-CORE-007, FR-1.4

**Purpose:** Transfer Syntax management and codec selection.

**Design:**

```cpp
namespace pacs::encoding {

class transfer_syntax {
public:
    // ─────────────────────────────────────────────────────
    // Well-Known Transfer Syntaxes
    // ─────────────────────────────────────────────────────
    static const transfer_syntax& implicit_vr_little_endian();
    static const transfer_syntax& explicit_vr_little_endian();
    static const transfer_syntax& explicit_vr_big_endian();
    static const transfer_syntax& deflated_explicit_vr_little_endian();

    // JPEG Transfer Syntaxes (encapsulated)
    static const transfer_syntax& jpeg_baseline();
    static const transfer_syntax& jpeg_lossless();
    static const transfer_syntax& jpeg2000_lossless();
    static const transfer_syntax& jpeg2000_lossy();

    // ─────────────────────────────────────────────────────
    // Construction
    // ─────────────────────────────────────────────────────
    explicit transfer_syntax(std::string uid);

    // ─────────────────────────────────────────────────────
    // Properties
    // ─────────────────────────────────────────────────────
    [[nodiscard]] const std::string& uid() const noexcept;
    [[nodiscard]] std::string_view name() const;

    [[nodiscard]] bool is_implicit_vr() const noexcept;
    [[nodiscard]] bool is_explicit_vr() const noexcept;
    [[nodiscard]] bool is_little_endian() const noexcept;
    [[nodiscard]] bool is_big_endian() const noexcept;
    [[nodiscard]] bool is_encapsulated() const noexcept;
    [[nodiscard]] bool is_compressed() const noexcept;

    // ─────────────────────────────────────────────────────
    // Comparison
    // ─────────────────────────────────────────────────────
    bool operator==(const transfer_syntax& other) const noexcept;

    // ─────────────────────────────────────────────────────
    // Validation
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static bool is_valid_uid(std::string_view uid);

private:
    std::string uid_;
    bool implicit_vr_;
    bool little_endian_;
    bool encapsulated_;
};

} // namespace pacs::encoding
```

**Transfer Syntax UIDs:**

| UID | Name | VR | Endian | Encapsulated |
|-----|------|-----|--------|--------------|
| 1.2.840.10008.1.2 | Implicit VR Little Endian | Implicit | Little | No |
| 1.2.840.10008.1.2.1 | Explicit VR Little Endian | Explicit | Little | No |
| 1.2.840.10008.1.2.2 | Explicit VR Big Endian | Explicit | Big | No |
| 1.2.840.10008.1.2.4.50 | JPEG Baseline | Explicit | Little | Yes |
| 1.2.840.10008.1.2.4.90 | JPEG 2000 Lossless | Explicit | Little | Yes |

---

## 3. Network Module Components

### DES-NET-001: pdu_encoder

**Traces to:** SRS-NET-001, FR-2.1

**Purpose:** Serialize PDUs to byte stream.

**Design:**

```cpp
namespace pacs::network::pdu {

class pdu_encoder {
public:
    // ─────────────────────────────────────────────────────
    // PDU Encoding
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static std::vector<uint8_t> encode(
        const associate_rq& pdu);

    [[nodiscard]] static std::vector<uint8_t> encode(
        const associate_ac& pdu);

    [[nodiscard]] static std::vector<uint8_t> encode(
        const associate_rj& pdu);

    [[nodiscard]] static std::vector<uint8_t> encode(
        const p_data_tf& pdu);

    [[nodiscard]] static std::vector<uint8_t> encode(
        const a_release_rq& pdu);

    [[nodiscard]] static std::vector<uint8_t> encode(
        const a_release_rp& pdu);

    [[nodiscard]] static std::vector<uint8_t> encode(
        const a_abort& pdu);

private:
    // Helper methods
    static void write_uint16_be(std::vector<uint8_t>& buf, uint16_t val);
    static void write_uint32_be(std::vector<uint8_t>& buf, uint32_t val);
    static void write_string(std::vector<uint8_t>& buf,
                              std::string_view str, size_t len);
};

} // namespace pacs::network::pdu
```

**PDU Header Format:**

```
┌─────────────────────────────────────────────────────────────────┐
│                       PDU Header (6 bytes)                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌────────────┬────────────┬────────────────────────────────┐   │
│  │  PDU Type  │  Reserved  │        PDU Length              │   │
│  │  (1 byte)  │  (1 byte)  │        (4 bytes, BE)           │   │
│  └────────────┴────────────┴────────────────────────────────┘   │
│                                                                  │
│  PDU Types:                                                      │
│    0x01 = A-ASSOCIATE-RQ                                        │
│    0x02 = A-ASSOCIATE-AC                                        │
│    0x03 = A-ASSOCIATE-RJ                                        │
│    0x04 = P-DATA-TF                                             │
│    0x05 = A-RELEASE-RQ                                          │
│    0x06 = A-RELEASE-RP                                          │
│    0x07 = A-ABORT                                               │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

### DES-NET-004: association

**Traces to:** SRS-NET-003, FR-2.2

**Purpose:** DICOM Association state machine and management.

**Design:**

```cpp
namespace pacs::network {

// Association State (per PS3.8)
enum class association_state : uint8_t {
    sta1_idle = 1,
    sta2_transport_open = 2,
    sta3_awaiting_associate_ac = 3,
    sta4_awaiting_transport = 4,
    sta5_awaiting_associate_rq = 5,
    sta6_established = 6,
    sta7_awaiting_release_rp = 7,
    sta8_awaiting_release_rq = 8,
    sta13_awaiting_close = 13
};

class association {
public:
    // ─────────────────────────────────────────────────────
    // Factory (SCU side)
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static common::Result<association> connect(
        const std::string& host,
        uint16_t port,
        const association_config& config,
        std::chrono::milliseconds timeout = std::chrono::seconds{30});

    // ─────────────────────────────────────────────────────
    // Factory (SCP side - created by dicom_server)
    // ─────────────────────────────────────────────────────
    static association accept(
        network_system::messaging_session_ptr session,
        const scp_config& config);

    // ─────────────────────────────────────────────────────
    // Move-only semantics
    // ─────────────────────────────────────────────────────
    association(const association&) = delete;
    association& operator=(const association&) = delete;
    association(association&&) noexcept;
    association& operator=(association&&) noexcept;
    ~association();

    // ─────────────────────────────────────────────────────
    // State
    // ─────────────────────────────────────────────────────
    [[nodiscard]] association_state state() const noexcept;
    [[nodiscard]] bool is_established() const noexcept;
    [[nodiscard]] bool is_released() const noexcept;
    [[nodiscard]] bool is_aborted() const noexcept;

    // ─────────────────────────────────────────────────────
    // Properties
    // ─────────────────────────────────────────────────────
    [[nodiscard]] const std::string& calling_ae_title() const noexcept;
    [[nodiscard]] const std::string& called_ae_title() const noexcept;
    [[nodiscard]] uint32_t max_pdu_size() const noexcept;
    [[nodiscard]] std::string_view remote_address() const;

    // ─────────────────────────────────────────────────────
    // Presentation Context
    // ─────────────────────────────────────────────────────
    [[nodiscard]] bool has_accepted_context(
        std::string_view abstract_syntax) const;

    [[nodiscard]] std::optional<uint8_t> accepted_context_id(
        std::string_view abstract_syntax) const;

    [[nodiscard]] const transfer_syntax& accepted_transfer_syntax(
        uint8_t context_id) const;

    // ─────────────────────────────────────────────────────
    // DIMSE Operations
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> send_dimse(
        uint8_t context_id,
        const dimse::dimse_message& message);

    [[nodiscard]] common::Result<dimse::dimse_message> receive_dimse(
        std::chrono::milliseconds timeout = std::chrono::seconds{30});

    // ─────────────────────────────────────────────────────
    // Lifecycle
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> release();
    void abort(uint8_t source = 0, uint8_t reason = 0);

private:
    // State machine transitions
    void handle_event(association_event event);
    void transition_to(association_state new_state);

    association_state state_ = association_state::sta1_idle;
    std::string calling_ae_;
    std::string called_ae_;
    uint32_t max_pdu_size_ = 16384;

    std::vector<presentation_context> accepted_contexts_;
    network_system::messaging_session_ptr session_;
};

} // namespace pacs::network
```

**State Machine Diagram:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Association State Machine (PS3.8)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                            SCU Initiated                                     │
│                                                                              │
│  ┌───────┐  A-ASSOCIATE   ┌───────┐  Transport    ┌───────┐                 │
│  │ Sta1  │ ──────────────►│ Sta4  │ ─────────────►│ Sta5  │                 │
│  │ Idle  │  request       │ Await │   connected   │ Await │                 │
│  └───┬───┘                │ Trans │               │ A-AC  │                 │
│      │                    └───────┘               └───┬───┘                 │
│      │                                                │                      │
│      │                    ┌───────────────────────────┘                      │
│      │                    │                                                  │
│      │                    │ A-ASSOCIATE-AC received                          │
│      │                    ▼                                                  │
│      │               ┌───────┐                                               │
│      │               │ Sta6  │◄────────────────────────────┐                │
│      │               │ Assoc │                              │                │
│      │               │ Estab │                              │                │
│      │               └───┬───┘                              │                │
│      │                   │                                  │                │
│      │                   │ A-RELEASE-RQ                     │                │
│      │                   ▼                                  │                │
│      │               ┌───────┐                              │                │
│      │               │ Sta7  │                              │                │
│      │               │ Await │                              │                │
│      │               │ A-RP  │                              │                │
│      │               └───┬───┘                              │                │
│      │                   │                                  │                │
│      │                   │ A-RELEASE-RP received            │                │
│      └───────────────────┴──────────────────────────────────┘                │
│                                                                              │
│                                                                              │
│                            SCP Initiated                                     │
│                                                                              │
│  ┌───────┐  Transport     ┌───────┐  A-ASSOCIATE-RQ  ┌───────┐              │
│  │ Sta1  │ ──────────────►│ Sta2  │ ────────────────►│ Sta3  │              │
│  │ Idle  │   connected    │ Trans │    received      │ Await │              │
│  └───────┘                │ Open  │                  │ A-RQ  │              │
│                           └───────┘                  └───┬───┘              │
│                                                          │                   │
│                                    A-ASSOCIATE-AC sent   │                   │
│                                    ──────────────────────┘                   │
│                                    │                                         │
│                                    ▼                                         │
│                               ┌───────┐                                      │
│                               │ Sta6  │ (Association Established)            │
│                               └───────┘                                      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Services Module Components

### DES-SVC-002: storage_scp

**Traces to:** SRS-SVC-002, FR-3.1

**Purpose:** DICOM Storage SCP (C-STORE receiver).

**Design:**

```cpp
namespace pacs::services {

// Storage result callback
using storage_handler = std::function<storage_status(
    const core::dicom_dataset& dataset,
    const std::string& calling_ae,
    const std::string& sop_class_uid,
    const std::string& sop_instance_uid
)>;

// Storage SCP configuration
struct storage_scp_config {
    std::string ae_title;
    uint16_t port = 11112;

    // Accepted SOP Classes (empty = accept all storage SOP classes)
    std::vector<std::string> accepted_sop_classes;

    // Transfer Syntaxes (empty = accept standard uncompressed)
    std::vector<transfer_syntax> accepted_transfer_syntaxes;

    // Association limits
    size_t max_associations = 10;
    std::chrono::seconds association_timeout{30};
    std::chrono::seconds dimse_timeout{60};

    // PDU settings
    uint32_t max_pdu_size = 16384;

    // Storage backend (optional - for automatic storage)
    storage::storage_interface* storage_backend = nullptr;
};

class storage_scp {
public:
    // ─────────────────────────────────────────────────────
    // Construction
    // ─────────────────────────────────────────────────────
    explicit storage_scp(const storage_scp_config& config);
    ~storage_scp();

    // Non-copyable, non-movable (owns threads)
    storage_scp(const storage_scp&) = delete;
    storage_scp& operator=(const storage_scp&) = delete;

    // ─────────────────────────────────────────────────────
    // Handler Registration
    // ─────────────────────────────────────────────────────
    void set_handler(storage_handler handler);
    void set_pre_store_handler(
        std::function<bool(const core::dicom_dataset&)> handler);

    // ─────────────────────────────────────────────────────
    // Lifecycle
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> start();
    void stop();
    void wait_for_shutdown();

    // ─────────────────────────────────────────────────────
    // Status
    // ─────────────────────────────────────────────────────
    [[nodiscard]] bool is_running() const noexcept;
    [[nodiscard]] size_t active_associations() const noexcept;
    [[nodiscard]] uint64_t total_stored() const noexcept;

    // ─────────────────────────────────────────────────────
    // Statistics
    // ─────────────────────────────────────────────────────
    struct statistics {
        uint64_t images_received;
        uint64_t images_stored;
        uint64_t images_failed;
        uint64_t bytes_received;
        std::chrono::steady_clock::time_point start_time;
    };

    [[nodiscard]] statistics get_statistics() const;

private:
    void handle_association(association assoc);
    void handle_c_store(association& assoc, const dimse::c_store_rq& request);

    storage_scp_config config_;
    storage_handler handler_;
    std::unique_ptr<network::dicom_server> server_;
    std::atomic<bool> running_{false};
    std::atomic<size_t> active_associations_{0};
    mutable std::mutex stats_mutex_;
    statistics stats_;
};

} // namespace pacs::services
```

---

### DES-SVC-004: query_scp

**Traces to:** SRS-SVC-004, FR-3.2

**Purpose:** DICOM Query SCP (C-FIND handler).

**Design:**

```cpp
namespace pacs::services {

// Query level
enum class query_level {
    patient,
    study,
    series,
    image
};

// Query handler returns matching datasets
using query_handler = std::function<std::vector<core::dicom_dataset>(
    query_level level,
    const core::dicom_dataset& query_keys,
    const std::string& calling_ae
)>;

// Query SCP configuration
struct query_scp_config {
    std::string ae_title;
    uint16_t port = 11112;

    // Query/Retrieve Information Models
    bool support_patient_root = true;
    bool support_study_root = true;

    // Limits
    size_t max_matches = 1000;
    std::chrono::seconds query_timeout{60};

    // Storage index (for automatic query handling)
    storage::index_database* index_db = nullptr;
};

class query_scp {
public:
    explicit query_scp(const query_scp_config& config);
    ~query_scp();

    // ─────────────────────────────────────────────────────
    // Handler Registration
    // ─────────────────────────────────────────────────────
    void set_handler(query_handler handler);

    // ─────────────────────────────────────────────────────
    // Lifecycle
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> start();
    void stop();

    [[nodiscard]] bool is_running() const noexcept;

private:
    void handle_c_find(association& assoc, const dimse::c_find_rq& request);

    query_level parse_query_level(const core::dicom_dataset& ds) const;
    std::vector<core::dicom_dataset> execute_query(
        query_level level,
        const core::dicom_dataset& keys) const;

    query_scp_config config_;
    query_handler handler_;
    std::unique_ptr<network::dicom_server> server_;
};

} // namespace pacs::services
```

---

## 5. Storage Module Components

### DES-STOR-002: file_storage

**Traces to:** SRS-STOR-002, FR-4.1

**Purpose:** Filesystem-based DICOM storage.

**Design:**

```cpp
namespace pacs::storage {

// File naming scheme
enum class naming_scheme {
    // {root}/{StudyUID}/{SeriesUID}/{SOPUID}.dcm
    uid_hierarchical,

    // {root}/{YYYY}/{MM}/{DD}/{SOPUID}.dcm
    date_hierarchical,

    // {root}/{SOPUID}.dcm
    flat
};

// File storage configuration
struct file_storage_config {
    std::filesystem::path root_path;
    naming_scheme scheme = naming_scheme::uid_hierarchical;

    bool create_directories = true;
    bool overwrite_duplicates = false;

    // Permissions
    std::filesystem::perms file_perms =
        std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write;

    std::filesystem::perms dir_perms =
        std::filesystem::perms::owner_all;
};

class file_storage : public storage_interface {
public:
    explicit file_storage(const file_storage_config& config);
    ~file_storage() override = default;

    // ─────────────────────────────────────────────────────
    // storage_interface Implementation
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> store(
        const core::dicom_dataset& dataset) override;

    [[nodiscard]] common::Result<core::dicom_dataset> retrieve(
        const std::string& sop_instance_uid) override;

    [[nodiscard]] common::Result<void> remove(
        const std::string& sop_instance_uid) override;

    [[nodiscard]] bool exists(
        const std::string& sop_instance_uid) override;

    // ─────────────────────────────────────────────────────
    // File Path Resolution
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::filesystem::path resolve_path(
        const core::dicom_dataset& dataset) const;

    [[nodiscard]] std::filesystem::path resolve_path(
        const std::string& sop_instance_uid) const;

    // ─────────────────────────────────────────────────────
    // Bulk Operations
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> store_file(
        const std::filesystem::path& source_path);

    [[nodiscard]] std::vector<std::filesystem::path> list_files() const;

private:
    std::filesystem::path build_hierarchical_path(
        const core::dicom_dataset& ds) const;
    std::filesystem::path build_date_path(
        const core::dicom_dataset& ds) const;
    std::filesystem::path build_flat_path(
        const core::dicom_dataset& ds) const;

    file_storage_config config_;
    mutable std::shared_mutex path_cache_mutex_;
    std::unordered_map<std::string, std::filesystem::path> path_cache_;
};

} // namespace pacs::storage
```

**Directory Structure (uid_hierarchical):**

```
{root}/
├── 1.2.840.113619.2.55.3.604688.../     # Study Instance UID
│   ├── 1.2.840.113619.2.55.3.60468.../  # Series Instance UID
│   │   ├── 1.2.840.113619.2.55.3.6...001.dcm  # SOP Instance UID
│   │   ├── 1.2.840.113619.2.55.3.6...002.dcm
│   │   └── 1.2.840.113619.2.55.3.6...003.dcm
│   │
│   └── 1.2.840.113619.2.55.3.60469.../  # Another Series
│       └── ...
│
└── 1.2.840.113619.2.55.3.604689.../     # Another Study
    └── ...
```

---

### DES-STOR-003: index_database

**Traces to:** SRS-STOR-003, FR-4.2

**Purpose:** SQLite-based index for fast DICOM queries.

**Design:**

```cpp
namespace pacs::storage {

// Index database configuration
struct index_database_config {
    std::filesystem::path db_path;

    bool create_if_not_exists = true;
    bool enable_wal_mode = true;
    size_t cache_size_mb = 64;

    // Connection pooling
    size_t pool_size = 4;
};

class index_database {
public:
    explicit index_database(const index_database_config& config);
    ~index_database();

    // ─────────────────────────────────────────────────────
    // Indexing Operations
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> index(
        const core::dicom_dataset& dataset,
        const std::filesystem::path& file_path);

    [[nodiscard]] common::Result<void> remove(
        const std::string& sop_instance_uid);

    [[nodiscard]] bool exists(
        const std::string& sop_instance_uid) const;

    // ─────────────────────────────────────────────────────
    // Query Operations
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::vector<core::dicom_dataset> find_patients(
        const core::dicom_dataset& query_keys) const;

    [[nodiscard]] std::vector<core::dicom_dataset> find_studies(
        const core::dicom_dataset& query_keys) const;

    [[nodiscard]] std::vector<core::dicom_dataset> find_series(
        const core::dicom_dataset& query_keys) const;

    [[nodiscard]] std::vector<core::dicom_dataset> find_instances(
        const core::dicom_dataset& query_keys) const;

    // ─────────────────────────────────────────────────────
    // Path Lookup
    // ─────────────────────────────────────────────────────
    [[nodiscard]] std::optional<std::filesystem::path> get_file_path(
        const std::string& sop_instance_uid) const;

    [[nodiscard]] std::vector<std::filesystem::path> get_series_paths(
        const std::string& series_instance_uid) const;

    [[nodiscard]] std::vector<std::filesystem::path> get_study_paths(
        const std::string& study_instance_uid) const;

    // ─────────────────────────────────────────────────────
    // Statistics
    // ─────────────────────────────────────────────────────
    struct statistics {
        size_t patient_count;
        size_t study_count;
        size_t series_count;
        size_t instance_count;
        uint64_t total_size_bytes;
    };

    [[nodiscard]] statistics get_statistics() const;

    // ─────────────────────────────────────────────────────
    // Maintenance
    // ─────────────────────────────────────────────────────
    [[nodiscard]] common::Result<void> vacuum();
    [[nodiscard]] common::Result<void> reindex();
    [[nodiscard]] common::Result<void> verify_integrity() const;

private:
    void create_schema();
    std::string build_where_clause(
        const core::dicom_dataset& keys) const;

    index_database_config config_;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db_;
    mutable std::shared_mutex db_mutex_;
};

} // namespace pacs::storage
```

---

## 6. Integration Module Components

### DES-INT-001: container_adapter

**Traces to:** IR-1 (container_system Integration)

**Purpose:** Map DICOM VR types to container_system values.

**Design:**

```cpp
namespace pacs::integration {

class container_adapter {
public:
    // ─────────────────────────────────────────────────────
    // VR to Container Value Mapping
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static container::value to_container_value(
        const core::dicom_element& element);

    [[nodiscard]] static core::dicom_element from_container_value(
        core::dicom_tag tag,
        encoding::vr_type vr,
        const container::value& val);

    // ─────────────────────────────────────────────────────
    // Dataset Serialization
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static container::value_container serialize_dataset(
        const core::dicom_dataset& dataset);

    [[nodiscard]] static common::Result<core::dicom_dataset>
        deserialize_dataset(const container::value_container& container);

    // ─────────────────────────────────────────────────────
    // Binary Serialization
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static std::vector<uint8_t> to_binary(
        const core::dicom_dataset& dataset);

    [[nodiscard]] static common::Result<core::dicom_dataset>
        from_binary(std::span<const uint8_t> data);
};

} // namespace pacs::integration
```

**VR to Container Type Mapping:**

| VR Category | DICOM VRs | container::value Type |
|-------------|-----------|----------------------|
| String | AE, AS, CS, DA, DS, DT, IS, LO, LT, PN, SH, ST, TM, UI, UT | `std::string` |
| Integer | SS, US, SL, UL | `int64_t` |
| Float | FL, FD, DS | `double` |
| Binary | OB, OW, OF, OD, OL, UN | `std::vector<uint8_t>` |
| Sequence | SQ | `std::vector<value_container>` |

---

### DES-INT-002: network_adapter

**Traces to:** IR-2 (network_system Integration)

**Purpose:** Adapt network_system for DICOM protocol.

**Design:**

```cpp
namespace pacs::integration {

class network_adapter {
public:
    // ─────────────────────────────────────────────────────
    // Server Creation
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static std::unique_ptr<network::dicom_server>
        create_server(const network::server_config& config);

    // ─────────────────────────────────────────────────────
    // Client Connection
    // ─────────────────────────────────────────────────────
    [[nodiscard]] static common::Result<
        network_system::messaging_session_ptr> connect(
            const std::string& host,
            uint16_t port,
            std::chrono::milliseconds timeout);

    // ─────────────────────────────────────────────────────
    // TLS Configuration
    // ─────────────────────────────────────────────────────
    static void configure_tls(
        network_system::tls_config& config,
        const std::filesystem::path& cert_path,
        const std::filesystem::path& key_path,
        const std::filesystem::path& ca_path);

    // ─────────────────────────────────────────────────────
    // Session Wrapper
    // ─────────────────────────────────────────────────────
    class dicom_session {
    public:
        explicit dicom_session(
            network_system::messaging_session_ptr session);

        [[nodiscard]] common::Result<void> send_pdu(
            const network::pdu::pdu_base& pdu);

        [[nodiscard]] common::Result<
            std::unique_ptr<network::pdu::pdu_base>> receive_pdu(
                std::chrono::milliseconds timeout);

        void close();
        [[nodiscard]] bool is_open() const noexcept;

    private:
        network_system::messaging_session_ptr session_;
        std::vector<uint8_t> receive_buffer_;
    };
};

} // namespace pacs::integration
```

---

*Document Version: 1.0.0*
*Created: 2025-11-30*
*Author: kcenon@naver.com*
