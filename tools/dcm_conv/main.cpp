/**
 * @file main.cpp
 * @brief DICOM Conversion - Transfer Syntax Conversion Utility
 * @example dcm_conv/main.cpp
 *
 * A command-line utility for converting DICOM files between Transfer
 * Syntaxes. Between native (uncompressed) Transfer Syntaxes only the dataset
 * encoding changes. When the source or the target is an encapsulated
 * (compressed) Transfer Syntax, every frame of Pixel Data is decoded with the
 * source codec and encoded with the target codec, and the attributes that the
 * new encoding changes are updated. A lossy target makes a new, derived image
 * with its own SOP Instance UID. A file fails to convert when this build
 * does not include a codec it needs or the codec cannot handle the image.
 *
 * @see Issue #280 - dcm_conv: Transfer Syntax conversion utility
 * @see Issue #1276 - dcm_conv relabels Transfer Syntaxes without transcoding
 * @see Issue #1294 - dcm_conv keeps the SOP Instance UID after lossy compression
 * @see DICOM PS3.3 Section C.7.6.1.1.5 - Lossy Image Compression
 * @see DICOM PS3.5 Section 8 - Encoding of Pixel, Overlay and Waveform Data
 * @see DICOM PS3.5 Section 10 - Transfer Syntax
 *
 * @code{.unparsed}
 * Usage:
 *   dcm_conv <input> <output> [options]
 *
 * Example:
 *   dcm_conv image.dcm converted.dcm --explicit
 *   dcm_conv image.dcm compressed.dcm --rle
 *   dcm_conv image.dcm compressed.dcm --jpeg-baseline -q 90
 *   dcm_conv ./input_dir/ ./output_dir/ --recursive --implicit
 * @endcode
 */

#include "kcenon/pacs/core/dicom_file.h"
#include "kcenon/pacs/core/dicom_tag_constants.h"
#include "kcenon/pacs/encoding/compression/codec_factory.h"
#include "kcenon/pacs/encoding/compression/compression_codec.h"
#include "kcenon/pacs/encoding/transfer_syntax.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using kcenon::pacs::core::dicom_tag;

/// Root of the UIDs dcm_conv creates, next to the pacs_system roots of the
/// other file tools (dcm_modify .2, dcm_dir .3, img_to_dcm .4)
constexpr std::string_view kUidRoot = "1.2.826.0.1.3680043.8.1055.5";

// Attributes without a constant in dicom_tag_constants.h
constexpr dicom_tag kDerivationDescription{0x0008, 0x2111};
constexpr dicom_tag kSourceImageSequence{0x0008, 0x2112};
constexpr dicom_tag kPlanarConfiguration{0x0028, 0x0006};
constexpr dicom_tag kNumberOfFrames{0x0028, 0x0008};
constexpr dicom_tag kLossyImageCompression{0x0028, 0x2110};
constexpr dicom_tag kLossyImageCompressionRatio{0x0028, 0x2112};
constexpr dicom_tag kLossyImageCompressionMethod{0x0028, 0x2114};
constexpr dicom_tag kExtendedOffsetTable{0x7FE0, 0x0001};
constexpr dicom_tag kExtendedOffsetTableLengths{0x7FE0, 0x0002};
constexpr dicom_tag kFloatPixelData{0x7FE0, 0x0008};
constexpr dicom_tag kDoubleFloatPixelData{0x7FE0, 0x0009};

/**
 * @brief Command line options
 */
struct options {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::string target_transfer_syntax;
    int quality{90};
    bool recursive{false};
    bool verify{false};
    bool overwrite{false};
    bool verbose{false};
    bool quiet{false};
    bool show_progress{true};
    bool list_syntaxes{false};
};

/**
 * @brief Conversion statistics
 */
struct conversion_stats {
    size_t total_files{0};
    size_t success_count{0};
    size_t skip_count{0};
    size_t error_count{0};
    std::chrono::milliseconds total_time{0};
};

/**
 * @brief Print usage information
 * @param program_name The name of the executable
 */
void print_usage(const char* program_name) {
    std::cout << R"(
DICOM Conversion - Transfer Syntax Conversion Utility

Usage: )" << program_name
              << R"( <input> <output> [options]

Arguments:
  input               Input DICOM file or directory
  output              Output DICOM file or directory

Transfer Syntax Options:
  --implicit          Implicit VR Little Endian (1.2.840.10008.1.2)
  --explicit          Explicit VR Little Endian (1.2.840.10008.1.2.1) [default]
  --explicit-be       Explicit VR Big Endian (1.2.840.10008.1.2.2) [retired]
  --jpeg-baseline     JPEG Baseline (Process 1), lossy [needs libjpeg-turbo]
  --jpeg-lossless     JPEG Lossless, Non-Hierarchical, First-Order Prediction
  --jpeg2000          JPEG 2000 Lossless Only [needs OpenJPEG]
  --jpeg2000-lossy    JPEG 2000, lossy [needs OpenJPEG]
  --htj2k             HTJ2K Lossless Only (1.2.840.10008.1.2.4.201) [needs OpenJPH]
  --htj2k-rpcl        HTJ2K with RPCL Options (1.2.840.10008.1.2.4.202) [needs OpenJPH]
  --htj2k-lossy       HTJ2K, lossy (1.2.840.10008.1.2.4.203) [needs OpenJPH]
  --rle               RLE Lossless
  -t, --transfer-syntax <uid>  Specify Transfer Syntax by UID

