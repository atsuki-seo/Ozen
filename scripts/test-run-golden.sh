#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

cat > "$work/fake" <<'SH'
#!/bin/sh
echo "args: $*"
echo "cwd: $(basename "$PWD")"
[ "$2" = "--entry" ] && echo "version 9.8.7"
[ "$1" = "slow" ] && sleep 3
[ "$1" = "fail" ] && exit 1
exit 0
SH
chmod +x "$work/fake"
g="$work/golden"
mkdir -p "$g/syntax" "$g/modules/project/sub"
printf 'x\n' > "$g/syntax/plain.ozen"
printf 'args: check plain.ozen\ncwd: syntax\nexit 0\n' > "$g/syntax/plain.out"
printf '// args: fail --flag\n' > "$g/syntax/args.ozen"
printf 'args: fail --flag args.ozen\ncwd: syntax\nexit 1\n' > "$g/syntax/args.out"
printf 'module\n' > "$g/modules/project/main.ozen"
printf 'module\n' > "$g/modules/project/sub/inner.ozen"
printf 'check --entry main\n' > "$g/modules/project/args"
printf 'args: check --entry main\ncwd: project\nversion {{STD_VERSION}}\nexit 0\n' > "$g/modules/project/expected.out"

expect() {
  local name=$1 want=$2 want_status=$3 got status
  shift 3
  cases=$((cases + 1))
  set +e
  got=$("$here/run-golden.sh" "$work/fake" "$g" --std-version 9.8.7 "$@" 2>/dev/null | tail -n 1)
  status=$?
  set -e
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

expect all-pass "golden: 3 passed, 0 failed, 0 timed out" 0
expect only-folder "golden: 1 passed, 0 failed, 0 timed out" 0 --only modules/project
expect only-file "golden: 1 passed, 0 failed, 0 timed out" 0 --only syntax/args
cases=$((cases + 1))
got=$("$here/run-golden.sh" "$work/fake" "$g" --std-version 1.0.0 --only modules/project 2>/dev/null | tail -n 1) || true
[ "$got" = "golden: 0 passed, 1 failed, 0 timed out" ] || { echo "FAIL version-replaced: got '$got'" >&2; failed=$((failed + 1)); }
printf 'args: wrong\nexit 0\n' > "$g/syntax/plain.out"
expect mismatch "golden: 2 passed, 1 failed, 0 timed out" 1
printf '// args: slow\n' > "$g/syntax/plain.ozen"
cases=$((cases + 1))
got=$(OZP_RUN_TIMEOUT=1 "$here/run-golden.sh" "$work/fake" "$g" --std-version 9.8.7 2>/dev/null | tail -n 1) || true
[ "$got" = "golden: 2 passed, 0 failed, 1 timed out" ] || { echo "FAIL timeout: got '$got'" >&2; failed=$((failed + 1)); }
rm -rf "$g"/*
expect empty "golden: 0 passed, 0 failed, 0 timed out" 1

echo "test-run-golden: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
