#!/bin/bash
#
# test_cli_tools.sh - Functional tests for file utility CLI tools
#
# Tests the 13 file utility CLI tools with real DICOM files,
# verifying correct output, file generation, tag modification,
# and error handling.
#
# Usage:
#   ./test_cli_tools.sh [build_dir]
#
# Exit codes:
#   0 - All tests passed
#   1 - One or more tests failed
#
# @see Issue #758 - Add shell script functional tests for file utility CLIs
# @see Issue #756 - Automated CLI Tool Testing (Epic)

set -o pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

PROJECT_ROOT="$(find_project_root "${SCRIPT_DIR}")"
BUILD_DIR="${1:-${PROJECT_ROOT}/build}"
BINARY_DIR="${BUILD_DIR}/bin"
TEST_DATA="${SCRIPT_DIR}/../test_data"

# Test tracking
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Working directory for temporary files
WORK_DIR=$(mktemp -d)
trap "rm -rf ${WORK_DIR}" EXIT

# Run a test case and track results
run_test() {
    local name="$1"
    shift
    local expect_fail=0

    # Check for --expect-fail flag
    if [[ "${*: -1}" == "--expect-fail" ]]; then
        expect_fail=1
        set -- "${@:1:$#-1}"
    fi

    TESTS_RUN=$((TESTS_RUN + 1))

    local output
    local exit_code
    output=$(eval "$@" 2>&1)
    exit_code=$?

    if [[ ${expect_fail} -eq 1 ]]; then
        if [[ ${exit_code} -ne 0 ]]; then
            TESTS_PASSED=$((TESTS_PASSED + 1))
            log_pass "${name}"
            return 0
        else
            TESTS_FAILED=$((TESTS_FAILED + 1))
            log_fail "${name} (expected failure but got exit code 0)"
            return 1
        fi
    else
        if [[ ${exit_code} -eq 0 ]]; then
            TESTS_PASSED=$((TESTS_PASSED + 1))
            log_pass "${name}"
            return 0
        else
            TESTS_FAILED=$((TESTS_FAILED + 1))
            log_fail "${name} (exit code: ${exit_code})"
            echo "  Output: $(echo "${output}" | tail -3)"
            return 1
        fi
    fi
}

# Assert output contains a pattern
assert_contains() {
    local description="$1"
    local pattern="$2"
    local input="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if echo "${input}" | grep -q "${pattern}"; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_pass "${description}"
        return 0
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_fail "${description} (pattern '${pattern}' not found)"
        return 1
    fi
}

# Assert file exists
assert_file_exists() {
    local description="$1"
    local filepath="$2"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [[ -f "${filepath}" ]]; then
        TESTS_PASSED=$((TESTS_PASSED + 1))
        log_pass "${description}"
        return 0
    else
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_fail "${description} (file not found: ${filepath})"
        return 1
    fi
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."

    if [[ ! -d "${BINARY_DIR}" ]]; then
        log_fail "Binary directory not found: ${BINARY_DIR}"
        return 1
    fi

    local tools=(dcm_dump dcm_info dcm_conv dcm_modify dcm_anonymize
                 dcm_dir dcm_to_json json_to_dcm dcm_to_xml xml_to_dcm
                 dcm_extract)
    local missing=0

    for tool in "${tools[@]}"; do
        if ! check_binary "${BINARY_DIR}/${tool}" "${tool}"; then
            missing=1
        fi
    done

    if [[ ${missing} -eq 1 ]]; then
        log_fail "Missing required binaries. Build with -DPACS_BUILD_EXAMPLES=ON"
        return 1
    fi

    if [[ ! -f "${TEST_DATA}/ct_minimal.dcm" ]]; then
        log_fail "Test data not found: ${TEST_DATA}/ct_minimal.dcm"
        return 1
    fi

    log_pass "All prerequisites satisfied"
    return 0
}

##############################################
# Test suites for each CLI tool
##############################################

