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
 * @see Issue #1276 - dcm_conv transcodes Pixel Data instead of relabeling it
 * @see Issue #1291 - dicom_file writes sequence items
 * @see Issue #1294 - dcm_conv gives lossy output a new SOP Instance UID
 */

#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/compression/codec_factory.h>
#include <kcenon/pacs/encoding/transfer_syntax.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
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
// Lightweight process execution (no test_fixtures.h dependency)
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

// ============================================================================
// Generated images for Pixel Data conversion (Issue #1276)
// ============================================================================

constexpr kcenon::pacs::core::dicom_tag planar_configuration_tag{0x0028, 0x0006};
constexpr kcenon::pacs::core::dicom_tag number_of_frames_tag{0x0028, 0x0008};
constexpr kcenon::pacs::core::dicom_tag lossy_image_compression_tag{0x0028, 0x2110};
constexpr kcenon::pacs::core::dicom_tag lossy_image_compression_method_tag{0x0028, 0x2114};
constexpr kcenon::pacs::core::dicom_tag derivation_description_tag{0x0008, 0x2111};
constexpr kcenon::pacs::core::dicom_tag source_image_sequence_tag{0x0008, 0x2112};

/// Prefix of the UIDs that dcm_conv creates
constexpr const char* dcm_conv_uid_prefix = "1.2.826.0.1.3680043.8.1055.5.";

/**
 * @brief Image Pixel attributes of a generated test image
 */
struct image_spec {
    uint16_t rows;
    uint16_t columns;
    uint16_t bits_allocated;
    uint16_t bits_stored;
    uint16_t samples_per_pixel;
    uint32_t frames;
    std::string photometric;
    uint16_t planar_configuration{0};
    std::string image_type{};  ///< Image Type (0008,0008); absent when empty
};

/**
 * @brief Native little-endian pixels in which neighboring samples differ
 */
std::vector<uint8_t> make_pixels(const image_spec& spec) {
    const size_t samples = size_t{spec.rows} * spec.columns *
                           spec.samples_per_pixel * spec.frames;
    const uint32_t mask = (1u << spec.bits_stored) - 1;

    std::vector<uint8_t> pixels;
    pixels.reserve(samples * spec.bits_allocated / 8);
    for (size_t i = 0; i < samples; ++i) {
        const auto value = static_cast<uint32_t>(i * 7 + (i / 13) * 3) & mask;
        pixels.push_back(static_cast<uint8_t>(value & 0xFF));
        if (spec.bits_allocated == 16) {
            pixels.push_back(static_cast<uint8_t>(value >> 8));
        }
    }
    return pixels;
}

/**
 * @brief Reorder 8-bit color-by-plane pixels to color-by-pixel
 */
std::vector<uint8_t> to_color_by_pixel(const std::vector<uint8_t>& planar,
                                       const image_spec& spec) {
    const size_t pixels = size_t{spec.rows} * spec.columns;
    const size_t frame = pixels * spec.samples_per_pixel;
    std::vector<uint8_t> result(planar.size());
    for (size_t f = 0; f < spec.frames; ++f) {
        for (size_t s = 0; s < spec.samples_per_pixel; ++s) {
            for (size_t p = 0; p < pixels; ++p) {
                result[f * frame + p * spec.samples_per_pixel + s] =
                    planar[f * frame + s * pixels + p];
            }
        }
    }
    return result;
}

/**
 * @brief Write a Secondary Capture image with the given Pixel Data value
 */
void write_image(const fs::path& path, const image_spec& spec,
                 const std::vector<uint8_t>& pixel_data,
                 const kcenon::pacs::encoding::transfer_syntax& ts =
                     kcenon::pacs::encoding::transfer_syntax::explicit_vr_little_endian) {
    using namespace kcenon::pacs::core;
    using kcenon::pacs::encoding::vr_type;

    dicom_dataset ds;
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.7");
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.826.0.1.3680043.9.9999.1276.1");
    ds.set_string(tags::patient_name, vr_type::PN, "TRANSCODE^TEST");
    ds.set_string(tags::patient_id, vr_type::LO, "TC1276");
    ds.set_string(tags::modality, vr_type::CS, "OT");
    if (!spec.image_type.empty()) {
        ds.set_string(tags::image_type, vr_type::CS, spec.image_type);
    }
    ds.set_numeric<uint16_t>(tags::samples_per_pixel, vr_type::US, spec.samples_per_pixel);
    ds.set_string(tags::photometric_interpretation, vr_type::CS, spec.photometric);
    if (spec.samples_per_pixel > 1) {
        ds.set_numeric<uint16_t>(planar_configuration_tag, vr_type::US,
                                 spec.planar_configuration);
    }
    if (spec.frames > 1) {
        ds.set_string(number_of_frames_tag, vr_type::IS, std::to_string(spec.frames));
    }
    ds.set_numeric<uint16_t>(tags::rows, vr_type::US, spec.rows);
    ds.set_numeric<uint16_t>(tags::columns, vr_type::US, spec.columns);
    ds.set_numeric<uint16_t>(tags::bits_allocated, vr_type::US, spec.bits_allocated);
    ds.set_numeric<uint16_t>(tags::bits_stored, vr_type::US, spec.bits_stored);
    ds.set_numeric<uint16_t>(tags::high_bit, vr_type::US,
                             static_cast<uint16_t>(spec.bits_stored - 1));
    ds.set_numeric<uint16_t>(tags::pixel_representation, vr_type::US, 0);
    ds.insert(dicom_element{tags::pixel_data,
                            spec.bits_allocated > 8 ? vr_type::OW : vr_type::OB,
                            pixel_data});

    auto file = dicom_file::create(std::move(ds), ts);
    REQUIRE(file.save(path).is_ok());
}

