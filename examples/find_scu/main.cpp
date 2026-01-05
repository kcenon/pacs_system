/**
 * @file main.cpp
 * @brief find_scu - DICOM C-FIND SCU utility (dcmtk-compatible)
 *
 * A command-line utility for querying PACS for patient, study, series, or
 * instance information. Provides dcmtk-compatible interface with -P/-S/-W
 * options and -k query key specification.
 *
 * @see Issue #289 - find_scu: Implement C-FIND SCU utility (Query)
 * @see Issue #286 - [Epic] PACS SCU/SCP Utilities Implementation
 * @see DICOM PS3.4 Section C - Query/Retrieve Service Class
 * @see DICOM PS3.7 Section 9.1.2 - C-FIND Service
 * @see https://support.dcmtk.org/docs/findscu.html
 *
 * Usage:
 *   find_scu [options] <peer> <port>
 *
 * Example:
 *   find_scu -P -L STUDY -k "0010,0010=Smith*" localhost 11112
 *   find_scu -S -k "0008,0060=CT" -k "0008,0020=20240101-20241231" pacs.example.com 104
 */

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/association.hpp"
#include "pacs/network/dimse/dimse_message.hpp"
#include "pacs/services/query_scp.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

// =============================================================================
// Constants
// =============================================================================

constexpr const char* version_string = "1.0.0";
constexpr const char* default_calling_ae = "FINDSCU";
constexpr const char* default_called_ae = "ANY-SCP";
constexpr auto default_timeout = std::chrono::seconds{30};
constexpr size_t max_ae_title_length = 16;

// =============================================================================
// Query Model and Level
// =============================================================================

enum class query_model {
    patient_root,
    study_root
};

enum class query_level {
    patient,
    study,
    series,
    image
};

enum class output_format {
    text,
    json,
    xml,
    csv
};

// =============================================================================
// Query Key
// =============================================================================

struct query_key {
    pacs::core::dicom_tag tag;
    std::string value;
};

// =============================================================================
// Command Line Options
// =============================================================================

struct options {
    // Network options
    std::string peer_host;
    uint16_t peer_port{0};
    std::string calling_ae_title{default_calling_ae};
    std::string called_ae_title{default_called_ae};

    // Timeout options
    std::chrono::seconds connection_timeout{default_timeout};
    std::chrono::seconds acse_timeout{default_timeout};
    std::chrono::seconds dimse_timeout{0};  // 0 = infinite

    // Query model and level
    query_model model{query_model::patient_root};
    query_level level{query_level::study};

    // Query keys
    std::vector<query_key> keys;
    std::string query_file;

    // Output options
    output_format format{output_format::text};
    std::string output_file;
    bool extract_to_files{false};
    size_t max_results{0};  // 0 = unlimited

    // Verbosity
    bool verbose{false};
    bool debug{false};
    bool quiet{false};

    // Help/version flags
    bool show_help{false};
    bool show_version{false};
};

// =============================================================================
// Utility Functions
// =============================================================================

std::string_view query_model_to_string(query_model model) {
    switch (model) {
        case query_model::patient_root: return "Patient Root";
        case query_model::study_root: return "Study Root";
        default: return "Unknown";
    }
}

std::string_view query_level_to_string(query_level level) {
    switch (level) {
        case query_level::patient: return "PATIENT";
        case query_level::study: return "STUDY";
        case query_level::series: return "SERIES";
        case query_level::image: return "IMAGE";
        default: return "UNKNOWN";
    }
}

std::string_view get_find_sop_class_uid(query_model model) {
    switch (model) {
        case query_model::patient_root:
            return pacs::services::patient_root_find_sop_class_uid;
        case query_model::study_root:
            return pacs::services::study_root_find_sop_class_uid;
        default:
            return pacs::services::patient_root_find_sop_class_uid;
    }
}

// =============================================================================
// Output Functions
// =============================================================================

