#!/usr/bin/env bash
# The release job must update main and the tag in one atomic push, and Git
# must leave the remote unchanged when the tag update is rejected.
set -euo pipefail

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$TEST_DIR/../.." && pwd)"
RELEASE="$ROOT/.github/workflows/release.yml"
FIRMWARE="$ROOT/.github/workflows/firmware-release.yml"

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
    exit 1
  fi
}

release_yml="$(cat "$RELEASE")"
firmware_yml="$(cat "$FIRMWARE")"
assert_contains "$release_yml" 'git push --atomic origin HEAD:main "refs/tags/${NEXT_TAG}"' \
  'release.yml pushes main and the tag atomically'
assert_contains "$release_yml" 'repositories: ${{ github.event.repository.name }}' \
  'release.yml scopes the App token to this repository'
assert_contains "$firmware_yml" 'repositories: ${{ github.event.repository.name }}' \
  'firmware-release.yml scopes the App token to this repository'
assert_contains "$firmware_yml" 'Overlay current release scripts' \
  'firmware-release.yml overlays scripts from the workflow revision'
assert_contains "$firmware_yml" 'mkdir -p scripts' \
  'overlay creates scripts/ when a historical tag lacks it'
persist_count="$(grep -c 'persist-credentials: false' "$FIRMWARE" || true)"
assert_eq "2" "$persist_count" \
  'both firmware-release checkouts drop persisted credentials'
assert_contains "$firmware_yml" 'inject_firmware_version.sh missing after overlay' \
  'firmware-release.yml fails closed if overlay did not restore inject'

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
REMOTE="$WORKDIR/remote.git"
LOCAL="$WORKDIR/local"

git -c init.defaultBranch=main init --bare "$REMOTE" >/dev/null
git init -b main "$LOCAL" >/dev/null
git -C "$LOCAL" config user.email "test@example.com"
git -C "$LOCAL" config user.name "test"
git -C "$LOCAL" remote add origin "$REMOTE"
echo base >"$LOCAL/note.txt"
git -C "$LOCAL" add note.txt
git -C "$LOCAL" commit -m "chore: base [SSD-1]" >/dev/null
git -C "$LOCAL" push -u origin HEAD:main >/dev/null
MAIN="$(git -C "$REMOTE" rev-parse refs/heads/main)"

echo bump >>"$LOCAL/note.txt"
git -C "$LOCAL" add note.txt
git -C "$LOCAL" commit -m "chore: bump [SSD-1]" >/dev/null
git -C "$LOCAL" tag -a v0.0.1 -m "v0.0.1"

cat >"$REMOTE/hooks/pre-receive" <<'HOOK'
#!/bin/sh
while read -r _old _new ref; do
  case "$ref" in
    refs/tags/*)
      echo "rejecting tag $ref" >&2
      exit 1
      ;;
  esac
done
exit 0
HOOK
chmod +x "$REMOTE/hooks/pre-receive"

if git -C "$LOCAL" push --atomic origin HEAD:main refs/tags/v0.0.1 >/dev/null 2>"$WORKDIR/push.err"; then
  echo "atomic push should fail when the tag is rejected" >&2
  exit 1
fi

assert_eq "$MAIN" "$(git -C "$REMOTE" rev-parse refs/heads/main)" \
  'main must not advance when the tag update is rejected'
if git -C "$REMOTE" rev-parse refs/tags/v0.0.1 >/dev/null 2>&1; then
  echo "tag v0.0.1 must not exist on the remote" >&2
  exit 1
fi

# Historical tags lack inject_firmware_version.sh. Overlaying current
# scripts/ onto that tree must restore it and replace compile.sh.
OVERLAY_SRC="$WORKDIR/current-scripts"
OVERLAY_DST="$WORKDIR/historical-tag/scripts"
mkdir -p "$OVERLAY_SRC" "$OVERLAY_DST"
cp -a "$ROOT/scripts/." "$OVERLAY_SRC/"
printf 'old-compile without version flag\n' >"$OVERLAY_DST/compile.sh"
if [[ -e "$OVERLAY_DST/inject_firmware_version.sh" ]]; then
  echo "historical fixture must not already contain inject_firmware_version.sh" >&2
  exit 1
fi
mkdir -p "$OVERLAY_DST"
cp -a "$OVERLAY_SRC/." "$OVERLAY_DST/"
if [[ ! -f "$OVERLAY_DST/inject_firmware_version.sh" ]]; then
  echo "overlay must restore inject_firmware_version.sh" >&2
  exit 1
fi
assert_contains "$(cat "$OVERLAY_DST/compile.sh")" '--version' \
  'overlay replaces historical compile.sh with current --version support'
