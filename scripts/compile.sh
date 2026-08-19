#!/usr/bin/env bash
# Compile InkAds display-device firmware with Arduino CLI.
# Copies the sketch into a temp folder named display-device because Arduino
# CLI requires the folder name to match display-device.ino.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

CORE='esp32:esp32@3.3.11'
INDEX_URL='https://espressif.github.io/arduino-esp32/package_esp32_index.json'

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

CATALOG_BIN="$REPO_ROOT/scripts/lib/targets-catalog.sh"
TARGET_ID=''
COMPILE_ALL=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --target)
      TARGET_ID="${2:-}"
      shift 2
      ;;
    --all)
      COMPILE_ALL=1
      shift
      ;;
    -h|--help)
      cat <<'EOF'
Usage: compile.sh [--target <id>] [--all]

Compiles catalog targets from targets.json. Default is --all.
EOF
      exit 0
      ;;
    *) die "Unknown argument: $1" ;;
  esac
done

command -v arduino-cli >/dev/null || die 'arduino-cli is not on PATH'
command -v jq >/dev/null || die 'jq is required to read targets.json'

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

check_size() {
  local log_file="$1"
  local bin_path="$2"
  local used max bin_size
  used="$(sed -nE 's/.*Sketch uses ([0-9]+) bytes.*Maximum is ([0-9]+) bytes.*/\1/p' "$log_file" | tail -n 1)"
  max="$(sed -nE 's/.*Sketch uses ([0-9]+) bytes.*Maximum is ([0-9]+) bytes.*/\2/p' "$log_file" | tail -n 1)"
  [[ -n "$used" && -n "$max" ]] || die "arduino-cli did not report sketch size for $bin_path"
  if [[ "$used" -gt "$max" ]]; then
    die "app image exceeds partition: $used > $max"
  fi
  if [[ -f "$bin_path" ]]; then
    bin_size="$(wc -c <"$bin_path" | tr -d ' ')"
    echo "App binary $bin_path is $bin_size bytes (sketch $used / $max)"
    if [[ "$bin_size" -gt "$max" ]]; then
      die "app .bin exceeds partition: $bin_size > $max"
    fi
  fi
}

compile_one() {
  local id="$1"
  local resolved fqbn stem sketch log_file
  resolved="$(bash "$CATALOG_BIN" resolve "$id")"
  fqbn="$(printf '%s' "$resolved" | jq -er '.fqbn')"
  stem="$(printf '%s' "$resolved" | jq -er '.artifactStem')"

  TMP="$(mktemp -d)"
  cleanup_tmp() { rm -rf "$TMP"; }
  trap cleanup_tmp EXIT

  sketch="$TMP/display-device"
  copy_sketch "$sketch"
  bash "$CATALOG_BIN" write-features "$id" "$sketch/src/config/InkAdsFeatures.h"

  mkdir -p "$DIST"
  log_file="$TMP/compile.log"
  echo "Compiling $id with $fqbn"
  if ! arduino-cli compile \
    --fqbn "$fqbn" \
    --output-dir "$DIST" \
    "$sketch" 2>&1 | tee "$log_file"; then
    die "compile failed for $id"
  fi

  [[ -f "$DIST/display-device.ino.bin" ]] || die "missing app image for $id"
  [[ -f "$DIST/display-device.ino.merged.bin" ]] || die "missing factory image for $id"
  check_size "$log_file" "$DIST/display-device.ino.bin"

  mv "$DIST/display-device.ino.bin" "$DIST/${stem}.bin"
  mv "$DIST/display-device.ino.merged.bin" "$DIST/${stem}-factory.bin"
  echo "Wrote $DIST/${stem}.bin and $DIST/${stem}-factory.bin"

  trap - EXIT
  cleanup_tmp
}

ensure_core

DIST="$REPO_ROOT/dist"
mkdir -p "$DIST"

bash "$CATALOG_BIN" validate

if [[ -n "$TARGET_ID" && "$COMPILE_ALL" -eq 1 ]]; then
  die 'use either --target or --all, not both'
fi

if [[ -n "$TARGET_ID" ]]; then
  compile_one "$TARGET_ID"
  exit 0
fi

while IFS= read -r id; do
  compile_one "$id"
done < <(bash "$CATALOG_BIN" list)
