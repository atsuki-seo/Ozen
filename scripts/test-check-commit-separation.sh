#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-commit-separation.sh"

cases=0
failed=0

new_repo() {
  local dir
  dir=$(mktemp -d)
  git -C "$dir" init -q
  git -C "$dir" config user.email test@example.invalid
  git -C "$dir" config user.name test
  git -C "$dir" commit -q --allow-empty -m base
  git -C "$dir" tag base
  printf '%s\n' "$dir"
}

commit() {
  local repo=$1
  shift
  local f
  for f in "$@"; do
    mkdir -p "$(dirname "$repo/$f")"
    date +%s%N >> "$repo/$f"
    git -C "$repo" add "$f"
  done
  git -C "$repo" commit -q -m "change $*"
}

expect() {
  local name=$1 repo=$2 want_last=$3 want_status=$4 got got_status
  cases=$((cases + 1))
  set +e
  got=$(cd "$repo" && "$checker" base..HEAD '^spec/fixtures/' '^tools/vocabcheck/' 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got" != "$want_last" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $got_status), want '$want_last' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
  rm -rf "$repo"
}

r=$(new_repo); commit "$r" spec/fixtures/a.yaml; commit "$r" tools/vocabcheck/v.py
expect separate "$r" "separation: 1 commits, 0 mixed" 0

r=$(new_repo); commit "$r" spec/fixtures/a.yaml tools/vocabcheck/v.py
expect mixed "$r" "separation: 1 commits, 1 mixed" 1

r=$(new_repo); commit "$r" tools/vocabcheck/v.py
expect no-expectations "$r" "separation: 0 commits, 0 mixed" 1

r=$(new_repo); commit "$r" spec/fixtures/a.yaml README.md; commit "$r" spec/fixtures/b.yaml
expect other-files-allowed "$r" "separation: 2 commits, 0 mixed" 0

echo "test-check-commit-separation: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
