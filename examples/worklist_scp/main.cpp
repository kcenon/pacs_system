/**
 * @file main.cpp
 * @brief Modality Worklist SCP - DICOM Worklist Server
 *
 * A command-line server for handling Modality Worklist C-FIND requests.
 * Provides scheduled procedure step information to modality devices.
 *
 * @see Issue #381 - worklist_scp: Implement Modality Worklist SCP utility
 * @see DICOM PS3.4 Section K - Basic Worklist Management Service Class
 *
 * Usage:
 *   worklist_scp <port> <ae_title> --worklist-file <path> [options]
 *
 * Examples:
 *   worklist_scp 11112 MY_WORKLIST --worklist-file ./worklist.json
 *   worklist_scp 11112 MY_WORKLIST --worklist-dir ./worklist_data
 */

#include "pacs/core/dicom_dataset.hpp"
#include "pacs/core/dicom_tag_constants.hpp"
#include "pacs/encoding/vr_type.hpp"
#include "pacs/network/dicom_server.hpp"
#include "pacs/network/server_config.hpp"
#include "pacs/services/verification_scp.hpp"
#include "pacs/services/worklist_scp.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {

// =============================================================================
// Global State for Signal Handling
// =============================================================================

/// Global pointer to server for signal handling
std::atomic<pacs::network::dicom_server*> g_server{nullptr};

/// Global running flag for signal handling
std::atomic<bool> g_running{true};

/**
 * @brief Signal handler for graceful shutdown
 * @param signal The signal received
 */
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running = false;

    auto* server = g_server.load();
    if (server) {
        server->stop();
    }
}

/**
 * @brief Install signal handlers for graceful shutdown
 */
void install_signal_handlers() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
#ifndef _WIN32
    std::signal(SIGHUP, signal_handler);
#endif
}

// =============================================================================
// Minimal JSON Parser
// =============================================================================

struct json_value;
using json_object = std::map<std::string, json_value>;
using json_array = std::vector<json_value>;

struct json_value {
    std::variant<std::nullptr_t, bool, double, std::string, json_array, json_object> data;

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool is_bool() const { return std::holds_alternative<bool>(data); }
    bool is_number() const { return std::holds_alternative<double>(data); }
    bool is_string() const { return std::holds_alternative<std::string>(data); }
    bool is_array() const { return std::holds_alternative<json_array>(data); }
    bool is_object() const { return std::holds_alternative<json_object>(data); }

    bool as_bool() const { return std::get<bool>(data); }
    double as_number() const { return std::get<double>(data); }
    const std::string& as_string() const { return std::get<std::string>(data); }
    const json_array& as_array() const { return std::get<json_array>(data); }
    const json_object& as_object() const { return std::get<json_object>(data); }

    bool has(const std::string& key) const {
        if (!is_object()) return false;
        return as_object().count(key) > 0;
    }

    const json_value& operator[](const std::string& key) const {
        return as_object().at(key);
    }

    const json_value& operator[](size_t idx) const {
        return as_array().at(idx);
    }

    std::string get_string(const std::string& key, const std::string& default_val = "") const {
        if (!is_object() || !has(key)) return default_val;
        const auto& val = (*this)[key];
        if (val.is_string()) return val.as_string();
        if (val.is_number()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << val.as_number();
            return oss.str();
        }
        return default_val;
    }
};

/**
 * @brief Simple JSON parser
 */
class json_parser {
public:
    explicit json_parser(const std::string& input) : input_(input), pos_(0) {}

    json_value parse() {
        skip_whitespace();
        return parse_value();
    }

private:
    const std::string& input_;
    size_t pos_;

    char peek() const {
        return pos_ < input_.size() ? input_[pos_] : '\0';
    }

    char get() {
        return pos_ < input_.size() ? input_[pos_++] : '\0';
    }

    void skip_whitespace() {
        while (pos_ < input_.size() && std::isspace(input_[pos_])) {
            ++pos_;
        }
    }

    json_value parse_value() {
        skip_whitespace();
        char c = peek();

        if (c == '"') return parse_string();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || std::isdigit(c)) return parse_number();

