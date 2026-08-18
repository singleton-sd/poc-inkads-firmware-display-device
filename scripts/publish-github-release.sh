#!/usr/bin/env bash
# Attach every dist/*.bin and dist/*.json to a GitHub Release.
# Skips .elf/.map. Creates the release if it does not exist yet.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

DRY_RUN=false
if [[ "${1:-}" == "--dry-run" ]]; then
  DRY_RUN=true
  shift
fi

RELEASE_TAG="${RELEASE_TAG:-${1:-}}"
[[ -n "$RELEASE_TAG" ]] || die 'RELEASE_TAG is required'

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DIST="$REPO_ROOT/dist"
cd "$REPO_ROOT"

shopt -s nullglob
assets=("$DIST"/*.bin "$DIST"/*.json)
if [[ ${#assets[@]} -eq 0 ]]; then
  die "No dist/*.bin or dist/*.json to attach"
fi

echo "Release assets for $RELEASE_TAG:"
printf '  %s\n' "${assets[@]}"

if [[ "$DRY_RUN" == true ]]; then
  exit 0
fi

command -v gh >/dev/null || die 'gh is not on PATH'
[[ -n "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ]] || die 'GH_TOKEN or GITHUB_TOKEN is required'

if gh release view "$RELEASE_TAG" >/dev/null 2>&1; then
  gh release upload "$RELEASE_TAG" "${assets[@]}" --clobber
else
  gh release create "$RELEASE_TAG" "${assets[@]}" \
    --verify-tag \
    --generate-notes \
    --title "InkAds firmware $RELEASE_TAG"
fi
