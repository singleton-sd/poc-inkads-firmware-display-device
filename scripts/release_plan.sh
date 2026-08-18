#!/usr/bin/env bash
set -euo pipefail

APPLY_VERSION=false
if [[ "${1:-}" == "--apply-version" ]]; then
  APPLY_VERSION=true
fi

write_output() {
  local key="$1"
  local value="$2"
  printf '%s=%s\n' "$key" "$value"
  if [[ -n "${GITHUB_OUTPUT:-}" ]]; then
    printf '%s=%s\n' "$key" "$value" >> "$GITHUB_OUTPUT"
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
else
  json_version="$(sed -n 's/.*"version": "\([^"]*\)".*/\1/p' version.json | head -n1)"
  if [[ ! "$json_version" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
    echo "version.json is not semver-like: $json_version" >&2
    exit 1
  fi
  major="${BASH_REMATCH[1]}"
  minor="${BASH_REMATCH[2]}"
  patch="${BASH_REMATCH[3]}"
  latest_tag="v${json_version}"
  base_commit="$(git log -1 --format='%H' -- version.json || true)"
  if [[ -n "$base_commit" ]]; then
    range="${base_commit}..HEAD"
  else
    range="HEAD"
  fi
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

apply_version() {
  local version="$1"
  local tmp

  tmp="$(mktemp)"
  sed "s/\"version\": \"[^\"]*\"/\"version\": \"${version}\"/" version.json > "$tmp"
  mv "$tmp" version.json

  tmp="$(mktemp)"
  if ! sed "s/firmwareVersion\\[\\] = \"[^\"]*\"/firmwareVersion[] = \"${version}\"/" \
    src/config/DeviceConfig.h > "$tmp"; then
    echo "Could not update firmwareVersion in DeviceConfig.h" >&2
    rm -f "$tmp"
    exit 1
  fi
  if grep -Fq "firmwareVersion[] = \"${version}\"" "$tmp"; then
    mv "$tmp" src/config/DeviceConfig.h
  else
    echo "Could not update firmwareVersion in DeviceConfig.h" >&2
    rm -f "$tmp"
    exit 1
  fi
}

if [[ "$APPLY_VERSION" == true ]]; then
  apply_version "$next_version"
fi

write_output release true
write_output bump "$bump"
write_output base_tag "$latest_tag"
write_output next_version "$next_version"
write_output next_tag "$next_tag"
write_output range "$range"
