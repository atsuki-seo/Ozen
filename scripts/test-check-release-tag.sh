#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-release-tag.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
v=$(cat "$here/../VERSION")

cases=0
failed=0

expect() {
  local name=$1 want=$2 status
  shift 2
  cases=$((cases + 1))
  set +e
  "$checker" "$@" > "$work/out" 2>&1
  status=$?
  set -e
  if [ "$status" -ne "$want" ]; then
    echo "FAIL $name: status $status, want $want: $(cat "$work/out")" >&2
    failed=$((failed + 1))
  fi
}

expect version-tag 0 "v$v"
expect no-v 1 "$v"
expect other-version 1 "v$v.1"
expect plan-tag 1 qc-end

workflow() {
  cat > "$work/release.yml" <<YAML
on:
  push:
    tags: ['v*']
jobs:
  linux:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: $1
        run: $2
      - run: scripts/build-core.sh --only core-gcc-release
YAML
}
workflow "Check the tag" 'scripts/check-release-tag.sh "$GITHUB_REF_NAME"'
expect workflow-first 0 --workflow-first-step "$work/release.yml"
workflow "Build first" 'scripts/build-core.sh'
expect workflow-later 1 --workflow-first-step "$work/release.yml"

echo "release-tag: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
