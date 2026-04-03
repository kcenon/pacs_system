/**
 * @file test_cli_roundtrip.cpp
 * @brief Catch2 round-trip integrity tests for conversion CLI tools
 *
 * Verifies data integrity through CLI conversion round-trips:
 * DCM->JSON->DCM, DCM->XML->DCM, Transfer Syntax conversion,
 * anonymization, and tag modification.
 *
 * Uses the PACS library API to programmatically compare input/output
 * datasets at the tag level.
 *
 * @see Issue #759 - Add Catch2 round-trip integrity tests for conversion CLIs
 * @see Issue #756 - Automated CLI Tool Testing (Epic)
 */

#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#define WEXITSTATUS(x) (x)
#else
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace {

// ============================================================================
// Lightweight process execution (no test_fixtures.hpp dependency)
// ============================================================================

struct cli_result {
    int exit_code;
    std::string output;
};

/**
 * @brief Build a shell command string from executable path and arguments
 */
std::string build_command(const std::string& executable,
                          const std::vector<std::string>& args) {
    std::string cmd = "\"" + executable + "\"";
    for (const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }
    cmd += " 2>&1";
#ifdef _WIN32
    // Wrap entire command in extra quotes for cmd.exe /c parsing.
    // Without this, cmd.exe strips the first and last quote characters
    // when the command string starts with a double-quote.
    cmd = "\"" + cmd + "\"";
#endif
    return cmd;
}

/**
 * @brief Run a CLI tool from the build binary directory
 * @param tool_name Name of the CLI tool (e.g., "dcm_to_json")
 * @param args Command-line arguments
 * @return cli_result with exit code and captured output
 */
cli_result run_cli(const std::string& tool_name,
                   const std::vector<std::string>& args = {}) {
    auto tool_path = fs::path(CLI_BINARY_DIR) / tool_name;
#ifdef _WIN32
    if (!tool_path.has_extension()) {
        tool_path.replace_extension(".exe");
    }
#endif
    auto cmd = build_command(tool_path.string(), args);

    cli_result result{};
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr) {
        result.exit_code = -1;
        return result;
    }

    std::array<char, 256> buffer{};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result.output += buffer.data();
    }

    int status = pclose(pipe);
    result.exit_code = WEXITSTATUS(status);
    return result;
}

/**
 * @brief Read a DICOM file and return the parsed dicom_file
 * @param path Path to the DICOM file
 * @return dicom_file object
 * @throws Catch2 REQUIRE failure if file cannot be read
 */
kcenon::pacs::core::dicom_file read_dicom(const fs::path& path) {
    auto result = kcenon::pacs::core::dicom_file::open(path);
    REQUIRE(result.is_ok());
    return std::move(result.value());
}

/**
 * @brief Get a string tag value from a DICOM dataset
 */
std::string get_tag(const kcenon::pacs::core::dicom_file& file,
                    kcenon::pacs::core::dicom_tag tag) {
    return file.dataset().get_string(tag);
}

// ============================================================================
// RAII temp directory
// ============================================================================

/**
 * @brief RAII wrapper for a temporary directory
 */
struct temp_dir {
    fs::path path;

    temp_dir() {
        path = fs::temp_directory_path() / ("pacs_roundtrip_" +
               std::to_string(std::hash<std::thread::id>{}(
                   std::this_thread::get_id())) +
               "_" + std::to_string(
                   std::chrono::steady_clock::now()
                       .time_since_epoch().count()));
        fs::create_directories(path);
    }

    ~temp_dir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    temp_dir(const temp_dir&) = delete;
    temp_dir& operator=(const temp_dir&) = delete;
};

/// Path to test data directory
const fs::path test_data_dir{CLI_TEST_DATA_DIR};

}  // namespace

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("JSON round-trip preserves metadata", "[cli][roundtrip]") {
    temp_dir tmp;
    auto src = test_data_dir / "ct_minimal.dcm";
    auto json_path = tmp.path / "ct.json";
    auto dcm_path = tmp.path / "roundtrip.dcm";

    REQUIRE(fs::exists(src));

    // DCM -> JSON
    auto to_json = run_cli("dcm_to_json",
        {src.string(), json_path.string(), "--pretty", "--no-pixel"});
    INFO("dcm_to_json output: " << to_json.output);
    REQUIRE(to_json.exit_code == 0);
    REQUIRE(fs::exists(json_path));

    // JSON -> DCM
    auto to_dcm = run_cli("json_to_dcm",
        {json_path.string(), dcm_path.string()});
    INFO("json_to_dcm output: " << to_dcm.output);
    REQUIRE(to_dcm.exit_code == 0);
    REQUIRE(fs::exists(dcm_path));

    // Compare tags
    auto original = read_dicom(src);
    auto roundtrip = read_dicom(dcm_path);

    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_name)
       == get_tag(roundtrip, kcenon::pacs::core::tags::patient_name));
    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_id)
       == get_tag(roundtrip, kcenon::pacs::core::tags::patient_id));
    CHECK(get_tag(original, kcenon::pacs::core::tags::modality)
       == get_tag(roundtrip, kcenon::pacs::core::tags::modality));
    CHECK(get_tag(original, kcenon::pacs::core::tags::study_date)
       == get_tag(roundtrip, kcenon::pacs::core::tags::study_date));
}