Conversion:
  Between native syntaxes only the dataset encoding changes. When the source
  or the target is compressed, every frame of Pixel Data is decoded and
  re-encoded with the codecs in this build (see --list-syntaxes), and
  Photometric Interpretation and, for lossy targets, Lossy Image Compression
  (0028,2110), Ratio (0028,2112), and Method (0028,2114) are updated. A file
  fails to convert when a codec it needs is not in this build or cannot
  encode the image (for example, JPEG Lossless with color images).

  A lossy target makes a new image (PS3.3 C.7.6.1.1.5): the output gets a
  new SOP Instance UID, which Media Storage SOP Instance UID repeats; Value 1
  of Image Type (0008,0008) becomes DERIVED; Source Image Sequence (0008,2112)
  references the source instance; and Derivation Description (0008,2111)
  names the compression and its ratio. Lossless targets and decompression
  keep the SOP Instance UID and Image Type.

Compression Options:
  -q, --quality <1-100>   Quality for lossy targets (default: 90): JPEG
                          Baseline quality, JPEG-LS near-lossless NEAR value
                          (100 = lossless), and JPEG 2000 compression ratio;
                          lossless targets and HTJ2K ignore it

Processing Options:
  -r, --recursive         Process directory recursively
  --overwrite             Overwrite existing output files
  --verify                Reopen each output and check that its Pixel Data
                          matches its Transfer Syntax: decode every frame
                          (compressed) or check the length against Rows,
                          Columns, Samples per Pixel, Bits Allocated, and
                          Number of Frames (native)
  -v, --verbose           Verbose output
  --quiet                 Minimal output (errors only)
  --no-progress           Disable progress display

Information:
  --list-syntaxes         List Transfer Syntaxes and the codecs in this build
  -h, --help              Show this help message

Examples:
  )" << program_name
              << R"( image.dcm converted.dcm --explicit
  )" << program_name
              << R"( image.dcm compressed.dcm --rle
  )" << program_name
              << R"( image.dcm compressed.dcm --jpeg-baseline -q 85
  )" << program_name
              << R"( ./input_dir/ ./output_dir/ --recursive --implicit
  )" << program_name
              << R"( image.dcm htj2k.dcm --htj2k
  )" << program_name
              << R"( image.dcm output.dcm -t 1.2.840.10008.1.2.4.50

Exit Codes:
  0  Success - All files converted successfully
  1  Error - Invalid arguments or help requested
  2  Error - Conversion or verification failed for one or more files
)";
}

/**
 * @brief List all supported transfer syntaxes
 */
void list_supported_syntaxes() {
    using namespace kcenon::pacs::encoding;

    std::cout << "\nSupported Transfer Syntaxes:\n";
    std::cout << std::string(70, '-') << "\n";
    std::cout << std::left << std::setw(40) << "Name"
              << std::setw(30) << "UID" << "\n";
    std::cout << std::string(70, '-') << "\n";

    auto syntaxes = supported_transfer_syntaxes();
    for (const auto& ts : syntaxes) {
        std::cout << std::left << std::setw(40) << ts.name()
                  << std::setw(30) << ts.uid() << "\n";
    }

    std::cout << std::string(70, '-') << "\n";
    std::cout << "Total: " << syntaxes.size() << " transfer syntaxes\n\n";

    // Also list the compression codecs this build includes
    auto codec_uids = compression::codec_factory::available_transfer_syntaxes();
    if (!codec_uids.empty()) {
        std::cout << "Compression Codecs Available:\n";
        std::cout << std::string(50, '-') << "\n";
        for (const auto& uid : codec_uids) {
            if (auto ts = find_transfer_syntax(uid); ts.has_value()) {
                std::cout << "  " << ts->name() << "\n";
            }
        }
        std::cout << std::string(50, '-') << "\n";
    }
}

/**
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param opts Output: parsed options
 * @return true if arguments are valid
 */
bool parse_arguments(int argc, char* argv[], options& opts) {
    using namespace kcenon::pacs::encoding;

    if (argc < 2) {
        return false;
    }

    // Default to Explicit VR Little Endian
    opts.target_transfer_syntax = std::string(transfer_syntax::explicit_vr_little_endian.uid());

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            return false;
        } else if (arg == "--list-syntaxes") {
            opts.list_syntaxes = true;
            return true;
        } else if (arg == "--implicit") {
            opts.target_transfer_syntax = std::string(transfer_syntax::implicit_vr_little_endian.uid());
        } else if (arg == "--explicit") {
            opts.target_transfer_syntax = std::string(transfer_syntax::explicit_vr_little_endian.uid());
        } else if (arg == "--explicit-be") {
            opts.target_transfer_syntax = std::string(transfer_syntax::explicit_vr_big_endian.uid());
        } else if (arg == "--jpeg-baseline") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg_baseline.uid());
        } else if (arg == "--jpeg-lossless") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg_lossless.uid());
        } else if (arg == "--jpeg2000") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg2000_lossless.uid());
        } else if (arg == "--jpeg2000-lossy") {
            opts.target_transfer_syntax = std::string(transfer_syntax::jpeg2000_lossy.uid());
        } else if (arg == "--htj2k") {
            opts.target_transfer_syntax = std::string(transfer_syntax::htj2k_lossless.uid());
        } else if (arg == "--htj2k-rpcl") {
            opts.target_transfer_syntax = std::string(transfer_syntax::htj2k_rpcl.uid());
        } else if (arg == "--htj2k-lossy") {
            opts.target_transfer_syntax = std::string(transfer_syntax::htj2k_lossy.uid());
        } else if (arg == "--rle") {
            opts.target_transfer_syntax = std::string(transfer_syntax::rle_lossless.uid());
        } else if ((arg == "-t" || arg == "--transfer-syntax") && i + 1 < argc) {
            opts.target_transfer_syntax = argv[++i];
        } else if ((arg == "-q" || arg == "--quality") && i + 1 < argc) {
            try {
                opts.quality = std::stoi(argv[++i]);
                if (opts.quality < 1 || opts.quality > 100) {
                    std::cerr << "Error: Quality must be between 1 and 100\n";
                    return false;
                }
            } catch (...) {
                std::cerr << "Error: Invalid quality value\n";
                return false;
            }
        } else if (arg == "-r" || arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "--overwrite") {
            opts.overwrite = true;
        } else if (arg == "--verify") {
            opts.verify = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "--quiet") {
            opts.quiet = true;
            opts.show_progress = false;
        } else if (arg == "--no-progress") {
            opts.show_progress = false;
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        } else if (opts.input_path.empty()) {
            opts.input_path = arg;
        } else if (opts.output_path.empty()) {
            opts.output_path = arg;
        } else {
            std::cerr << "Error: Too many arguments\n";
            return false;
        }
    }

    // Validate transfer syntax
    auto ts = find_transfer_syntax(opts.target_transfer_syntax);
    if (!ts.has_value()) {
        std::cerr << "Error: Unknown Transfer Syntax UID '" << opts.target_transfer_syntax << "'\n";
        std::cerr << "Use --list-syntaxes to see available options\n";
        return false;
    }

    if (!opts.list_syntaxes) {
        if (opts.input_path.empty()) {
            std::cerr << "Error: No input path specified\n";
            return false;
        }

        if (opts.output_path.empty()) {
            std::cerr << "Error: No output path specified\n";
            return false;
        }
    }

    // Quiet mode overrides verbose
    if (opts.quiet) {
        opts.verbose = false;
    }

    return true;
}

