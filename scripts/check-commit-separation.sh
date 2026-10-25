#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

if [ $# -ne 3 ]; then
  echo "usage: $0 RANGE EXPECT_REGEX IMPL_REGEX" >&2
  exit 2
fi
range=$1
expect_re=$2
impl_re=$3

commits=0
mixed=0
while IFS= read -r sha; do
  files=$(git diff-tree --no-commit-id --name-only -r --root "$sha")
  grep -qE "$expect_re" <<< "$files" || continue
  commits=$((commits + 1))
  if grep -qE "$impl_re" <<< "$files"; then
    echo "mixed: $(git log -1 --format='%h %s' "$sha")" >&2
    mixed=$((mixed + 1))
  fi
done < <(git rev-list --reverse "$range")

echo "separation: $commits commits, $mixed mixed"
[ "$commits" -ge 1 ] && [ "$mixed" -eq 0 ]
