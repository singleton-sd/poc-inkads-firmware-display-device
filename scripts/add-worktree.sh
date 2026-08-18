#!/usr/bin/env bash
# Create a firmware worktree beside the clone.
#
# Layout (open the InkAds workspace, not the clone):
#   InkAds/firmware/display-device/          git clone, stays on main
#   InkAds/firmware/worktrees/<id>-<slug>/   this script
#
# Usage:
#   ./scripts/add-worktree.sh --task-id POC-247 --slug conventional-commit-hooks
#   ./scripts/add-worktree.sh --task-id POC-247 --slug broken-wifi --hotfix
#   ./scripts/add-worktree.sh --type docs --slug agent-working-agreements

set -euo pipefail

die() { echo "error: $*" >&2; exit 1; }

TASK_ID=''
SLUG=''
TYPE=''
HOTFIX=0
DRY_RUN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --task-id) TASK_ID="${2:-}"; shift 2 ;;
    --type) TYPE="${2:-}"; shift 2 ;;
    --slug) SLUG="${2:-}"; shift 2 ;;
    --hotfix) HOTFIX=1; shift ;;
    --dry-run) DRY_RUN=1; shift ;;
    *) die "Unknown argument: $1" ;;
  esac
done

[[ -n "$SLUG" ]] || die '--slug is required'
[[ "$SLUG" =~ ^[a-z0-9]+(-[a-z0-9]+)*$ ]] || die "--slug must be kebab-case: $SLUG"

if [[ "$HOTFIX" -eq 1 ]]; then
  TYPE='hotfix'
elif [[ -z "$TYPE" ]]; then
  TYPE='feature'
fi

[[ "$TYPE" =~ ^[a-z0-9]+$ ]] || die "--type must be a conventional-commit prefix: $TYPE"

if [[ -n "$TASK_ID" ]]; then
  [[ "$TASK_ID" =~ ^[A-Z][A-Z0-9]*-[0-9]+$ ]] || die "--task-id must look like POC-247: $TASK_ID"
  FOLDER="${TASK_ID}-${SLUG}"
  BRANCH="${TYPE}/${TASK_ID}-${SLUG}"
else
  FOLDER="$SLUG"
  BRANCH="${TYPE}/${SLUG}"
fi

GIT_COMMON="$(git rev-parse --path-format=absolute --git-common-dir 2>/dev/null)" \
  || die 'Not inside a git repository.'
[[ "$(basename "$GIT_COMMON")" == ".git" ]] || die "git-common-dir should end with .git, got: $GIT_COMMON"
REPO_ROOT="$(cd "$(dirname "$GIT_COMMON")" && pwd)"
WT_PATH="$(cd "$REPO_ROOT/.." && pwd)/worktrees/${FOLDER}"

git_safe() {
  command git -c "safe.directory=$REPO_ROOT" "$@"
}

echo "Branch:   $BRANCH"
echo "Worktree: $WT_PATH"

if [[ "$DRY_RUN" -eq 1 ]]; then
  exit 0
fi

[[ ! -e "$WT_PATH" ]] || die "Worktree path already exists: $WT_PATH"
mkdir -p "$(dirname "$WT_PATH")"

echo 'Fetching origin/main...'
git_safe fetch origin main

if git_safe show-ref --verify --quiet "refs/heads/$BRANCH" \
  || git_safe show-ref --verify --quiet "refs/remotes/origin/$BRANCH"; then
  echo "Adding worktree for existing branch $BRANCH"
  git_safe worktree add "$WT_PATH" "$BRANCH"
else
  echo "Creating $BRANCH from origin/main"
  git_safe worktree add -b "$BRANCH" "$WT_PATH" origin/main
fi

echo "Worktree ready: $WT_PATH"
echo 'Open the InkAds workspace folder in Cursor (the directory that contains firmware/).'