test_dcm_dump() {
    log_info "--- dcm_dump ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"

    # Basic dump
    local output
    output=$("${BINARY_DIR}/dcm_dump" "${DCM}" 2>&1)
    run_test "dcm_dump: basic dump exits 0" \
        "\"${BINARY_DIR}/dcm_dump\" \"${DCM}\""
    assert_contains "dcm_dump: output contains tag data" \
        "PatientName\|PatientID\|0010,0010\|0010,0020" "${output}"

    # JSON format
    output=$("${BINARY_DIR}/dcm_dump" "${DCM}" --format json 2>&1)
    assert_contains "dcm_dump: JSON output contains 'vr'" \
        '"vr"' "${output}"

    # Tags filter
    output=$("${BINARY_DIR}/dcm_dump" "${DCM}" --tags PatientName 2>&1)
    assert_contains "dcm_dump: --tags filter shows PatientName" \
        "PatientName\|0010,0010" "${output}"

    # Nonexistent file
    run_test "dcm_dump: nonexistent file returns error" \
        "\"${BINARY_DIR}/dcm_dump\" /nonexistent_file.dcm" --expect-fail
}

test_dcm_info() {
    log_info "--- dcm_info ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"

    # Basic info
    local output
    output=$("${BINARY_DIR}/dcm_info" "${DCM}" 2>&1)
    run_test "dcm_info: basic info exits 0" \
        "\"${BINARY_DIR}/dcm_info\" \"${DCM}\""
    assert_contains "dcm_info: shows patient info" \
        "TEST.PATIENT\|TEST001\|Patient" "${output}"

    # JSON format
    output=$("${BINARY_DIR}/dcm_info" "${DCM}" --format json 2>&1)
    assert_contains "dcm_info: JSON output is valid" \
        '"patient"\|"study"\|"file"' "${output}"

    # Nonexistent file
    run_test "dcm_info: nonexistent file returns error" \
        "\"${BINARY_DIR}/dcm_info\" /nonexistent_file.dcm" --expect-fail
}

test_dcm_conv() {
    log_info "--- dcm_conv ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"
    local OUTPUT="${WORK_DIR}/conv_output.dcm"

    # Convert to Explicit VR Little Endian
    run_test "dcm_conv: convert to explicit VR" \
        "\"${BINARY_DIR}/dcm_conv\" \"${DCM}\" \"${OUTPUT}\" --explicit"
    assert_file_exists "dcm_conv: output file created" "${OUTPUT}"

    # Verify converted file with dcm_dump
    if [[ -f "${OUTPUT}" ]]; then
        local output
        output=$("${BINARY_DIR}/dcm_dump" "${OUTPUT}" 2>&1)
        assert_contains "dcm_conv: converted file is valid DICOM" \
            "PatientName\|0010,0010" "${output}"
    fi

    # Convert to Implicit VR
    local OUTPUT2="${WORK_DIR}/conv_implicit.dcm"
    run_test "dcm_conv: convert to implicit VR" \
        "\"${BINARY_DIR}/dcm_conv\" \"${DCM}\" \"${OUTPUT2}\" --implicit"
    assert_file_exists "dcm_conv: implicit output file created" "${OUTPUT2}"
}

test_dcm_modify() {
    log_info "--- dcm_modify ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"

    # Dry-run insert
    local output
    output=$("${BINARY_DIR}/dcm_modify" --dry-run -i "(0010,0010)=MODIFIED_NAME" "${DCM}" 2>&1)
    assert_contains "dcm_modify: dry-run shows action" \
        "Would modify\|modify\|dry" "${output}"

    # Insert tag with output file
    local OUTPUT="${WORK_DIR}/modified.dcm"
    run_test "dcm_modify: insert tag to output file" \
        "\"${BINARY_DIR}/dcm_modify\" -i \"(0010,0010)=MODIFIED_NAME\" -o \"${OUTPUT}\" \"${DCM}\""
    assert_file_exists "dcm_modify: output file created" "${OUTPUT}"

    # Verify modification
    if [[ -f "${OUTPUT}" ]]; then
        output=$("${BINARY_DIR}/dcm_dump" "${OUTPUT}" 2>&1)
        assert_contains "dcm_modify: tag was modified" \
            "MODIFIED_NAME" "${output}"
    fi

    # Nonexistent file
    run_test "dcm_modify: nonexistent file returns error" \
        "\"${BINARY_DIR}/dcm_modify\" -i \"(0010,0010)=TEST\" /nonexistent.dcm" --expect-fail
}

