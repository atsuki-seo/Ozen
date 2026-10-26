#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cat > "$work/toolchain.txt" <<'TXT'
gcc 13.3.0
clang 17.0.0
xcode 26.6 macos
ndk 30.0.16248370 ci
clang 17.0.6 ci
TXT
mkdir -p "$work/ok/tools-linux" "$work/ok/tools-ios" "$work/ok/tools-android" "$work/bad/tools-linux" \
  "$work/bad/tools-wasm" "$work/bad/tools-empty"
printf 'gcc 13.3.0\nclang 17.0.6\n' > "$work/ok/tools-linux/tools.txt"
echo "xcode 26.6" > "$work/ok/tools-ios/tools.txt"
echo "ndk 30.0.16248370" > "$work/ok/tools-android/tools.txt"
printf 'gcc 14.2.0\nclang 17.0.0\n' > "$work/bad/tools-linux/tools.txt"
echo "node 22.0.0" > "$work/bad/tools-wasm/tools.txt"
: > "$work/bad/tools-empty/tools.txt"
cases=0
failed=0
expect() {
  local name=$1 want=$2 status=$3 got st
  shift 3
  cases=$((cases + 1))
  got=$(python3 -B "$here/check-ci-tools.py" "$work/toolchain.txt" "$@" 2>/dev/null)
  st=$?
  got=$(tail -n 1 <<< "$got")
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}
expect all-pinned "ci-tools: 3 jobs, 0 mismatched" 0 "$work"/ok/tools-*/tools.txt
expect version-differs "ci-tools: 1 jobs, 2 mismatched" 1 "$work/bad/tools-linux/tools.txt"
expect not-pinned "ci-tools: 1 jobs, 1 mismatched" 1 "$work/bad/tools-wasm/tools.txt"
expect empty "ci-tools: 1 jobs, 1 mismatched" 1 "$work/bad/tools-empty/tools.txt"
expect missing-file "ci-tools: 1 jobs, 1 mismatched" 1 "$work/bad/tools-none/tools.txt"
echo "test-check-ci-tools: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
