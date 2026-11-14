#!/usr/bin/env bash
# Stamp version.json and DeviceConfig.h with a release version.
# Used in CI and in the temp sketch copy; committed files keep the 0.0.0 default.
# Accepts the current INKADS_FIRMWARE_VERSION define and the pre-tag-only
# firmwareVersion[] = "x.y.z" assignment so older tags can be republished.

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
if grep -Eq '#define[[:space:]]+INKADS_FIRMWARE_VERSION[[:space:]]+"' "$header"; then
  sed -E "s/#define INKADS_FIRMWARE_VERSION \"[^\"]*\"/#define INKADS_FIRMWARE_VERSION \"$version\"/" \
    "$header" >"$tmp"
elif grep -Eq 'firmwareVersion\[[[:space:]]*\][[:space:]]*=[[:space:]]*"' "$header"; then
  sed -E "s/firmwareVersion\[[[:space:]]*\][[:space:]]*=[[:space:]]*\"[^\"]*\"/firmwareVersion[] = \"$version\"/" \
    "$header" >"$tmp"
else
  rm -f "$tmp"
  die "could not find INKADS_FIRMWARE_VERSION or firmwareVersion[] in DeviceConfig.h"
fi

if grep -Fq "#define INKADS_FIRMWARE_VERSION \"$version\"" "$tmp" ||
  grep -Fq "firmwareVersion[] = \"$version\"" "$tmp"; then
  mv "$tmp" "$header"
else
  rm -f "$tmp"
  die "could not update firmware version in DeviceConfig.h"
fi