void print_banner() {
    std::cout << R"(
  _____ ___ _   _ ____    ____   ____ _   _
 |  ___|_ _| \ | |  _ \  / ___| / ___| | | |
 | |_   | ||  \| | | | | \___ \| |   | | | |
 |  _|  | || |\  | |_| |  ___) | |___| |_| |
 |_|   |___|_| \_|____/  |____/ \____|\___/

        DICOM C-FIND Client v)" << version_string << R"(
)" << "\n";
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << R"( [options] <peer> <port>

Arguments:
  peer                          Remote host address (IP or hostname)
  port                          Remote port number (typically 104 or 11112)

Options:
  -h, --help                    Show this help message and exit
  -v, --verbose                 Verbose output mode
  -d, --debug                   Debug output mode (more details than verbose)
  -q, --quiet                   Quiet mode (minimal output)
  --version                     Show version information

Network Options:
  -aet, --aetitle <aetitle>     Calling AE Title (default: FINDSCU)
  -aec, --call <aetitle>        Called AE Title (default: ANY-SCP)
  -to, --timeout <seconds>      Connection timeout (default: 30)
  -ta, --acse-timeout <seconds> ACSE timeout (default: 30)
  -td, --dimse-timeout <seconds> DIMSE timeout (default: 0=infinite)

Query Model:
  -P, --patient-root            Patient Root Query Model (default)
  -S, --study-root              Study Root Query Model

Query Level:
  -L, --level <level>           Query level (PATIENT|STUDY|SERIES|IMAGE)

Query Keys:
  -k, --key <tag=value>         Query key (e.g., 0010,0010=Smith*)
                                Multiple -k options allowed
  -f, --query-file <file>       Read query keys from file

Output Options:
  -o, --output <format>         Output format (text|json|xml|csv)
  --output-file <file>          Write results to file
  -X, --extract                 Extract results to DICOM files
  --max-results <n>             Maximum number of results (0=unlimited)

Common Query Keys:
  Patient Level:
    (0010,0010) PatientName         (0010,0020) PatientID
    (0010,0030) PatientBirthDate    (0010,0040) PatientSex

  Study Level:
    (0020,000D) StudyInstanceUID    (0008,0020) StudyDate
    (0008,0030) StudyTime           (0008,0050) AccessionNumber
    (0008,1030) StudyDescription    (0008,0060) Modality

  Series Level:
    (0020,000E) SeriesInstanceUID   (0008,0060) Modality
    (0020,0011) SeriesNumber        (0008,103E) SeriesDescription

  Image Level:
    (0008,0018) SOPInstanceUID      (0020,0013) InstanceNumber

Examples:
  # Find all studies for a patient
  )" << program_name << R"( -P -L STUDY -k "0010,0010=Smith*" localhost 11112

  # Find CT studies in date range
  )" << program_name << R"( -S -L STUDY \
    -k "0008,0060=CT" \
    -k "0008,0020=20240101-20241231" \
    pacs.example.com 104

  # Find series for a study
  )" << program_name << R"( -S -L SERIES \
    -k "0020,000D=1.2.840..." \
    -o json \
    localhost 11112

  # Query with file
  )" << program_name << R"( -f query_keys.txt localhost 11112

Query File Format (one key per line):
  (0010,0010)=Smith*
  (0010,0020)=
  (0008,0020)=20240101-20241231

Exit Codes:
  0  Success - Query completed
  1  Error - Query failed or no results
  2  Error - Invalid arguments or connection failure
)";
}

void print_version() {
    std::cout << "find_scu version " << version_string << "\n";
    std::cout << "PACS System DICOM Utilities\n";
    std::cout << "Copyright (c) 2024\n";
}

// =============================================================================
// Argument Parsing
// =============================================================================

bool parse_timeout(const std::string& value, std::chrono::seconds& result,
                   const std::string& option_name) {
    try {
        int seconds = std::stoi(value);
        if (seconds < 0) {
            std::cerr << "Error: " << option_name << " must be non-negative\n";
            return false;
        }
        result = std::chrono::seconds{seconds};
        return true;
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid value for " << option_name << ": '"
                  << value << "'\n";
        return false;
    }
}