/**
 * @brief Image Pixel attributes that describe the Pixel Data of a dataset
 */
struct pixel_description {
    kcenon::pacs::encoding::compression::image_params params;
    std::string photometric;  ///< Photometric Interpretation (0028,0004)
};

/**
 * @brief Read the Image Pixel attributes of a dataset
 * @param dataset Dataset with Pixel Data
 * @param desc Output: the attributes; Number of Frames is 1 when absent
 * @param error Output: the reason on failure
 * @return true if Rows, Columns, Bits Allocated, and Number of Frames are valid
 */
bool read_pixel_description(const kcenon::pacs::core::dicom_dataset& dataset,
                            pixel_description& desc, std::string& error) {
    using namespace kcenon::pacs::core;
    namespace compression = kcenon::pacs::encoding::compression;

    const auto rows = dataset.get_numeric<uint16_t>(tags::rows);
    const auto columns = dataset.get_numeric<uint16_t>(tags::columns);
    const auto bits_allocated = dataset.get_numeric<uint16_t>(tags::bits_allocated);
    if (!rows || !columns || !bits_allocated || *rows == 0 || *columns == 0 ||
        *bits_allocated == 0) {
        error = "Pixel Data needs non-zero Rows, Columns, and Bits Allocated";
        return false;
    }

    auto& params = desc.params;
    params.height = *rows;
    params.width = *columns;
    params.bits_allocated = *bits_allocated;
    params.bits_stored =
        dataset.get_numeric<uint16_t>(tags::bits_stored).value_or(params.bits_allocated);
    params.high_bit = dataset.get_numeric<uint16_t>(tags::high_bit)
                          .value_or(static_cast<uint16_t>(params.bits_stored - 1));
    params.samples_per_pixel =
        dataset.get_numeric<uint16_t>(tags::samples_per_pixel).value_or(1);
    params.pixel_representation =
        dataset.get_numeric<uint16_t>(tags::pixel_representation).value_or(0);
    params.planar_configuration =
        dataset.get_numeric<uint16_t>(kPlanarConfiguration).value_or(0);
    desc.photometric = dataset.get_string(tags::photometric_interpretation);
    params.photometric = compression::parse_photometric_interpretation(desc.photometric);

    params.number_of_frames = 1;
    if (const auto frames = dataset.get_string(kNumberOfFrames); !frames.empty()) {
        try {
            const auto value = std::stol(frames);
            if (value < 1) {
                throw std::out_of_range(frames);
            }
            params.number_of_frames = static_cast<uint32_t>(value);
        } catch (const std::exception&) {
            error = "Invalid Number of Frames '" + frames + "'";
            return false;
        }
    }

    if (params.samples_per_pixel == 0) {
        error = "Samples per Pixel is 0";
        return false;
    }
    return true;
}

/**
 * @brief Expected byte length of native Pixel Data (PS3.5 Section 8.1.1)
 *
 * Rows x Columns x Samples per Pixel x Bits Allocated x Number of Frames,
 * in bytes rounded up; YBR_FULL_422 stores two samples per pixel.
 */
uint64_t native_pixel_data_length(const pixel_description& desc) {
    const auto& params = desc.params;
    const uint64_t samples =
        desc.photometric == "YBR_FULL_422" ? 2 : params.samples_per_pixel;
    const uint64_t bits = uint64_t{params.width} * params.height * samples *
                          params.bits_allocated * params.number_of_frames;
    return (bits + 7) / 8;
}

/**
 * @brief Reorder a color-by-plane frame (Planar Configuration 1) to color-by-pixel
 */
std::vector<uint8_t> to_color_by_pixel(
    std::span<const uint8_t> frame,
    const kcenon::pacs::encoding::compression::image_params& params) {
    const size_t pixels = size_t{params.width} * params.height;
    const size_t samples = params.samples_per_pixel;
    const size_t sample_bytes = params.bits_allocated / 8;

    std::vector<uint8_t> result(frame.size());
    for (size_t s = 0; s < samples; ++s) {
        for (size_t p = 0; p < pixels; ++p) {
            std::memcpy(result.data() + (p * samples + s) * sample_bytes,
                        frame.data() + (s * pixels + p) * sample_bytes, sample_bytes);
        }
    }
    return result;
}

