# Fuzz targets for DICOM parsing components
#
# Build with: cmake -B build -DPACS_BUILD_FUZZ_TARGETS=ON \
#             -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
#             -DCMAKE_BUILD_TYPE=Debug
#
# Each target links against pacs libraries and uses libFuzzer via
# -fsanitize=fuzzer,address for both compilation and linking.

set(FUZZ_COMPILE_FLAGS -fsanitize=fuzzer,address -fno-omit-frame-pointer -g)
set(FUZZ_LINK_FLAGS -fsanitize=fuzzer,address)

# --- fuzz_dicom_file_parser ---
# Exercises dicom_file::from_bytes() with arbitrary byte streams.
# Covers: Part 10 preamble, DICM prefix, File Meta Information, dataset
# decoding across all transfer syntaxes, and sequence/item parsing.
add_executable(fuzz_dicom_file_parser fuzz_dicom_file_parser.cpp)
target_link_libraries(fuzz_dicom_file_parser PRIVATE pacs_core pacs_encoding)
target_compile_options(fuzz_dicom_file_parser PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_dicom_file_parser PRIVATE ${FUZZ_LINK_FLAGS})

# --- fuzz_implicit_vr_codec ---
# Exercises implicit_vr_codec::decode() with arbitrary byte streams.
# Covers: tag parsing, 32-bit length fields, dictionary VR lookup,
# undefined-length items, and nested sequences.
add_executable(fuzz_implicit_vr_codec fuzz_implicit_vr_codec.cpp)
target_link_libraries(fuzz_implicit_vr_codec PRIVATE pacs_core pacs_encoding)
target_compile_options(fuzz_implicit_vr_codec PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_implicit_vr_codec PRIVATE ${FUZZ_LINK_FLAGS})

# --- fuzz_explicit_vr_codec ---
# Exercises explicit_vr_codec::decode() with arbitrary byte streams.
# Covers: VR field parsing, 16-bit / 32-bit length variants, extended
# VR types, and malformed VR strings.
add_executable(fuzz_explicit_vr_codec fuzz_explicit_vr_codec.cpp)
target_link_libraries(fuzz_explicit_vr_codec PRIVATE pacs_core pacs_encoding)
target_compile_options(fuzz_explicit_vr_codec PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_explicit_vr_codec PRIVATE ${FUZZ_LINK_FLAGS})

# --- fuzz_pdu_decoder ---
# Exercises pdu_decoder::decode() with arbitrary byte streams.
# Covers: PDU type dispatch, A-ASSOCIATE-RQ/AC/RJ, P-DATA-TF,
# A-RELEASE, A-ABORT, variable items, and presentation context parsing.
add_executable(fuzz_pdu_decoder fuzz_pdu_decoder.cpp)
target_link_libraries(fuzz_pdu_decoder PRIVATE pacs_core pacs_encoding pacs_network)
target_compile_options(fuzz_pdu_decoder PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_pdu_decoder PRIVATE ${FUZZ_LINK_FLAGS})

# --- fuzz_rle_codec ---
# Exercises RLE codec decompression with arbitrary byte streams.
# Covers: RLE header parsing, segment offsets, run-length decoding,
# segment count validation, and buffer boundary handling.
add_executable(fuzz_rle_codec fuzz_rle_codec.cpp)
target_link_libraries(fuzz_rle_codec PRIVATE pacs_core pacs_encoding)
target_compile_options(fuzz_rle_codec PRIVATE ${FUZZ_COMPILE_FLAGS})
target_link_options(fuzz_rle_codec PRIVATE ${FUZZ_LINK_FLAGS})