bool validate_ae_title(const std::string& ae_title,
                       const std::string& option_name) {
    if (ae_title.empty()) {
        std::cerr << "Error: " << option_name << " cannot be empty\n";
        return false;
    }
    if (ae_title.length() > max_ae_title_length) {
        std::cerr << "Error: " << option_name << " exceeds "
                  << max_ae_title_length << " characters\n";
        return false;
    }
    return true;
}

std::optional<query_level> parse_level(const std::string& level_str) {
    std::string upper = level_str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "PATIENT") return query_level::patient;
    if (upper == "STUDY") return query_level::study;
    if (upper == "SERIES") return query_level::series;
    if (upper == "IMAGE" || upper == "INSTANCE") return query_level::image;
    return std::nullopt;
}

bool parse_query_key(const std::string& key_str, query_key& key) {
    // Expected format: "gggg,eeee=value" or "(gggg,eeee)=value"
    std::regex key_regex(R"(\(?([0-9A-Fa-f]{4}),([0-9A-Fa-f]{4})\)?=?(.*))");
    std::smatch match;

    if (!std::regex_match(key_str, match, key_regex)) {
        std::cerr << "Error: Invalid query key format: '" << key_str << "'\n";
        std::cerr << "Expected format: gggg,eeee=value or (gggg,eeee)=value\n";
        return false;
    }

    uint16_t group = static_cast<uint16_t>(std::stoul(match[1].str(), nullptr, 16));
    uint16_t element = static_cast<uint16_t>(std::stoul(match[2].str(), nullptr, 16));

    key.tag = pacs::core::dicom_tag{group, element};
    key.value = match[3].str();

    return true;
}

bool load_query_file(const std::string& filename, std::vector<query_key>& keys) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open query file: " << filename << "\n";
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(file, line)) {
        ++line_num;

        // Skip empty lines and comments
        auto pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos || line[pos] == '#') {
            continue;
        }

        query_key key;
        if (!parse_query_key(line, key)) {
            std::cerr << "  (at line " << line_num << ")\n";
            return false;
        }
        keys.push_back(key);
    }

    return true;
}

