# Level 1: Hello DICOM

An introductory sample demonstrating fundamental DICOM concepts. This is the entry point for developers new to DICOM and pacs_system.

## Learning Objectives

By completing this sample, you will understand:

- **DICOM Tags** - (Group, Element) structure and standard tag constants
- **Value Representations (VR)** - Data types in DICOM (PN, DA, UI, US, etc.)
- **DICOM Dataset** - Collection of elements with get/set operations
- **DICOM File** - Part 10 format, file meta information, and transfer syntax

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- pacs_system library built with samples enabled

## Build & Run

```bash
# Configure with samples enabled
cmake -B build -DPACS_BUILD_SAMPLES=ON

# Build this sample
cmake --build build --target sample_01_hello_dicom

# Run the sample
./build/samples/hello_dicom
```

## Sample Output

The sample will:

1. Demonstrate DICOM tag creation and parsing
2. Build a dataset with patient and study information
3. Save the dataset as a DICOM Part 10 file (`hello_dicom_output.dcm`)
4. Read the file back and display all elements

## Verify Output

You can verify the output file using DCMTK:

```bash
dcmdump hello_dicom_output.dcm
```

Expected output will show all DICOM elements including:
- Patient Name: DOE^JOHN^M
- Patient ID: PAT001
- Study Date: 20240115
- Modality: CT
- Image dimensions: 512x512

## Key Concepts Explained

### DICOM Tags

Tags are (Group, Element) pairs that identify data elements:

```cpp
// Manual tag creation
core::dicom_tag tag{0x0010, 0x0010};  // Patient Name

// Using predefined constants
auto tag = tags::patient_name;

// Parse from string
auto tag = core::dicom_tag::from_string("(0010,0010)");
```

### Value Representations (VR)

VR defines the data type of an element:

| VR | Name | Description |
|----|------|-------------|
| PN | Person Name | Family^Given^Middle format |
| DA | Date | YYYYMMDD format |
| TM | Time | HHMMSS format |
| UI | Unique ID | UID string (max 64 chars) |
| US | Unsigned Short | 16-bit integer |
| LO | Long String | Text up to 64 chars |

### Dataset Operations

```cpp
core::dicom_dataset dataset;

// Set string values
dataset.set_string(tags::patient_name, enc::vr_type::PN, "DOE^JOHN");

// Set numeric values
dataset.set_numeric<uint16_t>(tags::rows, enc::vr_type::US, 512);

// Get values
std::string name = dataset.get_string(tags::patient_name);
auto rows = dataset.get_numeric<uint16_t>(tags::rows);
```

### File I/O

```cpp
// Create and save
auto file = core::dicom_file::create(dataset,
    enc::transfer_syntax::explicit_vr_little_endian);
file.save("output.dcm");

// Read
auto result = core::dicom_file::open("output.dcm");
if (result.is_ok()) {
    auto& loaded_file = result.value();
    const auto& ds = loaded_file.dataset();
}
```

## Next Steps

Proceed to **Level 2: Echo Server** to learn DICOM networking concepts including:
- DICOM Message Service Element (DIMSE)
- Association negotiation
- C-ECHO verification service

## Related Documentation

- [DICOM PS3.5 - Data Structures](http://dicom.nema.org/medical/dicom/current/output/chtml/part05/PS3.5.html)
- [DICOM PS3.6 - Data Dictionary](http://dicom.nema.org/medical/dicom/current/output/chtml/part06/PS3.6.html)
- [DICOM PS3.10 - Media Storage](http://dicom.nema.org/medical/dicom/current/output/chtml/part10/PS3.10.html)