        throw std::runtime_error("Invalid JSON at position " + std::to_string(pos_));
    }

    json_value parse_string() {
        get();  // consume '"'
        std::string result;
        while (peek() != '"') {
            if (peek() == '\\') {
                get();  // consume backslash
                char escaped = get();
                switch (escaped) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'u': {
                        std::string hex;
                        for (int i = 0; i < 4; ++i) hex += get();
                        result += static_cast<char>(std::stoul(hex, nullptr, 16));
                        break;
                    }
                    default: result += escaped; break;
                }
            } else {
                result += get();
            }
        }
        get();  // consume closing '"'
        return json_value{result};
    }

    json_value parse_object() {
        get();  // consume '{'
        json_object obj;
        skip_whitespace();

        if (peek() == '}') {
            get();
            return json_value{obj};
        }

        while (true) {
            skip_whitespace();
            auto key = parse_string().as_string();
            skip_whitespace();
            get();  // consume ':'
            skip_whitespace();
            obj[key] = parse_value();
            skip_whitespace();

            if (peek() == '}') {
                get();
                break;
            }
            get();  // consume ','
        }

        return json_value{obj};
    }

    json_value parse_array() {
        get();  // consume '['
        json_array arr;
        skip_whitespace();

        if (peek() == ']') {
            get();
            return json_value{arr};
        }

        while (true) {
            skip_whitespace();
            arr.push_back(parse_value());
            skip_whitespace();

            if (peek() == ']') {
                get();
                break;
            }
            get();  // consume ','
        }

        return json_value{arr};
    }

    json_value parse_bool() {
        if (input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return json_value{true};
        }
        if (input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return json_value{false};
        }
        throw std::runtime_error("Invalid boolean at position " + std::to_string(pos_));
    }

    json_value parse_null() {
        if (input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return json_value{nullptr};
        }
        throw std::runtime_error("Invalid null at position " + std::to_string(pos_));
    }

    json_value parse_number() {
        size_t start = pos_;
        if (peek() == '-') get();
        while (std::isdigit(peek())) get();
        if (peek() == '.') {
            get();
            while (std::isdigit(peek())) get();
        }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            while (std::isdigit(peek())) get();
        }
        return json_value{std::stod(input_.substr(start, pos_ - start))};
    }
};

// =============================================================================
// Command Line Parsing
// =============================================================================

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
Modality Worklist SCP - DICOM Worklist Server

Usage: )" << program_name << R"( <port> <ae_title> [options]

Arguments:
  port            Port number to listen on (typically 104 or 11112)
  ae_title        Application Entity Title for this server (max 16 chars)

Data Source Options (at least one required):
  --worklist-file <path>    JSON file containing worklist items
  --worklist-dir <path>     Directory containing worklist JSON files

Optional Options:
  --max-assoc <n>           Maximum concurrent associations (default: 10)
  --timeout <sec>           Idle timeout in seconds (default: 300)
  --max-results <n>         Maximum results per query (default: unlimited)
  --reload                  Enable auto-reload of worklist files
  --help                    Show this help message

Examples:
  )" << program_name << R"( 11112 MY_WORKLIST --worklist-file ./worklist.json
  )" << program_name << R"( 11112 MY_WORKLIST --worklist-dir ./worklist_data
  )" << program_name << R"( 11112 MY_WORKLIST --worklist-file ./worklist.json --max-results 100

JSON Worklist File Format:
  [
    {
      "patientId": "12345",
      "patientName": "DOE^JOHN",
      "patientBirthDate": "19800101",
      "patientSex": "M",
      "studyInstanceUid": "1.2.3.4.5...",
      "accessionNumber": "ACC001",
      "scheduledStationAeTitle": "CT_01",
      "scheduledProcedureStepStartDate": "20241220",
      "scheduledProcedureStepStartTime": "100000",
      "modality": "CT",
      "scheduledProcedureStepId": "SPS001",
      "scheduledProcedureStepDescription": "CT Abdomen"
    }
  ]

Notes:
  - Press Ctrl+C to stop the server gracefully
  - Worklist items are loaded on startup
  - With --reload, files are re-read when changed

Exit Codes:
  0  Normal termination
  1  Error - Failed to start server or invalid arguments
)";
}

/**
 * @brief Configuration structure for command-line arguments
 */