bool parse_arguments(int argc, char* argv[], options& opts) {
    std::vector<std::string> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Help options
        if (arg == "-h" || arg == "--help") {
            opts.show_help = true;
            return true;
        }
        if (arg == "--version") {
            opts.show_version = true;
            return true;
        }

        // Verbosity options
        if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
            continue;
        }
        if (arg == "-d" || arg == "--debug") {
            opts.debug = true;
            opts.verbose = true;
            continue;
        }
        if (arg == "-q" || arg == "--quiet") {
            opts.quiet = true;
            continue;
        }

        // Network options
        if ((arg == "-aet" || arg == "--aetitle") && i + 1 < argc) {
            opts.calling_ae_title = argv[++i];
            if (!validate_ae_title(opts.calling_ae_title, "Calling AE Title")) {
                return false;
            }
            continue;
        }
        if ((arg == "-aec" || arg == "--call") && i + 1 < argc) {
            opts.called_ae_title = argv[++i];
            if (!validate_ae_title(opts.called_ae_title, "Called AE Title")) {
                return false;
            }
            continue;
        }

        // Timeout options
        if ((arg == "-to" || arg == "--timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], opts.connection_timeout, "timeout")) {
                return false;
            }
            continue;
        }
        if ((arg == "-ta" || arg == "--acse-timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], opts.acse_timeout, "ACSE timeout")) {
                return false;
            }
            continue;
        }
        if ((arg == "-td" || arg == "--dimse-timeout") && i + 1 < argc) {
            if (!parse_timeout(argv[++i], opts.dimse_timeout, "DIMSE timeout")) {
                return false;
            }
            continue;
        }

        // Query model
        if (arg == "-P" || arg == "--patient-root") {
            opts.model = query_model::patient_root;
            continue;
        }
        if (arg == "-S" || arg == "--study-root") {
            opts.model = query_model::study_root;
            continue;
        }

        // Query level
        if ((arg == "-L" || arg == "--level") && i + 1 < argc) {
            auto level = parse_level(argv[++i]);
            if (!level) {
                std::cerr << "Error: Invalid query level: '" << argv[i] << "'\n";
                return false;
            }
            opts.level = *level;
            continue;
        }

        // Query keys
        if ((arg == "-k" || arg == "--key") && i + 1 < argc) {
            query_key key;
            if (!parse_query_key(argv[++i], key)) {
                return false;
            }
            opts.keys.push_back(key);
            continue;
        }
        if ((arg == "-f" || arg == "--query-file") && i + 1 < argc) {
            opts.query_file = argv[++i];
            continue;
        }

        // Output options
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            std::string fmt = argv[++i];
            if (fmt == "text") opts.format = output_format::text;
            else if (fmt == "json") opts.format = output_format::json;
            else if (fmt == "xml") opts.format = output_format::xml;
            else if (fmt == "csv") opts.format = output_format::csv;
            else {
                std::cerr << "Error: Invalid output format: '" << fmt << "'\n";
                return false;
            }
            continue;
        }
        if (arg == "--output-file" && i + 1 < argc) {
            opts.output_file = argv[++i];
            continue;
        }
        if (arg == "-X" || arg == "--extract") {
            opts.extract_to_files = true;
            continue;
        }
        if (arg == "--max-results" && i + 1 < argc) {
            try {
                opts.max_results = static_cast<size_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Error: Invalid max-results value\n";
                return false;
            }
            continue;
        }

        // Unknown option
        if (arg.starts_with("-")) {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }

        positional_args.push_back(arg);
    }

    // Validate positional arguments
    if (positional_args.size() != 2) {
        std::cerr << "Error: Expected <peer> <port> arguments\n";
        return false;
    }

    opts.peer_host = positional_args[0];

    try {
        int port_int = std::stoi(positional_args[1]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        opts.peer_port = static_cast<uint16_t>(port_int);
    } catch (...) {
        std::cerr << "Error: Invalid port number '" << positional_args[1] << "'\n";
        return false;
    }

    // Load query file if specified
    if (!opts.query_file.empty()) {
        if (!load_query_file(opts.query_file, opts.keys)) {
            return false;
        }
    }

    return true;
}

// =============================================================================
// Query Dataset Building
// =============================================================================

pacs::core::dicom_dataset build_query_dataset(const options& opts) {
    using namespace pacs::core;
    using namespace pacs::encoding;

    dicom_dataset ds;

    // Set Query/Retrieve Level
    std::string level_str{query_level_to_string(opts.level)};
    ds.set_string(tags::query_retrieve_level, vr_type::CS, level_str);

    // Add default return keys based on level
    switch (opts.level) {
        case query_level::patient:
            // Patient level return keys
            ds.set_string(tags::patient_name, vr_type::PN, "");
            ds.set_string(tags::patient_id, vr_type::LO, "");
            ds.set_string(tags::patient_birth_date, vr_type::DA, "");
            ds.set_string(tags::patient_sex, vr_type::CS, "");
            break;

        case query_level::study:
            // Patient info
            ds.set_string(tags::patient_name, vr_type::PN, "");
            ds.set_string(tags::patient_id, vr_type::LO, "");
            // Study info
            ds.set_string(tags::study_instance_uid, vr_type::UI, "");
            ds.set_string(tags::study_date, vr_type::DA, "");
            ds.set_string(tags::study_time, vr_type::TM, "");
            ds.set_string(tags::accession_number, vr_type::SH, "");
            ds.set_string(tags::study_id, vr_type::SH, "");
            ds.set_string(tags::study_description, vr_type::LO, "");
            ds.set_string(tags::modalities_in_study, vr_type::CS, "");
            ds.set_string(tags::number_of_study_related_series, vr_type::IS, "");
            ds.set_string(tags::number_of_study_related_instances, vr_type::IS, "");
            break;

        case query_level::series:
            // Series info
            ds.set_string(tags::series_instance_uid, vr_type::UI, "");
            ds.set_string(tags::modality, vr_type::CS, "");
            ds.set_string(tags::series_number, vr_type::IS, "");
            ds.set_string(tags::series_description, vr_type::LO, "");
            ds.set_string(tags::number_of_series_related_instances, vr_type::IS, "");
            break;

        case query_level::image:
            // Image info
            ds.set_string(tags::sop_instance_uid, vr_type::UI, "");
            ds.set_string(tags::sop_class_uid, vr_type::UI, "");
            ds.set_string(tags::instance_number, vr_type::IS, "");
            break;
    }

    // Apply user-specified query keys
    for (const auto& key : opts.keys) {
        ds.set_string(key.tag, vr_type::UN, key.value);
    }

    return ds;
}

