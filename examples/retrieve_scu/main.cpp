/**
 * @file main.cpp
 * @brief Retrieve SCU - DICOM C-MOVE/C-GET Client
 *
 * A command-line utility for retrieving DICOM studies from a remote SCP.
 * Supports both C-MOVE (transfer to destination) and C-GET (direct retrieval).
 *
 * @see Issue #103 - Retrieve SCU Sample
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1.3 - C-MOVE Service
 * @see DICOM PS3.7 Section 9.1.4 - C-GET Service
 *
 * Usage:
 *   retrieve_scu <host> <port> <called_ae> [options]
 *
 * Example:
 *   retrieve_scu localhost 11112 PACS_SCP --mode get --study-uid "1.2.3.4.5"
 */

#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/retrieve_scp.hpp"
#include "pacs/services/storage_scp.hpp"
#include "pacs/core/dicom_file.hpp"
#include "pacs/core/dicom_tag_constants.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

/// Default calling AE title
constexpr const char* default_calling_ae = "RETRIEVE_SCU";

/// Default timeout (60 seconds for retrieve operations)
constexpr auto default_timeout = std::chrono::milliseconds{60000};

/// Progress bar width
constexpr int progress_bar_width = 40;

/**
 * @brief Retrieve mode enumeration
 */
enum class retrieve_mode {
    c_move,  ///< C-MOVE: Transfer to destination AE
    c_get    ///< C-GET: Direct retrieval
};

/**
 * @brief Retrieve level enumeration
 */
enum class retrieve_level {
    patient,
    study,
    series,
    image
};

/**
 * @brief Storage structure option
 */
enum class storage_structure {
    hierarchical,  ///< Patient/Study/Series/Instance structure
    flat           ///< All files in single directory
};

/**
 * @brief Command-line options structure
 */
struct options {
    // Connection
    std::string host;
    uint16_t port{0};
    std::string called_ae;
    std::string calling_ae{default_calling_ae};

    // Retrieve mode
    retrieve_mode mode{retrieve_mode::c_get};
    std::string query_model{"study"};  // "patient" or "study" root

    // C-MOVE specific
    std::string move_destination;      // Destination AE for C-MOVE
    uint16_t local_storage_port{0};    // Port for local Storage SCP (C-MOVE)

    // Retrieve identifiers
    retrieve_level level{retrieve_level::study};
    std::string patient_id;
    std::string study_uid;
    std::string series_uid;
    std::string sop_instance_uid;

    // Output options
    std::filesystem::path output_dir{"./downloads"};
    storage_structure structure{storage_structure::hierarchical};
    bool overwrite{false};
    bool show_progress{true};
    bool verbose{false};
};

/**
 * @brief Print usage information
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Retrieve SCU - DICOM C-MOVE/C-GET Client

Usage: )" << program_name << R"( <host> <port> <called_ae> [options]

Arguments:
  host        Remote host address (IP or hostname)
  port        Remote port number (typically 104 or 11112)
  called_ae   Called AE Title (remote SCP's AE title)

Retrieve Mode:
  --mode <mode>       Retrieve mode: move, get (default: get)
                      move: Transfer to destination AE (requires --dest-ae)
                      get:  Direct retrieval to local machine

  --dest-ae <ae>      Destination AE Title (for C-MOVE mode)
  --local-port <port> Local Storage SCP port (for C-MOVE, default: auto)

Query Model:
  --model <model>     Query model: patient, study (default: study)

Retrieve Level and Identifiers:
  --level <level>     Retrieve level: PATIENT, STUDY, SERIES, IMAGE
                      (default: STUDY)
  --patient-id <id>   Patient ID (for PATIENT level)
  --study-uid <uid>   Study Instance UID
  --series-uid <uid>  Series Instance UID
  --sop-instance-uid <uid>  SOP Instance UID (for IMAGE level)

Output Options:
  --output, -o <dir>  Output directory (default: ./downloads)
  --structure <type>  Storage structure: hierarchical, flat (default: hierarchical)
  --overwrite         Overwrite existing files (default: skip)
  --no-progress       Disable progress display

General Options:
  --calling-ae <ae>   Calling AE Title (default: RETRIEVE_SCU)
  --verbose, -v       Show detailed progress
  --help, -h          Show this help message

Examples:
  # C-GET: Retrieve study directly
  )" << program_name << R"( localhost 11112 PACS_SCP --mode get --study-uid "1.2.3.4.5" -o ./data

  # C-MOVE: Transfer study to another PACS
  )" << program_name << R"( localhost 11112 PACS_SCP --mode move --dest-ae LOCAL_SCP --study-uid "1.2.3.4.5"

  # Retrieve specific series
  )" << program_name << R"( localhost 11112 PACS_SCP --level SERIES --series-uid "1.2.3.4.5.6"

  # Retrieve all studies for a patient
  )" << program_name << R"( localhost 11112 PACS_SCP --level PATIENT --patient-id "12345"

Exit Codes:
  0  Success - Retrieval completed
  1  Partial success - Some images failed
  2  Error - Retrieval failed or invalid arguments
)";
}

/**
 * @brief Parse retrieve mode from string
 */
