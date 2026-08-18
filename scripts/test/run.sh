#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
failed=0

command -v jq >/dev/null || {
  echo "error: jq is required to run scripts/test" >&2
  exit 1
}

for test in "$TEST_DIR"/*.test.sh; do
  echo "==> $(basename "$test")"
  if ! bash "$test"; then
    echo "FAIL $(basename "$test")" >&2
    failed=1
  fi
done

if [[ "$failed" -ne 0 ]]; then
  echo 'catalog tests failed' >&2
  exit 1
fi

echo 'catalog tests passed'
