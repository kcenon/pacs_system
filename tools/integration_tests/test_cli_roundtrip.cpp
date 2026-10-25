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
 */

#include <kcenon/pacs/core/dicom_file.h>
#include <kcenon/pacs/core/dicom_tag_constants.h>
#include <kcenon/pacs/encoding/compression/codec_factory.h>
#include <kcenon/pacs/encoding/transfer_syntax.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
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