/**
 * @brief The Pixel Data value of a file
 */
std::vector<uint8_t> pixel_data_of(const kcenon::pacs::core::dicom_file& file) {
    const auto* element = file.dataset().get(kcenon::pacs::core::tags::pixel_data);
    REQUIRE(element != nullptr);
    const auto raw = element->raw_data();
    return {raw.begin(), raw.end()};
}

/**
 * @brief Decode every frame of an encapsulated file with this build's codec
 */
std::vector<uint8_t> decode_frames(const kcenon::pacs::core::dicom_file& file,
                                   const image_spec& spec) {
    using namespace kcenon::pacs::encoding::compression;

    auto frames = kcenon::pacs::core::dicom_file::encapsulated_frames(
        pixel_data_of(file), spec.frames);
    REQUIRE(frames.is_ok());
    REQUIRE(frames.value().size() == spec.frames);

    auto codec = codec_factory::create(file.transfer_syntax());
    REQUIRE(codec != nullptr);

    image_params params;
    params.width = spec.columns;
    params.height = spec.rows;
    params.bits_allocated = spec.bits_allocated;
    params.bits_stored = spec.bits_stored;
    params.high_bit = static_cast<uint16_t>(spec.bits_stored - 1);
    params.samples_per_pixel = spec.samples_per_pixel;

    std::vector<uint8_t> pixels;
    for (const auto& frame : frames.value()) {
        auto decoded = codec->decode(frame, params);
        REQUIRE(decoded.is_ok());
        pixels.insert(pixels.end(), decoded.value().data.begin(),
                      decoded.value().data.end());
    }
    return pixels;
}

/**
 * @brief First codec that pacs_system implements but this build lacks
 * @return Its Transfer Syntax UID, or an empty string if every codec is built
 */
std::string first_unavailable_codec() {
    using kcenon::pacs::encoding::compression::codec_factory;
    for (const auto uid : codec_factory::supported_transfer_syntaxes()) {
        if (!codec_factory::is_available(uid)) {
            return std::string(uid);
        }
    }
    return {};
}

/**
 * @brief First lossy codec that this build includes
 * @return Its Transfer Syntax UID, or an empty string if none is built
 *
 * JPEG Baseline comes first because CI builds it with libjpeg-turbo. HTJ2K
 * comes last: its encoder crashes on AVX-512 runners (#1257).
 */
std::string first_available_lossy_codec() {
    using kcenon::pacs::encoding::compression::codec_factory;
    for (const char* uid : {"1.2.840.10008.1.2.4.50", "1.2.840.10008.1.2.4.91",
                            "1.2.840.10008.1.2.4.81", "1.2.840.10008.1.2.4.203"}) {
        if (codec_factory::is_available(uid)) {
            return uid;
        }
    }
    return {};
}

/**
 * @brief Whether a UID has valid syntax (PS3.5 Section 9.1)
 *
 * At most 64 characters; components of digits separated by periods, none
 * empty and none with a leading zero.
 */
bool is_valid_uid(const std::string& uid) {
    if (uid.empty() || uid.size() > 64) {
        return false;
    }
    size_t start = 0;
    while (start <= uid.size()) {
        const auto end = (std::min)(uid.find('.', start), uid.size());
        const auto component = uid.substr(start, end - start);
        if (component.empty() || (component.size() > 1 && component[0] == '0') ||
            component.find_first_not_of("0123456789") != std::string::npos) {
            return false;
        }
        start = end + 1;
    }
    return true;
}

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

// ============================================================================
// Pixel Data conversion (Issue #1276)
// ============================================================================

TEST_CASE("Transfer Syntax conversion to and from RLE keeps the pixels",
          "[cli][roundtrip][transcode]") {
    using kcenon::pacs::encoding::transfer_syntax;
    namespace tags = kcenon::pacs::core::tags;

    const std::vector<image_spec> specs = {
        {24, 32, 16, 12, 1, 2, "MONOCHROME2"},
        {16, 20, 8, 8, 3, 1, "RGB", 1},
    };

    for (const auto& spec : specs) {
        DYNAMIC_SECTION(spec.photometric << ", " << spec.bits_allocated << "-bit, "
                                         << spec.frames << " frame(s)") {
            temp_dir tmp;
            const auto src = tmp.path / "native.dcm";
            const auto rle_path = tmp.path / "rle.dcm";
            const auto back_path = tmp.path / "back.dcm";

            const auto pixels = make_pixels(spec);
            const auto expected = spec.planar_configuration == 1
                                      ? to_color_by_pixel(pixels, spec)
                                      : pixels;
            write_image(src, spec, pixels);

            // Native -> RLE
            auto to_rle = run_cli("dcm_conv",
                {src.string(), rle_path.string(), "--rle", "--verify"});
            INFO("dcm_conv --rle output: " << to_rle.output);
            REQUIRE(to_rle.exit_code == 0);

            auto rle_file = read_dicom(rle_path);
            CHECK(rle_file.transfer_syntax() == transfer_syntax::rle_lossless);
            CHECK(decode_frames(rle_file, spec) == expected);
            CHECK(get_tag(rle_file, tags::photometric_interpretation) == spec.photometric);
            CHECK(get_tag(rle_file, number_of_frames_tag) ==
                  get_tag(read_dicom(src), number_of_frames_tag));
            if (spec.samples_per_pixel > 1) {
                CHECK(rle_file.dataset().get_numeric<uint16_t>(planar_configuration_tag) == 0);
            }
            CHECK(get_tag(rle_file, lossy_image_compression_tag).empty());

            // RLE -> native
            auto to_native = run_cli("dcm_conv",
                {rle_path.string(), back_path.string(), "--explicit", "--verify"});
            INFO("dcm_conv --explicit output: " << to_native.output);
            REQUIRE(to_native.exit_code == 0);

            auto back_file = read_dicom(back_path);
            CHECK(back_file.transfer_syntax() == transfer_syntax::explicit_vr_little_endian);
            CHECK(pixel_data_of(back_file) == expected);
            CHECK(get_tag(back_file, tags::photometric_interpretation) == spec.photometric);
        }
    }
}

