#!/bin/bash
# scripts/calculate-stats.sh
#
# Calculate code statistics for the PACS system.
# Generates metrics for README.md automation.
#
# Usage:
#   ./scripts/calculate-stats.sh              # Output to stdout
#   ./scripts/calculate-stats.sh --json       # Output as JSON
#   ./scripts/calculate-stats.sh --markdown   # Output as Markdown table

set -euo pipefail

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Color codes for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Output format (default: markdown)
OUTPUT_FORMAT="${1:-markdown}"

# Calculate statistics
calculate_stats() {
    cd "${PROJECT_ROOT}"

    # File counts
    local header_files
    local source_files
    local test_files
    local example_dirs
    local doc_files
    local workflow_files

    header_files=$(find include -name '*.hpp' -type f 2>/dev/null | wc -l | tr -d ' ')
    source_files=$(find src -name '*.cpp' -type f 2>/dev/null | wc -l | tr -d ' ')
    test_files=$(find tests -name '*.cpp' -type f 2>/dev/null | wc -l | tr -d ' ')
    example_dirs=$(find examples -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l | tr -d ' ')
    doc_files=$(find docs -name '*.md' -type f 2>/dev/null | wc -l | tr -d ' ')
    workflow_files=$(find .github/workflows -name '*.yml' -type f 2>/dev/null | wc -l | tr -d ' ')

    # Line counts (approximate)
    local header_loc
    local source_loc
    local test_loc
    local example_loc
    local total_loc

    header_loc=$(find include -name '*.hpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    source_loc=$(find src -name '*.cpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    test_loc=$(find tests -name '*.cpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    example_loc=$(find examples -name '*.cpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    total_loc=$((header_loc + source_loc + test_loc + example_loc))

    # Format LOC numbers (round to nearest 100 for display)
    local header_loc_fmt
    local source_loc_fmt
    local test_loc_fmt
    local example_loc_fmt
    local total_loc_fmt

    header_loc_fmt=$(printf "~%'d" $(( (header_loc + 50) / 100 * 100 )))
    source_loc_fmt=$(printf "~%'d" $(( (source_loc + 50) / 100 * 100 )))
    test_loc_fmt=$(printf "~%'d" $(( (test_loc + 50) / 100 * 100 )))
    example_loc_fmt=$(printf "~%'d" $(( (example_loc + 50) / 100 * 100 )))
    total_loc_fmt=$(printf "~%'d" $(( (total_loc + 50) / 100 * 100 )))

    # Test case count (approximate from test files)
    local test_cases
    test_cases=$(grep -r 'TEST_CASE\|TEST_F\|TEST\s*(' tests/ 2>/dev/null | wc -l | tr -d ' ')

    # Version from CMakeLists.txt (from project() command)
    local version
    version=$(sed -n '/^project(/,/)/p' CMakeLists.txt | grep -oE 'VERSION\s+[0-9]+\.[0-9]+\.[0-9]+' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || echo "0.0.0")

    # Current date
    local last_updated
    last_updated=$(date -u +%Y-%m-%d)

    # Output based on format
    case "${OUTPUT_FORMAT}" in
        --json)
            cat <<EOF
{
  "header_files": ${header_files},
  "source_files": ${source_files},
  "header_loc": ${header_loc},
  "source_loc": ${source_loc},
  "example_loc": ${example_loc},
  "test_loc": ${test_loc},
  "total_loc": ${total_loc},
  "test_files": ${test_files},
  "test_cases": ${test_cases},
  "example_programs": ${example_dirs},
  "documentation": ${doc_files},
  "workflows": ${workflow_files},
  "version": "${version}",
  "last_updated": "${last_updated}"
}
EOF
            ;;
        --markdown|*)
            cat <<EOF
| Metric | Value |
|--------|-------|
| **Header Files** | ${header_files} files |
| **Source Files** | ${source_files} files |
| **Header LOC** | ${header_loc_fmt} lines |
| **Source LOC** | ${source_loc_fmt} lines |
| **Example LOC** | ${example_loc_fmt} lines |
| **Test LOC** | ${test_loc_fmt} lines |
| **Total LOC** | ${total_loc_fmt} lines |
| **Test Files** | ${test_files} files |
| **Test Cases** | ${test_cases}+ tests |
| **Example Programs** | ${example_dirs} apps |
| **Documentation** | ${doc_files} markdown files |
| **CI/CD Workflows** | ${workflow_files} workflows |
| **Version** | ${version} |
| **Last Updated** | ${last_updated} |
EOF
            ;;
    esac
}

# Export raw statistics for drift checking
export_raw_stats() {
    cd "${PROJECT_ROOT}"

    local header_files source_files test_files example_dirs doc_files workflow_files
    local header_loc source_loc test_loc example_loc total_loc test_cases

    header_files=$(find include -name '*.hpp' -type f 2>/dev/null | wc -l | tr -d ' ')
    source_files=$(find src -name '*.cpp' -type f 2>/dev/null | wc -l | tr -d ' ')
    test_files=$(find tests -name '*.cpp' -type f 2>/dev/null | wc -l | tr -d ' ')
    example_dirs=$(find examples -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l | tr -d ' ')
    doc_files=$(find docs -name '*.md' -type f 2>/dev/null | wc -l | tr -d ' ')
    workflow_files=$(find .github/workflows -name '*.yml' -type f 2>/dev/null | wc -l | tr -d ' ')
    header_loc=$(find include -name '*.hpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    source_loc=$(find src -name '*.cpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    test_loc=$(find tests -name '*.cpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    example_loc=$(find examples -name '*.cpp' -type f -exec cat {} + 2>/dev/null | wc -l | tr -d ' ')
    total_loc=$((header_loc + source_loc + test_loc + example_loc))
    test_cases=$(grep -r 'TEST_CASE\|TEST_F\|TEST\s*(' tests/ 2>/dev/null | wc -l | tr -d ' ')

    # Output as key=value pairs for easy parsing
    echo "HEADER_FILES=${header_files}"
    echo "SOURCE_FILES=${source_files}"
    echo "HEADER_LOC=${header_loc}"
    echo "SOURCE_LOC=${source_loc}"
    echo "EXAMPLE_LOC=${example_loc}"
    echo "TEST_LOC=${test_loc}"
    echo "TOTAL_LOC=${total_loc}"
    echo "TEST_FILES=${test_files}"
    echo "TEST_CASES=${test_cases}"
    echo "EXAMPLE_PROGRAMS=${example_dirs}"
    echo "DOCUMENTATION=${doc_files}"
    echo "WORKFLOWS=${workflow_files}"
}

# Main
if [[ "${OUTPUT_FORMAT}" == "--raw" ]]; then
    export_raw_stats
else
    calculate_stats
fi
