# Documentation Statistics Automation

This document describes the automated documentation statistics system that keeps README.md metrics in sync with actual codebase measurements.

## Overview

The documentation statistics automation prevents documentation drift by:
- Calculating accurate code metrics automatically
- Checking for discrepancies between documented and actual values
- Providing tools to update statistics when needed
- Integrating with CI/CD to enforce accuracy

## Scripts

### calculate-stats.sh

Calculates current code statistics from the codebase.

```bash
# Output as Markdown table (default)
./scripts/calculate-stats.sh

# Output as JSON
./scripts/calculate-stats.sh --json

# Output raw key=value pairs
./scripts/calculate-stats.sh --raw
```

**Metrics Calculated:**
| Metric | Description |
|--------|-------------|
| Header Files | Count of `.hpp` files in `include/` |
| Source Files | Count of `.cpp` files in `src/` |
| Header LOC | Lines of code in header files |
| Source LOC | Lines of code in source files |
| Example LOC | Lines of code in example programs |
| Test LOC | Lines of code in test files |
| Total LOC | Sum of all LOC metrics |
| Test Files | Count of `.cpp` files in `tests/` |
| Test Cases | Count of TEST_CASE/TEST_F patterns |
| Example Programs | Count of example directories |
| Documentation | Count of `.md` files in `docs/` |
| CI/CD Workflows | Count of `.yml` files in `.github/workflows/` |
| Version | Project version from CMakeLists.txt |
| Last Updated | Current date |

### check-stats-drift.sh

Compares documented statistics in README.md with actual values.

```bash
# Check with default 10% threshold
./scripts/check-stats-drift.sh

# Check with custom threshold (15%)
./scripts/check-stats-drift.sh --threshold 15
```

**Exit Codes:**
- `0`: All statistics within threshold
- `1`: One or more statistics exceed threshold

### update-readme-stats.sh

Updates the Code Statistics section in README.md.

```bash
# Preview changes without modifying
./scripts/update-readme-stats.sh --dry-run

# Update README.md in place
./scripts/update-readme-stats.sh
```

**Requirements:**
- README.md must contain `<!-- STATS_START -->` and `<!-- STATS_END -->` markers
- Statistics table must be between these markers

## CI/CD Integration

### GitHub Action Workflow

The `docs-stats.yml` workflow runs on:
- Push to `main` or `develop` branches
- Pull requests targeting `main` or `develop`
- Manual trigger via workflow dispatch

**Triggers:**
Changes to any of these directories:
- `src/**`
- `include/**`
- `tests/**`
- `examples/**`
- `docs/**`
- `.github/workflows/**`

**Jobs:**

1. **calculate-stats**: Generates statistics report as artifact
2. **check-drift**: Fails if any metric exceeds 10% drift threshold
3. **auto-update**: (Optional) Auto-updates README.md on main branch

### PR Comments

When drift is detected in a pull request, the workflow automatically adds a comment with:
- List of metrics that exceed the threshold
- Command to fix the issue locally

## README.md Template Markers

The Code Statistics section in README.md uses template markers for automated updates:

```markdown
## Code Statistics

<!-- STATS_START -->

| Metric | Value |
|--------|-------|
| **Header Files** | 157 files |
...

<!-- STATS_END -->
```

**Important:** Do not modify the markers or content between them manually. Use `update-readme-stats.sh` instead.

## Local Development Workflow

### When Adding New Files

After adding significant new code (files, tests, examples):

```bash
# Check if statistics need updating
./scripts/check-stats-drift.sh

# If drift detected, update README.md
./scripts/update-readme-stats.sh
```

### Before Creating a PR

```bash
# Verify statistics are current
./scripts/check-stats-drift.sh --threshold 10

# Update if necessary
./scripts/update-readme-stats.sh

# Commit the changes
git add README.md
git commit -m "docs: update code statistics"
```

## Troubleshooting

### "STATS_START marker not found"

Add template markers around the Code Statistics section:

```markdown
## Code Statistics

<!-- STATS_START -->
(existing table content)
<!-- STATS_END -->
```

### Drift Threshold Too Sensitive

Increase the threshold for specific use cases:

```bash
./scripts/check-stats-drift.sh --threshold 20
```

### Version Shows 0.0.0

Ensure CMakeLists.txt has a `VERSION` in the `project()` command:

```cmake
project(pacs_system
    VERSION 0.1.0
    ...
)
```

## Related Issues

- #406 - Add CI/CD Pipeline for Automated Documentation Statistics
- #403 - Initial documentation statistics update
