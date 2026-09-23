# Common Utilities

Shared utilities library for all developer samples. These components handle cross-cutting concerns like signal handling, console output, and test data generation.

## Overview

| Component | Purpose | Header |
|-----------|---------|--------|
| Signal Handler | Cross-platform graceful shutdown | `signal_handler.hpp` |
| Console Utils | Formatted output with colors | `console_utils.hpp` |
| Test Data Generator | Generate test DICOM datasets | `test_data_generator.hpp` |

## Signal Handler

Cross-platform signal handling for graceful server shutdown. Supports SIGINT (Ctrl+C) and SIGTERM on POSIX systems, SIGINT/SIGBREAK on Windows.

### Quick Usage

```cpp
#include "signal_handler.hpp"

int main() {
    // Install signal handlers
    pacs::samples::signal_handler::install([]() {
        std::cout << "Shutdown requested\n";
    });

    // Start your server...

    // Wait for shutdown signal
    pacs::samples::signal_handler::wait_for_shutdown();

    // Cleanup...
    return 0;
}
```

### RAII Wrapper (Recommended)

```cpp
#include "signal_handler.hpp"

int main() {
    pacs::samples::scoped_signal_handler signals([]() {
        std::cout << "Shutting down...\n";
    });

    // Start your server...

    signals.wait();  // Blocks until Ctrl+C

    // Cleanup happens automatically
    return 0;
}
```

### API Reference

#### `signal_handler` (Static Interface)

| Method | Description |
|--------|-------------|
| `install(callback)` | Install signal handlers with optional callback |
| `should_shutdown()` | Check if shutdown was requested |
| `wait_for_shutdown()` | Block until shutdown signal received |
| `request_shutdown()` | Trigger shutdown programmatically |
| `reset()` | Reset shutdown state (for tests) |

#### `scoped_signal_handler` (RAII)

| Method | Description |
|--------|-------------|
| Constructor | Installs handlers automatically |
| Destructor | Resets handler state |
| `wait()` | Block until shutdown |
| `should_shutdown()` | Check shutdown state |

## Console Utils

Formatted console output with ANSI color support and DICOM-specific formatting.

### Message Printing

```cpp
#include "console_utils.hpp"

using namespace pacs::samples;

// Headers and sections
print_header("My Application");       // Decorative box header
print_section("Configuration");       // Section divider

// Status messages
print_success("Operation completed"); // Green checkmark
print_error("Operation failed");      // Red X
print_warning("Check configuration"); // Yellow warning
print_info("Processing...");          // Cyan info
print_debug("Debug details");         // Dim/gray
```

### DICOM Data Display

```cpp
#include "console_utils.hpp"

core::dicom_dataset ds;
// ... populate dataset ...

// Summary view (key patient/study info)
pacs::samples::print_dataset_summary(ds);

// All elements
pacs::samples::print_dataset_elements(ds);
pacs::samples::print_dataset_elements(ds, 40);  // Max value length 40
```

### Box Drawing

```cpp
// Simple box
print_box({
    "Line 1",
    "Line 2",
    "Line 3"
});

// Key-value table
print_table("Patient Info", {
    {"Name", "DOE^JOHN"},
    {"ID", "PAT001"},
    {"Sex", "M"}
});
```

### Progress Display

```cpp
// Progress bar (useful for batch operations)
for (size_t i = 0; i <= total; ++i) {
    print_progress(i, total, 40, "Processing");
}
```

### Formatting Utilities

```cpp
// Byte size formatting
format_bytes(1536);         // "1.5 KB"
format_bytes(2147483648);   // "2.0 GB"

// Duration formatting
format_duration(1500);      // "1.5s"
format_duration(250);       // "250ms"

// Text utilities
center_text("Title", 40);   // Centers text in 40 chars
truncate("Long text...", 10); // "Long te..."
```

### Color Configuration

```cpp
// Check color support
if (supports_color()) {
    set_color_enabled(true);
}

// Disable colors (for file output)
set_color_enabled(false);

// Check current state
bool enabled = is_color_enabled();
```

### Available Colors

```cpp
namespace colors {
    reset, bold, dim
    black, red, green, yellow, blue, magenta, cyan, white
    bright_red, bright_green, bright_yellow, bright_blue
    bright_magenta, bright_cyan, bright_white
    bg_red, bg_green, bg_yellow, bg_blue
}
```

## Test Data Generator