TEST_CASE("Transfer Syntax conversion through JPEG Lossless keeps the pixels",
          "[cli][roundtrip][transcode]") {
    using kcenon::pacs::encoding::transfer_syntax;

    temp_dir tmp;
    const image_spec spec{24, 32, 16, 12, 1, 1, "MONOCHROME2"};
    const auto src = tmp.path / "native.dcm";
    const auto jpeg_path = tmp.path / "jpeg_lossless.dcm";
    const auto back_path = tmp.path / "back.dcm";

    const auto pixels = make_pixels(spec);
    write_image(src, spec, pixels);

    auto to_jpeg = run_cli("dcm_conv",
        {src.string(), jpeg_path.string(), "--jpeg-lossless", "--verify"});
    INFO("dcm_conv --jpeg-lossless output: " << to_jpeg.output);
    REQUIRE(to_jpeg.exit_code == 0);

    auto jpeg_file = read_dicom(jpeg_path);
    CHECK(jpeg_file.transfer_syntax() == transfer_syntax::jpeg_lossless);
    CHECK(decode_frames(jpeg_file, spec) == pixels);

    auto to_native = run_cli("dcm_conv",
        {jpeg_path.string(), back_path.string(), "--implicit", "--verify"});
    INFO("dcm_conv --implicit output: " << to_native.output);
    REQUIRE(to_native.exit_code == 0);
    CHECK(pixel_data_of(read_dicom(back_path)) == pixels);
}

TEST_CASE("Tag modification keeps encapsulated Pixel Data", "[cli][roundtrip][transcode]") {
    temp_dir tmp;
    const image_spec spec{24, 32, 16, 12, 1, 2, "MONOCHROME2"};
    const auto src = tmp.path / "native.dcm";
    const auto rle_path = tmp.path / "rle.dcm";
    const auto modified_path = tmp.path / "modified.dcm";

    const auto pixels = make_pixels(spec);
    write_image(src, spec, pixels);
    REQUIRE(run_cli("dcm_conv", {src.string(), rle_path.string(), "--rle"}).exit_code == 0);

    auto result = run_cli("dcm_modify",
        {"-i", "(0010,0010)=MODIFIED^NAME", "-o", modified_path.string(),
         rle_path.string()});
    INFO("dcm_modify output: " << result.output);
    REQUIRE(result.exit_code == 0);

    auto modified = read_dicom(modified_path);
    CHECK(get_tag(modified, kcenon::pacs::core::tags::patient_name) == "MODIFIED^NAME");
    CHECK(pixel_data_of(modified) == pixel_data_of(read_dicom(rle_path)));
    CHECK(decode_frames(modified, spec) == pixels);
}

TEST_CASE("Transfer Syntax conversion fails when a codec is missing or unsuitable",
          "[cli][roundtrip][transcode]") {
    temp_dir tmp;
    const image_spec gray{16, 16, 16, 12, 1, 1, "MONOCHROME2"};
    const auto src = tmp.path / "native.dcm";
    const auto out = tmp.path / "out.dcm";
    write_image(src, gray, make_pixels(gray));

    SECTION("a stub codec (HEVC) is refused") {
        auto result = run_cli("dcm_conv",
            {src.string(), out.string(), "-t", "1.2.840.10008.1.2.4.107"});
        INFO("dcm_conv output: " << result.output);
        CHECK(result.exit_code == 2);
        CHECK_FALSE(fs::exists(out));
    }

    SECTION("a codec that this build does not include is refused") {
        const auto uid = first_unavailable_codec();
        if (uid.empty()) {
            WARN("Every codec is built; the missing-codec path is not exercised");
            return;
        }
        INFO("Codec not built: " << uid);

        auto to_target = run_cli("dcm_conv", {src.string(), out.string(), "-t", uid});
        INFO("dcm_conv output: " << to_target.output);
        CHECK(to_target.exit_code == 2);
        CHECK(to_target.output.find("needs a codec that this build does not include") !=
              std::string::npos);
        CHECK_FALSE(fs::exists(out));

        // A source in that syntax cannot be decoded either
        const auto encapsulated_src = tmp.path / "encapsulated.dcm";
        write_image(encapsulated_src, gray,
                    kcenon::pacs::core::dicom_file::encapsulate_frames({{0xFF, 0xD8, 0xFF, 0xD9}}),
                    kcenon::pacs::encoding::transfer_syntax{uid});
        auto from_source = run_cli("dcm_conv",
            {encapsulated_src.string(), out.string(), "--explicit"});
        INFO("dcm_conv output: " << from_source.output);
        CHECK(from_source.exit_code == 2);
        CHECK(from_source.output.find("needs a codec that this build does not include") !=
              std::string::npos);
        CHECK_FALSE(fs::exists(out));
    }

    SECTION("a codec that cannot encode the image is refused") {
        const image_spec color{16, 16, 8, 8, 3, 1, "RGB"};
        const auto color_src = tmp.path / "color.dcm";
        write_image(color_src, color, make_pixels(color));

        auto result = run_cli("dcm_conv",
            {color_src.string(), out.string(), "--jpeg-lossless"});
        INFO("dcm_conv output: " << result.output);
        CHECK(result.exit_code == 2);
        CHECK(result.output.find("cannot encode") != std::string::npos);
        CHECK_FALSE(fs::exists(out));
    }
}

