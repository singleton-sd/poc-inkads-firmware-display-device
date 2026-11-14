#!/usr/bin/env bash
# Stamp version.json and DeviceConfig.h with a release version.
# Used in CI and in the temp sketch copy; committed files keep the 0.0.0 default.

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

version="${1:-}"
root="${2:-}"

[[ -n "$version" ]] || die 'usage: inject_firmware_version.sh <major.minor.patch> [root]'
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "version must be major.minor.patch: $version"

if [[ -z "$root" ]]; then
  root="$(cd "$(dirname "$0")/.." && pwd)"
fi

version_json="$root/version.json"
header="$root/src/config/DeviceConfig.h"
[[ -f "$version_json" ]] || die "missing $version_json"
[[ -f "$header" ]] || die "missing $header"

tmp="$(mktemp)"
jq --arg v "$version" '.version = $v' "$version_json" >"$tmp"
mv "$tmp" "$version_json"

tmp="$(mktemp)"
if ! sed -E "s/#define INKADS_FIRMWARE_VERSION \"[^\"]*\"/#define INKADS_FIRMWARE_VERSION \"$version\"/" \
  "$header" >"$tmp"; then
  rm -f "$tmp"
  die "could not update INKADS_FIRMWARE_VERSION in DeviceConfig.h"
fi
if ! grep -Fq "#define INKADS_FIRMWARE_VERSION \"$version\"" "$tmp"; then
  rm -f "$tmp"
  die "could not update INKADS_FIRMWARE_VERSION in DeviceConfig.h"
fi
mv "$tmp" "$header"