struct worklist_scp_args {
    uint16_t port = 0;
    std::string ae_title;
    std::filesystem::path worklist_file;
    std::filesystem::path worklist_dir;
    size_t max_associations = 10;
    uint32_t idle_timeout = 300;
    size_t max_results = 0;
    bool auto_reload = false;
};

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param args Output: parsed arguments
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], worklist_scp_args& args) {
    if (argc < 3) {
        return false;
    }

    // Check for help flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            return false;
        }
    }

    // Parse port
    try {
        int port_int = std::stoi(argv[1]);
        if (port_int < 1 || port_int > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535\n";
            return false;
        }
        args.port = static_cast<uint16_t>(port_int);
    } catch (const std::exception&) {
        std::cerr << "Error: Invalid port number '" << argv[1] << "'\n";
        return false;
    }

    // Parse AE title
    args.ae_title = argv[2];
    if (args.ae_title.length() > 16) {
        std::cerr << "Error: AE title exceeds 16 characters\n";
        return false;
    }

    // Parse optional arguments
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--worklist-file" && i + 1 < argc) {
            args.worklist_file = argv[++i];
        } else if (arg == "--worklist-dir" && i + 1 < argc) {
            args.worklist_dir = argv[++i];
        } else if (arg == "--max-assoc" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 1) {
                    std::cerr << "Error: max-assoc must be positive\n";
                    return false;
                }
                args.max_associations = static_cast<size_t>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid max-assoc value\n";
                return false;
            }
        } else if (arg == "--timeout" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 0) {
                    std::cerr << "Error: timeout cannot be negative\n";
                    return false;
                }
                args.idle_timeout = static_cast<uint32_t>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid timeout value\n";
                return false;
            }
        } else if (arg == "--max-results" && i + 1 < argc) {
            try {
                int val = std::stoi(argv[++i]);
                if (val < 0) {
                    std::cerr << "Error: max-results cannot be negative\n";
                    return false;
                }
                args.max_results = static_cast<size_t>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: Invalid max-results value\n";
                return false;
            }
        } else if (arg == "--reload") {
            args.auto_reload = true;
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }
    }

    // Validate required arguments
    if (args.worklist_file.empty() && args.worklist_dir.empty()) {
        std::cerr << "Error: --worklist-file or --worklist-dir is required\n";
        return false;
    }

    return true;
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Format timestamp for logging
 * @return Current time as formatted string
 */
std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// =============================================================================
// Worklist Data Management
// =============================================================================

/**
 * @brief Worklist item structure
 */
struct worklist_item {
    // Patient demographics
    std::string patient_id;
    std::string patient_name;
    std::string patient_birth_date;
    std::string patient_sex;

    // Study information
    std::string study_instance_uid;
    std::string accession_number;
    std::string referring_physician;
    std::string study_description;

    // Scheduled Procedure Step
    std::string scheduled_station_ae_title;
    std::string scheduled_procedure_step_start_date;
    std::string scheduled_procedure_step_start_time;
    std::string modality;
    std::string scheduled_performing_physician;
    std::string scheduled_procedure_step_description;
    std::string scheduled_procedure_step_id;
    std::string scheduled_procedure_step_location;

    // Requested Procedure
    std::string requested_procedure_id;
    std::string requested_procedure_description;
};

/**
 * @brief Parse worklist item from JSON
 */
worklist_item parse_worklist_item(const json_value& json) {
    worklist_item item;

    item.patient_id = json.get_string("patientId");
    item.patient_name = json.get_string("patientName");
    item.patient_birth_date = json.get_string("patientBirthDate");
    item.patient_sex = json.get_string("patientSex");

    item.study_instance_uid = json.get_string("studyInstanceUid");
    item.accession_number = json.get_string("accessionNumber");
    item.referring_physician = json.get_string("referringPhysician");
    item.study_description = json.get_string("studyDescription");

    item.scheduled_station_ae_title = json.get_string("scheduledStationAeTitle");
    item.scheduled_procedure_step_start_date = json.get_string("scheduledProcedureStepStartDate");
    item.scheduled_procedure_step_start_time = json.get_string("scheduledProcedureStepStartTime");
    item.modality = json.get_string("modality");
    item.scheduled_performing_physician = json.get_string("scheduledPerformingPhysician");
    item.scheduled_procedure_step_description = json.get_string("scheduledProcedureStepDescription");
    item.scheduled_procedure_step_id = json.get_string("scheduledProcedureStepId");
    item.scheduled_procedure_step_location = json.get_string("scheduledProcedureStepLocation");

    item.requested_procedure_id = json.get_string("requestedProcedureId");
    item.requested_procedure_description = json.get_string("requestedProcedureDescription");

    return item;
}