TEST_CASE("dcm_conv --verify rejects Pixel Data that does not match the Transfer Syntax",
          "[cli][roundtrip][transcode]") {
    using kcenon::pacs::encoding::transfer_syntax;

    temp_dir tmp;
    const image_spec spec{16, 16, 8, 8, 1, 1, "MONOCHROME2"};
    const auto src = tmp.path / "input.dcm";
    const auto out = tmp.path / "out.dcm";
    const auto pixels = make_pixels(spec);

    SECTION("native Pixel Data labeled RLE Lossless") {
        write_image(src, spec, pixels, transfer_syntax::rle_lossless);

        auto result = run_cli("dcm_conv", {src.string(), out.string(), "--rle", "--verify"});
        INFO("dcm_conv output: " << result.output);
        CHECK(result.exit_code == 2);
        CHECK(result.output.find("Verification failed") != std::string::npos);
    }

    SECTION("encapsulated Pixel Data labeled Explicit VR Little Endian") {
        write_image(src, spec, kcenon::pacs::core::dicom_file::encapsulate_frames({pixels}));

        auto result = run_cli("dcm_conv",
            {src.string(), out.string(), "--explicit", "--verify"});
        INFO("dcm_conv output: " << result.output);
        CHECK(result.exit_code == 2);
        CHECK(result.output.find("Verification failed") != std::string::npos);
    }

    SECTION("native Pixel Data shorter than the image") {
        write_image(src, spec, std::vector<uint8_t>(pixels.begin(), pixels.end() - 2));

        auto result = run_cli("dcm_conv",
            {src.string(), out.string(), "--explicit", "--verify"});
        INFO("dcm_conv output: " << result.output);
        CHECK(result.exit_code == 2);
    }

    SECTION("matching Pixel Data passes") {
        write_image(src, spec, pixels);

        auto result = run_cli("dcm_conv",
            {src.string(), out.string(), "--explicit", "--verify"});
        INFO("dcm_conv output: " << result.output);
        CHECK(result.exit_code == 0);
    }
}

TEST_CASE("dcm_conv --quality reaches the JPEG Baseline encoder",
          "[cli][roundtrip][transcode]") {
    using kcenon::pacs::encoding::compression::codec_factory;
    namespace tags = kcenon::pacs::core::tags;

    if (!codec_factory::is_available("1.2.840.10008.1.2.4.50")) {
        WARN("JPEG Baseline codec not built; --quality is not exercised");
        return;
    }

    temp_dir tmp;
    const image_spec gray{64, 64, 8, 8, 1, 1, "MONOCHROME2"};
    const auto src = tmp.path / "gray.dcm";
    const auto low = tmp.path / "q10.dcm";
    const auto high = tmp.path / "q95.dcm";
    write_image(src, gray, make_pixels(gray));

    auto low_result = run_cli("dcm_conv",
        {src.string(), low.string(), "--jpeg-baseline", "-q", "10", "--verify"});
    INFO("dcm_conv -q 10 output: " << low_result.output);
    REQUIRE(low_result.exit_code == 0);
    auto high_result = run_cli("dcm_conv",
        {src.string(), high.string(), "--jpeg-baseline", "-q", "95", "--verify"});
    INFO("dcm_conv -q 95 output: " << high_result.output);
    REQUIRE(high_result.exit_code == 0);

    auto low_file = read_dicom(low);
    auto high_file = read_dicom(high);
    CHECK(pixel_data_of(low_file).size() < pixel_data_of(high_file).size());
    CHECK(get_tag(low_file, lossy_image_compression_tag) == "01");
    CHECK(get_tag(low_file, lossy_image_compression_method_tag) == "ISO_10918_1");

    // Color JPEG Baseline is stored as YBR_FULL_422 and decodes back to RGB
    const image_spec color{32, 32, 8, 8, 3, 1, "RGB"};
    const auto color_src = tmp.path / "color.dcm";
    const auto color_jpeg = tmp.path / "color_jpeg.dcm";
    const auto color_back = tmp.path / "color_back.dcm";
    write_image(color_src, color, make_pixels(color));

    auto to_jpeg = run_cli("dcm_conv",
        {color_src.string(), color_jpeg.string(), "--jpeg-baseline", "--verify"});
    INFO("dcm_conv color output: " << to_jpeg.output);
    REQUIRE(to_jpeg.exit_code == 0);
    CHECK(get_tag(read_dicom(color_jpeg), tags::photometric_interpretation) ==
          "YBR_FULL_422");

    auto to_native = run_cli("dcm_conv",
        {color_jpeg.string(), color_back.string(), "--explicit", "--verify"});
    INFO("dcm_conv color back output: " << to_native.output);
    REQUIRE(to_native.exit_code == 0);
    auto back_file = read_dicom(color_back);
    CHECK(get_tag(back_file, tags::photometric_interpretation) == "RGB");
    CHECK(get_tag(back_file, lossy_image_compression_tag) == "01");
}

