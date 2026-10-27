#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/json" "$work/empty"
(cd "$root/renderers/test/inputs" && "$root/build/core-gcc-release/ozen" check --format json \
  --emit ir,schedule,steps,definitions,types small.ozen) > "$work/json/small.json"
node "$root/renderers/tools/bundle.mjs" "$work/bundle.js" > /dev/null
{ echo "require('node:fs');"; cat "$work/bundle.js"; } > "$work/node-api.js"
sed 's/heading-ingredients/heading-steps/' "$work/bundle.js" > "$work/altered.js"
cases=0
failed=0
expect() {
  local name=$1 want=$2 got
  shift 2
  cases=$((cases + 1))
  got=$(env BROWSER_JSON="$work/json" "$@" "$root/renderers/tools/browser-test.sh" 2>/dev/null | tail -n 1)
  [ "$got" = "$want" ] || { echo "FAIL $name: $got" >&2; failed=$((failed + 1)); }
}
expect same "browser: 1 inputs, 0 mismatches"
expect node-api "browser: 1 inputs, 1 mismatches" BROWSER_BUNDLE="$work/node-api.js"
expect altered "browser: 1 inputs, 1 mismatches" BROWSER_BUNDLE="$work/altered.js"
expect no-inputs "browser: 0 inputs, 0 mismatches" BROWSER_JSON="$work/empty"
cases=$((cases + 1))
BROWSER_JSON="$work/empty" "$root/renderers/tools/browser-test.sh" > /dev/null 2>&1 && { echo "FAIL no-inputs status" >&2; failed=$((failed + 1)); }
echo "test-browser-test: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