/**
 * @brief Load worklist items from JSON file
 */
std::vector<worklist_item> load_worklist_file(const std::filesystem::path& path) {
    std::vector<worklist_item> items;

    std::ifstream file(path);
    if (!file) {
        std::cerr << "Warning: Could not open worklist file: " << path << "\n";
        return items;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    try {
        json_parser parser(content);
        auto json = parser.parse();

        if (json.is_array()) {
            for (size_t i = 0; i < json.as_array().size(); ++i) {
                items.push_back(parse_worklist_item(json[i]));
            }
        } else if (json.is_object()) {
            // Single item
            items.push_back(parse_worklist_item(json));
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to parse worklist file " << path
                  << ": " << e.what() << "\n";
    }

    return items;
}

/**
 * @brief Load worklist items from directory
 */
std::vector<worklist_item> load_worklist_directory(const std::filesystem::path& dir) {
    std::vector<worklist_item> items;
    namespace fs = std::filesystem;

    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        std::cerr << "Warning: Worklist directory does not exist: " << dir << "\n";
        return items;
    }

    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if (ext != ".json" && ext != ".JSON") continue;

        auto file_items = load_worklist_file(entry.path());
        items.insert(items.end(), file_items.begin(), file_items.end());
    }

    return items;
}

/**
 * @brief Thread-safe worklist repository
 */
class worklist_repository {
public:
    void load(const worklist_scp_args& args) {
        std::lock_guard<std::mutex> lock(mutex_);
        items_.clear();

        if (!args.worklist_file.empty()) {
            auto file_items = load_worklist_file(args.worklist_file);
            items_.insert(items_.end(), file_items.begin(), file_items.end());
        }

        if (!args.worklist_dir.empty()) {
            auto dir_items = load_worklist_directory(args.worklist_dir);
            items_.insert(items_.end(), dir_items.begin(), dir_items.end());
        }

        std::cout << "Loaded " << items_.size() << " worklist item(s)\n";
    }

