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

assert_order() {
  local file="$1"
  local first="$2"
  local second="$3"
  local label="$4"
  local a b
  a="$(grep -n -F "$first" "$file" | head -1 | cut -d: -f1)"
  b="$(grep -n -F "$second" "$file" | head -1 | cut -d: -f1)"
  if [[ -z "$a" || -z "$b" || "$a" -ge "$b" ]]; then
    echo "assert_order failed: $label" >&2
    echo "  first ($first): ${a:-missing}" >&2
    echo "  second ($second): ${b:-missing}" >&2
    exit 1
  fi
}

assert_checkout_has_no_token() {
  local file="$1"
  local label="$2"
  local block=""
  local in_checkout=0
  while IFS= read -r line; do
    if [[ "$line" == *'uses: actions/checkout@'* ]]; then
      in_checkout=1
      block="$line"$'\n'
      continue
    fi
    if [[ "$in_checkout" -eq 1 ]]; then
      if [[ "$line" =~ ^[[:space:]]+-[[:space:]] ]]; then
        in_checkout=0
        if [[ "$block" == *'token:'* ]]; then
          echo "assert failed: $label" >&2
          exit 1
        fi
      else
        block+="$line"$'\n'
      fi
    fi
  done <"$file"
  if [[ "$in_checkout" -eq 1 && "$block" == *'token:'* ]]; then
    echo "assert failed: $label" >&2
    exit 1
  fi
}

release_yml="$(cat "$RELEASE")"
firmware_yml="$(cat "$FIRMWARE")"
assert_contains "$release_yml" \
  'push --no-verify --atomic origin HEAD:main "refs/tags/${NEXT_TAG}"' \
  'release.yml pushes main and the tag atomically without hooks'
assert_contains "$release_yml" 'git commit --no-verify' \
  'release.yml commits the version bump without hooks'
assert_contains "$release_yml" \
  'http.https://github.com/.extraheader=AUTHORIZATION: bearer ${RELEASE_TOKEN}' \
  'release.yml passes the App token via http.extraheader, not the remote URL'
if [[ "$release_yml" == *'x-access-token:'* ]]; then
  echo 'assert failed: release.yml must not put the App token in the origin URL' >&2
  exit 1
fi
assert_contains "$release_yml" 'repositories: ${{ github.event.repository.name }}' \
  'release.yml scopes the App token to this repository'
assert_contains "$release_yml" 'permission-contents: write' \
  'release.yml mints the App token with contents write only'
assert_contains "$release_yml" 'persist-credentials: false' \
  'release.yml checkout does not persist credentials'
if [[ "$release_yml" == *'persist-credentials: true'* ]]; then
  echo 'assert failed: release.yml must not persist checkout credentials' >&2
  exit 1
fi
assert_checkout_has_no_token "$RELEASE" \
  'release.yml checkout must not receive the App token'
assert_order "$RELEASE" 'Validate version consistency' 'actions/create-github-app-token' \
  'release.yml mints the App token after plan, git-cliff, and validate'
assert_order "$RELEASE" 'actions/create-github-app-token' 'Commit, tag, and push to main' \
  'release.yml mints the App token immediately before push'
assert_contains "$release_yml" \
  'orhunp/git-cliff:2.13.1@sha256:d49216b61658fc1b10bab6c5f82dfca03b8e37278618fdc3db235d95cf3c33f5' \
  'release.yml pins git-cliff by digest'
assert_contains "$firmware_yml" 'repositories: ${{ github.event.repository.name }}' \
  'firmware-release.yml scopes the App token to this repository'
assert_contains "$firmware_yml" 'permission-contents: write' \
  'firmware-release.yml mints the App token with contents write only'
assert_checkout_has_no_token "$FIRMWARE" \
  'firmware-release.yml checkouts must not receive the App token'
assert_order "$FIRMWARE" 'Compile release firmware' 'actions/create-github-app-token' \
  'firmware-release.yml mints the App token after tests and compile'
assert_order "$FIRMWARE" 'actions/create-github-app-token' 'Publish GitHub release' \
  'firmware-release.yml mints the App token immediately before publish'
assert_contains "$firmware_yml" 'Overlay current release scripts' \
  'firmware-release.yml overlays scripts from the workflow revision'
assert_contains "$firmware_yml" 'mkdir -p scripts' \
  'overlay creates scripts/ when a historical tag lacks it'
persist_count="$(grep -c 'persist-credentials: false' "$FIRMWARE" || true)"
assert_eq "2" "$persist_count" \
  'both firmware-release checkouts drop persisted credentials'
assert_contains "$firmware_yml" 'inject_firmware_version.sh missing after overlay' \
  'firmware-release.yml fails closed if overlay did not restore inject'
assert_contains "$(cat "$ROOT/scripts/publish-github-release.sh")" \
  "die 'GH_TOKEN is required'" \
  'publish-github-release.sh accepts GH_TOKEN only'

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

# update (not pre-receive) rejects only tags, so a non-atomic push can
# still advance main. That control would pass if --atomic were omitted.
cat >"$REMOTE/hooks/update" <<'HOOK'
#!/bin/sh
ref="$1"
case "$ref" in
  refs/tags/*)
    echo "rejecting tag $ref" >&2
    exit 1
    ;;
esac
exit 0
HOOK
chmod +x "$REMOTE/hooks/update"

BUMP="$(git -C "$LOCAL" rev-parse HEAD)"
if git -C "$LOCAL" push origin HEAD:main refs/tags/v0.0.1 >/dev/null 2>"$WORKDIR/nonatomic.err"; then
  echo "non-atomic push should fail because the tag is rejected" >&2
  exit 1
fi
assert_eq "$BUMP" "$(git -C "$REMOTE" rev-parse refs/heads/main)" \
  'non-atomic control: main advances when only the tag is rejected'
if git -C "$REMOTE" rev-parse refs/tags/v0.0.1 >/dev/null 2>&1; then
  echo "non-atomic control: tag must still be rejected" >&2
  exit 1
fi

git -C "$REMOTE" update-ref refs/heads/main "$MAIN"

if git -C "$LOCAL" push --atomic origin HEAD:main refs/tags/v0.0.1 >/dev/null 2>"$WORKDIR/push.err"; then
  echo "atomic push should fail when the tag is rejected" >&2
  exit 1
fi

assert_eq "$MAIN" "$(git -C "$REMOTE" rev-parse refs/heads/main)" \
  'atomic push must not advance main when the tag update is rejected'
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