TEST_CASE("dcm_conv gives a lossy output a new SOP Instance UID and Image Type DERIVED",
          "[cli][roundtrip][transcode]") {
    using namespace kcenon::pacs::core;
    using kcenon::pacs::encoding::vr_type;

    const auto lossy_ts = first_available_lossy_codec();
    if (lossy_ts.empty()) {
        WARN("No lossy codec built; the derived instance is not exercised");
        return;
    }
    INFO("Lossy target: " << lossy_ts);

    temp_dir tmp;
    image_spec gray{64, 64, 8, 8, 1, 1, "MONOCHROME2"};
    gray.image_type = "ORIGINAL\\PRIMARY\\AXIAL";
    const auto src = tmp.path / "original.dcm";
    const auto lossy = tmp.path / "lossy.dcm";
    write_image(src, gray, make_pixels(gray));
    const auto source = read_dicom(src);
    const auto source_class = get_tag(source, tags::sop_class_uid);
    const auto source_uid = get_tag(source, tags::sop_instance_uid);

    auto result = run_cli("dcm_conv",
        {src.string(), lossy.string(), "-t", lossy_ts, "--verify"});
    INFO("dcm_conv lossy output: " << result.output);
    REQUIRE(result.exit_code == 0);

    // PS3.3 C.7.6.1.1.5: new SOP Instance UID and Image Type Value 1 DERIVED
    const auto lossy_file = read_dicom(lossy);
    const auto lossy_uid = get_tag(lossy_file, tags::sop_instance_uid);
    CHECK(lossy_uid != source_uid);
    CHECK(lossy_uid.starts_with(dcm_conv_uid_prefix));
    CHECK(is_valid_uid(lossy_uid));
    CHECK(lossy_file.meta_information().get_string(tags::media_storage_sop_instance_uid) ==
          lossy_uid);
    CHECK(get_tag(lossy_file, tags::sop_class_uid) == source_class);
    CHECK(get_tag(lossy_file, tags::image_type) == "DERIVED\\PRIMARY\\AXIAL");
    CHECK(get_tag(lossy_file, lossy_image_compression_tag) == "01");
    CHECK(get_tag(lossy_file, derivation_description_tag).starts_with("Lossy compression with "));

    const auto* items = lossy_file.dataset().get_sequence(source_image_sequence_tag);
    REQUIRE(items != nullptr);
    REQUIRE(items->size() == 1);
    CHECK(items->front().get_string(tags::referenced_sop_class_uid) == source_class);
    CHECK(items->front().get_string(tags::referenced_sop_instance_uid) == source_uid);

    SECTION("each lossy conversion gets its own UID") {
        const auto again = tmp.path / "lossy_again.dcm";
        REQUIRE(run_cli("dcm_conv", {src.string(), again.string(), "-t", lossy_ts})
                    .exit_code == 0);
        CHECK(get_tag(read_dicom(again), tags::sop_instance_uid) != lossy_uid);
    }

    SECTION("decompression keeps the UID and Lossy Image Compression") {
        const auto native = tmp.path / "native.dcm";
        auto to_native = run_cli("dcm_conv",
            {lossy.string(), native.string(), "--explicit", "--verify"});
        INFO("dcm_conv --explicit output: " << to_native.output);
        REQUIRE(to_native.exit_code == 0);

        const auto native_file = read_dicom(native);
        CHECK(get_tag(native_file, tags::sop_instance_uid) == lossy_uid);
        CHECK(native_file.meta_information().get_string(
                  tags::media_storage_sop_instance_uid) == lossy_uid);
        CHECK(get_tag(native_file, lossy_image_compression_tag) == "01");
        CHECK(get_tag(native_file, tags::image_type) == "DERIVED\\PRIMARY\\AXIAL");
    }

    SECTION("lossless recompression keeps the UID") {
        const auto rle = tmp.path / "rle.dcm";
        REQUIRE(run_cli("dcm_conv", {lossy.string(), rle.string(), "--rle"}).exit_code == 0);
        CHECK(get_tag(read_dicom(rle), tags::sop_instance_uid) == lossy_uid);
    }

    SECTION("an absent or single-valued Image Type becomes DERIVED\\SECONDARY") {
        for (const std::string image_type : {"", "ORIGINAL"}) {
            INFO("Source Image Type: '" << image_type << "'");
            image_spec spec = gray;
            spec.image_type = image_type;
            const auto input = tmp.path / "image_type.dcm";
            const auto output = tmp.path / "image_type_lossy.dcm";
            write_image(input, spec, make_pixels(spec));

            REQUIRE(run_cli("dcm_conv",
                {input.string(), output.string(), "-t", lossy_ts, "--overwrite"})
                        .exit_code == 0);
            CHECK(get_tag(read_dicom(output), tags::image_type) == "DERIVED\\SECONDARY");
        }
    }

    SECTION("a previous Derivation Description follows in brackets") {
        auto file = read_dicom(src);
        auto dataset = file.dataset();
        dataset.set_string(derivation_description_tag, vr_type::ST, std::string(1100, 'x'));
        REQUIRE(dicom_file::create(std::move(dataset), file.transfer_syntax())
                    .save(src).is_ok());

        const auto derived = tmp.path / "derived.dcm";
        REQUIRE(run_cli("dcm_conv", {src.string(), derived.string(), "-t", lossy_ts})
                    .exit_code == 0);

        // Derivation Description is ST, at most 1024 characters
        const auto description = get_tag(read_dicom(derived), derivation_description_tag);
        CHECK(description.size() == 1024);
        CHECK(description.starts_with("Lossy compression with "));
        CHECK(description.find(" [xxx") != std::string::npos);
        CHECK(description.ends_with("x...]"));
    }
}

