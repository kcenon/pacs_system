#!/bin/bash
# scripts/check-stats-drift.sh
#
# Check if documented statistics in README.md drift from actual values.
# Fails if any statistic is off by more than the allowed threshold.
#
# Usage:
#   ./scripts/check-stats-drift.sh              # Check with 10% threshold
#   ./scripts/check-stats-drift.sh --threshold 15  # Custom threshold (%)

set -euo pipefail

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default threshold (10%)
THRESHOLD=10

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --threshold)
            THRESHOLD="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Extract documented value from README.md
# Args: metric_pattern
extract_documented_value() {
    local pattern="$1"
    local value

    # Use grep -E (extended regex) for cross-platform compatibility
    value=$(grep -E "${pattern}" "${PROJECT_ROOT}/README.md" 2>/dev/null | \
            grep -oE '[~]?[0-9,]+' | tr -d '~,' | head -1)
    echo "${value:-0}"
}

# Calculate percentage difference
# Args: documented_value actual_value
calculate_drift() {
    local documented="$1"
    local actual="$2"

    if [[ "${documented}" -eq 0 ]]; then
        echo "100"
        return
    fi

    local diff
    diff=$(( actual - documented ))
    if [[ ${diff} -lt 0 ]]; then
        diff=$(( -diff ))
    fi

    local percent
    percent=$(( diff * 100 / documented ))
    echo "${percent}"
}

# Main check function
check_drift() {
    cd "${PROJECT_ROOT}"

    echo "Checking documentation statistics drift..."
    echo "Threshold: ${THRESHOLD}%"
    echo ""

    local has_error=0

    # Get actual statistics (use temp file for compatibility)
    local stats_tmp
    stats_tmp=$(mktemp)
    "${SCRIPT_DIR}/calculate-stats.sh" --raw > "${stats_tmp}"
    source "${stats_tmp}"
    rm -f "${stats_tmp}"

    # Define metrics to check: "display_name|readme_pattern|actual_variable"
    local metrics=(
        "Header Files|\*\*Header Files\*\*|${HEADER_FILES}"
        "Source Files|\*\*Source Files\*\*|${SOURCE_FILES}"
        "Header LOC|\*\*Header LOC\*\*|${HEADER_LOC}"
        "Source LOC|\*\*Source LOC\*\*|${SOURCE_LOC}"
        "Example LOC|\*\*Example LOC\*\*|${EXAMPLE_LOC}"
        "Test LOC|\*\*Test LOC\*\*|${TEST_LOC}"
        "Total LOC|\*\*Total LOC\*\*|${TOTAL_LOC}"
        "Test Files|\*\*Test Files\*\*|${TEST_FILES}"
        "Test Cases|\*\*Test Cases\*\*|${TEST_CASES}"
        "Example Programs|\*\*Example Programs\*\*|${EXAMPLE_PROGRAMS}"
        "Documentation|\*\*Documentation\*\*|${DOCUMENTATION}"
        "CI/CD Workflows|\*\*CI/CD Workflows\*\*|${WORKFLOWS}"
    )

    printf "%-20s %-12s %-12s %-10s %s\n" "Metric" "Documented" "Actual" "Drift" "Status"
    printf "%s\n" "-------------------------------------------------------------------------------"

    for metric in "${metrics[@]}"; do
        IFS='|' read -r name pattern actual <<< "${metric}"

        local documented
        documented=$(extract_documented_value "${pattern}")

        local drift
        drift=$(calculate_drift "${documented}" "${actual}")

        local status
        if [[ ${drift} -gt ${THRESHOLD} ]]; then
            status="${RED}FAIL${NC}"
            has_error=1
        elif [[ ${drift} -gt $((THRESHOLD / 2)) ]]; then
            status="${YELLOW}WARN${NC}"
        else
            status="${GREEN}OK${NC}"
        fi

        printf "%-20s %-12s %-12s %-10s " "${name}" "${documented}" "${actual}" "${drift}%"
        echo -e "${status}"
    done

    echo ""

    if [[ ${has_error} -eq 1 ]]; then
        echo -e "${RED}FAILED: Some statistics exceed ${THRESHOLD}% drift threshold.${NC}"
        echo "Please update README.md or run: ./scripts/update-readme-stats.sh"
        exit 1
    else
        echo -e "${GREEN}PASSED: All statistics within ${THRESHOLD}% threshold.${NC}"
        exit 0
    fi
}

# Run check
check_drift