TEST_CASE("XML round-trip preserves metadata", "[cli][roundtrip]") {
    temp_dir tmp;
    auto src = test_data_dir / "mr_minimal.dcm";
    auto xml_path = tmp.path / "mr.xml";
    auto dcm_path = tmp.path / "roundtrip.dcm";

    REQUIRE(fs::exists(src));

    // DCM -> XML
    auto to_xml = run_cli("dcm_to_xml",
        {src.string(), xml_path.string(), "--no-pixel"});
    INFO("dcm_to_xml output: " << to_xml.output);
    REQUIRE(to_xml.exit_code == 0);
    REQUIRE(fs::exists(xml_path));

    // XML -> DCM
    auto to_dcm = run_cli("xml_to_dcm",
        {xml_path.string(), dcm_path.string()});
    INFO("xml_to_dcm output: " << to_dcm.output);
    REQUIRE(to_dcm.exit_code == 0);
    REQUIRE(fs::exists(dcm_path));

    // Compare tags
    auto original = read_dicom(src);
    auto roundtrip = read_dicom(dcm_path);

    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_name)
       == get_tag(roundtrip, kcenon::pacs::core::tags::patient_name));
    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_id)
       == get_tag(roundtrip, kcenon::pacs::core::tags::patient_id));
    CHECK(get_tag(original, kcenon::pacs::core::tags::modality)
       == get_tag(roundtrip, kcenon::pacs::core::tags::modality));
}

TEST_CASE("Transfer Syntax conversion preserves tags", "[cli][roundtrip]") {
    temp_dir tmp;
    auto src = test_data_dir / "ct_minimal.dcm";
    auto explicit_path = tmp.path / "explicit.dcm";
    auto implicit_path = tmp.path / "implicit.dcm";

    REQUIRE(fs::exists(src));

    // Convert to Explicit VR LE
    auto to_explicit = run_cli("dcm_conv",
        {src.string(), explicit_path.string(), "--explicit"});
    INFO("dcm_conv --explicit output: " << to_explicit.output);
    REQUIRE(to_explicit.exit_code == 0);
    REQUIRE(fs::exists(explicit_path));

    // Convert back to Implicit VR LE
    auto to_implicit = run_cli("dcm_conv",
        {explicit_path.string(), implicit_path.string(), "--implicit"});
    INFO("dcm_conv --implicit output: " << to_implicit.output);
    REQUIRE(to_implicit.exit_code == 0);
    REQUIRE(fs::exists(implicit_path));

    // Verify both outputs are valid DICOM with preserved tags
    auto original = read_dicom(src);
    auto explicit_file = read_dicom(explicit_path);
    auto implicit_file = read_dicom(implicit_path);

    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_name)
       == get_tag(explicit_file, kcenon::pacs::core::tags::patient_name));
    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_name)
       == get_tag(implicit_file, kcenon::pacs::core::tags::patient_name));
    CHECK(get_tag(original, kcenon::pacs::core::tags::modality)
       == get_tag(implicit_file, kcenon::pacs::core::tags::modality));
    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_id)
       == get_tag(implicit_file, kcenon::pacs::core::tags::patient_id));
}

TEST_CASE("Anonymization changes patient identifiers", "[cli][roundtrip]") {
    temp_dir tmp;
    auto src = test_data_dir / "ct_minimal.dcm";
    auto anon_path = tmp.path / "anonymized.dcm";

    REQUIRE(fs::exists(src));

    // Anonymize
    auto result = run_cli("dcm_anonymize",
        {src.string(), anon_path.string()});
    INFO("dcm_anonymize output: " << result.output);
    REQUIRE(result.exit_code == 0);
    REQUIRE(fs::exists(anon_path));

    // Verify anonymization
    auto original = read_dicom(src);
    auto anonymized = read_dicom(anon_path);

    auto orig_name = get_tag(original, kcenon::pacs::core::tags::patient_name);
    auto anon_name = get_tag(anonymized, kcenon::pacs::core::tags::patient_name);

    // Patient name must be different after anonymization
    CHECK(orig_name != anon_name);

    // Anonymized file must still be valid DICOM (modality preserved)
    CHECK_FALSE(get_tag(anonymized, kcenon::pacs::core::tags::modality).empty());
}

TEST_CASE("Tag modification updates values correctly", "[cli][roundtrip]") {
    temp_dir tmp;
    auto src = test_data_dir / "ct_minimal.dcm";
    auto modified_path = tmp.path / "modified.dcm";

    REQUIRE(fs::exists(src));

    const std::string new_name = "ROUNDTRIP^TEST";

    // Modify PatientName
    auto result = run_cli("dcm_modify",
        {"-i", "(0010,0010)=" + new_name, "-o", modified_path.string(),
         src.string()});
    INFO("dcm_modify output: " << result.output);
    REQUIRE(result.exit_code == 0);
    REQUIRE(fs::exists(modified_path));

    // Verify modification
    auto modified = read_dicom(modified_path);
    CHECK(get_tag(modified, kcenon::pacs::core::tags::patient_name) == new_name);

    // Other tags should be unchanged
    auto original = read_dicom(src);
    CHECK(get_tag(original, kcenon::pacs::core::tags::patient_id)
       == get_tag(modified, kcenon::pacs::core::tags::patient_id));
    CHECK(get_tag(original, kcenon::pacs::core::tags::modality)
       == get_tag(modified, kcenon::pacs::core::tags::modality));
}
