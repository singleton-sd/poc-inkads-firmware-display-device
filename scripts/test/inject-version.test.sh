#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$TEST_DIR/../.." && pwd)"
INJECT="$ROOT/scripts/inject_firmware_version.sh"

assert_eq() {
  local expected="$1"
  local actual="$2"
  local label="$3"
  if [[ "$expected" != "$actual" ]]; then
    echo "assert_eq failed: $label" >&2
    echo "  expected: $expected" >&2
    echo "  actual:   $actual" >&2
    exit 1
  fi
}

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/src/config"
cp "$ROOT/version.json" "$TMP/version.json"
cp "$ROOT/src/config/DeviceConfig.h" "$TMP/src/config/DeviceConfig.h"

bash "$INJECT" 9.9.9 "$TMP"

assert_eq "9.9.9" "$(jq -er '.version' "$TMP/version.json")" "injected version.json"
assert_eq "stable" "$(jq -er '.channel' "$TMP/version.json")" "channel preserved"
grep -Fq '#define INKADS_FIRMWARE_VERSION "9.9.9"' "$TMP/src/config/DeviceConfig.h" \
  || { echo "header was not stamped" >&2; exit 1; }

assert_eq "0.0.0" "$(jq -er '.version' "$ROOT/version.json")" "repo version.json unchanged"
grep -Fq '#define INKADS_FIRMWARE_VERSION "0.0.0"' "$ROOT/src/config/DeviceConfig.h" \
  || { echo "repo DeviceConfig.h was changed" >&2; exit 1; }

(
  cd "$TMP"
  node "$ROOT/scripts/check-firmware-version.mjs" v9.9.9
)

if bash "$INJECT" not-a-version "$TMP" >/dev/null 2>"$TMP/err"; then
  echo "expected invalid version to fail" >&2
  exit 1
fi
grep -q 'major.minor.patch' "$TMP/err" || { echo "missing semver error" >&2; exit 1; }

echo "ok inject-version.test.sh"