    std::vector<pacs::core::dicom_dataset> query(
        const pacs::core::dicom_dataset& query_keys,
        [[maybe_unused]] const std::string& calling_ae) const {

        using namespace pacs::core;
        using namespace pacs::encoding;
        namespace tags = pacs::core::tags;

        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<dicom_dataset> results;

        // Extract query filters
        std::string query_patient_id = query_keys.get_string(tags::patient_id, "");
        std::string query_patient_name = query_keys.get_string(tags::patient_name, "");
        std::string query_accession = query_keys.get_string(tags::accession_number, "");

        // Get Scheduled Procedure Step filters (flat structure - no sequence support)
        std::string query_station_ae = query_keys.get_string(tags::scheduled_station_ae_title, "");
        std::string query_start_date = query_keys.get_string(tags::scheduled_procedure_step_start_date, "");
        std::string query_modality = query_keys.get_string(tags::modality, "");

        for (const auto& item : items_) {
            // Apply filters (empty filter matches all)
            if (!query_patient_id.empty() && !matches_wildcard(item.patient_id, query_patient_id)) {
                continue;
            }
            if (!query_patient_name.empty() && !matches_wildcard(item.patient_name, query_patient_name)) {
                continue;
            }
            if (!query_accession.empty() && !matches_wildcard(item.accession_number, query_accession)) {
                continue;
            }
            if (!query_station_ae.empty() && !matches_wildcard(item.scheduled_station_ae_title, query_station_ae)) {
                continue;
            }
            if (!query_start_date.empty() && !matches_date_range(item.scheduled_procedure_step_start_date, query_start_date)) {
                continue;
            }
            if (!query_modality.empty() && !matches_wildcard(item.modality, query_modality)) {
                continue;
            }

            // Build response dataset
            dicom_dataset ds;

            // Patient demographics
            ds.set_string(tags::patient_id, vr_type::LO, item.patient_id);
            ds.set_string(tags::patient_name, vr_type::PN, item.patient_name);
            ds.set_string(tags::patient_birth_date, vr_type::DA, item.patient_birth_date);
            ds.set_string(tags::patient_sex, vr_type::CS, item.patient_sex);

            // Study information
            ds.set_string(tags::study_instance_uid, vr_type::UI, item.study_instance_uid);
            ds.set_string(tags::accession_number, vr_type::SH, item.accession_number);
            ds.set_string(tags::referring_physician_name, vr_type::PN, item.referring_physician);
            ds.set_string(tags::study_description, vr_type::LO, item.study_description);

            // Requested Procedure
            ds.set_string(tags::requested_procedure_id, vr_type::SH, item.requested_procedure_id);
            ds.set_string(tags::requested_procedure_description, vr_type::LO, item.requested_procedure_description);

            // Scheduled Procedure Step attributes (flat structure - no sequence support)
            ds.set_string(tags::scheduled_station_ae_title, vr_type::AE, item.scheduled_station_ae_title);
            ds.set_string(tags::scheduled_procedure_step_start_date, vr_type::DA, item.scheduled_procedure_step_start_date);
            ds.set_string(tags::scheduled_procedure_step_start_time, vr_type::TM, item.scheduled_procedure_step_start_time);
            ds.set_string(tags::modality, vr_type::CS, item.modality);
            ds.set_string(tags::scheduled_performing_physician_name, vr_type::PN, item.scheduled_performing_physician);
            ds.set_string(tags::scheduled_procedure_step_description, vr_type::LO, item.scheduled_procedure_step_description);
            ds.set_string(tags::scheduled_procedure_step_id, vr_type::SH, item.scheduled_procedure_step_id);
            ds.set_string(tags::scheduled_procedure_step_location, vr_type::SH, item.scheduled_procedure_step_location);

            results.push_back(std::move(ds));
        }

        return results;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return items_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<worklist_item> items_;

    /**
     * @brief Check if value matches wildcard pattern
     * Supports * (multiple chars) and ? (single char)
     */
    static bool matches_wildcard(const std::string& value, const std::string& pattern) {
        if (pattern.empty() || pattern == "*") return true;

        size_t v = 0, p = 0;
        size_t v_star = std::string::npos, p_star = std::string::npos;

        while (v < value.size()) {
            if (p < pattern.size() && (pattern[p] == '?' || std::toupper(pattern[p]) == std::toupper(value[v]))) {
                ++v;
                ++p;
            } else if (p < pattern.size() && pattern[p] == '*') {
                v_star = v;
                p_star = p++;
            } else if (p_star != std::string::npos) {
                p = p_star + 1;
                v = ++v_star;
            } else {
                return false;
            }
        }

        while (p < pattern.size() && pattern[p] == '*') ++p;
        return p == pattern.size();
    }

    /**
     * @brief Check if date matches range pattern
     * Supports single date (YYYYMMDD) or range (YYYYMMDD-YYYYMMDD)
     */
    static bool matches_date_range(const std::string& value, const std::string& pattern) {
        if (pattern.empty()) return true;

        auto dash_pos = pattern.find('-');
        if (dash_pos == std::string::npos) {
            // Single date match
            return value == pattern;
        }

        // Date range
        std::string start_date = pattern.substr(0, dash_pos);
        std::string end_date = pattern.substr(dash_pos + 1);

        if (start_date.empty()) start_date = "00000000";
        if (end_date.empty()) end_date = "99999999";

        return value >= start_date && value <= end_date;
    }
};

// =============================================================================
// Server Implementation
// =============================================================================

/**
 * @brief Run the Modality Worklist SCP server
 * @param args Parsed command-line arguments
 * @return true if server ran successfully
 */
bool run_server(const worklist_scp_args& args) {
    using namespace pacs::network;
    using namespace pacs::services;

    std::cout << "\nStarting Modality Worklist SCP...\n";
    std::cout << "  AE Title:           " << args.ae_title << "\n";
    std::cout << "  Port:               " << args.port << "\n";
    if (!args.worklist_file.empty()) {
        std::cout << "  Worklist File:      " << args.worklist_file << "\n";
    }
    if (!args.worklist_dir.empty()) {
        std::cout << "  Worklist Directory: " << args.worklist_dir << "\n";
    }
    std::cout << "  Max Associations:   " << args.max_associations << "\n";
    std::cout << "  Idle Timeout:       " << args.idle_timeout << " seconds\n";
    if (args.max_results > 0) {
        std::cout << "  Max Results:        " << args.max_results << "\n";
    }
    std::cout << "\n";

    // Load worklist data
    worklist_repository repository;
    repository.load(args);

    if (repository.size() == 0) {
        std::cout << "\nWarning: No worklist items loaded.\n";
        std::cout << "         Server will start but queries will return no results.\n\n";
    }

    // Configure server
    server_config config;
    config.ae_title = args.ae_title;
    config.port = args.port;
    config.max_associations = args.max_associations;
    config.idle_timeout = std::chrono::seconds{args.idle_timeout};
    config.implementation_class_uid = "1.2.826.0.1.3680043.2.1545.2";
    config.implementation_version_name = "WL_SCP_001";

    // Create server
    dicom_server server{config};
    g_server = &server;

    // Register Verification service (C-ECHO)
    server.register_service(std::make_shared<verification_scp>());

    // Configure Worklist SCP
    auto worklist_service = std::make_shared<worklist_scp>();
    worklist_service->set_handler(
        [&repository](const pacs::core::dicom_dataset& keys, const std::string& ae) {
            return repository.query(keys, ae);
        });

    if (args.max_results > 0) {
        worklist_service->set_max_results(args.max_results);
    }

    server.register_service(worklist_service);

    // Set up callbacks for logging
    server.on_association_established([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "Association established from: " << assoc.calling_ae()
                  << " -> " << assoc.called_ae() << "\n";
    });

    server.on_association_released([](const association& assoc) {
        std::cout << "[" << current_timestamp() << "] "
                  << "Association released: " << assoc.calling_ae() << "\n";
    });

    server.on_error([](const std::string& error) {
        std::cerr << "[" << current_timestamp() << "] "
                  << "Error: " << error << "\n";
    });

    // Start server
    auto result = server.start();
    if (result.is_err()) {
        std::cerr << "Failed to start server: " << result.error().message << "\n";
        g_server = nullptr;
        return false;
    }

    std::cout << "=================================================\n";
    std::cout << " Modality Worklist SCP is running on port " << args.port << "\n";
    std::cout << " Worklist Items: " << repository.size() << "\n";
    std::cout << " Press Ctrl+C to stop\n";
    std::cout << "=================================================\n\n";

    // Wait for shutdown
    server.wait_for_shutdown();

    // Print final statistics
    auto server_stats = server.get_statistics();

    std::cout << "\n";
    std::cout << "=================================================\n";
    std::cout << " Server Statistics\n";
    std::cout << "=================================================\n";
    std::cout << "  Total Associations:    " << server_stats.total_associations << "\n";
    std::cout << "  Rejected Associations: " << server_stats.rejected_associations << "\n";
    std::cout << "  Messages Processed:    " << server_stats.messages_processed << "\n";
    std::cout << "  Worklist Queries:      " << worklist_service->queries_processed() << "\n";
    std::cout << "  Items Returned:        " << worklist_service->items_returned() << "\n";
    std::cout << "  Uptime:                " << server_stats.uptime().count() << " seconds\n";
    std::cout << "=================================================\n";

    g_server = nullptr;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::cout << R"(
 __        __         _    _ _     _      ____   ____ ____
 \ \      / /__  _ __| | _| (_)___| |_   / ___| / ___|  _ \
  \ \ /\ / / _ \| '__| |/ / | / __| __| | \___ \| |   | |_) |
   \ V  V / (_) | |  |   <| | \__ \ |_   ___) | |___|  __/
    \_/\_/ \___/|_|  |_|\_\_|_|___/\__| |____/ \____|_|

     DICOM Modality Worklist Server
)" << "\n";

    worklist_scp_args args;

    if (!parse_arguments(argc, argv, args)) {
        print_usage(argv[0]);
        return 1;
    }

    // Install signal handlers
    install_signal_handlers();

    bool success = run_server(args);

    std::cout << "\nModality Worklist SCP terminated\n";
    return success ? 0 : 1;
}
