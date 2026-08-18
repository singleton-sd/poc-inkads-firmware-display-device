#!/bin/sh

title="$1"
allowed_types='build|chore|ci|docs|feat|fix|perf|refactor|revert|style|test'
pattern="^(${allowed_types})(\\([a-z0-9][a-z0-9._/-]*\\))?!?: .+ \\[[A-Z][A-Z0-9]+-[0-9]+\\]$"

if [ -z "$title" ]; then
  echo "A Conventional Commit title is required." >&2
  exit 1
fi

title=$(printf '%s' "$title" | tr -d '\r')
branch_name="${GIT_BRANCH:-}"
if [ -z "$branch_name" ]; then
  branch_name=$(git symbolic-ref --quiet --short HEAD 2>/dev/null || true)
fi

branch_ticket=$(printf '%s\n' "$branch_name" | sed -nE 's#^(feature|hotfix)/([A-Z][A-Z0-9]+-[0-9]+)(-.+)?$#\2#p')
message_ticket=$(printf '%s\n' "$title" | sed -nE 's#^.*\[([A-Z][A-Z0-9]+-[0-9]+)\]$#\1#p')

if [ -n "$branch_ticket" ] && [ -n "$message_ticket" ] && [ "$branch_ticket" != "$message_ticket" ]; then
  cat >&2 <<EOF
Commit ticket does not match the current branch.

Branch:
  $branch_name

Expected ticket:
  $branch_ticket

Found in commit message:
  $message_ticket
EOF
  exit 1
fi

if printf '%s\n' "$title" | grep -Eq "$pattern"; then
  printf 'Valid Conventional Commit title: %s\n' "$title"
  exit 0
fi

cat >&2 <<'EOF'
Invalid commit message.

Expected format:
  <type>(optional-scope): <description> [ABC-123]

If you are on a `feature/ABC-123-...` or `hotfix/ABC-123-...` branch, the
local `prepare-commit-msg` hook appends the ticket automatically.

Examples:
  feat(ota): check the stable channel for updates [POC-247]
  fix(wifi): recover after disconnect [POC-247]

Allowed types:
  build, chore, ci, docs, feat, fix, perf, refactor, revert, style, test
EOF

exit 1