/**
 * @brief Photometric Interpretation of the pixels a source codec decodes
 *
 * The JPEG Baseline decoder returns RGB for color images, and the JPEG 2000
 * and HTJ2K decoders undo the component transform that YBR_RCT and YBR_ICT
 * describe. The other codecs return the components as stored.
 */
std::string decoded_photometric(const kcenon::pacs::encoding::transfer_syntax& source_ts,
                                const pixel_description& desc) {
    if (desc.params.samples_per_pixel != 3) {
        return desc.photometric;
    }
    if (source_ts == kcenon::pacs::encoding::transfer_syntax::jpeg_baseline ||
        desc.photometric == "YBR_RCT" || desc.photometric == "YBR_ICT") {
        return "RGB";
    }
    return desc.photometric;
}

/**
 * @brief Photometric Interpretation of the pixels a target codec encodes
 * @param target_ts The encapsulated target Transfer Syntax
 * @param lossy Whether the target codec is lossy
 * @param desc The native pixels to encode
 * @param photometric Output: the value to store with the encoded pixels
 * @param error Output: the reason on failure
 * @return true if the target can store this Photometric Interpretation
 *
 * PS3.5 Section 8.2: the JPEG Baseline encoder converts RGB to YCbCr with
 * 4:2:2 subsampling (YBR_FULL_422); the HTJ2K encoder applies the reversible
 * (YBR_RCT) or irreversible (YBR_ICT) component transform to RGB; the JPEG
 * 2000, JPEG-LS, and RLE encoders store RGB or YBR_FULL components
 * unchanged. PALETTE COLOR indices are not compressed lossily.
 */
bool encoded_photometric(const kcenon::pacs::encoding::transfer_syntax& target_ts,
                         bool lossy, const pixel_description& desc,
                         std::string& photometric, std::string& error) {
    using kcenon::pacs::encoding::transfer_syntax;

    photometric = desc.photometric;
    if (lossy && desc.photometric == "PALETTE COLOR") {
        error = "PALETTE COLOR images are not compressed with the lossy " +
                std::string(target_ts.name());
        return false;
    }
    if (desc.params.samples_per_pixel != 3) {
        return true;
    }

    const bool rgb = desc.photometric == "RGB";
    if (target_ts == transfer_syntax::jpeg_baseline) {
        photometric = "YBR_FULL_422";
    } else if (target_ts == transfer_syntax::htj2k_lossless ||
               target_ts == transfer_syntax::htj2k_rpcl) {
        photometric = "YBR_RCT";
    } else if (target_ts == transfer_syntax::htj2k_lossy) {
        photometric = "YBR_ICT";
    } else if (rgb || desc.photometric == "YBR_FULL") {
        return true;
    }

    if (!rgb) {
        error = "Photometric Interpretation '" + desc.photometric +
                "' cannot be encoded as " + std::string(target_ts.name());
        return false;
    }
    return true;
}

/**
 * @brief Lossy Image Compression Method (0028,2114) of a lossy target
 * @return The Defined Term of PS3.3 Section C.7.6.1.1.5.1
 */
std::string lossy_compression_method(const kcenon::pacs::encoding::transfer_syntax& target_ts) {
    using kcenon::pacs::encoding::transfer_syntax;

    if (target_ts == transfer_syntax::jpeg_baseline) {
        return "ISO_10918_1";
    }
    if (target_ts == transfer_syntax::jpeg_ls_near_lossless) {
        return "ISO_14495_1";
    }
    if (target_ts == transfer_syntax::htj2k_lossy) {
        return "ISO_15444_15";
    }
    return "ISO_15444_1";  // JPEG 2000 (irreversible)
}

/**
 * @brief Append a value to a multi-valued string attribute
 */
void append_value(kcenon::pacs::core::dicom_dataset& dataset, dicom_tag tag,
                  kcenon::pacs::encoding::vr_type vr, const std::string& value) {
    const auto existing = dataset.get_string(tag);
    dataset.set_string(tag, vr, existing.empty() ? value : existing + "\\" + value);
}

/**
 * @brief Generate a new UID under kUidRoot
 * @return <root>.<milliseconds since the epoch>.<random>, at most 58 characters
 *
 * The random component keeps UIDs apart that two dcm_conv processes create
 * in the same millisecond. Each thread owns its engine, seeded once from
 * std::random_device.
 */
std::string generate_uid() {
    static thread_local std::mt19937_64 gen{std::random_device{}()};
    static thread_local std::uniform_int_distribution<uint64_t> dist{
        0, 999'999'999'999'999};

    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    return std::string(kUidRoot) + "." + std::to_string(timestamp) + "." +
           std::to_string(dist(gen));
}

/**
 * @brief Give a lossy compressed image the identity of a derived image
 * @param dataset The dataset whose Pixel Data was just compressed lossily
 * @param target_ts The lossy target Transfer Syntax
 * @param ratio The compression ratio of this step, as in (0028,2112)
 *
 * PS3.3 Section C.7.6.1.1.5: Value 1 of Image Type becomes DERIVED and the
 * image receives a new SOP Instance UID, which dicom_file::create() repeats
 * in Media Storage SOP Instance UID. Image Type has VM 2-n, so an absent or
 * single-valued Image Type becomes the two values DERIVED and SECONDARY.
 * Source Image Sequence is replaced by one item that references the source
 * instance, and Derivation Description names the compression, followed by
 * the previous description in brackets.
 */