TEST_CASE("dcm_conv lossless conversions keep the SOP Instance UID and Image Type",
          "[cli][roundtrip][transcode]") {
    using kcenon::pacs::encoding::compression::codec_factory;
    namespace tags = kcenon::pacs::core::tags;

    temp_dir tmp;
    image_spec gray{32, 32, 16, 12, 1, 1, "MONOCHROME2"};
    gray.image_type = "ORIGINAL\\PRIMARY";
    const auto src = tmp.path / "original.dcm";
    write_image(src, gray, make_pixels(gray));
    const auto source_uid = get_tag(read_dicom(src), tags::sop_instance_uid);

    // Implicit VR, RLE, JPEG Lossless, JPEG-LS Lossless, and JPEG 2000
    // Lossless; HTJ2K Lossless is left out because its encoder crashes on
    // AVX-512 runners (#1257)
    for (const std::string uid : {"1.2.840.10008.1.2", "1.2.840.10008.1.2.5",
                                  "1.2.840.10008.1.2.4.70", "1.2.840.10008.1.2.4.80",
                                  "1.2.840.10008.1.2.4.90"}) {
        DYNAMIC_SECTION(uid) {
            const kcenon::pacs::encoding::transfer_syntax target{uid};
            if (target.is_encapsulated() && !codec_factory::is_available(uid)) {
                WARN("Codec not built: " << uid);
                continue;
            }
            const auto out = tmp.path / "lossless.dcm";
            auto result = run_cli("dcm_conv", {src.string(), out.string(), "-t", uid});
            INFO("dcm_conv output: " << result.output);
            REQUIRE(result.exit_code == 0);

            const auto file = read_dicom(out);
            CHECK(get_tag(file, tags::sop_instance_uid) == source_uid);
            CHECK(file.meta_information().get_string(tags::media_storage_sop_instance_uid) ==
                  source_uid);
            CHECK(get_tag(file, tags::image_type) == "ORIGINAL\\PRIMARY");
            CHECK(get_tag(file, lossy_image_compression_tag).empty());
            CHECK_FALSE(file.dataset().contains(source_image_sequence_tag));
            CHECK_FALSE(file.dataset().contains(derivation_description_tag));
        }
    }
}

TEST_CASE("json_to_dcm and xml_to_dcm keep Pixel Data and Transfer Syntax consistent",
          "[cli][roundtrip][transcode]") {
    using kcenon::pacs::encoding::transfer_syntax;

    temp_dir tmp;
    const image_spec spec{16, 16, 16, 12, 1, 1, "MONOCHROME2"};
    const auto native = tmp.path / "native.dcm";
    const auto rle = tmp.path / "rle.dcm";
    const auto pixels = make_pixels(spec);
    write_image(native, spec, pixels);
    REQUIRE(run_cli("dcm_conv", {native.string(), rle.string(), "--rle"}).exit_code == 0);
    const std::string rle_uid{transfer_syntax::rle_lossless.uid()};

    for (const std::string format : {"json", "xml"}) {
        DYNAMIC_SECTION(format) {
            const std::string exporter = "dcm_to_" + format;
            const std::string importer = format + "_to_dcm";
            const auto native_text = tmp.path / ("native." + format);
            const auto rle_text = tmp.path / ("rle." + format);

            REQUIRE(run_cli(exporter, {native.string(), native_text.string(),
                                       "--bulk-data", "inline"}).exit_code == 0);
            REQUIRE(run_cli(exporter, {rle.string(), rle_text.string(),
                                       "--bulk-data", "inline"}).exit_code == 0);

            // Native Pixel Data under a compressed Transfer Syntax
            const auto refused_native = tmp.path / ("refused_native_" + format + ".dcm");
            auto result = run_cli(importer,
                {native_text.string(), refused_native.string(), "-t", rle_uid});
            INFO(importer << " output: " << result.output);
            CHECK(result.exit_code == 2);
            CHECK_FALSE(fs::exists(refused_native));

            // Encapsulated Pixel Data under the default native Transfer Syntax
            const auto refused_rle = tmp.path / ("refused_rle_" + format + ".dcm");
            result = run_cli(importer, {rle_text.string(), refused_rle.string()});
            INFO(importer << " output: " << result.output);
            CHECK(result.exit_code == 2);
            CHECK_FALSE(fs::exists(refused_rle));

            // Encapsulated Pixel Data in the syntax it was compressed with
            const auto accepted = tmp.path / ("accepted_" + format + ".dcm");
            result = run_cli(importer,
                {rle_text.string(), accepted.string(), "-t", rle_uid});
            INFO(importer << " output: " << result.output);
            REQUIRE(result.exit_code == 0);
            auto accepted_file = read_dicom(accepted);
            CHECK(accepted_file.transfer_syntax() == transfer_syntax::rle_lossless);
            CHECK(decode_frames(accepted_file, spec) == pixels);
        }
    }
}

// ============================================================================
// Sequences (Issue #1291)
// ============================================================================

