#!/usr/bin/env bash
# Rename Arduino CLI outputs to stable OTA names and write the OTA manifest.
# Leaves other dist/*.bin files (bootloader, partitions, future extras) in place.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

[[ -n "${RELEASE_TAG:-}" ]] || die 'RELEASE_TAG is required'
[[ -n "${GITHUB_REPOSITORY:-}" ]] || die 'GITHUB_REPOSITORY is required'

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$REPO_ROOT/dist"
cd "$REPO_ROOT"

[[ -f "$DIST/display-device.ino.bin" ]] \
  || die "missing $DIST/display-device.ino.bin"
[[ -f "$DIST/display-device.ino.merged.bin" ]] \
  || die "missing $DIST/display-device.ino.merged.bin"

mv "$DIST/display-device.ino.bin" "$DIST/inkads-esp32.bin"
mv "$DIST/display-device.ino.merged.bin" "$DIST/inkads-esp32-factory.bin"

node scripts/generate-ota-manifest.mjs \
  "$DIST/inkads-esp32.bin" \
  "$DIST/inkads-manifest.json"