void derive_lossy_instance(kcenon::pacs::core::dicom_dataset& dataset,
                           const kcenon::pacs::encoding::transfer_syntax& target_ts,
                           const std::string& ratio) {
    using namespace kcenon::pacs::core;
    using kcenon::pacs::encoding::vr_type;

    const auto image_type = dataset.get_string(tags::image_type);
    const auto separator = image_type.find('\\');
    dataset.set_string(tags::image_type, vr_type::CS,
                       separator == std::string::npos
                           ? std::string("DERIVED\\SECONDARY")
                           : "DERIVED" + image_type.substr(separator));

    const auto sop_class_uid = dataset.get_string(tags::sop_class_uid);
    const auto sop_instance_uid = dataset.get_string(tags::sop_instance_uid);
    dataset.remove(kSourceImageSequence);
    if (!sop_class_uid.empty() && !sop_instance_uid.empty()) {
        dicom_dataset source;
        source.set_string(tags::referenced_sop_class_uid, vr_type::UI, sop_class_uid);
        source.set_string(tags::referenced_sop_instance_uid, vr_type::UI,
                          sop_instance_uid);
        dataset.get_or_create_sequence(kSourceImageSequence).push_back(std::move(source));
    }

    // Derivation Description is ST, at most 1024 characters
    std::string description = "Lossy compression with " + std::string(target_ts.name()) +
                              ", ratio " + ratio + ":1";
    if (const auto previous = dataset.get_string(kDerivationDescription);
        !previous.empty()) {
        description += " [" + previous + "]";
        if (description.size() > 1024) {
            description.resize(1020);
            description += "...]";
        }
    }
    dataset.set_string(kDerivationDescription, vr_type::ST, description);

    dataset.set_string(tags::sop_instance_uid, vr_type::UI, generate_uid());
}

/**
 * @brief Decode and re-encode Pixel Data for a new Transfer Syntax
 * @param dataset The dataset to change in place
 * @param source_ts Transfer Syntax the Pixel Data is encoded in
 * @param target_ts Transfer Syntax to encode the Pixel Data in
 * @param opts Conversion options (quality)
 * @param error Output: the reason on failure
 * @return true on success; on failure dataset may be partly changed
 *
 * Decodes each frame with the source codec (or splits native Pixel Data into
 * frames), then encodes each frame with the target codec (or concatenates
 * native frames). Updates Photometric Interpretation and Planar
 * Configuration, adds Lossy Image Compression attributes and makes a derived
 * instance (derive_lossy_instance()) for lossy targets, and removes the
 * Extended Offset Table, which describes the old encoding. Number of Frames
 * does not change.
 */