test_dcm_anonymize() {
    log_info "--- dcm_anonymize ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"

    # Dry-run
    local output
    output=$("${BINARY_DIR}/dcm_anonymize" --dry-run "${DCM}" 2>&1)
    run_test "dcm_anonymize: dry-run exits 0" \
        "\"${BINARY_DIR}/dcm_anonymize\" --dry-run \"${DCM}\""
    assert_contains "dcm_anonymize: dry-run shows action" \
        "Would anonymize\|anonymize\|dry" "${output}"

    # Anonymize with output
    local OUTPUT="${WORK_DIR}/anon_output.dcm"
    run_test "dcm_anonymize: anonymize to output file" \
        "\"${BINARY_DIR}/dcm_anonymize\" \"${DCM}\" \"${OUTPUT}\""
    assert_file_exists "dcm_anonymize: output file created" "${OUTPUT}"

    # Verify anonymized file doesn't contain original patient name
    if [[ -f "${OUTPUT}" ]]; then
        output=$("${BINARY_DIR}/dcm_dump" "${OUTPUT}" 2>&1)
        TESTS_RUN=$((TESTS_RUN + 1))
        if ! echo "${output}" | grep -q "TEST.PATIENT"; then
            TESTS_PASSED=$((TESTS_PASSED + 1))
            log_pass "dcm_anonymize: original patient name removed"
        else
            TESTS_FAILED=$((TESTS_FAILED + 1))
            log_fail "dcm_anonymize: original patient name still present"
        fi
    fi
}

test_dcm_dir() {
    log_info "--- dcm_dir ---"

    # Set up directory with DICOM files
    local DIR_INPUT="${WORK_DIR}/dicomdir_input"
    local DICOMDIR_FILE="${WORK_DIR}/DICOMDIR"
    mkdir -p "${DIR_INPUT}"
    cp "${TEST_DATA}/ct_minimal.dcm" "${DIR_INPUT}/"
    cp "${TEST_DATA}/mr_minimal.dcm" "${DIR_INPUT}/"

    # Create DICOMDIR
    run_test "dcm_dir: create DICOMDIR" \
        "\"${BINARY_DIR}/dcm_dir\" create \"${DIR_INPUT}\" -o \"${DICOMDIR_FILE}\""
    assert_file_exists "dcm_dir: DICOMDIR file created" "${DICOMDIR_FILE}"

    # List DICOMDIR
    if [[ -f "${DICOMDIR_FILE}" ]]; then
        local output
        output=$("${BINARY_DIR}/dcm_dir" list "${DICOMDIR_FILE}" 2>&1)
        run_test "dcm_dir: list DICOMDIR" \
            "\"${BINARY_DIR}/dcm_dir\" list \"${DICOMDIR_FILE}\""
        assert_contains "dcm_dir: list shows patient/study data" \
            "Patient\|PATIENT\|Study\|STUDY\|TEST" "${output}"
    fi

    # Invalid command
    run_test "dcm_dir: invalid command returns error" \
        "\"${BINARY_DIR}/dcm_dir\" invalid_command" --expect-fail
}

test_dcm_to_json() {
    log_info "--- dcm_to_json ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"
    local OUTPUT="${WORK_DIR}/output.json"

    # Convert to JSON (stdout)
    local output
    output=$("${BINARY_DIR}/dcm_to_json" "${DCM}" 2>&1)
    run_test "dcm_to_json: convert exits 0" \
        "\"${BINARY_DIR}/dcm_to_json\" \"${DCM}\""
    assert_contains "dcm_to_json: output contains DICOM tag" \
        "00100010\|00080060" "${output}"
    assert_contains "dcm_to_json: output has vr field" \
        '"vr"' "${output}"

    # Convert to JSON file
    run_test "dcm_to_json: convert to file" \
        "\"${BINARY_DIR}/dcm_to_json\" \"${DCM}\" \"${OUTPUT}\""
    assert_file_exists "dcm_to_json: output file created" "${OUTPUT}"

    # Nonexistent file
    run_test "dcm_to_json: nonexistent file returns error" \
        "\"${BINARY_DIR}/dcm_to_json\" /nonexistent.dcm" --expect-fail
}

