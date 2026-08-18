#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$TEST_DIR/../.." && pwd)"
CATALOG_BIN="$ROOT/scripts/lib/targets-catalog.sh"

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
mkdir -p "$TMP/dist"

stem="$("$CATALOG_BIN" resolve mhetesp32minikit-full | jq -er '.artifactStem')"
version="$("$CATALOG_BIN" resolve mhetesp32minikit-full | jq -er '.version')"
printf dummy >"$TMP/dist/${stem}.bin"

GITHUB_REPOSITORY=example/inkads RELEASE_TAG="v${version}" \
  bash "$ROOT/scripts/generate-release-manifest.sh" "$TMP/dist" "$TMP/dist/inkads-manifest.json"

url="$(jq -er '.targets["mhetesp32minikit-full"].url' "$TMP/dist/inkads-manifest.json")"
assert_eq \
  "https://github.com/example/inkads/releases/download/v${version}/${stem}.bin" \
  "$url" \
  "manifest url"
assert_eq "2" "$(jq -er '.schemaVersion' "$TMP/dist/inkads-manifest.json")" "schema 2"
assert_eq "full" "$(jq -er '.targets["mhetesp32minikit-full"].suffix' "$TMP/dist/inkads-manifest.json")" "suffix"
assert_eq "$version" "$(jq -er '.version' "$TMP/dist/inkads-manifest.json")" "manifest version"
echo "ok manifest.test.sh"