bool transcode_pixel_data(kcenon::pacs::core::dicom_dataset& dataset,
                          const kcenon::pacs::encoding::transfer_syntax& source_ts,
                          const kcenon::pacs::encoding::transfer_syntax& target_ts,
                          const options& opts, std::string& error) {
    using namespace kcenon::pacs::core;
    using namespace kcenon::pacs::encoding;

    const auto* pixel_data = dataset.get(tags::pixel_data);
    if (pixel_data == nullptr) {
        return true;
    }

    pixel_description desc;
    if (!read_pixel_description(dataset, desc, error)) {
        return false;
    }
    auto& params = desc.params;
    if (params.bits_allocated % 8 != 0) {
        error = "Bits Allocated " + std::to_string(params.bits_allocated) +
                " cannot be compressed or decompressed";
        return false;
    }
    if (params.bits_allocated > 8 &&
        (source_ts.endianness() == byte_order::big_endian ||
         target_ts.endianness() == byte_order::big_endian)) {
        error = "Explicit VR Big Endian Pixel Data with Bits Allocated > 8 "
                "cannot be compressed or decompressed";
        return false;
    }
    const size_t frame_bytes = params.frame_size_bytes();

    // Native frames, color-by-pixel
    std::vector<std::vector<uint8_t>> frames;
    if (source_ts.is_encapsulated()) {
        auto split = dicom_file::encapsulated_frames(pixel_data->raw_data(),
                                                     params.number_of_frames);
        if (split.is_err()) {
            error = split.error().message;
            return false;
        }

        auto codec = compression::codec_factory::create(source_ts);
        for (size_t i = 0; i < split.value().size(); ++i) {
            auto decoded = codec->decode(split.value()[i], params);
            if (decoded.is_err()) {
                error = "Frame " + std::to_string(i + 1) + ": " +
                        decoded.error().message;
                return false;
            }
            if (decoded.value().data.size() != frame_bytes) {
                error = "Frame " + std::to_string(i + 1) + " decoded to " +
                        std::to_string(decoded.value().data.size()) +
                        " bytes, expected " + std::to_string(frame_bytes);
                return false;
            }
            frames.push_back(std::move(decoded.value().data));
        }
        desc.photometric = decoded_photometric(source_ts, desc);
    } else {
        if (desc.photometric == "YBR_FULL_422" ||
            desc.photometric.starts_with("YBR_PARTIAL")) {
            error = "Native " + desc.photometric + " Pixel Data cannot be compressed";
            return false;
        }

        const auto raw = pixel_data->raw_data();
        const uint64_t expected = uint64_t{frame_bytes} * params.number_of_frames;
        if (raw.size() < expected) {
            error = "Pixel Data has " + std::to_string(raw.size()) +
                    " bytes, expected " + std::to_string(expected);
            return false;
        }
        for (uint32_t i = 0; i < params.number_of_frames; ++i) {
            const auto frame = raw.subspan(i * frame_bytes, frame_bytes);
            if (params.planar_configuration == 1 && params.samples_per_pixel > 1) {
                frames.push_back(to_color_by_pixel(frame, params));
            } else {
                frames.emplace_back(frame.begin(), frame.end());
            }
        }
    }
    params.planar_configuration = 0;
    params.photometric = compression::parse_photometric_interpretation(desc.photometric);

    std::string photometric = desc.photometric;
    if (target_ts.is_encapsulated()) {
        auto codec = compression::codec_factory::create(target_ts);
        if (!codec->can_encode(params)) {
            error = std::string(codec->name()) + " cannot encode images with " +
                    std::to_string(params.samples_per_pixel) +
                    " samples per pixel, Bits Allocated " +
                    std::to_string(params.bits_allocated) + ", and Bits Stored " +
                    std::to_string(params.bits_stored);
            return false;
        }
        if (!encoded_photometric(target_ts, codec->is_lossy(), desc, photometric,
                                 error)) {
            return false;
        }

        compression::compression_options compression_opts;
        compression_opts.quality = opts.quality;
        compression_opts.chroma_subsampling = 1;  // 4:2:2, as YBR_FULL_422 states

        std::vector<std::vector<uint8_t>> encoded_frames;
        size_t compressed_bytes = 0;
        for (size_t i = 0; i < frames.size(); ++i) {
            auto encoded = codec->encode(frames[i], params, compression_opts);
            if (encoded.is_err()) {
                error = "Frame " + std::to_string(i + 1) + ": " +
                        encoded.error().message;
                return false;
            }
            compressed_bytes += encoded.value().data.size();
            encoded_frames.push_back(std::move(encoded.value().data));
        }

        const auto value = dicom_file::encapsulate_frames(encoded_frames);
        dataset.insert(dicom_element{tags::pixel_data, vr_type::OB, value});

        if (codec->is_lossy()) {
            // PS3.3 Section C.7.6.1.1.5: once lossy, always lossy; ratio and
            // method values accumulate, one per lossy compression step
            std::ostringstream ratio;
            ratio << std::fixed << std::setprecision(2)
                  << static_cast<double>(frame_bytes * frames.size()) /
                         static_cast<double>((std::max)(compressed_bytes, size_t{1}));
            dataset.set_string(kLossyImageCompression, vr_type::CS, "01");
            append_value(dataset, kLossyImageCompressionRatio, vr_type::DS, ratio.str());
            append_value(dataset, kLossyImageCompressionMethod, vr_type::CS,
                         lossy_compression_method(target_ts));

            derive_lossy_instance(dataset, target_ts, ratio.str());
            if (opts.verbose) {
                std::cout << "    New SOP Instance UID: "
                          << dataset.get_string(tags::sop_instance_uid) << "\n";
            }
        }
    } else {
        std::vector<uint8_t> value;
        value.reserve(frame_bytes * frames.size() + 1);
        for (const auto& frame : frames) {
            value.insert(value.end(), frame.begin(), frame.end());
        }
        if (value.size() % 2 != 0) {
            value.push_back(0x00);
        }
        dataset.insert(dicom_element{
            tags::pixel_data,
            params.bits_allocated > 8 ? vr_type::OW : vr_type::OB, value});
    }

    dataset.set_string(tags::photometric_interpretation, vr_type::CS, photometric);
    if (params.samples_per_pixel > 1) {
        dataset.set_numeric<uint16_t>(kPlanarConfiguration, vr_type::US, 0);
    }
    dataset.remove(kExtendedOffsetTable);
    dataset.remove(kExtendedOffsetTableLengths);
    return true;
}

/**
 * @brief Check that the Pixel Data of a file matches its Transfer Syntax
 * @param file The file to check
 * @param opts Conversion options (verbose)
 * @param error Output: the reason on failure
 * @return true if the file has no Pixel Data or its Pixel Data matches
 *
 * Encapsulated Pixel Data must split into Number of Frames frames that this
 * build's codec decodes to frames of the expected size; when the build has
 * no codec for the Transfer Syntax, only the encapsulation is checked.
 * Native Pixel Data must have the length native_pixel_data_length() gives,
 * plus one padding byte when that is odd.
 */
bool verify_pixel_data(const kcenon::pacs::core::dicom_file& file,
                       const options& opts, std::string& error) {
    using namespace kcenon::pacs::core;
    using namespace kcenon::pacs::encoding;

    const auto& dataset = file.dataset();
    const auto* pixel_data = dataset.get(tags::pixel_data);
    if (pixel_data == nullptr) {
        return true;
    }

    pixel_description desc;
    if (!read_pixel_description(dataset, desc, error)) {
        return false;
    }

    const auto ts = file.transfer_syntax();
    if (ts.is_encapsulated()) {
        auto split = dicom_file::encapsulated_frames(pixel_data->raw_data(),
                                                     desc.params.number_of_frames);
        if (split.is_err()) {
            error = "Pixel Data is not encapsulated as " + std::string(ts.name()) +
                    " requires: " + split.error().message;
            return false;
        }

        if (!compression::codec_factory::is_available(ts)) {
            if (opts.verbose) {
                std::cout << "    Pixel Data not decoded: this build has no codec for "
                          << ts.name() << "\n";
            }
            return true;
        }

        auto codec = compression::codec_factory::create(ts);
        const size_t frame_bytes = desc.params.frame_size_bytes();
        for (size_t i = 0; i < split.value().size(); ++i) {
            auto decoded = codec->decode(split.value()[i], desc.params);
            if (decoded.is_err()) {
                error = "Frame " + std::to_string(i + 1) + " does not decode as " +
                        std::string(ts.name()) + ": " + decoded.error().message;
                return false;
            }
            if (decoded.value().data.size() != frame_bytes) {
                error = "Frame " + std::to_string(i + 1) + " decodes to " +
                        std::to_string(decoded.value().data.size()) +
                        " bytes, expected " + std::to_string(frame_bytes);
                return false;
            }
        }
        return true;
    }

    const uint64_t expected = native_pixel_data_length(desc);
    const uint64_t actual = pixel_data->raw_data().size();
    if (actual != expected && actual != expected + (expected % 2)) {
        error = "Pixel Data has " + std::to_string(actual) + " bytes, but " +
                std::string(ts.name()) + " with these Image Pixel attributes needs " +
                std::to_string(expected);
        return false;
    }
    return true;
}

