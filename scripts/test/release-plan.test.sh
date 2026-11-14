#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$TEST_DIR/../.." && pwd)"
PLAN="$ROOT/scripts/release_plan.sh"

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

output_value() {
  local key="$1"
  sed -n "s/^${key}=//p" <<<"$OUTPUT" | tail -n 1
}

init_repo() {
  git init -b main "$REPO" >/dev/null
  git -C "$REPO" config user.email "test@example.com"
  git -C "$REPO" config user.name "test"
  printf '%s\n' '{"version":"0.0.0","channel":"stable"}' >"$REPO/version.json"
  git -C "$REPO" add version.json
  git -C "$REPO" commit -m "chore: init [SSD-1]" >/dev/null
}

add_commit() {
  local message="$1"
  echo "$message" >>"$REPO/notes.txt"
  git -C "$REPO" add notes.txt
  git -C "$REPO" commit -m "$message" >/dev/null
}

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
REPO="$TMP/repo"
init_repo

git -C "$REPO" tag -a v1.2.0 -m "v1.2.0"
add_commit "docs: no release [SSD-1]"
OUTPUT="$(cd "$REPO" && bash "$PLAN")"
assert_eq "false" "$(output_value release)" "docs does not release"

add_commit "feat(ota): check the stable channel [SSD-1]"
OUTPUT="$(cd "$REPO" && bash "$PLAN")"
assert_eq "true" "$(output_value release)" "feat releases"
assert_eq "minor" "$(output_value bump)" "feat is minor"
assert_eq "v1.3.0" "$(output_value next_tag)" "feat bumps minor"

git -C "$REPO" reset --hard v1.2.0 >/dev/null
add_commit "fix(wifi): recover after disconnect [SSD-1]"
OUTPUT="$(cd "$REPO" && bash "$PLAN")"
assert_eq "v1.2.1" "$(output_value next_tag)" "fix bumps patch"

git -C "$REPO" reset --hard v1.2.0 >/dev/null
add_commit "feat(manifest)!: require schema version 2 [SSD-1]"
OUTPUT="$(cd "$REPO" && bash "$PLAN")"
assert_eq "v2.0.0" "$(output_value next_tag)" "breaking feat bumps major"

UNTAGGED="$TMP/untagged"
git init -b main "$UNTAGGED" >/dev/null
git -C "$UNTAGGED" config user.email "test@example.com"
git -C "$UNTAGGED" config user.name "test"
printf '%s\n' '{"version":"0.0.0","channel":"stable"}' >"$UNTAGGED/version.json"
git -C "$UNTAGGED" add version.json
git -C "$UNTAGGED" commit -m "chore: init [SSD-1]" >/dev/null
git -C "$UNTAGGED" commit --allow-empty -m "feat: first feature [SSD-1]" >/dev/null
if cd "$UNTAGGED" && bash "$PLAN" >"$TMP/untagged.out" 2>"$TMP/untagged.err"; then
  echo "expected missing tag to fail closed" >&2
  exit 1
fi
grep -Fq 'no v* tag found' "$TMP/untagged.err" || {
  echo "missing fail-closed error" >&2
  cat "$TMP/untagged.err" >&2
  exit 1
}

OUTPUT="$(cd "$UNTAGGED" && bash "$PLAN" --bootstrap)"
assert_eq "true" "$(output_value release)" "bootstrap feat releases"
assert_eq "v0.1.0" "$(output_value next_tag)" "bootstrap feat is v0.1.0"

APPLY="$TMP/apply"
git init -b main "$APPLY" >/dev/null
git -C "$APPLY" config user.email "test@example.com"
git -C "$APPLY" config user.name "test"
mkdir -p "$APPLY/src/config"
printf '%s\n' '{"version":"1.2.0","channel":"stable"}' >"$APPLY/version.json"
cp "$ROOT/src/config/DeviceConfig.h" "$APPLY/src/config/DeviceConfig.h"
git -C "$APPLY" add version.json src
git -C "$APPLY" commit -m "chore: init [SSD-1]" >/dev/null
git -C "$APPLY" tag -a v1.2.0 -m "v1.2.0"
git -C "$APPLY" commit --allow-empty -m "fix: stamp files [SSD-1]" >/dev/null
OUTPUT="$(cd "$APPLY" && bash "$PLAN" --apply-version)"
assert_eq "v1.2.1" "$(output_value next_tag)" "apply-version still plans"
assert_eq "1.2.1" "$(jq -er '.version' "$APPLY/version.json")" "apply-version stamps version.json"
grep -Fq '#define INKADS_FIRMWARE_VERSION "1.2.1"' "$APPLY/src/config/DeviceConfig.h" \
  || { echo "apply-version did not stamp DeviceConfig.h" >&2; exit 1; }

echo "ok release-plan.test.sh"