test_json_to_dcm() {
    log_info "--- json_to_dcm ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"
    local JSON_FILE="${WORK_DIR}/roundtrip.json"
    local OUTPUT="${WORK_DIR}/from_json.dcm"

    # First create a JSON file from DICOM
    "${BINARY_DIR}/dcm_to_json" "${DCM}" "${JSON_FILE}" 2>/dev/null

    if [[ -f "${JSON_FILE}" ]]; then
        # Convert JSON back to DICOM
        run_test "json_to_dcm: convert JSON to DICOM" \
            "\"${BINARY_DIR}/json_to_dcm\" \"${JSON_FILE}\" \"${OUTPUT}\""
        assert_file_exists "json_to_dcm: output file created" "${OUTPUT}"

        # Verify the output is valid DICOM
        if [[ -f "${OUTPUT}" ]]; then
            local output
            output=$("${BINARY_DIR}/dcm_dump" "${OUTPUT}" 2>&1)
            assert_contains "json_to_dcm: output is valid DICOM" \
                "0010,0010\|PatientName" "${output}"
        fi
    else
        TESTS_RUN=$((TESTS_RUN + 1))
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_fail "json_to_dcm: prerequisite JSON file not created"
    fi

    # Invalid JSON
    echo "{ invalid json" > "${WORK_DIR}/bad.json"
    run_test "json_to_dcm: invalid JSON returns error" \
        "\"${BINARY_DIR}/json_to_dcm\" \"${WORK_DIR}/bad.json\" \"${WORK_DIR}/bad_out.dcm\"" --expect-fail
}

test_dcm_to_xml() {
    log_info "--- dcm_to_xml ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"
    local OUTPUT="${WORK_DIR}/output.xml"

    # Convert to XML (stdout)
    local output
    output=$("${BINARY_DIR}/dcm_to_xml" "${DCM}" 2>&1)
    run_test "dcm_to_xml: convert exits 0" \
        "\"${BINARY_DIR}/dcm_to_xml\" \"${DCM}\""
    assert_contains "dcm_to_xml: output is XML format" \
        "DicomAttribute\|NativeDicomModel\|xml" "${output}"

    # Convert to XML file
    run_test "dcm_to_xml: convert to file" \
        "\"${BINARY_DIR}/dcm_to_xml\" \"${DCM}\" \"${OUTPUT}\""
    assert_file_exists "dcm_to_xml: output file created" "${OUTPUT}"

    # Nonexistent file
    run_test "dcm_to_xml: nonexistent file returns error" \
        "\"${BINARY_DIR}/dcm_to_xml\" /nonexistent.dcm" --expect-fail
}

test_xml_to_dcm() {
    log_info "--- xml_to_dcm ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"
    local XML_FILE="${WORK_DIR}/roundtrip.xml"
    local OUTPUT="${WORK_DIR}/from_xml.dcm"

    # First create an XML file from DICOM
    "${BINARY_DIR}/dcm_to_xml" "${DCM}" "${XML_FILE}" 2>/dev/null

    if [[ -f "${XML_FILE}" ]]; then
        # Convert XML back to DICOM
        run_test "xml_to_dcm: convert XML to DICOM" \
            "\"${BINARY_DIR}/xml_to_dcm\" \"${XML_FILE}\" \"${OUTPUT}\""
        assert_file_exists "xml_to_dcm: output file created" "${OUTPUT}"

        # Verify the output is valid DICOM
        if [[ -f "${OUTPUT}" ]]; then
            local output
            output=$("${BINARY_DIR}/dcm_dump" "${OUTPUT}" 2>&1)
            assert_contains "xml_to_dcm: output is valid DICOM" \
                "0010,0010\|PatientName" "${output}"
        fi
    else
        TESTS_RUN=$((TESTS_RUN + 1))
        TESTS_FAILED=$((TESTS_FAILED + 1))
        log_fail "xml_to_dcm: prerequisite XML file not created"
    fi
}