/**
 * @brief Reopen an output file and verify it
 * @param output_path The written file
 * @param target_ts The Transfer Syntax it must have
 * @param opts Conversion options
 * @return true if the Transfer Syntax and the Pixel Data match
 */
bool verify_output(const std::filesystem::path& output_path,
                   const kcenon::pacs::encoding::transfer_syntax& target_ts,
                   const options& opts) {
    using namespace kcenon::pacs::core;

    auto verify_result = dicom_file::open(output_path);
    if (verify_result.is_err()) {
        std::cerr << "Error: Verification failed for '" << output_path.string()
                  << "': " << verify_result.error().message << "\n";
        return false;
    }

    auto& verified_file = verify_result.value();
    if (verified_file.transfer_syntax() != target_ts) {
        std::cerr << "Error: Verification failed - Transfer Syntax mismatch\n";
        return false;
    }

    std::string error;
    if (!verify_pixel_data(verified_file, opts, error)) {
        std::cerr << "Error: Verification failed for '" << output_path.string()
                  << "': " << error << "\n";
        return false;
    }

    if (opts.verbose) {
        std::cout << "    Verified: OK\n";
    }
    return true;
}

/**
 * @brief Convert a single DICOM file to the target transfer syntax
 * @param input_path Input file path
 * @param output_path Output file path
 * @param opts Conversion options
 * @return true on success
 */
bool convert_file(const std::filesystem::path& input_path,
                  const std::filesystem::path& output_path,
                  const options& opts) {
    using namespace kcenon::pacs::core;
    using namespace kcenon::pacs::encoding;

    // Check if output exists and overwrite is disabled
    if (std::filesystem::exists(output_path) && !opts.overwrite) {
        if (opts.verbose) {
            std::cout << "  Skipped (exists): " << output_path.filename().string() << "\n";
        }
        return true;  // Not an error, just skipped
    }

    // Open input file
    auto result = dicom_file::open(input_path);
    if (result.is_err()) {
        std::cerr << "Error: Failed to open '" << input_path.string()
                  << "': " << result.error().message << "\n";
        return false;
    }

    auto& input_file = result.value();
    auto source_ts = input_file.transfer_syntax();
    auto target_ts = transfer_syntax(opts.target_transfer_syntax);

    // Check if conversion is needed
    if (source_ts == target_ts) {
        if (opts.verbose) {
            std::cout << "  Skipped (same TS): " << input_path.filename().string() << "\n";
        }
        // Just copy the file
        std::filesystem::copy_file(input_path, output_path,
            std::filesystem::copy_options::overwrite_existing);
        return !opts.verify || verify_output(output_path, target_ts, opts);
    }

    // Check if target transfer syntax is supported
    if (!target_ts.is_supported()) {
        std::cerr << "Error: Target Transfer Syntax '" << target_ts.name()
                  << "' is not currently supported\n";
        return false;
    }

    // An encapsulated target also needs its codec in this build
    if (target_ts.is_encapsulated() &&
        !compression::codec_factory::is_available(target_ts)) {
        std::cerr << "Error: Target Transfer Syntax '" << target_ts.name()
                  << "' needs a codec that this build does not include\n";
        return false;
    }

    // dicom_file does not inflate a deflated dataset
    if (source_ts.is_deflated()) {
        std::cerr << "Error: Source Transfer Syntax '" << source_ts.name()
                  << "' is not supported\n";
        return false;
    }

    auto& dataset = input_file.dataset();

    // Float and Double Float Pixel Data are never encapsulated
    if (target_ts.is_encapsulated() &&
        (dataset.contains(kFloatPixelData) || dataset.contains(kDoubleFloatPixelData))) {
        std::cerr << "Error: '" << input_path.string()
                  << "' has Float Pixel Data, which cannot be stored as "
                  << target_ts.name() << "\n";
        return false;
    }

    // Pixel Data is decoded or encoded when either side is encapsulated
    const bool transcode = dataset.contains(tags::pixel_data) &&
                           (source_ts.is_encapsulated() || target_ts.is_encapsulated());
    if (transcode && source_ts.is_encapsulated() &&
        !compression::codec_factory::is_available(source_ts)) {
        std::cerr << "Error: Source Transfer Syntax '" << source_ts.name()
                  << "' needs a codec that this build does not include\n";
        return false;
    }

    if (opts.verbose) {
        std::cout << "  Converting: " << input_path.filename().string() << "\n";
        std::cout << "    From: " << source_ts.name() << "\n";
        std::cout << "    To:   " << target_ts.name() << "\n";
    }

    if (transcode) {
        std::string error;
        if (!transcode_pixel_data(dataset, source_ts, target_ts, opts, error)) {
            std::cerr << "Error: Cannot convert the Pixel Data of '"
                      << input_path.string() << "' to " << target_ts.name()
                      << ": " << error << "\n";
            return false;
        }
    }

    // Create output file with new transfer syntax
    auto output_file = dicom_file::create(std::move(dataset), target_ts);

    // Ensure output directory exists
    auto output_dir = output_path.parent_path();
    if (!output_dir.empty() && !std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }

    // Save output file
    auto save_result = output_file.save(output_path);
    if (save_result.is_err()) {
        std::cerr << "Error: Failed to save '" << output_path.string()
                  << "': " << save_result.error().message << "\n";
        return false;
    }

    return !opts.verify || verify_output(output_path, target_ts, opts);
}