Generate realistic test DICOM datasets for various modalities.

### Single Dataset Generation

```cpp
#include "test_data_generator.hpp"

using namespace pacs::samples;

// Default CT dataset
auto ct_ds = test_data_generator::create_ct_dataset();

// Custom patient
patient_info patient{"PAT123", "SMITH^JANE", "19900515", "F"};
auto ds = test_data_generator::create_ct_dataset(patient);

// Different modalities
auto mr_ds = test_data_generator::create_mr_dataset(patient);
auto cr_ds = test_data_generator::create_cr_dataset(patient);
auto dx_ds = test_data_generator::create_dx_dataset(patient);
```

### Study Generation

```cpp
// Create patient and study info
patient_info patient{"PAT001", "DOE^JOHN", "19800101", "M"};
study_info study;
study.study_uid = test_data_generator::generate_uid();
study.study_date = test_data_generator::current_date();
study.study_time = test_data_generator::current_time();
study.description = "CT CHEST W/CONTRAST";

// Generate study with 2 series, 10 instances each
auto datasets = test_data_generator::create_study(
    patient, study, 2, 10);  // 20 total instances
```

### Save to Files

```cpp
// Save datasets to directory structure
// Creates: directory/PatientID/StudyUID/SeriesUID/InstanceUID.dcm
test_data_generator::save_to_directory(datasets, "./test_data");
```

### UID Generation

```cpp
// Generate unique DICOM UIDs
auto uid = test_data_generator::generate_uid();
// e.g., "1.2.410.200001.1.1.1705912345.1"

// Custom root
auto uid = test_data_generator::generate_uid("1.2.840.123456");
```

### Date/Time Utilities

```cpp
// Current date in DICOM format (YYYYMMDD)
std::string date = test_data_generator::current_date();
// e.g., "20240122"

// Current time in DICOM format (HHMMSS.FFFFFF)
std::string time = test_data_generator::current_time();
// e.g., "143022.123456"
```

### Data Structures

#### `patient_info`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `patient_id` | string | "PAT001" | Patient identifier |
| `patient_name` | string | "DOE^JOHN" | Patient name (PN format) |
| `birth_date` | string | "19800101" | Birth date (YYYYMMDD) |
| `sex` | string | "M" | Patient sex (M/F/O) |

#### `study_info`

| Field | Type | Description |
|-------|------|-------------|
| `study_uid` | string | Study Instance UID |
| `study_date` | string | Study date (YYYYMMDD) |
| `study_time` | string | Study time (HHMMSS) |
| `accession_number` | string | Accession number |
| `description` | string | Study description |
| `referring_physician` | string | Referring physician name |

#### `image_params`

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `rows` | uint16 | 512 | Image rows |
| `columns` | uint16 | 512 | Image columns |
| `bits_allocated` | uint16 | 16 | Bits per pixel |
| `bits_stored` | uint16 | 12 | Significant bits |
| `high_bit` | uint16 | 11 | High bit position |
| `pixel_representation` | uint16 | 0 | 0=unsigned, 1=signed |
| `modality` | string | "CT" | Modality code |
| `photometric` | string | "MONOCHROME2" | Photometric interpretation |
| `window_center` | double | 40.0 | Window center (HU for CT) |
| `window_width` | double | 400.0 | Window width |

## Build Configuration

The common library is built as a static library and linked to all samples:

```cmake
# In samples/common/CMakeLists.txt
add_library(pacs_samples_common STATIC
    signal_handler.cpp
    console_utils.cpp
    test_data_generator.cpp
)

target_link_libraries(pacs_samples_common PUBLIC
    pacs::core
    pacs::encoding
)
```

To use in your sample:

```cmake
target_link_libraries(my_sample PRIVATE
    pacs_samples_common
)
```

## Thread Safety

| Component | Thread Safety |
|-----------|---------------|
| `signal_handler` | Fully thread-safe (atomic operations) |
| `console_utils` | Single-threaded (console output only) |
| `test_data_generator` | UID generation is thread-safe |

## Platform Support

| Platform | Signal Handler | Console Colors |
|----------|----------------|----------------|
| Linux | SIGINT, SIGTERM | Full support |
| macOS | SIGINT, SIGTERM | Full support |
| Windows | SIGINT, SIGBREAK | Windows Terminal (10+) |

Note: Older Windows cmd.exe may not display ANSI colors correctly. Colors are automatically disabled when output is redirected to a file.