// =============================================================================
// Result Formatting
// =============================================================================

void format_text_result(std::ostream& os, const pacs::core::dicom_dataset& ds,
                        size_t index) {
    os << "Result " << (index + 1) << ":\n";

    for (const auto& [tag, element] : ds) {
        auto value = ds.get_string(tag, "");

        os << "  (" << std::hex << std::setw(4) << std::setfill('0')
           << tag.group() << ","
           << std::setw(4) << std::setfill('0') << tag.element() << std::dec
           << ") = \"" << value << "\"\n";
    }
    os << "\n";
}

void format_json_results(std::ostream& os,
                         const std::vector<pacs::core::dicom_dataset>& results) {
    os << "[\n";
    for (size_t i = 0; i < results.size(); ++i) {
        os << "  {\n";
        const auto& ds = results[i];
        bool first = true;
        for (const auto& [tag, element] : ds) {
            if (!first) os << ",\n";
            first = false;

            auto value = ds.get_string(tag, "");

            std::ostringstream tag_str;
            tag_str << std::hex << std::uppercase << std::setw(4)
                    << std::setfill('0') << tag.group()
                    << std::setw(4) << std::setfill('0') << tag.element();

            os << "    \"" << tag_str.str() << "\": \"" << value << "\"";
        }
        os << "\n  }";
        if (i < results.size() - 1) os << ",";
        os << "\n";
    }
    os << "]\n";
}

void format_csv_results(std::ostream& os,
                        const std::vector<pacs::core::dicom_dataset>& results,
                        query_level level) {
    // Header based on level
    switch (level) {
        case query_level::patient:
            os << "PatientName,PatientID,PatientBirthDate,PatientSex\n";
            break;
        case query_level::study:
            os << "PatientName,PatientID,StudyInstanceUID,StudyDate,StudyTime,"
               << "AccessionNumber,StudyDescription,Modalities\n";
            break;
        case query_level::series:
            os << "SeriesInstanceUID,Modality,SeriesNumber,SeriesDescription,"
               << "NumberOfInstances\n";
            break;
        case query_level::image:
            os << "SOPInstanceUID,SOPClassUID,InstanceNumber\n";
            break;
    }

    for (const auto& ds : results) {
        using namespace pacs::core;
        switch (level) {
            case query_level::patient:
                os << "\"" << ds.get_string(tags::patient_name, "") << "\","
                   << "\"" << ds.get_string(tags::patient_id, "") << "\","
                   << "\"" << ds.get_string(tags::patient_birth_date, "") << "\","
                   << "\"" << ds.get_string(tags::patient_sex, "") << "\"\n";
                break;
            case query_level::study:
                os << "\"" << ds.get_string(tags::patient_name, "") << "\","
                   << "\"" << ds.get_string(tags::patient_id, "") << "\","
                   << "\"" << ds.get_string(tags::study_instance_uid, "") << "\","
                   << "\"" << ds.get_string(tags::study_date, "") << "\","
                   << "\"" << ds.get_string(tags::study_time, "") << "\","
                   << "\"" << ds.get_string(tags::accession_number, "") << "\","
                   << "\"" << ds.get_string(tags::study_description, "") << "\","
                   << "\"" << ds.get_string(tags::modalities_in_study, "") << "\"\n";
                break;
            case query_level::series:
                os << "\"" << ds.get_string(tags::series_instance_uid, "") << "\","
                   << "\"" << ds.get_string(tags::modality, "") << "\","
                   << "\"" << ds.get_string(tags::series_number, "") << "\","
                   << "\"" << ds.get_string(tags::series_description, "") << "\","
                   << "\"" << ds.get_string(tags::number_of_series_related_instances, "") << "\"\n";
                break;
            case query_level::image:
                os << "\"" << ds.get_string(tags::sop_instance_uid, "") << "\","
                   << "\"" << ds.get_string(tags::sop_class_uid, "") << "\","
                   << "\"" << ds.get_string(tags::instance_number, "") << "\"\n";
                break;
        }
    }
}

