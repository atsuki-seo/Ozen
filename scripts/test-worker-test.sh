#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
echo "bindings/inputs/utf8.ozen" > "$work/inputs.txt"
printf '#!/bin/sh\necho different\n' > "$work/native"
chmod +x "$work/native"
sed 's/mod.HEAPU32\[(table >> 2) + 2 \* n + i\] = text.length;/mod.HEAPU32[(table >> 2) + 2 * n + i] = s.text.length;/' \
  "$root/bindings/wasm/ozen.mjs" > "$work/utf16.mjs"
cases=0
failed=0
expect() {
  local name=$1 want=$2 got
  shift 2
  cases=$((cases + 1))
  got=$(env BRIDGE_INPUTS="$work/inputs.txt" "$@" "$root/bindings/wasm/worker-test.sh" 2>/dev/null | tail -n 1)
  [ "$got" = "$want" ] || { echo "FAIL $name: $got" >&2; failed=$((failed + 1)); }
}
expect same "worker: 1 inputs, 0 mismatches"
expect native-differs "worker: 1 inputs, 1 mismatches" BRIDGE_NATIVE="$work/native"
cmp -s "$root/bindings/wasm/ozen.mjs" "$work/utf16.mjs" && { echo "FAIL utf16: mutation did not apply" >&2; failed=$((failed + 1)); }
expect utf16-length "worker: 1 inputs, 1 mismatches" BRIDGE_JS="$work/utf16.mjs"
echo "test-worker-test: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
