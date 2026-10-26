#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$root/build/core-gcc-release}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
echo "bindings/inputs/utf8.ozen" > "$work/inputs.txt"
printf '#!/bin/sh\necho different\n' > "$work/native"
chmod +x "$work/native"
cases=0
failed=0
for t in "linux/swift-test.sh swift" "android/jvm-test.sh jni"; do
  set -- $t
  cases=$((cases + 2))
  got=$(BRIDGE_INPUTS="$work/inputs.txt" "$root/bindings/$1" "$build" 2>/dev/null | tail -n 1)
  [ "$got" = "$2: 1 inputs, 0 mismatches" ] || { echo "FAIL $2 same: $got" >&2; failed=$((failed + 1)); }
  got=$(BRIDGE_INPUTS="$work/inputs.txt" BRIDGE_NATIVE="$work/native" "$root/bindings/$1" "$build" 2>/dev/null | tail -n 1)
  [ "$got" = "$2: 1 inputs, 1 mismatches" ] || { echo "FAIL $2 different: $got" >&2; failed=$((failed + 1)); }
done
echo "test-bridge-tests: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
