---
doc_id: "PAC-API-002-SERVICES"
doc_title: "API Reference - Services, Storage & Modality Modules"
doc_version: "0.1.5.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "pacs_system"
category: "API"
---

# API Reference - Services, Storage & Modality Modules

> **Part of**: [API Reference Index](API_REFERENCE.md)
> **Covers**: SCP/SCU implementations (Verification, Storage, Query, Worklist, Retrieve, Print), MPPS, caching, storage backends, DX and MG modality IOD validators

> **Version:** 0.1.5.0
> **Last Updated:** 2025-12-13
> **Language:** **English**

---

## Table of Contents

- [Services Module](#services-module)
- [Storage Module](#storage-module)
- [DX Modality Module](#dx-modality-module)
- [MG Modality Module](#mg-modality-module)

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

### `pacs::services::query_scu`

Query SCU (C-FIND sender) for querying remote PACS servers.

```cpp
#include <pacs/services/query_scu.hpp>

namespace pacs::services {

// Query information model
enum class query_model {
    patient_root,  // Patient Root Q/R Information Model
    study_root     // Study Root Q/R Information Model
};

// Query result structure
struct query_result {
    std::vector<core::dicom_dataset> matches;
    uint16_t status;
    std::chrono::milliseconds elapsed;
    size_t total_pending;

    [[nodiscard]] bool is_success() const noexcept;
    [[nodiscard]] bool is_cancelled() const noexcept;
};

// Typed query key structures
struct patient_query_keys {
    std::string patient_name;
    std::string patient_id;
    std::string birth_date;
    std::string sex;
};

struct study_query_keys {
    std::string patient_id;
    std::string study_uid;
    std::string study_date;      // YYYYMMDD or range
    std::string accession_number;
    std::string modality;
    std::string study_description;
};

struct series_query_keys {
    std::string study_uid;       // Required
    std::string series_uid;
    std::string modality;
    std::string series_number;
};

struct instance_query_keys {
    std::string series_uid;      // Required
    std::string sop_instance_uid;
    std::string instance_number;
};

// Configuration
struct query_scu_config {
    query_model model{query_model::study_root};
    query_level level{query_level::study};
    std::chrono::milliseconds timeout{30000};
    size_t max_results{0};       // 0 = unlimited
    bool cancel_on_max{true};
};

class query_scu {
public:
    explicit query_scu(std::shared_ptr<di::ILogger> logger = nullptr);
    explicit query_scu(const query_scu_config& config,
                       std::shared_ptr<di::ILogger> logger = nullptr);

    // Generic query with raw dataset
    [[nodiscard]] network::Result<query_result> find(
        network::association& assoc,
        const core::dicom_dataset& query_keys);

    // Typed convenience methods
    [[nodiscard]] network::Result<query_result> find_patients(
        network::association& assoc,
        const patient_query_keys& keys);

    [[nodiscard]] network::Result<query_result> find_studies(
        network::association& assoc,
        const study_query_keys& keys);

    [[nodiscard]] network::Result<query_result> find_series(
        network::association& assoc,
        const series_query_keys& keys);

    [[nodiscard]] network::Result<query_result> find_instances(
        network::association& assoc,
        const instance_query_keys& keys);

    // Streaming callback for large result sets
    [[nodiscard]] network::Result<size_t> find_streaming(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        std::function<bool(const core::dicom_dataset&)> callback);

    // Cancel ongoing query
    network::Result<std::monostate> cancel(
        network::association& assoc,
        uint16_t message_id);

    // Configuration
    void set_config(const query_scu_config& config);
    [[nodiscard]] const query_scu_config& config() const noexcept;

    // Statistics
    [[nodiscard]] size_t queries_performed() const noexcept;
    [[nodiscard]] size_t total_matches() const noexcept;
    void reset_statistics() noexcept;
};

} // namespace pacs::services
```

**Usage Example:**

```cpp
// Establish association
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.proposed_contexts.push_back({
    1,
    std::string(study_root_find_sop_class_uid),
    {"1.2.840.10008.1.2.1"}
});

auto assoc_result = association::connect("pacs.example.com", 104, config);
auto& assoc = assoc_result.value();

// Query for studies
query_scu scu;
study_query_keys keys;
keys.patient_id = "12345";
keys.study_date = "20240101-20241231";

auto result = scu.find_studies(assoc, keys);
if (result.is_ok() && result.value().is_success()) {
    for (const auto& ds : result.value().matches) {
        auto study_uid = ds.get_string(tags::study_instance_uid);
        // Process study...
    }
}

assoc.release();
```

---

### `pacs::services::worklist_scu`

Worklist SCU (MWL C-FIND sender) for querying Modality Worklist from RIS/HIS systems.

```cpp
#include <pacs/services/worklist_scu.hpp>

namespace pacs::services {

// Query keys for worklist queries
struct worklist_query_keys {
    // Scheduled Procedure Step Attributes
    std::string scheduled_station_ae;      // (0040,0001)
    std::string modality;                  // (0008,0060)
    std::string scheduled_date;            // (0040,0002) YYYYMMDD or range
    std::string scheduled_time;            // (0040,0003)
    std::string scheduled_physician;       // (0040,0006)
    std::string scheduled_procedure_step_id;  // (0040,0009)

    // Requested Procedure Attributes
    std::string requested_procedure_id;
    std::string requested_procedure_description;

    // Patient Attributes
    std::string patient_name;              // Supports wildcards
    std::string patient_id;
    std::string patient_birth_date;
    std::string patient_sex;

    // Visit Attributes
    std::string accession_number;
    std::string referring_physician;
    std::string institution;
};

// Parsed worklist item from query response
struct worklist_item {
    std::string patient_name;
    std::string patient_id;
    std::string patient_birth_date;
    std::string patient_sex;
    std::string scheduled_station_ae;
    std::string modality;
    std::string scheduled_date;
    std::string scheduled_time;
    std::string scheduled_procedure_step_id;
    std::string scheduled_procedure_step_description;
    std::string study_instance_uid;
    std::string accession_number;
    std::string requested_procedure_id;
    std::string referring_physician;
    std::string institution;
    core::dicom_dataset dataset;           // Original dataset
};

// Query result
struct worklist_result {
    std::vector<worklist_item> items;
    uint16_t status;
    std::chrono::milliseconds elapsed;
    size_t total_pending;

    [[nodiscard]] bool is_success() const noexcept;
    [[nodiscard]] bool is_cancelled() const noexcept;
};

// Configuration
struct worklist_scu_config {
    std::chrono::milliseconds timeout{30000};
    size_t max_results{0};       // 0 = unlimited
    bool cancel_on_max{true};
};

class worklist_scu {
public:
    explicit worklist_scu(std::shared_ptr<di::ILogger> logger = nullptr);
    explicit worklist_scu(const worklist_scu_config& config,
                          std::shared_ptr<di::ILogger> logger = nullptr);

    // Query with typed keys
    [[nodiscard]] network::Result<worklist_result> query(
        network::association& assoc,
        const worklist_query_keys& keys);

    // Query with raw dataset
    [[nodiscard]] network::Result<worklist_result> query(
        network::association& assoc,
        const core::dicom_dataset& query_keys);

    // Convenience methods
    [[nodiscard]] network::Result<worklist_result> query_today(
        network::association& assoc,
        std::string_view station_ae,
        std::string_view modality = "");

    [[nodiscard]] network::Result<worklist_result> query_date_range(
        network::association& assoc,
        std::string_view start_date,
        std::string_view end_date,
        std::string_view modality = "");

    [[nodiscard]] network::Result<worklist_result> query_patient(
        network::association& assoc,
        std::string_view patient_id);

    // Streaming for large worklists
    [[nodiscard]] network::Result<size_t> query_streaming(
        network::association& assoc,
        const worklist_query_keys& keys,
        std::function<bool(const worklist_item&)> callback);

    // Cancel ongoing query
    network::Result<std::monostate> cancel(
        network::association& assoc,
        uint16_t message_id);

    // Configuration
    void set_config(const worklist_scu_config& config);
    [[nodiscard]] const worklist_scu_config& config() const noexcept;

    // Statistics
    [[nodiscard]] size_t queries_performed() const noexcept;
    [[nodiscard]] size_t total_items() const noexcept;
    void reset_statistics() noexcept;
};

} // namespace pacs::services
```

**Usage Example:**

```cpp
// Establish association with MWL SOP Class
association_config config;
config.calling_ae_title = "CT_SCANNER_01";
config.called_ae_title = "RIS_SCP";
config.proposed_contexts.push_back({
    1,
    std::string(worklist_find_sop_class_uid),
    {"1.2.840.10008.1.2.1"}
});

auto assoc_result = association::connect("ris.example.com", 104, config);
auto& assoc = assoc_result.value();

// Query today's worklist for CT modality
worklist_scu scu;
auto result = scu.query_today(assoc, "CT_SCANNER_01", "CT");

if (result.is_ok() && result.value().is_success()) {
    for (const auto& item : result.value().items) {
        std::cout << "Patient: " << item.patient_name
                  << " (" << item.patient_id << ")\n";
        std::cout << "Scheduled: " << item.scheduled_date
                  << " " << item.scheduled_time << "\n";
        std::cout << "Accession: " << item.accession_number << "\n\n";
    }
}

assoc.release();
```

---

### `pacs::services::retrieve_scu`

Retrieve SCU (C-MOVE/C-GET sender) for retrieving images from remote PACS servers.

```cpp
#include <pacs/services/retrieve_scu.hpp>

namespace pacs::services {

// Retrieve mode
enum class retrieve_mode {
    c_move,  // Request SCP to send to specified destination
    c_get    // Receive directly on same association
};

// Progress information
struct retrieve_progress {
    uint16_t remaining;
    uint16_t completed;
    uint16_t failed;
    uint16_t warning;
    std::chrono::steady_clock::time_point start_time;

    [[nodiscard]] uint16_t total() const noexcept;
    [[nodiscard]] float percent() const noexcept;
    [[nodiscard]] std::chrono::milliseconds elapsed() const noexcept;
};

// Retrieve result
struct retrieve_result {
    uint16_t completed;
    uint16_t failed;
    uint16_t warning;
    uint16_t final_status;
    std::chrono::milliseconds elapsed;
    std::vector<core::dicom_dataset> received_instances;  // C-GET only

    [[nodiscard]] bool is_success() const noexcept;
    [[nodiscard]] bool is_cancelled() const noexcept;
    [[nodiscard]] bool has_failures() const noexcept;
    [[nodiscard]] bool has_warnings() const noexcept;
};

// Configuration
struct retrieve_scu_config {
    retrieve_mode mode{retrieve_mode::c_move};
    query_model model{query_model::study_root};
    query_level level{query_level::study};
    std::string move_destination;     // Required for C-MOVE
    std::chrono::milliseconds timeout{120000};
    uint16_t priority{0};
};

class retrieve_scu {
public:
    explicit retrieve_scu(std::shared_ptr<di::ILogger> logger = nullptr);
    explicit retrieve_scu(const retrieve_scu_config& config,
                          std::shared_ptr<di::ILogger> logger = nullptr);

    // C-MOVE operation
    [[nodiscard]] network::Result<retrieve_result> move(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        std::string_view destination_ae,
        std::function<void(const retrieve_progress&)> progress = nullptr);

    // C-GET operation
    [[nodiscard]] network::Result<retrieve_result> get(
        network::association& assoc,
        const core::dicom_dataset& query_keys,
        std::function<void(const retrieve_progress&)> progress = nullptr);

    // Convenience methods (use mode from config)
    [[nodiscard]] network::Result<retrieve_result> retrieve_study(
        network::association& assoc,
        std::string_view study_uid,
        std::function<void(const retrieve_progress&)> progress = nullptr);

    [[nodiscard]] network::Result<retrieve_result> retrieve_series(
        network::association& assoc,
        std::string_view series_uid,
        std::function<void(const retrieve_progress&)> progress = nullptr);

    [[nodiscard]] network::Result<retrieve_result> retrieve_instance(
        network::association& assoc,
        std::string_view sop_instance_uid,
        std::function<void(const retrieve_progress&)> progress = nullptr);

    // Cancel ongoing retrieve
    [[nodiscard]] network::Result<std::monostate> cancel(
        network::association& assoc,
        uint16_t message_id);

    // Configuration
    void set_config(const retrieve_scu_config& config);
    void set_move_destination(std::string_view ae_title);
    [[nodiscard]] const retrieve_scu_config& config() const noexcept;

    // Statistics
    [[nodiscard]] size_t retrieves_performed() const noexcept;
    [[nodiscard]] size_t instances_retrieved() const noexcept;
    [[nodiscard]] size_t bytes_retrieved() const noexcept;
    void reset_statistics() noexcept;
};

} // namespace pacs::services
```

**C-MOVE Usage Example:**

```cpp
// Establish association
association_config config;
config.calling_ae_title = "MY_SCU";
config.called_ae_title = "PACS_SCP";
config.proposed_contexts.push_back({
    1,
    std::string(study_root_move_sop_class_uid),
    {"1.2.840.10008.1.2.1"}
});

auto assoc_result = association::connect("pacs.example.com", 104, config);
auto& assoc = assoc_result.value();

// Move a study to workstation
retrieve_scu scu;
auto result = scu.move(assoc, query_ds, "WORKSTATION",
    [](const retrieve_progress& p) {
        std::cout << "Progress: " << p.percent() << "%\n";
    });

if (result.is_ok() && result.value().is_success()) {
    std::cout << "Retrieved " << result.value().completed << " images\n";
}

assoc.release();
```

**C-GET Usage Example:**

```cpp
retrieve_scu_config cfg;
cfg.mode = retrieve_mode::c_get;
retrieve_scu scu(cfg);

auto result = scu.retrieve_series(assoc, "1.2.3.4.5.6.7");
if (result.is_ok()) {
    for (const auto& ds : result.value().received_instances) {
        // Process received dataset
    }
}
```

---

### `pacs::services::sop_classes` - XA/XRF Storage

X-Ray Angiographic and Radiofluoroscopic Storage SOP Classes.

```cpp
#include <pacs/services/sop_classes/xa_storage.hpp>

namespace pacs::services::sop_classes {

// SOP Class UIDs
inline constexpr std::string_view xa_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.1";
inline constexpr std::string_view enhanced_xa_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.1.1";
inline constexpr std::string_view xrf_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.12.2";
inline constexpr std::string_view xray_3d_angiographic_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.1";
inline constexpr std::string_view xray_3d_craniofacial_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.2";

// Photometric Interpretation (grayscale only for XA)
enum class xa_photometric_interpretation {
    monochrome1,  // Minimum value = white
    monochrome2   // Minimum value = black
};

// XA SOP Class information
struct xa_sop_class_info {
    std::string_view uid;
    std::string_view name;
    bool is_enhanced;
    bool is_3d;
    bool supports_multiframe;
};

// Positioner angles (LAO/RAO, Cranial/Caudal)
struct xa_positioner_angles {
    double primary_angle;    // LAO(+) / RAO(-) in degrees
    double secondary_angle;  // Cranial(+) / Caudal(-) in degrees
};

// QCA calibration data
struct xa_calibration_data {
    double imager_pixel_spacing[2];       // mm/pixel
    double distance_source_to_detector;   // SID in mm
    double distance_source_to_patient;    // SOD in mm
};

// Utility functions
[[nodiscard]] bool is_xa_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_enhanced_xa_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_xrf_sop_class(std::string_view uid) noexcept;
[[nodiscard]] const xa_sop_class_info* get_xa_sop_class_info(std::string_view uid);

[[nodiscard]] std::vector<std::string_view> get_xa_transfer_syntaxes();
[[nodiscard]] bool is_valid_xa_photometric(std::string_view photometric) noexcept;
[[nodiscard]] xa_photometric_interpretation
    parse_xa_photometric(std::string_view value) noexcept;

// Calibration utilities
[[nodiscard]] double calculate_magnification_factor(
    double distance_source_to_detector,
    double distance_source_to_patient
) noexcept;

[[nodiscard]] double calculate_real_world_size(
    double pixel_size,
    double imager_pixel_spacing,
    double magnification_factor
) noexcept;

} // namespace pacs::services::sop_classes
```

**Example:**
```cpp
using namespace pacs::services::sop_classes;

// Check if dataset is XA
if (is_xa_sop_class(sop_class_uid)) {
    auto info = get_xa_sop_class_info(sop_class_uid);
    std::cout << "SOP Class: " << info->name << std::endl;

    if (info->is_enhanced) {
        // Handle enhanced XA with functional groups
    }
}

// Calculate real-world measurement from QCA
xa_calibration_data cal{
    {0.2, 0.2},  // 0.2mm pixel spacing
    1000.0,      // SID = 1000mm
    750.0        // SOD = 750mm
};
double mag_factor = calculate_magnification_factor(cal.distance_source_to_detector,
                                                    cal.distance_source_to_patient);
double real_size = calculate_real_world_size(100.0, cal.imager_pixel_spacing[0], mag_factor);
```

---

### `pacs::services::validation::xa_iod_validator`

IOD Validator for X-Ray Angiographic images.

```cpp
#include <pacs/services/validation/xa_iod_validator.hpp>

namespace pacs::services::validation {

// XA-specific DICOM tags
namespace xa_tags {
    inline constexpr core::dicom_tag positioner_primary_angle{0x0018, 0x1510};
    inline constexpr core::dicom_tag positioner_secondary_angle{0x0018, 0x1511};
    inline constexpr core::dicom_tag imager_pixel_spacing{0x0018, 0x1164};
    inline constexpr core::dicom_tag distance_source_to_detector{0x0018, 0x1110};
    inline constexpr core::dicom_tag distance_source_to_patient{0x0018, 0x1111};
    inline constexpr core::dicom_tag frame_time{0x0018, 0x1063};
    inline constexpr core::dicom_tag cine_rate{0x0018, 0x0040};
    inline constexpr core::dicom_tag radiation_setting{0x0018, 0x1155};
}

// Validation severity levels
enum class validation_severity {
    error,    // Must be corrected
    warning,  // Should be corrected
    info      // Informational
};

// Validation result for a single finding
struct validation_finding {
    validation_severity severity;
    std::string module;
    std::string message;
    std::optional<core::dicom_tag> tag;
};

// Complete validation result
struct validation_result {
    bool is_valid;
    std::vector<validation_finding> findings;

    [[nodiscard]] bool has_errors() const noexcept;
    [[nodiscard]] bool has_warnings() const noexcept;
    [[nodiscard]] std::vector<validation_finding> errors() const;
    [[nodiscard]] std::vector<validation_finding> warnings() const;
};

// Validation options
struct xa_validation_options {
    bool validate_patient_module = true;
    bool validate_study_module = true;
    bool validate_series_module = true;
    bool validate_equipment_module = true;
    bool validate_acquisition_module = true;
    bool validate_image_module = true;
    bool validate_calibration = true;       // XA-specific
    bool validate_multiframe = true;        // XA-specific
    bool strict_mode = false;               // Treat warnings as errors
};

class xa_iod_validator {
public:
    // Validate complete XA IOD
    [[nodiscard]] static validation_result validate(
        const core::dicom_dataset& dataset,
        const xa_validation_options& options = {}
    );

    // Validate specific modules
    [[nodiscard]] static validation_result validate_patient_module(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_xa_acquisition_module(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_xa_image_module(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_calibration_data(
        const core::dicom_dataset& dataset
    );
    [[nodiscard]] static validation_result validate_multiframe_data(
        const core::dicom_dataset& dataset
    );

    // Quick checks
    [[nodiscard]] static bool has_required_xa_attributes(
        const core::dicom_dataset& dataset
    ) noexcept;
    [[nodiscard]] static bool has_valid_photometric_interpretation(
        const core::dicom_dataset& dataset
    ) noexcept;
    [[nodiscard]] static bool has_calibration_data(
        const core::dicom_dataset& dataset
    ) noexcept;
};

} // namespace pacs::services::validation
```

**Example:**
```cpp
using namespace pacs::services::validation;

// Full validation
xa_validation_options opts;
opts.strict_mode = true;  // Fail on warnings

auto result = xa_iod_validator::validate(dataset, opts);

if (!result.is_valid) {
    for (const auto& finding : result.errors()) {
        std::cerr << "[" << finding.module << "] "
                  << finding.message << std::endl;
    }
}

// Quick calibration check
if (xa_iod_validator::has_calibration_data(dataset)) {
    auto cal_result = xa_iod_validator::validate_calibration_data(dataset);
    // Process calibration validation...
}
```

---

### `pacs::services::cache::simple_lru_cache<Key, Value>`

Thread-safe LRU (Least Recently Used) cache with TTL support.

```cpp
#include <pacs/services/cache/simple_lru_cache.hpp>

namespace pacs::services::cache {

// Cache configuration
struct cache_config {
    std::size_t max_size{1000};           // Maximum entries
    std::chrono::seconds ttl{300};        // Time-to-live (5 min default)
    bool enable_metrics{true};            // Hit/miss tracking
    std::string cache_name{"lru_cache"};  // For logging/metrics
};

// Cache statistics
struct cache_stats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expirations{0};

    double hit_rate() const noexcept;     // Returns 0.0-100.0
    void reset() noexcept;
};

template <typename Key, typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
class simple_lru_cache {
public:
    // Construction
    explicit simple_lru_cache(const cache_config& config = {});
    simple_lru_cache(size_type max_size, std::chrono::seconds ttl);

    // Cache operations
    [[nodiscard]] std::optional<Value> get(const Key& key);
    void put(const Key& key, const Value& value);
    void put(const Key& key, Value&& value);
    bool invalidate(const Key& key);
    template<typename Predicate>
    size_type invalidate_if(Predicate pred);  // Conditional invalidation
    void clear();
    size_type purge_expired();

    // Information
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_type max_size() const noexcept;
    [[nodiscard]] std::chrono::seconds ttl() const noexcept;

    // Statistics
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;
    void reset_stats() noexcept;
};

// Type alias for string-keyed cache
template <typename Value>
using string_lru_cache = simple_lru_cache<std::string, Value>;

} // namespace pacs::services::cache
```

**Example:**
```cpp
using namespace pacs::services::cache;

// Create cache with 1000 entry limit and 5-minute TTL
cache_config config;
config.max_size = 1000;
config.ttl = std::chrono::seconds{300};
config.cache_name = "query_cache";

simple_lru_cache<std::string, QueryResult> cache(config);

// Store and retrieve
cache.put("patient_123", result);

auto cached = cache.get("patient_123");
if (cached) {
    // Use cached result
    process(*cached);
}

// Check performance
auto rate = cache.hit_rate();
logger_adapter::info("Cache hit rate: {:.1f}%", rate);
```

---

### `pacs::services::cache::query_cache`

Specialized cache for DICOM C-FIND query results.

```cpp
#include <pacs/services/cache/query_cache.hpp>

namespace pacs::services::cache {

// Configuration
struct query_cache_config {
    std::size_t max_entries{1000};
    std::chrono::seconds ttl{300};
    bool enable_logging{false};
    bool enable_metrics{true};
    std::string cache_name{"cfind_query_cache"};
};

// Cached query result
struct cached_query_result {
    std::vector<uint8_t> data;                        // Serialized result
    uint32_t match_count{0};                          // Number of matches
    std::chrono::steady_clock::time_point cached_at;  // Cache timestamp
    std::string query_level;                          // PATIENT/STUDY/SERIES/IMAGE
};

class query_cache {
public:
    explicit query_cache(const query_cache_config& config = {});

    // Cache operations
    [[nodiscard]] std::optional<cached_query_result> get(const std::string& key);
    void put(const std::string& key, const cached_query_result& result);
    void put(const std::string& key, cached_query_result&& result);
    bool invalidate(const std::string& key);
    size_type invalidate_by_prefix(const std::string& prefix);
    size_type invalidate_by_query_level(const std::string& level);
    template<typename Predicate>
    size_type invalidate_if(Predicate pred);
    void clear();
    size_type purge_expired();

    // Information
    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;
    [[nodiscard]] const cache_stats& stats() const noexcept;
    [[nodiscard]] double hit_rate() const noexcept;

    // Key generation helpers
    [[nodiscard]] static std::string build_key(
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);

    [[nodiscard]] static std::string build_key_with_ae(
        const std::string& calling_ae,
        const std::string& query_level,
        const std::vector<std::pair<std::string, std::string>>& params);
};

// Global cache access
[[nodiscard]] query_cache& global_query_cache();
bool configure_global_cache(const query_cache_config& config);

} // namespace pacs::services::cache
```

**Example:**
```cpp
using namespace pacs::services::cache;

// Build cache key from query parameters
std::string key = query_cache::build_key("STUDY", {
    {"PatientID", "12345"},
    {"StudyDate", "20240101-20241231"}
});

// Check cache first
auto& cache = global_query_cache();
auto result = cache.get(key);

if (result) {
    // Return cached result
    return result->data;
}

// Execute query and cache result
auto query_result = execute_cfind(...);

cached_query_result cached;
cached.data = serialize(query_result);
cached.match_count = query_result.size();
cached.query_level = "STUDY";
cached.cached_at = std::chrono::steady_clock::now();

cache.put(key, std::move(cached));

// Cache invalidation on C-STORE (when new data is stored)
// Use post_store_handler to invalidate affected cache entries
storage_scp scp{config};
scp.set_post_store_handler([&cache](const auto& dataset,
                                     const auto& patient_id,
                                     const auto& study_uid,
                                     const auto& series_uid,
                                     const auto& sop_uid) {
    // Invalidate all cached queries that might be affected
    cache.invalidate_by_query_level("IMAGE");
    cache.invalidate_by_query_level("SERIES");
    cache.invalidate_by_query_level("STUDY");
    cache.invalidate_by_query_level("PATIENT");
});

// Targeted invalidation using prefix
cache.invalidate_by_prefix("WORKSTATION1/");  // Invalidate specific AE

// Custom predicate invalidation
cache.invalidate_if([](const auto& key, const cached_query_result& r) {
    return r.match_count > 1000;  // Remove large result sets
});
```

---

### `pacs::services::cache::streaming_query_handler`

Streaming query handler for memory-efficient C-FIND responses with pagination.

```cpp
#include <pacs/services/cache/streaming_query_handler.hpp>

namespace pacs::services {

class streaming_query_handler {
public:
    using StreamResult = Result<std::unique_ptr<query_result_stream>>;

    // Construction
    explicit streaming_query_handler(storage::index_database* db);

    // Configuration
    void set_page_size(size_t size) noexcept;        // Default: 100
    [[nodiscard]] auto page_size() const noexcept -> size_t;

    void set_max_results(size_t max) noexcept;       // Default: 0 (unlimited)
    [[nodiscard]] auto max_results() const noexcept -> size_t;

    // Stream Operations
    [[nodiscard]] auto create_stream(
        query_level level,
        const core::dicom_dataset& query_keys,
        const std::string& calling_ae
    ) -> StreamResult;

    [[nodiscard]] auto resume_stream(
        const std::string& cursor_state,
        query_level level,
        const core::dicom_dataset& query_keys
    ) -> StreamResult;

    // Compatibility with query_scp
    [[nodiscard]] auto as_query_handler() -> query_handler;
};

} // namespace pacs::services
```

**Example - Streaming interface:**
```cpp
using namespace pacs::services;

streaming_query_handler handler(db);
handler.set_page_size(100);

dicom_dataset query_keys;
query_keys.set_string(tags::modality, vr_type::CS, "CT");

auto stream_result = handler.create_stream(
    query_level::study, query_keys, "CALLING_AE");

if (stream_result.is_ok()) {
    auto& stream = stream_result.value();

    while (stream->has_more()) {
        auto batch = stream->next_batch();
        if (batch.has_value()) {
            for (const auto& dataset : batch.value()) {
                // Process each DICOM dataset
            }
        }
    }
}
```

**Example - Query SCP integration:**
```cpp
using namespace pacs::services;

query_scp scp;
streaming_query_handler handler(db);
handler.set_max_results(500);

// Use adapter for backward compatibility
scp.set_handler(handler.as_query_handler());
scp.start();
```

---

### `pacs::services::cache::query_result_stream`

Converts database cursor results to DICOM datasets with batch support.

```cpp
#include <pacs/services/cache/query_result_stream.hpp>

namespace pacs::services {

// Stream configuration
struct stream_config {
    size_t page_size{100};  // Batch size for fetching
};

class query_result_stream {
public:
    // Factory methods
    static auto create(
        storage::index_database* db,
        query_level level,
        const core::dicom_dataset& query_keys,
        const stream_config& config = {}
    ) -> Result<std::unique_ptr<query_result_stream>>;

    static auto from_cursor(
        storage::index_database* db,
        const std::string& cursor_state,
        query_level level,
        const core::dicom_dataset& query_keys,
        const stream_config& config = {}
    ) -> Result<std::unique_ptr<query_result_stream>>;

    // Stream state
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto level() const noexcept -> query_level;
    [[nodiscard]] auto total_count() const -> std::optional<size_t>;

    // Data retrieval
    [[nodiscard]] auto next_batch()
        -> std::optional<std::vector<core::dicom_dataset>>;

    // Cursor for resumption
    [[nodiscard]] auto cursor() const -> std::string;
};

} // namespace pacs::services
```

---

### `pacs::services::cache::database_cursor`

Low-level SQLite cursor for streaming query results with lazy evaluation.

```cpp
#include <pacs/services/cache/database_cursor.hpp>

namespace pacs::services {

// Query record types (variant of all DICOM levels)
using query_record = std::variant<
    storage::patient_record,
    storage::study_record,
    storage::series_record,
    storage::instance_record
>;

class database_cursor {
public:
    enum class record_type { patient, study, series, instance };

    // Factory methods for each query level
    static auto create_patient_cursor(
        sqlite3* db,
        const storage::patient_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    static auto create_study_cursor(
        sqlite3* db,
        const storage::study_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    static auto create_series_cursor(
        sqlite3* db,
        const storage::series_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    static auto create_instance_cursor(
        sqlite3* db,
        const storage::instance_query& query
    ) -> Result<std::unique_ptr<database_cursor>>;

    // Cursor state
    [[nodiscard]] auto has_more() const noexcept -> bool;
    [[nodiscard]] auto position() const noexcept -> size_t;
    [[nodiscard]] auto type() const noexcept -> record_type;

    // Data retrieval
    [[nodiscard]] auto fetch_next() -> std::optional<query_record>;
    [[nodiscard]] auto fetch_batch(size_t batch_size)
        -> std::vector<query_record>;

    // Cursor control
    [[nodiscard]] auto reset() -> VoidResult;
    [[nodiscard]] auto serialize() const -> std::string;
};

} // namespace pacs::services
```

**Example - Resumable pagination:**
```cpp
using namespace pacs::services;

// First page request
auto cursor_result = database_cursor::create_study_cursor(
    db->native_handle(), study_query{});

if (cursor_result.is_ok()) {
    auto& cursor = cursor_result.value();

    // Fetch first 100 records
    auto batch = cursor->fetch_batch(100);

    // Save cursor state for resumption
    std::string saved_state = cursor->serialize();

    // ... Later, resume from saved state
    // query_result_stream::from_cursor(..., saved_state, ...)
}
```

### `pacs::services::print_scu`

Print Management SCU for sending DICOM print requests to remote Print SCP systems (PS3.4 Annex H).

#### Data Types

```cpp
#include "pacs/services/print_scu.hpp"

// Result of a Print DIMSE-N operation
struct print_result {
    std::string sop_instance_uid;     // SOP Instance UID
    uint16_t status{0};               // DIMSE status (0x0000 = success)
    std::string error_comment;        // Error comment from SCP
    std::chrono::milliseconds elapsed{0};
    core::dicom_dataset response_data; // Response dataset

    bool is_success() const noexcept;  // status == 0x0000
    bool is_warning() const noexcept;  // (status & 0xF000) == 0xB000
    bool is_error() const noexcept;    // !success && !warning
};

// Film Session creation data
struct print_session_data {
    std::string sop_instance_uid;      // Auto-generated if empty
    uint32_t number_of_copies{1};
    std::string print_priority{"MED"}; // HIGH, MED, LOW
    std::string medium_type{"BLUE FILM"};
    std::string film_destination{"MAGAZINE"};
    std::string film_session_label;
};

// Film Box creation data
struct print_film_box_data {
    std::string image_display_format{"STANDARD\\1,1"};
    std::string film_orientation{"PORTRAIT"};
    std::string film_size_id{"8INX10IN"};
    std::string magnification_type;
    std::string film_session_uid;      // Parent session UID
};

// Image Box data (for N-SET)
struct print_image_data {
    core::dicom_dataset pixel_data;
    uint16_t image_position{0};        // 1-based position in film box
};
```

#### Usage

```cpp
#include "pacs/services/print_scu.hpp"
using namespace pacs::services;

// Create Print SCU
print_scu scu;

// 1. Create Film Session
print_session_data session_data;
session_data.number_of_copies = 1;
session_data.print_priority = "HIGH";
auto session_result = scu.create_film_session(assoc, session_data);
auto session_uid = session_result.value().sop_instance_uid;

// 2. Create Film Box (SCP auto-creates Image Boxes)
print_film_box_data box_data;
box_data.image_display_format = "STANDARD\\1,1";
box_data.film_session_uid = session_uid;
auto box_result = scu.create_film_box(assoc, box_data);
auto film_box_uid = box_result.value().sop_instance_uid;

// 3. Set Image Box pixel data
print_image_data image_data;
image_data.pixel_data = my_image_dataset;
scu.set_image_box(assoc, image_box_uid, image_data);

// 4. Print and cleanup
scu.print_film_box(assoc, film_box_uid);
scu.delete_film_session(assoc, session_uid);

// Query printer status
auto status = scu.query_printer_status(assoc);
```

### `pacs::services::print_scp`

Print Management SCP that handles incoming print requests from remote SCU systems.

#### Handler Types

```cpp
#include "pacs/services/print_scp.hpp"
using namespace pacs::services;

// Register print handler with SCP service
auto handler = std::make_shared<print_handler>();
scp_service.register_handler(handler);
```

The Print SCP handles the following SOP Classes:
- Basic Film Session SOP Class (1.2.840.10008.5.1.1.1)
- Basic Film Box SOP Class (1.2.840.10008.5.1.1.2)
- Basic Grayscale Image Box SOP Class (1.2.840.10008.5.1.1.4)
- Basic Color Image Box SOP Class (1.2.840.10008.5.1.1.4.1)
- Printer SOP Class (1.2.840.10008.5.1.1.16)
- Basic Grayscale Print Meta SOP Class (1.2.840.10008.5.1.1.9)
- Basic Color Print Meta SOP Class (1.2.840.10008.5.1.1.18)

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

### `pacs::storage::index_database`

SQLite-based index database for PACS metadata.

All query methods return `Result<T>` for proper error handling.

```cpp
#include <pacs/storage/index_database.hpp>

namespace pacs::storage {

class index_database {
public:
    // Factory method
    static Result<std::unique_ptr<index_database>> open(const std::string& path);

    // Patient operations
    Result<int64_t> upsert_patient(const std::string& patient_id,
                                    const std::string& patient_name,
                                    const std::string& birth_date,
                                    const std::string& sex);
    std::optional<patient_record> find_patient(const std::string& patient_id);
    Result<std::vector<patient_record>> search_patients(const patient_query& query);

    // Study operations
    Result<int64_t> upsert_study(int64_t patient_pk, const std::string& study_uid, ...);
    std::optional<study_record> find_study(const std::string& study_uid);
    Result<std::vector<study_record>> search_studies(const study_query& query);
    Result<std::vector<study_record>> list_studies(const std::string& patient_id);
    Result<void> delete_study(const std::string& study_uid);

    // Series operations
    Result<int64_t> upsert_series(int64_t study_pk, const std::string& series_uid, ...);
    std::optional<series_record> find_series(const std::string& series_uid);
    Result<std::vector<series_record>> search_series(const series_query& query);
    Result<std::vector<series_record>> list_series(const std::string& study_uid);

    // Instance operations
    Result<int64_t> upsert_instance(int64_t series_pk, const std::string& sop_uid, ...);
    std::optional<instance_record> find_instance(const std::string& sop_uid);
    Result<std::vector<instance_record>> search_instances(const instance_query& query);
    Result<std::vector<instance_record>> list_instances(const std::string& series_uid);
    Result<std::optional<std::string>> get_file_path(const std::string& sop_uid);

    // File path operations
    Result<std::vector<std::string>> get_study_files(const std::string& study_uid);
    Result<std::vector<std::string>> get_series_files(const std::string& series_uid);

    // Count operations
    Result<size_t> patient_count();
    Result<size_t> study_count();
    Result<size_t> study_count(const std::string& patient_id);
    Result<size_t> series_count();
    Result<size_t> series_count(const std::string& study_uid);
    Result<size_t> instance_count();
    Result<size_t> instance_count(const std::string& series_uid);

    // Audit operations
    Result<int64_t> add_audit_log(const audit_record& record);
    Result<std::vector<audit_record>> query_audit_log(const audit_query& query);
    Result<size_t> audit_count();

    // Worklist operations
    Result<int64_t> add_worklist_item(const worklist_item& item);
    Result<std::vector<worklist_item>> query_worklist(const worklist_query& query);
    Result<size_t> worklist_count(const std::string& status = "");

    // Database integrity
    Result<void> verify_integrity();
};

} // namespace pacs::storage
```

**Example - Using Result<T> pattern:**
```cpp
auto db_result = pacs::storage::index_database::open("/data/pacs/index.db");
if (!db_result.is_ok()) {
    std::cerr << "Failed to open database: " << db_result.error().message << "\n";
    return;
}
auto& db = db_result.value();

// Query studies with error handling
pacs::storage::study_query query;
query.patient_id = "P001";
auto studies_result = db->search_studies(query);
if (!studies_result.is_ok()) {
    std::cerr << "Query failed: " << studies_result.error().message << "\n";
    return;
}

for (const auto& study : studies_result.value()) {
    std::cout << "Study: " << study.study_uid << "\n";
}
```

---

## DX Modality Module

### `pacs::services::sop_classes::dx_storage`

Digital X-Ray (DX) SOP Class definitions and utilities for general radiography imaging.

```cpp
#include <pacs/services/sop_classes/dx_storage.hpp>

namespace pacs::services::sop_classes {

// SOP Class UIDs
inline constexpr std::string_view dx_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.1";
inline constexpr std::string_view dx_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.1.1";
inline constexpr std::string_view mammography_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.2";
inline constexpr std::string_view mammography_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.2.1";
inline constexpr std::string_view intraoral_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.3";
inline constexpr std::string_view intraoral_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.3.1";

// Enumerations
enum class dx_photometric_interpretation { monochrome1, monochrome2 };
enum class dx_image_type { for_presentation, for_processing };
enum class dx_view_position { ap, pa, lateral, oblique, other };
enum class dx_detector_type { direct, indirect, storage, film };
enum class dx_body_part { chest, abdomen, pelvis, spine, skull, hand, foot,
                          knee, elbow, shoulder, hip, wrist, ankle, extremity,
                          breast, other };

// SOP Class information structure
struct dx_sop_class_info {
    std::string_view uid;
    std::string_view name;
    std::string_view description;
    dx_image_type image_type;
    bool is_mammography;
    bool is_intraoral;
};

// Utility functions
[[nodiscard]] std::vector<std::string> get_dx_transfer_syntaxes();
[[nodiscard]] std::vector<std::string> get_dx_storage_sop_classes(
    bool include_mammography = true, bool include_intraoral = true);
[[nodiscard]] const dx_sop_class_info* get_dx_sop_class_info(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_storage_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_for_processing_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_dx_for_presentation_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_mammography_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_valid_dx_photometric(std::string_view value) noexcept;

// Enum conversion utilities
[[nodiscard]] std::string_view to_string(dx_photometric_interpretation interp) noexcept;
[[nodiscard]] std::string_view to_string(dx_view_position position) noexcept;
[[nodiscard]] std::string_view to_string(dx_detector_type type) noexcept;
[[nodiscard]] std::string_view to_string(dx_body_part part) noexcept;
[[nodiscard]] dx_view_position parse_view_position(std::string_view value) noexcept;
[[nodiscard]] dx_body_part parse_body_part(std::string_view value) noexcept;

} // namespace pacs::services::sop_classes
```

**Example:**
```cpp
using namespace pacs::services::sop_classes;

// Check if a SOP Class UID is DX
if (is_dx_storage_sop_class(sop_class_uid)) {
    const auto* info = get_dx_sop_class_info(sop_class_uid);

    if (info->image_type == dx_image_type::for_presentation) {
        // Display-ready image, apply VOI LUT
    } else {
        // Raw data, requires processing
    }

    if (info->is_mammography) {
        // Apply mammography-specific display settings
    }
}

// Get all general DX SOP Classes (exclude mammography/intraoral)
auto dx_classes = get_dx_storage_sop_classes(false, false);

// Parse body part from DICOM dataset
auto body_part = parse_body_part("CHEST");  // Returns dx_body_part::chest
auto view = parse_view_position("PA");       // Returns dx_view_position::pa
```

---

### `pacs::services::validation::dx_iod_validator`

IOD Validator for Digital X-Ray images per DICOM PS3.3 Section A.26.

```cpp
#include <pacs/services/validation/dx_iod_validator.hpp>

namespace pacs::services::validation {

// Validation options
struct dx_validation_options {
    bool check_type1 = true;               // Required attributes
    bool check_type2 = true;               // Required, can be empty
    bool check_conditional = true;         // Conditionally required
    bool validate_pixel_data = true;       // Pixel consistency checks
    bool validate_dx_specific = true;      // DX module validation
    bool validate_anatomy = true;          // Body part/view validation
    bool validate_presentation_requirements = true;
    bool validate_processing_requirements = true;
    bool allow_both_photometric = true;    // MONOCHROME1 and 2
    bool strict_mode = false;              // Warnings as errors
};

class dx_iod_validator {
public:
    dx_iod_validator() = default;
    explicit dx_iod_validator(const dx_validation_options& options);

    // Full validation
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    // Specialized validation
    [[nodiscard]] validation_result validate_for_presentation(
        const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_for_processing(
        const core::dicom_dataset& dataset) const;

    // Quick Type 1 check only
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    // Options access
    [[nodiscard]] const dx_validation_options& options() const noexcept;
    void set_options(const dx_validation_options& options);
};

// Convenience functions
[[nodiscard]] validation_result validate_dx_iod(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_valid_dx_dataset(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_presentation_dx(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_processing_dx(const core::dicom_dataset& dataset);

} // namespace pacs::services::validation
```

**Example:**
```cpp
using namespace pacs::services::validation;

// Basic validation
auto result = validate_dx_iod(dataset);

if (!result.is_valid) {
    for (const auto& finding : result.findings) {
        if (finding.severity == validation_severity::error) {
            std::cerr << "[ERROR] " << finding.code << ": " << finding.message << "\n";
        }
    }
    return;
}

// Strict validation with custom options
dx_validation_options options;
options.strict_mode = true;  // Treat warnings as errors

dx_iod_validator validator(options);
auto strict_result = validator.validate(dataset);

// Validate For Presentation image with VOI LUT checks
if (is_for_presentation_dx(dataset)) {
    auto result = validator.validate_for_presentation(dataset);
    // Includes Window Center/Width or VOI LUT Sequence validation
}
```

---

## MG Modality Module

### `pacs::services::sop_classes::mg_storage`

Digital Mammography X-Ray SOP Class definitions and utilities for breast imaging.

```cpp
#include <pacs/services/sop_classes/mg_storage.hpp>

namespace pacs::services::sop_classes {

// SOP Class UIDs
inline constexpr std::string_view mg_image_storage_for_presentation_uid =
    "1.2.840.10008.5.1.4.1.1.1.2";
inline constexpr std::string_view mg_image_storage_for_processing_uid =
    "1.2.840.10008.5.1.4.1.1.1.2.1";
inline constexpr std::string_view breast_tomosynthesis_image_storage_uid =
    "1.2.840.10008.5.1.4.1.1.13.1.3";

// Breast laterality enumeration
enum class breast_laterality { left, right, bilateral, unknown };

// Mammography view positions
enum class mg_view_position {
    cc,      ///< Craniocaudal
    mlo,     ///< Mediolateral Oblique
    ml,      ///< Mediolateral
    lm,      ///< Lateromedial
    xccl,    ///< Exaggerated CC Laterally
    xccm,    ///< Exaggerated CC Medially
    fb,      ///< From Below
    spot,    ///< Spot compression
    mag,     ///< Magnification
    spot_mag,///< Spot with magnification
    implant, ///< Implant displaced (Eklund)
    // ... additional views
    other
};

// Mammography SOP class information
struct mg_sop_class_info {
    std::string_view uid;
    std::string_view name;
    std::string_view description;
    mg_image_type image_type;
    bool is_tomosynthesis;
    bool supports_multiframe;
};

// Utility functions
[[nodiscard]] std::string_view to_string(breast_laterality laterality) noexcept;
[[nodiscard]] breast_laterality parse_breast_laterality(std::string_view value) noexcept;
[[nodiscard]] bool is_valid_breast_laterality(std::string_view value) noexcept;

[[nodiscard]] std::string_view to_string(mg_view_position position) noexcept;
[[nodiscard]] mg_view_position parse_mg_view_position(std::string_view value) noexcept;
[[nodiscard]] bool is_screening_view(mg_view_position position) noexcept;
[[nodiscard]] bool is_magnification_view(mg_view_position position) noexcept;

[[nodiscard]] bool is_valid_compression_force(double force_n) noexcept;
[[nodiscard]] std::pair<double, double> get_typical_compression_force_range() noexcept;

[[nodiscard]] std::vector<std::string> get_mg_storage_sop_classes(bool include_tomosynthesis = true);
[[nodiscard]] const mg_sop_class_info* get_mg_sop_class_info(std::string_view uid) noexcept;
[[nodiscard]] bool is_mg_storage_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_breast_tomosynthesis_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_mg_for_processing_sop_class(std::string_view uid) noexcept;
[[nodiscard]] bool is_mg_for_presentation_sop_class(std::string_view uid) noexcept;

} // namespace pacs::services::sop_classes
```

**Example:**
```cpp
using namespace pacs::services::sop_classes;

// Check if a SOP Class UID is Mammography
if (is_mg_storage_sop_class(sop_class_uid)) {
    const auto* info = get_mg_sop_class_info(sop_class_uid);

    if (info->is_tomosynthesis) {
        // Handle 3D breast tomosynthesis
    }
}

// Parse mammography-specific attributes
auto laterality = parse_breast_laterality("L");  // Left breast
auto view = parse_mg_view_position("MLO");       // Mediolateral oblique

// Check if this is a standard screening view
if (is_screening_view(view)) {
    // CC or MLO - standard screening exam
}

// Validate compression force (typical: 50-200 N)
if (is_valid_compression_force(compression_force)) {
    // Within acceptable range
}
```

---

### `pacs::services::validation::mg_iod_validator`

IOD Validator for Digital Mammography images per DICOM PS3.3 Section A.26.2.

```cpp
#include <pacs/services/validation/mg_iod_validator.hpp>

namespace pacs::services::validation {

// Validation options specific to mammography
struct mg_validation_options {
    bool check_type1 = true;
    bool check_type2 = true;
    bool check_conditional = true;
    bool validate_pixel_data = true;
    bool validate_mg_specific = true;
    bool validate_laterality = true;        // Breast laterality (0020,0060)
    bool validate_view_position = true;     // View position (0018,5101)
    bool validate_compression = true;       // Compression force (0018,11A2)
    bool validate_implant_attributes = true;
    bool validate_dose_parameters = true;
    bool strict_mode = false;
};

class mg_iod_validator {
public:
    mg_iod_validator() = default;
    explicit mg_iod_validator(const mg_validation_options& options);

    // Full IOD validation
    [[nodiscard]] validation_result validate(const core::dicom_dataset& dataset) const;

    // Specialized validations
    [[nodiscard]] validation_result validate_for_presentation(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_for_processing(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_laterality(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_view_position(const core::dicom_dataset& dataset) const;
    [[nodiscard]] validation_result validate_compression_force(const core::dicom_dataset& dataset) const;

    // Quick validation
    [[nodiscard]] bool quick_check(const core::dicom_dataset& dataset) const;

    // Configuration
    [[nodiscard]] const mg_validation_options& options() const noexcept;
    void set_options(const mg_validation_options& options);
};

// Convenience functions
[[nodiscard]] validation_result validate_mg_iod(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_valid_mg_dataset(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_presentation_mg(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_for_processing_mg(const core::dicom_dataset& dataset);
[[nodiscard]] bool has_breast_implant(const core::dicom_dataset& dataset);
[[nodiscard]] bool is_screening_mammogram(const core::dicom_dataset& dataset);

} // namespace pacs::services::validation
```

**Error Codes (MG-specific):**
| Code | Severity | Description |
|------|----------|-------------|
| MG-ERR-001 | Error | SOPClassUID is not a mammography storage class |
| MG-ERR-002 | Error | Modality must be 'MG' |
| MG-ERR-003 | Error | Laterality not specified (required for mammography) |
| MG-ERR-004 | Error | Invalid laterality value |
| MG-ERR-005 | Error | Invalid image laterality value |
| MG-ERR-006 | Error | BitsStored exceeds BitsAllocated |
| MG-ERR-007 | Error | Mammography must be grayscale |
| MG-ERR-008 | Error | Invalid photometric interpretation |
| MG-WARN-001 | Warning | Laterality mismatch between series and image level |
| MG-WARN-003 | Warning | Body Part Examined should be 'BREAST' |
| MG-WARN-005 | Warning | View Position not present |
| MG-WARN-010 | Warning | Unrecognized view position |
| MG-WARN-013 | Warning | Compression force outside typical range |
| MG-INFO-007 | Info | Breast implant present but not using ID view |
| MG-INFO-008 | Info | Compression force not documented |

**Example:**
```cpp
using namespace pacs::services::validation;

// Validate a mammography dataset
auto result = validate_mg_iod(dataset);
if (!result.is_valid) {
    for (const auto& finding : result.findings) {
        if (finding.code.starts_with("MG-ERR-")) {
            std::cerr << "Error: " << finding.message << "\n";
        }
    }
}

// Validate specific aspects
mg_iod_validator validator;

auto laterality_result = validator.validate_laterality(dataset);
auto view_result = validator.validate_view_position(dataset);
auto compression_result = validator.validate_compression_force(dataset);

// Check for screening exam
if (is_screening_mammogram(dataset)) {
    // Standard 4-view screening (RCC, LCC, RMLO, LMLO)
}

// Check for implant
if (has_breast_implant(dataset)) {
    // May need implant-displaced views
}
```

---
