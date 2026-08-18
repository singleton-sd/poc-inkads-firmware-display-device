#!/usr/bin/env bash
# Compile InkAds display-device firmware with Arduino CLI.
# Copies the sketch into a temp folder named display-device because Arduino
# CLI requires the folder name to match display-device.ino.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

FQBN='esp32:esp32:mhetesp32minikit'
CORE='esp32:esp32@3.3.11'
INDEX_URL='https://espressif.github.io/arduino-esp32/package_esp32_index.json'

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

command -v arduino-cli >/dev/null || die 'arduino-cli is not on PATH'

ensure_core() {
  if arduino-cli core list 2>/dev/null | grep -F 'esp32:esp32' | grep -q '3.3.11'; then
    return 0
  fi

  if ! arduino-cli config dump >/dev/null 2>&1; then
    arduino-cli config init
  fi
  arduino-cli config add board_manager.additional_urls "$INDEX_URL" || true
  arduino-cli core update-index
  arduino-cli core install "$CORE"
}

copy_sketch() {
  local dest="$1"
  mkdir -p "$dest"
  if command -v rsync >/dev/null; then
    rsync -a --exclude '.git' --exclude dist --exclude build ./ "$dest/"
    return
  fi
  tar -C "$REPO_ROOT" \
    --exclude .git \
    --exclude dist \
    --exclude build \
    -cf - . | tar -C "$dest" -xf -
}

ensure_core

DIST="$REPO_ROOT/dist"
mkdir -p "$DIST"

TMP="$(mktemp -d)"
cleanup() { rm -rf "$TMP"; }
trap cleanup EXIT

SKETCH="$TMP/display-device"
copy_sketch "$SKETCH"

echo "Compiling $SKETCH with $FQBN"
arduino-cli compile \
  --fqbn "$FQBN" \
  --output-dir "$DIST" \
  "$SKETCH"

echo "Firmware binaries:"
ls -l "$DIST"/display-device.ino.bin "$DIST"/display-device.ino.merged.bin