test_dcm_extract() {
    log_info "--- dcm_extract ---"
    local DCM="${TEST_DATA}/ct_minimal.dcm"

    # Info mode
    local output
    output=$("${BINARY_DIR}/dcm_extract" --info "${DCM}" 2>&1)
    run_test "dcm_extract: --info exits 0" \
        "\"${BINARY_DIR}/dcm_extract\" --info \"${DCM}\""
    assert_contains "dcm_extract: shows pixel dimensions" \
        "64\|Dimensions\|Pixel\|pixel\|Rows\|Columns" "${output}"

    # Extract raw pixel data
    local OUTPUT="${WORK_DIR}/pixels.raw"
    run_test "dcm_extract: extract raw pixel data" \
        "\"${BINARY_DIR}/dcm_extract\" \"${DCM}\" \"${OUTPUT}\" --raw"
    assert_file_exists "dcm_extract: raw output created" "${OUTPUT}"
}

test_img_to_dcm() {
    log_info "--- img_to_dcm ---"

    # Skip unless a test JPEG fixture exists
    local JPEG_FILE="${TEST_DATA}/test_image.jpg"
    if [[ ! -f "${JPEG_FILE}" ]]; then
        log_warn "img_to_dcm: skipped (no test JPEG fixture at ${JPEG_FILE})"
        return 0
    fi

    local OUTPUT="${WORK_DIR}/from_image.dcm"
    run_test "img_to_dcm: convert JPEG to DICOM" \
        "\"${BINARY_DIR}/img_to_dcm\" \"${JPEG_FILE}\" \"${OUTPUT}\""
    assert_file_exists "img_to_dcm: output file created" "${OUTPUT}"
}

test_db_browser() {
    log_info "--- db_browser ---"

    # db_browser requires a database fixture; test --help only
    if [[ -x "${BINARY_DIR}/db_browser" ]]; then
        local output
        output=$("${BINARY_DIR}/db_browser" --help 2>&1)
        assert_contains "db_browser: --help shows usage" \
            "Usage\|usage\|USAGE" "${output}"
    else
        log_warn "db_browser: binary not found (requires pacs_storage)"
    fi
}

##############################################
# Main
##############################################

main() {
    echo ""
    echo "============================================"
    echo "  PACS CLI File Utility Functional Tests"
    echo "============================================"
    echo ""
    echo "Project root: ${PROJECT_ROOT}"
    echo "Build dir:    ${BUILD_DIR}"
    echo "Test data:    ${TEST_DATA}"
    echo "Work dir:     ${WORK_DIR}"
    echo ""

    if ! check_prerequisites; then
        exit 1
    fi

    echo ""

    # Run all test suites
    test_dcm_dump
    test_dcm_info
    test_dcm_conv
    test_dcm_modify
    test_dcm_anonymize
    test_dcm_dir
    test_dcm_to_json
    test_json_to_dcm
    test_dcm_to_xml
    test_xml_to_dcm
    test_dcm_extract
    test_img_to_dcm
    test_db_browser

    # Summary
    echo ""
    echo "============================================"
    echo "  Test Summary"
    echo "============================================"
    echo "  Tests run:    ${TESTS_RUN}"
    echo "  Passed:       ${TESTS_PASSED}"
    echo "  Failed:       ${TESTS_FAILED}"
    echo "============================================"
    echo ""

    if [[ ${TESTS_FAILED} -eq 0 ]]; then
        log_pass "All CLI functional tests passed!"
        exit 0
    else
        log_fail "${TESTS_FAILED} test(s) failed"
        exit 1
    fi
}

main "$@"
