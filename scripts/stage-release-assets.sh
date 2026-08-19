#!/usr/bin/env bash
# Write the multi-target OTA manifest after compile.sh has named versioned bins.
# Leaves other dist/*.bin files (bootloader, partitions, extras) in place.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

[[ -n "${RELEASE_TAG:-}" ]] || die 'RELEASE_TAG is required'
[[ -n "${GITHUB_REPOSITORY:-}" ]] || die 'GITHUB_REPOSITORY is required'

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$REPO_ROOT/dist"
cd "$REPO_ROOT"

bash "$REPO_ROOT/scripts/generate-release-manifest.sh" \
  "$DIST" \
  "$DIST/inkads-manifest.json"
