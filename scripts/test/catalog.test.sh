#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
CATALOG_BIN="$(cd "$TEST_DIR/../lib" && pwd)/targets-catalog.sh"
FIXTURES="$TEST_DIR/fixtures"

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

assert_contains() {
  local haystack="$1"
  local needle="$2"
  local label="$3"
  if [[ "$haystack" != *"$needle"* ]]; then
    echo "assert_contains failed: $label" >&2
    echo "  missing: $needle" >&2
    echo "  in: $haystack" >&2
    exit 1
  fi
}

must_fail() {
  local label="$1"
  shift
  if INKADS_CATALOG="$1" INKADS_VERSION_FILE="$2" bash "$CATALOG_BIN" validate >/dev/null 2>"$TMP/err"; then
    echo "expected failure: $label" >&2
    exit 1
  fi
  assert_contains "$(cat "$TMP/err")" "$3" "$label"
}

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

resolved="$(
  INKADS_CATALOG="$FIXTURES/catalog-valid.json" \
    INKADS_VERSION_FILE="$FIXTURES/version-0.2.0.json" \
    bash "$CATALOG_BIN" resolve mhetesp32minikit-full
)"
assert_eq "inkads-mhetesp32minikit-full-v0.2.0" "$(printf '%s' "$resolved" | jq -er '.artifactStem')" "full stem v0.2.0"
assert_eq "mhetesp32minikit-full" "$(printf '%s' "$resolved" | jq -er '.id')" "target id omits version"
assert_eq "full" "$(printf '%s' "$resolved" | jq -er '.suffix')" "suffix"
assert_eq "0.2.0" "$(printf '%s' "$resolved" | jq -er '.version')" "version from version.json"
assert_eq "esp32:esp32:mhetesp32minikit" "$(printf '%s' "$resolved" | jq -er '.fqbn')" "default fqbn"

resolved_new="$(
  INKADS_CATALOG="$FIXTURES/catalog-valid.json" \
    INKADS_VERSION_FILE="$FIXTURES/version-1.4.0.json" \
    bash "$CATALOG_BIN" resolve mhetesp32minikit-full
)"
assert_eq "inkads-mhetesp32minikit-full-v1.4.0" "$(printf '%s' "$resolved_new" | jq -er '.artifactStem')" "stem follows version.json"
assert_eq "mhetesp32minikit-full" "$(printf '%s' "$resolved_new" | jq -er '.id')" "target id stable across versions"

lite="$(
  INKADS_CATALOG="$FIXTURES/catalog-lite.json" \
    INKADS_VERSION_FILE="$FIXTURES/version-0.2.0.json" \
    bash "$CATALOG_BIN" resolve mhetesp32minikit-lite
)"
assert_eq "inkads-mhetesp32minikit-lite-v0.2.0" "$(printf '%s' "$lite" | jq -er '.artifactStem')" "lite stem"
assert_eq "false" "$(printf '%s' "$lite" | jq -er '.features | index("entra") != null')" "lite omits entra"

INKADS_CATALOG="$FIXTURES/catalog-lite.json" \
  INKADS_VERSION_FILE="$FIXTURES/version-0.2.0.json" \
  bash "$CATALOG_BIN" write-features mhetesp32minikit-full "$TMP/full.h"
INKADS_CATALOG="$FIXTURES/catalog-lite.json" \
  INKADS_VERSION_FILE="$FIXTURES/version-0.2.0.json" \
  bash "$CATALOG_BIN" write-features mhetesp32minikit-lite "$TMP/lite.h"

assert_contains "$(cat "$TMP/full.h")" '#define INKADS_TARGET_ID "mhetesp32minikit-full"' "full target id define"
assert_contains "$(cat "$TMP/full.h")" '#define INKADS_FEATURE_ENTRA 1' "full entra on"
assert_contains "$(cat "$TMP/full.h")" '#define INKADS_FEATURE_EPAPER 0' "epaper off when omitted"
assert_contains "$(cat "$TMP/lite.h")" '#define INKADS_FEATURE_ENTRA 0' "lite entra off"
assert_contains "$(cat "$TMP/lite.h")" '#define INKADS_FEATURE_WIFI 1' "lite wifi on"

matrix="$(
  INKADS_CATALOG="$FIXTURES/catalog-lite.json" \
    INKADS_VERSION_FILE="$FIXTURES/version-0.2.0.json" \
    bash "$CATALOG_BIN" matrix
)"
assert_eq "2" "$(printf '%s' "$matrix" | jq -er '.include | length')" "matrix has two targets"
assert_eq "inkads-mhetesp32minikit-full-v0.2.0" "$(printf '%s' "$matrix" | jq -er '.include[0].artifactStem')" "matrix full stem"
assert_eq "inkads-mhetesp32minikit-lite-v0.2.0" "$(printf '%s' "$matrix" | jq -er '.include[1].artifactStem')" "matrix lite stem"
assert_eq "0.2.0" "$(printf '%s' "$matrix" | jq -er '.include[0].version')" "matrix version"

must_fail "fqbn array" \
  "$FIXTURES/catalog-fqbn-array.json" \
  "$FIXTURES/version-0.2.0.json" \
  "fqbn must be a string"
must_fail "unknown board" \
  "$FIXTURES/catalog-unknown-board.json" \
  "$FIXTURES/version-0.2.0.json" \
  "not in catalog.boards"
must_fail "empty suffix" \
  "$FIXTURES/catalog-empty-suffix.json" \
  "$FIXTURES/version-0.2.0.json" \
  "invalid or missing target suffix"
must_fail "unknown feature" \
  "$FIXTURES/catalog-unknown-feature.json" \
  "$FIXTURES/version-0.2.0.json" \
  "unknown feature"
must_fail "duplicate suffix" \
  "$FIXTURES/catalog-duplicate-suffix.json" \
  "$FIXTURES/version-0.2.0.json" \
  "duplicate target board+suffix"
must_fail "invalid version" \
  "$FIXTURES/catalog-valid.json" \
  "$FIXTURES/version-invalid.json" \
  "major.minor.patch"

echo "ok catalog.test.sh"
