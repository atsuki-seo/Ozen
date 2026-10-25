#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-selftests.sh"

cases=0
failed=0

new_repo() {
  local dir
  dir=$(mktemp -d)
  git -C "$dir" init -q
  mkdir -p "$dir/scripts"
  printf '%s\n' "$dir"
}

script() {
  printf '#!/bin/sh\nexit %s\n' "$3" > "$1/$2"
  chmod +x "$1/$2"
}

expect() {
  local name=$1 repo=$2 want_last=$3 want_status=$4 got got_status
  cases=$((cases + 1))
  set +e
  got=$("$checker" --root "$repo" 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got" != "$want_last" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $got_status), want '$want_last' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
  rm -rf "$repo"
}

r=$(new_repo)
script "$r" scripts/check-a.sh 0; script "$r" scripts/test-check-a.sh 0
printf 'scripts/check-a.sh scripts/test-check-a.sh\n' > "$r/scripts/selftests.txt"
expect all-pass "$r" "selftests: 1 scripts, 0 without test, 0 failed" 0

r=$(new_repo)
script "$r" scripts/check-a.sh 0; script "$r" scripts/run-b.sh 0; script "$r" scripts/test-check-a.sh 0
printf 'scripts/check-a.sh scripts/test-check-a.sh\n' > "$r/scripts/selftests.txt"
expect unlisted-script "$r" "selftests: 2 scripts, 1 without test, 0 failed" 1

r=$(new_repo)
script "$r" scripts/check-a.sh 0
printf 'scripts/check-a.sh scripts/test-check-a.sh\n' > "$r/scripts/selftests.txt"
expect test-file-missing "$r" "selftests: 1 scripts, 1 without test, 0 failed" 1

r=$(new_repo)
script "$r" scripts/check-a.sh 0; script "$r" scripts/test-check-a.sh 1
printf 'scripts/check-a.sh scripts/test-check-a.sh\n' > "$r/scripts/selftests.txt"
expect test-fails "$r" "selftests: 1 scripts, 0 without test, 1 failed" 1

r=$(new_repo)
mkdir -p "$r/tools/schedcheck"
script "$r" tools/schedcheck/schedcheck.py 0; script "$r" scripts/build-all.sh 0
printf 'tools/schedcheck/schedcheck.py true\n' > "$r/scripts/selftests.txt"
expect command-test-and-build-excluded "$r" "selftests: 1 scripts, 0 without test, 0 failed" 0

echo "test-check-selftests: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
