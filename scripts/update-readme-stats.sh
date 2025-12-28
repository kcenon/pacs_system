#!/bin/bash
# scripts/update-readme-stats.sh
#
# Update the Code Statistics section in README.md with current values.
# Uses template markers to identify the section to update.
#
# Usage:
#   ./scripts/update-readme-stats.sh              # Update in place
#   ./scripts/update-readme-stats.sh --dry-run    # Preview changes

set -euo pipefail

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Options
DRY_RUN=0

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

README_FILE="${PROJECT_ROOT}/README.md"
TEMP_FILE="${PROJECT_ROOT}/README.md.tmp"
STATS_FILE="${PROJECT_ROOT}/README.md.stats"

# Check for template markers
if ! grep -q '<!-- STATS_START -->' "${README_FILE}"; then
    echo "Error: README.md does not contain <!-- STATS_START --> marker."
    echo "Please add template markers around the Code Statistics section."
    exit 1
fi

if ! grep -q '<!-- STATS_END -->' "${README_FILE}"; then
    echo "Error: README.md does not contain <!-- STATS_END --> marker."
    exit 1
fi

# Generate new statistics
echo "Calculating current statistics..."
"${SCRIPT_DIR}/calculate-stats.sh" --markdown > "${STATS_FILE}"

# Create updated README using sed
echo "Updating README.md..."

# Use sed to replace content between markers
{
    # Print everything up to and including STATS_START
    sed -n '1,/<!-- STATS_START -->/p' "${README_FILE}"
    # Print empty line and new stats
    echo ""
    cat "${STATS_FILE}"
    echo ""
    # Print everything from STATS_END to end
    sed -n '/<!-- STATS_END -->/,$p' "${README_FILE}"
} > "${TEMP_FILE}"

# Clean up stats file
rm -f "${STATS_FILE}"

if [[ ${DRY_RUN} -eq 1 ]]; then
    echo ""
    echo "=== Dry run - changes that would be made ==="
    echo ""
    diff -u "${README_FILE}" "${TEMP_FILE}" || true
    rm -f "${TEMP_FILE}"
    echo ""
    echo "Dry run complete. No changes made."
else
    mv "${TEMP_FILE}" "${README_FILE}"
    echo "README.md updated successfully."
fi