namespace {

constexpr kcenon::pacs::core::dicom_tag code_value_tag{0x0008, 0x0100};
constexpr kcenon::pacs::core::dicom_tag coding_scheme_designator_tag{0x0008, 0x0102};
constexpr kcenon::pacs::core::dicom_tag code_meaning_tag{0x0008, 0x0104};
constexpr kcenon::pacs::core::dicom_tag referenced_series_sequence_tag{0x0008, 0x1115};
constexpr kcenon::pacs::core::dicom_tag referenced_image_sequence_tag{0x0008, 0x1140};
constexpr kcenon::pacs::core::dicom_tag anatomic_region_sequence_tag{0x0008, 0x2218};
constexpr kcenon::pacs::core::dicom_tag anatomic_region_modifier_sequence_tag{0x0008, 0x2220};

/**
 * @brief A code sequence item
 */
kcenon::pacs::core::dicom_dataset code_item(const std::string& value,
                                            const std::string& meaning) {
    using kcenon::pacs::encoding::vr_type;

    kcenon::pacs::core::dicom_dataset item;
    item.set_string(code_value_tag, vr_type::SH, value);
    item.set_string(coding_scheme_designator_tag, vr_type::SH, "SCT");
    item.set_string(code_meaning_tag, vr_type::LO, meaning);
    return item;
}

/**
 * @brief Write an image whose Anatomic Region Sequence item holds an Anatomic
 *        Region Modifier Sequence
 */
void write_image_with_sequences(const fs::path& path) {
    using namespace kcenon::pacs::core;
    using kcenon::pacs::encoding::vr_type;

    dicom_dataset ds;
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.7");
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.826.0.1.3680043.9.9999.1291.1");
    ds.set_string(tags::patient_name, vr_type::PN, "SEQUENCE^TEST");
    ds.set_string(tags::patient_id, vr_type::LO, "SQ1291");
    ds.set_string(tags::modality, vr_type::CS, "OT");

    auto region = code_item("818981001", "Abdomen");
    region.get_or_create_sequence(anatomic_region_modifier_sequence_tag)
        .push_back(code_item("7771000", "Left"));
    ds.get_or_create_sequence(anatomic_region_sequence_tag).push_back(std::move(region));

    auto file = dicom_file::create(
        std::move(ds), kcenon::pacs::encoding::transfer_syntax::explicit_vr_little_endian);
    REQUIRE(file.save(path).is_ok());
}

/**
 * @brief Check the sequences that write_image_with_sequences() writes
 */
void check_image_sequences(const kcenon::pacs::core::dicom_file& file) {
    const auto* regions = file.dataset().get_sequence(anatomic_region_sequence_tag);
    REQUIRE(regions != nullptr);
    REQUIRE(regions->size() == 1);
    CHECK((*regions)[0].get_string(code_value_tag) == "818981001");
    CHECK((*regions)[0].get_string(code_meaning_tag) == "Abdomen");

    const auto* modifiers =
        (*regions)[0].get_sequence(anatomic_region_modifier_sequence_tag);
    REQUIRE(modifiers != nullptr);
    REQUIRE(modifiers->size() == 1);
    CHECK((*modifiers)[0].get_string(code_value_tag) == "7771000");
    CHECK((*modifiers)[0].get_string(code_meaning_tag) == "Left");
}

/**
 * @brief Little-endian bytes of a data element with a defined length
 * @param vr Two-character VR; SQ uses the 32-bit length form in explicit VR
 * @param explicit_vr Explicit VR encoding if true, Implicit VR otherwise
 */
std::vector<uint8_t> le_element(kcenon::pacs::core::dicom_tag tag, std::string_view vr,
                                const std::vector<uint8_t>& value, bool explicit_vr) {
    std::vector<uint8_t> bytes;
    auto write_u16 = [&bytes](uint16_t val) {
        bytes.push_back(static_cast<uint8_t>(val & 0xFF));
        bytes.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    };
    auto write_u32 = [&bytes](uint32_t val) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes.push_back(static_cast<uint8_t>((val >> shift) & 0xFF));
        }
    };

    const auto length = static_cast<uint32_t>(value.size());
    write_u16(tag.group());
    write_u16(tag.element());
    if (!explicit_vr) {
        write_u32(length);
    } else {
        bytes.push_back(static_cast<uint8_t>(vr[0]));
        bytes.push_back(static_cast<uint8_t>(vr[1]));
        if (vr == "SQ") {
            write_u16(0);
            write_u32(length);
        } else {
            write_u16(static_cast<uint16_t>(length));
        }
    }
    bytes.insert(bytes.end(), value.begin(), value.end());
    return bytes;
}

/**
 * @brief An item (FFFE,E000) with a defined length
 */
std::vector<uint8_t> le_item(const std::vector<uint8_t>& dataset) {
    const auto length = static_cast<uint32_t>(dataset.size());
    std::vector<uint8_t> bytes = {0xFE, 0xFF, 0x00, 0xE0};
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<uint8_t>((length >> shift) & 0xFF));
    }
    bytes.insert(bytes.end(), dataset.begin(), dataset.end());
    return bytes;
}

/// Referenced SOP Instance UID of the nested item, padded to even length
const std::vector<uint8_t> nested_uid_value = {'1', '.', '2', '.', '3', '.', '1',
                                               '2', '9', '1', '.', '5', '.', '1'};

/**
 * @brief Write a file whose Referenced Series Sequence and the nested
 *        Referenced Image Sequence have defined lengths
 */
void write_defined_length_sequences(const fs::path& path,
                                    const kcenon::pacs::encoding::transfer_syntax& ts) {
    using namespace kcenon::pacs::core;
    using kcenon::pacs::encoding::vr_type;

    const bool explicit_vr = ts.vr_type() == kcenon::pacs::encoding::vr_encoding::explicit_vr;
    auto concat = [](std::vector<uint8_t> a, const std::vector<uint8_t>& b) {
        a.insert(a.end(), b.begin(), b.end());
        return a;
    };

    const auto images = le_item(
        le_element(tags::referenced_sop_instance_uid, "UI", nested_uid_value, explicit_vr));
    const auto series = le_item(concat(
        le_element(referenced_image_sequence_tag, "SQ", images, explicit_vr),
        le_element(tags::series_instance_uid, "UI",
                   {'1', '.', '2', '.', '3', '.', '1', '2', '9', '1', '.', '5'},
                   explicit_vr)));

    dicom_dataset ds;
    ds.set_string(tags::sop_class_uid, vr_type::UI, "1.2.840.10008.5.1.4.1.1.7");
    ds.set_string(tags::sop_instance_uid, vr_type::UI,
                  "1.2.826.0.1.3680043.9.9999.1291.2");
    const auto bytes = concat(dicom_file::create(std::move(ds), ts).to_bytes(),
                              le_element(referenced_series_sequence_tag, "SQ", series,
                                         explicit_vr));

    std::ofstream out(path, std::ios::binary);
    REQUIRE(out.write(reinterpret_cast<const char*>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size())));
}

