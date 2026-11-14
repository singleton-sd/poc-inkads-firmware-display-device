#!/usr/bin/env bash
# Choose the next vX.Y.Z tag from the latest tag plus Conventional Commits.
# Does not modify version.json, DeviceConfig.h, or CHANGELOG.md.
# Fails closed when no v* tag exists unless --bootstrap is passed.

set -euo pipefail

BOOTSTRAP=false
if [[ "${1:-}" == "--bootstrap" ]]; then
  BOOTSTRAP=true
elif [[ -n "${1:-}" ]]; then
  echo "usage: release_plan.sh [--bootstrap]" >&2
  exit 1
fi

write_output() {
  local key="$1"
  local value="$2"
  printf '%s=%s\n' "$key" "$value"
  if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
    printf '%s=%s\n' "$key" "$value" >>"$GITHUB_OUTPUT"
  fi
}

latest_tag="$(git describe --tags --abbrev=0 --match 'v*' 2>/dev/null || true)"
range=""

if [[ -n "$latest_tag" ]]; then
  if [[ ! "$latest_tag" =~ ^v([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    echo "Tag is not semver-like: $latest_tag" >&2
    exit 1
  fi
  major="${BASH_REMATCH[1]}"
  minor="${BASH_REMATCH[2]}"
  patch="${BASH_REMATCH[3]}"
  range="${latest_tag}..HEAD"
elif [[ "$BOOTSTRAP" == true ]]; then
  echo "No v* tag found; --bootstrap starts from v0.0.0" >&2
  major=0
  minor=0
  patch=0
  latest_tag="v0.0.0"
  range="HEAD"
else
  echo "error: no v* tag found; refusing to plan a release from version.json" >&2
  echo "Create an annotated vX.Y.Z tag, or pass --bootstrap to start from 0.0.0." >&2
  exit 1
fi

subjects="$(git log --no-merges --format='%s' "$range" || true)"
bodies="$(git log --no-merges --format='%b' "$range" || true)"

bump=""
if [[ -n "$subjects" ]] && grep -Eq '^[a-z]+(\([^)]+\))?!:' <<<"$subjects"; then
  bump="major"
elif [[ -n "$bodies" ]] && grep -Eq '(^|[[:space:]])BREAKING[- ]CHANGE:' <<<"$bodies"; then
  bump="major"
elif [[ -n "$subjects" ]] && grep -Eq '^feat(\(|:)' <<<"$subjects"; then
  bump="minor"
elif [[ -n "$subjects" ]] && grep -Eq '^(fix|perf|refactor|revert)(\(|:)' <<<"$subjects"; then
  bump="patch"
fi

if [[ -z "$bump" ]]; then
  write_output release false
  write_output base_tag "$latest_tag"
  exit 0
fi

case "$bump" in
  major)
    next_version="$((major + 1)).0.0"
    ;;
  minor)
    next_version="${major}.$((minor + 1)).0"
    ;;
  patch)
    next_version="${major}.${minor}.$((patch + 1))"
    ;;
  *)
    echo "Unknown bump: $bump" >&2
    exit 1
    ;;
esac

next_tag="v${next_version}"

write_output release true
write_output bump "$bump"
write_output base_tag "$latest_tag"
write_output next_version "$next_version"
write_output next_tag "$next_tag"
write_output range "$range"