/**
 * @brief Process a directory of DICOM files
 * @param input_dir Input directory
 * @param output_dir Output directory
 * @param opts Conversion options
 * @param stats Output statistics
 */
void process_directory(const std::filesystem::path& input_dir,
                       const std::filesystem::path& output_dir,
                       const options& opts,
                       conversion_stats& stats) {
    auto process_file = [&](const std::filesystem::path& file_path) {
        // Check file extension
        auto ext = file_path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".dcm" && ext != ".dicom" && !ext.empty()) {
            return;  // Skip non-DICOM files
        }

        ++stats.total_files;

        // Calculate relative path and construct output path
        auto relative_path = std::filesystem::relative(file_path, input_dir);
        auto output_path = output_dir / relative_path;

        auto start = std::chrono::steady_clock::now();

        if (convert_file(file_path, output_path, opts)) {
            ++stats.success_count;
        } else {
            ++stats.error_count;
        }

        auto end = std::chrono::steady_clock::now();
        stats.total_time += std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (opts.show_progress && !opts.quiet) {
            std::cout << "\rProcessed: " << stats.total_files
                      << " (Success: " << stats.success_count
                      << ", Errors: " << stats.error_count << ")" << std::flush;
        }
    };

    if (opts.recursive) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(input_dir)) {
            if (entry.is_regular_file()) {
                process_file(entry.path());
            }
        }
    } else {
        for (const auto& entry : std::filesystem::directory_iterator(input_dir)) {
            if (entry.is_regular_file()) {
                process_file(entry.path());
            }
        }
    }

    if (opts.show_progress && !opts.quiet) {
        std::cout << "\n";  // New line after progress
    }
}

/**
 * @brief Print conversion summary
 * @param stats The conversion statistics
 */
void print_summary(const conversion_stats& stats) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "        Conversion Summary\n";
    std::cout << "========================================\n";
    std::cout << "  Total files:   " << stats.total_files << "\n";
    std::cout << "  Successful:    " << stats.success_count << "\n";
    std::cout << "  Skipped:       " << stats.skip_count << "\n";
    std::cout << "  Errors:        " << stats.error_count << "\n";
    std::cout << "  Total time:    " << stats.total_time.count() << " ms\n";
    if (stats.total_files > 0) {
        auto avg_time = stats.total_time.count() / static_cast<double>(stats.total_files);
        std::cout << "  Avg per file:  " << std::fixed << std::setprecision(1)
                  << avg_time << " ms\n";
    }
    std::cout << "========================================\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    options opts;

    if (!parse_arguments(argc, argv, opts)) {
        std::cout << R"(
  ____   ____ __  __    ____ ___  _   ___     __
 |  _ \ / ___|  \/  |  / ___/ _ \| \ | \ \   / /
 | | | | |   | |\/| | | |  | | | |  \| |\ \ / /
 | |_| | |___| |  | | | |__| |_| | |\  | \ V /
 |____/ \____|_|  |_|  \____\___/|_| \_|  \_/

       DICOM Transfer Syntax Converter
)" << "\n";
        print_usage(argv[0]);
        return 1;
    }

    if (opts.list_syntaxes) {
        list_supported_syntaxes();
        return 0;
    }

    // Show banner in verbose mode
    if (!opts.quiet) {
        std::cout << R"(
  ____   ____ __  __    ____ ___  _   ___     __
 |  _ \ / ___|  \/  |  / ___/ _ \| \ | \ \   / /
 | | | | |   | |\/| | | |  | | | |  \| |\ \ / /
 | |_| | |___| |  | | | |__| |_| | |\  | \ V /
 |____/ \____|_|  |_|  \____\___/|_| \_|  \_/

       DICOM Transfer Syntax Converter
)" << "\n";
    }

    // Check input exists
    if (!std::filesystem::exists(opts.input_path)) {
        std::cerr << "Error: Input path does not exist: " << opts.input_path.string() << "\n";
        return 2;
    }

    auto target_ts = kcenon::pacs::encoding::transfer_syntax(opts.target_transfer_syntax);
    if (!opts.quiet) {
        std::cout << "Target Transfer Syntax: " << target_ts.name() << "\n";
        std::cout << "                   UID: " << target_ts.uid() << "\n\n";
    }

    conversion_stats stats;
    auto start_time = std::chrono::steady_clock::now();

    if (std::filesystem::is_directory(opts.input_path)) {
        // Process directory
        if (!std::filesystem::exists(opts.output_path)) {
            std::filesystem::create_directories(opts.output_path);
        }

        if (!opts.quiet) {
            std::cout << "Processing directory: " << opts.input_path.string() << "\n";
            if (opts.recursive) {
                std::cout << "Mode: Recursive\n\n";
            }
        }

        process_directory(opts.input_path, opts.output_path, opts, stats);
    } else {
        // Process single file
        ++stats.total_files;

        if (convert_file(opts.input_path, opts.output_path, opts)) {
            ++stats.success_count;
        } else {
            ++stats.error_count;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    stats.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Print summary for directory processing
    if (std::filesystem::is_directory(opts.input_path) && !opts.quiet) {
        print_summary(stats);
    } else if (!opts.quiet) {
        if (stats.success_count > 0) {
            std::cout << "Conversion completed successfully.\n";
        }
    }

    return stats.error_count > 0 ? 2 : 0;
}
