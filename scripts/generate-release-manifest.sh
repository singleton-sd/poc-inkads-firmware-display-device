#!/usr/bin/env bash
# Build a multi-target OTA manifest from versioned dist binaries.
# Does not extend scripts/generate-ota-manifest.mjs.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

CATALOG_BIN="$REPO_ROOT/scripts/lib/targets-catalog.sh"
DIST="${1:-$REPO_ROOT/dist}"
OUT="${2:-$DIST/inkads-manifest.json}"

command -v jq >/dev/null || die 'jq is required'
[[ -n "${GITHUB_REPOSITORY:-}" ]] || die 'GITHUB_REPOSITORY is required'
[[ -n "${RELEASE_TAG:-}" ]] || die 'RELEASE_TAG is required'

sha256_file() {
  local path="$1"
  if command -v sha256sum >/dev/null; then
    sha256sum "$path" | awk '{print $1}'
    return
  fi
  if command -v shasum >/dev/null; then
    shasum -a 256 "$path" | awk '{print $1}'
    return
  fi
  die 'sha256sum or shasum is required'
}

version="$(jq -er '.version' version.json)"
channel="$(jq -er '.channel' version.json)"
published="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

targets_json='{}'
while IFS= read -r id; do
  resolved="$(bash "$CATALOG_BIN" resolve "$id")"
  stem="$(printf '%s' "$resolved" | jq -er '.artifactStem')"
  bin="$DIST/${stem}.bin"
  [[ -f "$bin" ]] || die "missing release binary: $bin"
  size="$(wc -c <"$bin" | tr -d ' ')"
  hash="$(sha256_file "$bin")"
  url="https://github.com/${GITHUB_REPOSITORY}/releases/download/${RELEASE_TAG}/${stem}.bin"
  entry="$(jq -nc \
    --arg url "$url" \
    --arg sha "$hash" \
    --argjson size "$size" \
    --arg suffix "$(printf '%s' "$resolved" | jq -er '.suffix')" \
    --argjson features "$(printf '%s' "$resolved" | jq -c '.features')" \
    '{url:$url,sha256:$sha,size:$size,suffix:$suffix,features:$features}')"
  targets_json="$(jq -c --arg id "$id" --argjson entry "$entry" '. + {($id): $entry}' <<<"$targets_json")"
done < <(bash "$CATALOG_BIN" list)


jq -n \
  --argjson schema 2 \
  --arg version "$version" \
  --arg channel "$channel" \
  --arg published "$published" \
  --argjson targets "$targets_json" \
  '{
    schemaVersion: $schema,
    version: $version,
    channel: $channel,
    publishedAt: $published,
    targets: $targets
  }' >"$OUT"

echo "Wrote OTA manifest to $OUT"