/**
 * @brief Check the sequences that write_defined_length_sequences() writes
 */
void check_defined_length_sequences(const kcenon::pacs::core::dicom_file& file) {
    using namespace kcenon::pacs::core;

    const auto* series = file.dataset().get_sequence(referenced_series_sequence_tag);
    REQUIRE(series != nullptr);
    REQUIRE(series->size() == 1);
    CHECK((*series)[0].get_string(tags::series_instance_uid) == "1.2.3.1291.5");

    const auto* images = (*series)[0].get_sequence(referenced_image_sequence_tag);
    REQUIRE(images != nullptr);
    REQUIRE(images->size() == 1);
    CHECK((*images)[0].get_string(tags::referenced_sop_instance_uid) == "1.2.3.1291.5.1");
}

/**
 * @brief The bytes of a file
 */
std::vector<uint8_t> read_bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/**
 * @brief Find a byte sequence in a buffer
 */
bool contains_bytes(const std::vector<uint8_t>& haystack,
                    const std::vector<uint8_t>& needle) {
    return std::search(haystack.begin(), haystack.end(), needle.begin(),
                       needle.end()) != haystack.end();
}

}  // namespace

TEST_CASE("Tag modification, anonymization, and conversion keep sequence items",
          "[cli][roundtrip][sequence]") {
    namespace tags = kcenon::pacs::core::tags;

    temp_dir tmp;
    const auto src = tmp.path / "sequences.dcm";
    write_image_with_sequences(src);

    SECTION("dcm_modify") {
        const auto out = tmp.path / "modified.dcm";
        auto result = run_cli("dcm_modify", {"-i", "(0010,0010)=MODIFIED^NAME", "-o",
                                             out.string(), src.string()});
        INFO("dcm_modify output: " << result.output);
        REQUIRE(result.exit_code == 0);

        auto modified = read_dicom(out);
        CHECK(get_tag(modified, tags::patient_name) == "MODIFIED^NAME");
        check_image_sequences(modified);
    }

    SECTION("dcm_anonymize") {
        const auto out = tmp.path / "anonymized.dcm";
        auto result = run_cli("dcm_anonymize", {src.string(), out.string()});
        INFO("dcm_anonymize output: " << result.output);
        REQUIRE(result.exit_code == 0);

        auto anonymized = read_dicom(out);
        CHECK(get_tag(anonymized, tags::patient_name) != "SEQUENCE^TEST");
        check_image_sequences(anonymized);
    }

    SECTION("dcm_conv to Implicit VR Little Endian and back") {
        using kcenon::pacs::encoding::transfer_syntax;

        const auto implicit_path = tmp.path / "implicit.dcm";
        auto result = run_cli("dcm_conv",
                              {src.string(), implicit_path.string(), "--implicit"});
        INFO("dcm_conv --implicit output: " << result.output);
        REQUIRE(result.exit_code == 0);
        auto implicit_file = read_dicom(implicit_path);
        CHECK(implicit_file.transfer_syntax() == transfer_syntax::implicit_vr_little_endian);
        check_image_sequences(implicit_file);

        const auto explicit_path = tmp.path / "explicit.dcm";
        result = run_cli("dcm_conv",
                         {implicit_path.string(), explicit_path.string(), "--explicit"});
        INFO("dcm_conv --explicit output: " << result.output);
        REQUIRE(result.exit_code == 0);
        check_image_sequences(read_dicom(explicit_path));
    }
}

TEST_CASE("Transfer Syntax conversion re-encodes defined-length sequences",
          "[cli][roundtrip][sequence]") {
    using kcenon::pacs::encoding::transfer_syntax;
    namespace tags = kcenon::pacs::core::tags;

    temp_dir tmp;
    // The nested Referenced SOP Instance UID in each VR encoding
    const auto implicit_uid =
        le_element(tags::referenced_sop_instance_uid, "UI", nested_uid_value, false);
    const auto explicit_uid =
        le_element(tags::referenced_sop_instance_uid, "UI", nested_uid_value, true);

    SECTION("Explicit VR Little Endian to Implicit VR Little Endian") {
        const auto src = tmp.path / "explicit_defined.dcm";
        const auto out = tmp.path / "implicit.dcm";
        write_defined_length_sequences(src, transfer_syntax::explicit_vr_little_endian);

        auto result = run_cli("dcm_conv", {src.string(), out.string(), "--implicit"});
        INFO("dcm_conv --implicit output: " << result.output);
        REQUIRE(result.exit_code == 0);

        const auto bytes = read_bytes(out);
        CHECK(contains_bytes(bytes, implicit_uid));
        CHECK_FALSE(contains_bytes(bytes, explicit_uid));
        check_defined_length_sequences(read_dicom(out));
    }

    SECTION("Implicit VR Little Endian to Explicit VR Little Endian") {
        const auto src = tmp.path / "implicit_defined.dcm";
        const auto out = tmp.path / "explicit.dcm";
        write_defined_length_sequences(src, transfer_syntax::implicit_vr_little_endian);

        auto result = run_cli("dcm_conv", {src.string(), out.string(), "--explicit"});
        INFO("dcm_conv --explicit output: " << result.output);
        REQUIRE(result.exit_code == 0);

        const auto bytes = read_bytes(out);
        CHECK(contains_bytes(bytes, explicit_uid));
        CHECK_FALSE(contains_bytes(bytes, implicit_uid));
        check_defined_length_sequences(read_dicom(out));
    }
}