// =============================================================================
// Query Implementation
// =============================================================================

int perform_query(const options& opts) {
    using namespace pacs::network;
    using namespace pacs::network::dimse;
    using namespace pacs::services;

    auto sop_class_uid = get_find_sop_class_uid(opts.model);

    // Print connection info
    if (!opts.quiet) {
        std::cout << "Requesting Association\n";
        if (opts.verbose) {
            std::cout << "  Peer:        " << opts.peer_host << ":"
                      << opts.peer_port << "\n";
            std::cout << "  Calling AE:  " << opts.calling_ae_title << "\n";
            std::cout << "  Called AE:   " << opts.called_ae_title << "\n";
            std::cout << "  Query Model: " << query_model_to_string(opts.model)
                      << "\n";
            std::cout << "  Query Level: " << query_level_to_string(opts.level)
                      << "\n";
            std::cout << "\n";
        }
    }

    // Configure association
    association_config config;
    config.calling_ae_title = opts.calling_ae_title;
    config.called_ae_title = opts.called_ae_title;
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.1";
    config.implementation_version_name = "FIND_SCU_100";

    // Propose Query SOP Class
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
    auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        opts.connection_timeout);
    auto connect_result = association::connect(opts.peer_host, opts.peer_port,
                                               config, timeout);

    if (connect_result.is_err()) {
        std::cerr << "Association Failed: " << connect_result.error().message
                  << "\n";
        return 2;
    }

    auto& assoc = connect_result.value();
    auto connect_time = std::chrono::steady_clock::now();

    if (!opts.quiet) {
        std::cout << "Association Accepted\n";
        if (opts.verbose) {
            auto connect_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    connect_time - start_time);
            std::cout << "  (established in " << connect_duration.count()
                      << " ms)\n";
        }
    }

    // Check if context was accepted
    if (!assoc.has_accepted_context(sop_class_uid)) {
        std::cerr << "Error: Query SOP Class not accepted by remote SCP\n";
        assoc.abort();
        return 2;
    }

    auto context_id_opt = assoc.accepted_context_id(sop_class_uid);
    if (!context_id_opt) {
        std::cerr << "Error: Could not get presentation context ID\n";
        assoc.abort();
        return 2;
    }
    uint8_t context_id = *context_id_opt;

    // Build query dataset
    auto query_ds = build_query_dataset(opts);

    // Create C-FIND request
    auto find_rq = make_c_find_rq(1, sop_class_uid);
    find_rq.set_dataset(std::move(query_ds));

    if (!opts.quiet && opts.verbose) {
        std::cout << "Sending C-FIND Request\n";
    }

    // Send C-FIND request
    auto send_result = assoc.send_dimse(context_id, find_rq);
    if (send_result.is_err()) {
        std::cerr << "Send Failed: " << send_result.error().message << "\n";
        assoc.abort();
        return 2;
    }

    // Receive responses
    std::vector<pacs::core::dicom_dataset> results;
    bool query_complete = false;
    size_t pending_count = 0;

    auto dimse_timeout = opts.dimse_timeout.count() > 0
        ? std::chrono::duration_cast<std::chrono::milliseconds>(opts.dimse_timeout)
        : std::chrono::milliseconds{30000};

    while (!query_complete) {
        auto recv_result = assoc.receive_dimse(dimse_timeout);
        if (recv_result.is_err()) {
            std::cerr << "Receive Failed: " << recv_result.error().message
                      << "\n";
            assoc.abort();
            return 2;
        }

        auto& [recv_context_id, find_rsp] = recv_result.value();

        if (find_rsp.command() != command_field::c_find_rsp) {
            std::cerr << "Error: Unexpected response (expected C-FIND-RSP)\n";
            assoc.abort();
            return 2;
        }

        auto status = find_rsp.status();

        if (status == status_pending || status == status_pending_warning) {
            ++pending_count;

            if (find_rsp.has_dataset()) {
                if (opts.max_results == 0 || results.size() < opts.max_results) {
                    auto dataset_result = find_rsp.dataset();
                    if (dataset_result.is_ok()) {
                        results.push_back(dataset_result.value().get());
                    }
                }
            }

            if (!opts.quiet && opts.verbose && pending_count % 10 == 0) {
                std::cout << "\rReceived " << pending_count << " results..."
                          << std::flush;
            }
        } else if (status == status_success) {
            query_complete = true;
        } else if (status == status_cancel) {
            query_complete = true;
            if (!opts.quiet) {
                std::cout << "Query was cancelled.\n";
            }
        } else {
            query_complete = true;
            std::cerr << "Query failed with status: 0x" << std::hex << status
                      << std::dec << "\n";
        }
    }

    if (!opts.quiet && opts.verbose) {
        std::cout << "\n";
    }

    // Release association
    if (!opts.quiet && opts.verbose) {
        std::cout << "Releasing Association\n";
    }

    auto release_result = assoc.release(timeout);
    if (release_result.is_err() && opts.verbose) {
        std::cerr << "Warning: Release failed: "
                  << release_result.error().message << "\n";
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Output results
    std::ostream* out = &std::cout;
    std::ofstream file_out;

    if (!opts.output_file.empty()) {
        file_out.open(opts.output_file);
        if (file_out.is_open()) {
            out = &file_out;
        } else {
            std::cerr << "Warning: Could not open output file: "
                      << opts.output_file << "\n";
        }
    }

    switch (opts.format) {
        case output_format::text:
            for (size_t i = 0; i < results.size(); ++i) {
                format_text_result(*out, results[i], i);
            }
            break;
        case output_format::json:
            format_json_results(*out, results);
            break;
        case output_format::csv:
            format_csv_results(*out, results, opts.level);
            break;
        case output_format::xml:
            // TODO: Implement XML output
            for (size_t i = 0; i < results.size(); ++i) {
                format_text_result(*out, results[i], i);
            }
            break;
    }

    // Print summary
    if (!opts.quiet) {
        std::cout << "\nTotal Results: " << results.size();
        if (opts.max_results > 0 && pending_count > opts.max_results) {
            std::cout << " (limited from " << pending_count << ")";
        }
        std::cout << "\n";

        if (opts.verbose) {
            std::cout << "Query Time: " << total_duration.count() << " ms\n";
        }

        std::cout << "Query Complete\n";
    }

    return results.empty() ? 1 : 0;
}

}  // namespace

// =============================================================================
// Main Entry Point
// =============================================================================

int main(int argc, char* argv[]) {
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        if (!opts.show_help && !opts.show_version) {
            std::cerr << "\nUse --help for usage information.\n";
            return 2;
        }
    }

    if (opts.show_version) {
        print_version();
        return 0;
    }

    if (opts.show_help) {
        print_banner();
        print_usage(argv[0]);
        return 0;
    }

    // Print banner unless quiet mode or structured output
    bool suppress_banner = opts.quiet ||
                           opts.format == output_format::json ||
                           opts.format == output_format::csv ||
                           opts.format == output_format::xml;

    if (!suppress_banner) {
        print_banner();
    }

    return perform_query(opts);
}
