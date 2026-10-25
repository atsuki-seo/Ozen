#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
bin=${1:-$root/build/gcc-debug-san/ozenproto}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

expect_status() {
  local name=$1 want=$2 got
  shift 2
  cases=$((cases + 1))
  "$bin" "$@" >/dev/null 2>&1
  got=$?
  if [ "$got" -ne "$want" ]; then
    echo "FAIL $name: exit $got, want $want" >&2
    failed=$((failed + 1))
  fi
}

printf 'action a(x)\nrecipe r() {\n  a(1 g)\n}\n' > "$work/ok.ozen"
printf 'action a(x)\nrecipe r() {\n  a(1 g + 1 min)\n}\n' > "$work/error.ozen"

expect_status result-ok 0 "$work/ok.ozen"
expect_status result-error 1 "$work/error.ozen"
expect_status unknown-option 2 --frobnicate "$work/ok.ozen"
expect_status missing-file-argument 2
expect_status two-files 2 "$work/ok.ozen" "$work/ok.ozen"
expect_status unreadable-file 2 "$work/no-such-file.ozen"
expect_status bad-limit 2 --limit-steps abc "$work/ok.ozen"

n=$("$bin" --count-allocs "$work/ok.ozen" 2>&1 >/dev/null | sed -nE 's/^allocs: ([0-9]+) .*/\1/p')
expect_status result-alloc-fails 2 --fail-alloc-at "$n" "$work/ok.ozen"
expect_status after-last-alloc 0 --fail-alloc-at "$((n + 1))" "$work/ok.ozen"

echo "test-driver: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