std::optional<retrieve_mode> parse_mode(std::string_view mode_str) {
    if (mode_str == "move" || mode_str == "MOVE" || mode_str == "c-move") {
        return retrieve_mode::c_move;
    }
    if (mode_str == "get" || mode_str == "GET" || mode_str == "c-get") {
        return retrieve_mode::c_get;
    }
    return std::nullopt;
}

/**
 * @brief Parse retrieve level from string
 */
std::optional<retrieve_level> parse_level(std::string_view level_str) {
    if (level_str == "PATIENT" || level_str == "patient") {
        return retrieve_level::patient;
    }
    if (level_str == "STUDY" || level_str == "study") {
        return retrieve_level::study;
    }
    if (level_str == "SERIES" || level_str == "series") {
        return retrieve_level::series;
    }
    if (level_str == "IMAGE" || level_str == "image" ||
        level_str == "INSTANCE" || level_str == "instance") {
        return retrieve_level::image;
    }
    return std::nullopt;
}

/**
 * @brief Convert retrieve level to string
 */
std::string_view to_string(retrieve_level level) {
    switch (level) {
        case retrieve_level::patient: return "PATIENT";
        case retrieve_level::study: return "STUDY";
        case retrieve_level::series: return "SERIES";
        case retrieve_level::image: return "IMAGE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Parse command line arguments
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    if (argc < 4) {
        return false;
    }

    opts.host = argv[1];

    // Parse port
    try {
        int port_int = std::stoi(argv[2]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        opts.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[2] << "'\n";
        return false;
    }

    opts.called_ae = argv[3];
    if (opts.called_ae.length() > 16) {
        std::cerr << "Error: Called AE title exceeds 16 characters\n";
        return false;
    }

    // Parse optional arguments
    for (int i = 4; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        }
        if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--mode" && i + 1 < argc) {
            auto mode = parse_mode(argv[++i]);
            if (!mode) {
                std::cerr << "Error: Invalid mode '" << argv[i] << "'\n";
                return false;
            }
            opts.mode = *mode;
        } else if (arg == "--model" && i + 1 < argc) {
            opts.query_model = argv[++i];
            if (opts.query_model != "patient" && opts.query_model != "study") {
                std::cerr << "Error: Invalid query model (use 'patient' or 'study')\n";
                return false;
            }
        } else if (arg == "--dest-ae" && i + 1 < argc) {
            opts.move_destination = argv[++i];
            if (opts.move_destination.length() > 16) {
                std::cerr << "Error: Destination AE title exceeds 16 characters\n";
                return false;
            }
        } else if (arg == "--local-port" && i + 1 < argc) {
            try {
                int port_int = std::stoi(argv[++i]);
                if (port_int < 1 || port_int > 65535) {
                    std::cerr << "Error: Local port must be between 1 and 65535\n";
                    return false;
                }
                opts.local_storage_port = static_cast<uint16_t>(port_int);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid local port number\n";
                return false;
            }
        } else if (arg == "--level" && i + 1 < argc) {
            auto level = parse_level(argv[++i]);
            if (!level) {
                std::cerr << "Error: Invalid retrieve level '" << argv[i] << "'\n";
                return false;
            }
            opts.level = *level;
        } else if (arg == "--patient-id" && i + 1 < argc) {
            opts.patient_id = argv[++i];
        } else if (arg == "--study-uid" && i + 1 < argc) {
            opts.study_uid = argv[++i];
        } else if (arg == "--series-uid" && i + 1 < argc) {
            opts.series_uid = argv[++i];
        } else if (arg == "--sop-instance-uid" && i + 1 < argc) {
            opts.sop_instance_uid = argv[++i];
        } else if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            opts.output_dir = argv[++i];
        } else if (arg == "--structure" && i + 1 < argc) {
            std::string struct_str = argv[++i];
            if (struct_str == "hierarchical") {
                opts.structure = storage_structure::hierarchical;
            } else if (struct_str == "flat") {
                opts.structure = storage_structure::flat;
            } else {
                std::cerr << "Error: Invalid structure (use 'hierarchical' or 'flat')\n";
                return false;
            }
        } else if (arg == "--overwrite") {
            opts.overwrite = true;
        } else if (arg == "--no-progress") {
            opts.show_progress = false;
        } else if (arg == "--calling-ae" && i + 1 < argc) {
            opts.calling_ae = argv[++i];
            if (opts.calling_ae.length() > 16) {
                std::cerr << "Error: Calling AE title exceeds 16 characters\n";
                return false;
            }
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    return true;
}

/**
 * @brief Validate options consistency
 */
bool validate_options(const options& opts) {
    // C-MOVE requires destination AE
    if (opts.mode == retrieve_mode::c_move && opts.move_destination.empty()) {
        std::cerr << "Error: C-MOVE mode requires --dest-ae option\n";
        return false;
    }

    // At least one identifier required
    bool has_identifier = !opts.patient_id.empty() ||
                          !opts.study_uid.empty() ||
                          !opts.series_uid.empty() ||
                          !opts.sop_instance_uid.empty();

    if (!has_identifier) {
        std::cerr << "Error: At least one identifier is required "
                  << "(--patient-id, --study-uid, --series-uid, or --sop-instance-uid)\n";
        return false;
    }

    // Validate level matches identifiers
    switch (opts.level) {
        case retrieve_level::patient:
            if (opts.patient_id.empty()) {
                std::cerr << "Error: PATIENT level requires --patient-id\n";
                return false;
            }
            break;
        case retrieve_level::study:
            if (opts.study_uid.empty()) {
                std::cerr << "Error: STUDY level requires --study-uid\n";
                return false;
            }
            break;
        case retrieve_level::series:
            if (opts.series_uid.empty()) {
                std::cerr << "Error: SERIES level requires --series-uid\n";
                return false;
            }
            break;
        case retrieve_level::image:
            if (opts.sop_instance_uid.empty()) {
                std::cerr << "Error: IMAGE level requires --sop-instance-uid\n";
                return false;
            }
            break;
    }

    return true;
}

/**
 * @brief Get SOP Class UID for the retrieve operation
 */
std::string_view get_retrieve_sop_class_uid(const options& opts) {
    if (opts.mode == retrieve_mode::c_move) {
        if (opts.query_model == "patient") {
            return pacs::services::patient_root_move_sop_class_uid;
        }
        return pacs::services::study_root_move_sop_class_uid;
    } else {
        if (opts.query_model == "patient") {
            return pacs::services::patient_root_get_sop_class_uid;
        }
        return pacs::services::study_root_get_sop_class_uid;
    }
}

/**
 * @brief Build retrieve query dataset
 */
pacs::core::dicom_dataset build_query_dataset(const options& opts) {
    using namespace pacs::core;

    dicom_dataset ds;

    // Set Query/Retrieve Level
    std::string level_str{to_string(opts.level)};
    ds.set_string(tags::query_retrieve_level, pacs::encoding::vr_type::CS, level_str);

    // Set identifiers based on level
    if (!opts.patient_id.empty()) {
        ds.set_string(tags::patient_id, pacs::encoding::vr_type::LO, opts.patient_id);
    }

    if (!opts.study_uid.empty()) {
        ds.set_string(tags::study_instance_uid, pacs::encoding::vr_type::UI, opts.study_uid);
    }

    if (!opts.series_uid.empty()) {
        ds.set_string(tags::series_instance_uid, pacs::encoding::vr_type::UI, opts.series_uid);
    }

    if (!opts.sop_instance_uid.empty()) {
        ds.set_string(tags::sop_instance_uid, pacs::encoding::vr_type::UI, opts.sop_instance_uid);
    }

    return ds;
}

/**
 * @brief Progress display state
 */
struct progress_state {
    std::atomic<uint16_t> remaining{0};
    std::atomic<uint16_t> completed{0};
    std::atomic<uint16_t> failed{0};
    std::atomic<uint16_t> warning{0};
    std::atomic<size_t> bytes_received{0};
    std::chrono::steady_clock::time_point start_time;

    void reset() {
        remaining = 0;
        completed = 0;
        failed = 0;
        warning = 0;
        bytes_received = 0;
        start_time = std::chrono::steady_clock::now();
    }

    [[nodiscard]] uint16_t total() const {
        return remaining + completed + failed + warning;
    }
};

/**
 * @brief Display progress bar
 */
void display_progress(const progress_state& state, bool verbose) {
    auto total = state.total();
    if (total == 0) return;

    uint16_t done = state.completed + state.failed + state.warning;
    float progress = static_cast<float>(done) / total;

    // Calculate elapsed time and speed
    auto elapsed = std::chrono::steady_clock::now() - state.start_time;
    auto elapsed_sec = std::chrono::duration<double>(elapsed).count();
    double speed = elapsed_sec > 0 ? state.bytes_received / elapsed_sec / 1024.0 : 0;

    // Clear line and print progress
    std::cout << "\r";

    // Progress bar
    std::cout << "[";
    int filled = static_cast<int>(progress * progress_bar_width);
    for (int i = 0; i < progress_bar_width; ++i) {
        if (i < filled) std::cout << "=";
        else if (i == filled) std::cout << ">";
        else std::cout << " ";
    }
    std::cout << "] ";

    // Percentage and counts
    std::cout << std::fixed << std::setprecision(1) << (progress * 100) << "% ";
    std::cout << "(" << done << "/" << total << ") ";

    if (verbose) {
        std::cout << std::setprecision(1) << speed << " KB/s ";
        if (state.failed > 0) {
            std::cout << "[" << state.failed << " failed] ";
        }
    }

    std::cout << std::flush;
}

/**
 * @brief Generate file path for received DICOM file
 */
std::filesystem::path generate_file_path(
    const options& opts,
    const pacs::core::dicom_dataset& dataset) {

    using namespace pacs::core;

    std::filesystem::path path = opts.output_dir;

    if (opts.structure == storage_structure::hierarchical) {
        // Get identifiers for path (using default_value parameter)
        auto patient_id = dataset.get_string(tags::patient_id, "UNKNOWN");
        auto study_uid = dataset.get_string(tags::study_instance_uid, "UNKNOWN");
        auto series_uid = dataset.get_string(tags::series_instance_uid, "UNKNOWN");
        auto sop_uid = dataset.get_string(tags::sop_instance_uid, "UNKNOWN");

        // Build hierarchical path
        path /= patient_id;
        path /= study_uid;
        path /= series_uid;
        path /= sop_uid + ".dcm";
    } else {
        // Flat structure - just use SOP Instance UID
        auto sop_uid = dataset.get_string(tags::sop_instance_uid, "UNKNOWN");
        path /= sop_uid + ".dcm";
    }

    return path;
}

/**
 * @brief Save DICOM dataset to file
 */
bool save_dicom_file(
    const std::filesystem::path& path,
    const pacs::core::dicom_dataset& dataset,
    bool overwrite) {

    // Check if file exists
    if (std::filesystem::exists(path) && !overwrite) {
        return true;  // Skip existing file (not an error)
    }

    // Create parent directories
    std::filesystem::create_directories(path.parent_path());

    // Create DICOM file using static factory method
    auto file = pacs::core::dicom_file::create(
        dataset,
        pacs::encoding::transfer_syntax::explicit_vr_little_endian);

    auto result = file.save(path);
    return result.is_ok();
}

/**
 * @brief Create C-MOVE request message
 */
pacs::network::dimse::dimse_message make_c_move_rq(
    uint16_t message_id,
    std::string_view sop_class_uid,
    std::string_view move_destination) {

    using namespace pacs::network::dimse;

    dimse_message msg{command_field::c_move_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_priority(priority_medium);

    // Set Move Destination AE
    msg.command_set().set_string(
        tag_move_destination,
        pacs::encoding::vr_type::AE,
        std::string(move_destination));

    return msg;
}

/**
 * @brief Create C-GET request message
 */
pacs::network::dimse::dimse_message make_c_get_rq(
    uint16_t message_id,
    std::string_view sop_class_uid) {

    using namespace pacs::network::dimse;

    dimse_message msg{command_field::c_get_rq, message_id};
    msg.set_affected_sop_class_uid(sop_class_uid);
    msg.set_priority(priority_medium);

    return msg;
}

/**
 * @brief Perform C-GET retrieval
 */
int perform_c_get(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;

    auto sop_class_uid = get_retrieve_sop_class_uid(opts);

    if (opts.verbose) {
        std::cout << "Performing C-GET retrieval\n";
        std::cout << "  Host:        " << opts.host << ":" << opts.port << "\n";
        std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
        std::cout << "  Called AE:   " << opts.called_ae << "\n";
        std::cout << "  Query Model: " << opts.query_model << " root\n";
        std::cout << "  Level:       " << to_string(opts.level) << "\n";
        std::cout << "  Output:      " << opts.output_dir << "\n\n";
    }

    // Create output directory
    std::filesystem::create_directories(opts.output_dir);

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "RETRIEVE_SCU_01";

    // Propose C-GET SOP Class
    config.proposed_contexts.push_back({
        1,  // Context ID
        std::string(sop_class_uid),
        {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        }
    });

    // For C-GET, we need to propose storage SOP classes as SCP role
    // to receive the C-STORE sub-operations
    // Common storage SOP classes
    static const std::vector<std::string_view> storage_sop_classes = {
        "1.2.840.10008.5.1.4.1.1.2",      // CT Image Storage
        "1.2.840.10008.5.1.4.1.1.4",      // MR Image Storage
        "1.2.840.10008.5.1.4.1.1.7",      // Secondary Capture Image Storage
        "1.2.840.10008.5.1.4.1.1.1",      // CR Image Storage
        "1.2.840.10008.5.1.4.1.1.1.1",    // Digital X-Ray Image Storage
        "1.2.840.10008.5.1.4.1.1.12.1",   // X-Ray Angiographic Image Storage
        "1.2.840.10008.5.1.4.1.1.6.1",    // US Image Storage
        "1.2.840.10008.5.1.4.1.1.88.11",  // Basic Text SR
        "1.2.840.10008.5.1.4.1.1.88.22",  // Enhanced SR
    };

    uint8_t context_id = 3;  // Start from 3 (odd numbers for proposed contexts)
    for (auto sop_class : storage_sop_classes) {
        config.proposed_contexts.push_back({
            context_id,
            std::string(sop_class),
            {
                "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
                "1.2.840.10008.1.2"     // Implicit VR Little Endian
            }
        });
        context_id += 2;
    }

    // Establish association
    auto start_time = std::chrono::steady_clock::now();
    auto connect_result = association::connect(opts.host, opts.port, config, default_timeout);

    if (connect_result.is_err()) {
        std::cerr << "Failed to establish association: "
                  << connect_result.error().message << "\n";
        return 2;
    }

    auto& assoc = connect_result.value();

    if (opts.verbose) {
        auto connect_time = std::chrono::steady_clock::now();
        auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            connect_time - start_time);
        std::cout << "Association established in " << connect_duration.count() << " ms\n";
    }

    // Check if C-GET context was accepted
    if (!assoc.has_accepted_context(sop_class_uid)) {
        std::cerr << "Error: C-GET SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    auto context_id_opt = assoc.accepted_context_id(sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t get_context_id = *context_id_opt;

    // Build query dataset
    auto query_ds = build_query_dataset(opts);

    // Create C-GET request
    auto get_rq = make_c_get_rq(1, sop_class_uid);
    get_rq.set_dataset(std::move(query_ds));

    if (opts.verbose) {
        std::cout << "Sending C-GET request...\n";
    }

    // Send C-GET request
    auto send_result = assoc.send_dimse(get_context_id, get_rq);
    if (send_result.is_err()) {
        std::cerr << "Failed to send C-GET: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Progress tracking
    progress_state progress;
    progress.reset();

    // Process responses and C-STORE sub-operations
    bool retrieve_complete = false;
    uint16_t total_completed = 0;
    uint16_t total_failed = 0;
    uint16_t total_warning = 0;

    while (!retrieve_complete) {
        auto recv_result = assoc.receive_dimse(default_timeout);
        if (recv_result.is_err()) {
            std::cerr << "\nFailed to receive response: "
                      << recv_result.error().message << "\n";
            assoc.abort();
            return 2;
        }

        auto& [recv_context_id, msg] = recv_result.value();
        auto cmd = msg.command();

        if (cmd == command_field::c_get_rsp) {
            // C-GET response
            auto status = msg.status();

            // Update progress from response
            if (auto remaining = msg.remaining_subops()) {
                progress.remaining = *remaining;
            }
            if (auto completed = msg.completed_subops()) {
                progress.completed = *completed;
                total_completed = *completed;
            }
            if (auto failed = msg.failed_subops()) {
                progress.failed = *failed;
                total_failed = *failed;
            }
            if (auto warning = msg.warning_subops()) {
                progress.warning = *warning;
                total_warning = *warning;
            }

            if (opts.show_progress) {
                display_progress(progress, opts.verbose);
            }

            // Check if retrieval is complete
            if (status == status_success ||
                status == status_cancel ||
                (status & 0xF000) == 0xA000 ||  // Failure
                (status & 0xF000) == 0xC000) {  // Unable to process

                retrieve_complete = true;

                if (status != status_success && status != status_cancel) {
                    std::cerr << "\nC-GET failed with status: 0x"
                              << std::hex << status << std::dec << "\n";
                }
            }

        } else if (cmd == command_field::c_store_rq) {
            // Incoming C-STORE sub-operation
            if (msg.has_dataset()) {
                const auto& dataset = msg.dataset();

                // Generate file path
                auto file_path = generate_file_path(opts, dataset);

                // Save file
                bool saved = save_dicom_file(file_path, dataset, opts.overwrite);

                // Update bytes received
                progress.bytes_received += 1024;  // Approximate

                // Send C-STORE response
                auto sop_class = msg.affected_sop_class_uid();
                auto sop_instance = msg.affected_sop_instance_uid();

                auto store_rsp = make_c_store_rsp(
                    msg.message_id(),
                    sop_class,
                    sop_instance,
                    saved ? status_success : 0xA700  // Out of resources
                );

                auto send_rsp_result = assoc.send_dimse(recv_context_id, store_rsp);
                if (send_rsp_result.is_err() && opts.verbose) {
                    std::cerr << "\nWarning: Failed to send C-STORE response\n";
                }

                if (opts.verbose && !saved) {
                    std::cerr << "\nWarning: Failed to save " << file_path << "\n";
                }
            }
        }
    }

    // Clear progress line
    if (opts.show_progress) {
        std::cout << "\n";
    }

    // Release association gracefully
    if (opts.verbose) {
        std::cout << "Releasing association...\n";
    }

    auto release_result = assoc.release(default_timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
    }

    // Print summary
    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    std::cout << "\n========================================\n";
    std::cout << "           Retrieve Summary\n";
    std::cout << "========================================\n";
    std::cout << "  Mode:            C-GET\n";
    std::cout << "  Level:           " << to_string(opts.level) << "\n";
    std::cout << "  Output:          " << opts.output_dir << "\n";
    std::cout << "  ----------------------------------------\n";
    std::cout << "  Completed:       " << total_completed << "\n";
    if (total_warning > 0) {
        std::cout << "  Warnings:        " << total_warning << "\n";
    }
    if (total_failed > 0) {
        std::cout << "  Failed:          " << total_failed << "\n";
    }
    std::cout << "  Total time:      " << total_duration.count() << " ms\n";
    std::cout << "========================================\n";

    // Return appropriate exit code
    if (total_failed > 0 && total_completed == 0) {
        return 2;  // Complete failure
    } else if (total_failed > 0) {
        return 1;  // Partial failure
    }
    return 0;  // Success
}

/**
 * @brief Perform C-MOVE retrieval
 */
int perform_c_move(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;

    auto sop_class_uid = get_retrieve_sop_class_uid(opts);

    if (opts.verbose) {
        std::cout << "Performing C-MOVE retrieval\n";
        std::cout << "  Host:        " << opts.host << ":" << opts.port << "\n";
        std::cout << "  Calling AE:  " << opts.calling_ae << "\n";
        std::cout << "  Called AE:   " << opts.called_ae << "\n";
        std::cout << "  Destination: " << opts.move_destination << "\n";
        std::cout << "  Query Model: " << opts.query_model << " root\n";
        std::cout << "  Level:       " << to_string(opts.level) << "\n\n";
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae;
    config.called_ae_title = opts.called_ae;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "RETRIEVE_SCU_01";

    // Propose C-MOVE SOP Class
    config.proposed_contexts.push_back({
        1,  // Context ID
        std::string(sop_class_uid),
        {
            "1.2.840.10008.1.2.1",  // Explicit VR Little Endian
            "1.2.840.10008.1.2"     // Implicit VR Little Endian
        }
    });

    // Establish association
    auto start_time = std::chrono::steady_clock::now();
    auto connect_result = association::connect(opts.host, opts.port, config, default_timeout);

    if (connect_result.is_err()) {
        std::cerr << "Failed to establish association: "
                  << connect_result.error().message << "\n";
        return 2;
    }

    auto& assoc = connect_result.value();

    if (opts.verbose) {
        auto connect_time = std::chrono::steady_clock::now();
        auto connect_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            connect_time - start_time);
        std::cout << "Association established in " << connect_duration.count() << " ms\n";
    }

    // Check if C-MOVE context was accepted
    if (!assoc.has_accepted_context(sop_class_uid)) {
        std::cerr << "Error: C-MOVE SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    auto context_id_opt = assoc.accepted_context_id(sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t move_context_id = *context_id_opt;

    // Build query dataset
    auto query_ds = build_query_dataset(opts);

    // Create C-MOVE request
    auto move_rq = make_c_move_rq(1, sop_class_uid, opts.move_destination);
    move_rq.set_dataset(std::move(query_ds));

    if (opts.verbose) {
        std::cout << "Sending C-MOVE request to move images to " << opts.move_destination << "...\n";
    }

    // Send C-MOVE request
    auto send_result = assoc.send_dimse(move_context_id, move_rq);
    if (send_result.is_err()) {
        std::cerr << "Failed to send C-MOVE: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Progress tracking
    progress_state progress;
    progress.reset();

    // Process C-MOVE responses
    bool move_complete = false;
    uint16_t total_completed = 0;
    uint16_t total_failed = 0;
    uint16_t total_warning = 0;

    while (!move_complete) {
        auto recv_result = assoc.receive_dimse(default_timeout);
        if (recv_result.is_err()) {
            std::cerr << "\nFailed to receive C-MOVE response: "
                      << recv_result.error().message << "\n";
            assoc.abort();
            return 2;
        }

        auto& [recv_context_id, msg] = recv_result.value();

        if (msg.command() != command_field::c_move_rsp) {
            std::cerr << "\nError: Unexpected response (expected C-MOVE-RSP)\n";
            assoc.abort();
            return 2;
        }

        auto status = msg.status();

        // Update progress from response
        if (auto remaining = msg.remaining_subops()) {
            progress.remaining = *remaining;
        }
        if (auto completed = msg.completed_subops()) {
            progress.completed = *completed;
            total_completed = *completed;
        }
        if (auto failed = msg.failed_subops()) {
            progress.failed = *failed;
            total_failed = *failed;
        }
        if (auto warning = msg.warning_subops()) {
            progress.warning = *warning;
            total_warning = *warning;
        }

        if (opts.show_progress) {
            display_progress(progress, opts.verbose);
        }

        // Check if C-MOVE is complete
        if (status == status_success ||
            status == status_cancel ||
            (status & 0xF000) == 0xA000 ||  // Failure
            (status & 0xF000) == 0xC000) {  // Unable to process

            move_complete = true;

            if (status != status_success && status != status_cancel) {
                std::cerr << "\nC-MOVE failed with status: 0x"
                          << std::hex << status << std::dec << "\n";
            }
        }
    }

    // Clear progress line
    if (opts.show_progress) {
        std::cout << "\n";
    }

    // Release association gracefully
    if (opts.verbose) {
        std::cout << "Releasing association...\n";
    }

    auto release_result = assoc.release(default_timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: " << release_result.error().message << "\n";
    }

    // Print summary
    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    std::cout << "\n========================================\n";
    std::cout << "           Retrieve Summary\n";
    std::cout << "========================================\n";
    std::cout << "  Mode:            C-MOVE\n";
    std::cout << "  Destination:     " << opts.move_destination << "\n";
    std::cout << "  Level:           " << to_string(opts.level) << "\n";
    std::cout << "  ----------------------------------------\n";
    std::cout << "  Completed:       " << total_completed << "\n";
    if (total_warning > 0) {
        std::cout << "  Warnings:        " << total_warning << "\n";
    }
    if (total_failed > 0) {
        std::cout << "  Failed:          " << total_failed << "\n";
    }
    std::cout << "  Total time:      " << total_duration.count() << " ms\n";
    std::cout << "========================================\n";

    // Return appropriate exit code
    if (total_failed > 0 && total_completed == 0) {
        return 2;  // Complete failure
    } else if (total_failed > 0) {
        return 1;  // Partial failure
    }
    return 0;  // Success
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
  ____  _____ _____ ____  ___ _______     _______   ____   ____ _   _
 |  _ \| ____|_   _|  _ \|_ _| ____\ \   / / ____| / ___| / ___| | | |
 | |_) |  _|   | | | |_) || ||  _|  \ \ / /|  _|   \___ \| |   | | | |
 |  _ <| |___  | | |  _ < | || |___  \ V / | |___   ___) | |___| |_| |
 |_| \_\_____| |_| |_| \_\___|_____|  \_/  |_____| |____/ \____|\___/

          DICOM C-MOVE/C-GET Client
)" << "\n";

    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        print_usage(argv[0]);
        return 2;
    }

    if (!validate_options(opts)) {
        return 2;
    }

    // Perform retrieval based on mode
    if (opts.mode == retrieve_mode::c_move) {
        return perform_c_move(opts);
    } else {
        return perform_c_get(opts);
    }
}
