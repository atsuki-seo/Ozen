#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
mkdir -p "$work/g/a"
printf '#!/bin/sh\nfor a; do f=$a; done\necho "args: $*"\ngrep -q fail "$f" && exit 1\nexit 0\n' > "$work/run.sh"
chmod +x "$work/run.sh"
printf 'ok\n' > "$work/g/a/one.ozen"
printf 'args: %s\nexit 0\n' "$work/g/a/one.ozen" > "$work/g/a/one.out"
printf '// args: --x 1\nfail\n' > "$work/g/a/two.ozen"
printf 'args: --x 1 %s\nexit 1\n' "$work/g/a/two.ozen" > "$work/g/a/two.out"

expect() {
  local name=$1 want=$2 status=$3 got st
  shift 3
  cases=$((cases + 1))
  set +e
  got=$("$here/run-golden.sh" "$work/run.sh" --dir "$work/g" "$@" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

expect all-pass "golden: 2 passed, 0 failed, 0 timed out" 0
expect only-one "golden: 1 passed, 0 failed, 0 timed out" 0 --only a/two
printf 'args: wrong\nexit 0\n' > "$work/g/a/one.out"
expect mismatch "golden: 1 passed, 1 failed, 0 timed out" 1
printf 'args: %s\nexit 1\n' "$work/g/a/one.ozen" > "$work/g/a/one.out"
expect exit-code-compared "golden: 1 passed, 1 failed, 0 timed out" 1
printf '#!/bin/sh\nsleep 3\n' > "$work/slow.sh"
chmod +x "$work/slow.sh"
cases=$((cases + 1))
got=$(OZP_RUN_TIMEOUT=1 "$here/run-golden.sh" "$work/slow.sh" --dir "$work/g" 2>/dev/null | tail -n 1) || true
if [ "$got" != "golden: 0 passed, 0 failed, 2 timed out" ]; then
  echo "FAIL timeout-env: got '$got'" >&2
  failed=$((failed + 1))
fi
rm "$work/g/a/one.ozen" "$work/g/a/one.out" "$work/g/a/two.ozen" "$work/g/a/two.out"
expect empty-dir "golden: 0 passed, 0 failed, 0 timed out" 1

echo "test-run-golden: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
