#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

expect() {
  local name=$1 want=$2 status=$3 got st
  shift 3
  cases=$((cases + 1))
  set +e
  got=$("$here/worker-test.sh" "$@" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

expect real-worker "worker: 4 inputs, 0 mismatches" 0
sed 's/mod._ozpw_expand(ptr, bytes.length)/mod._ozpw_expand(ptr, text.length)/' "$root/proto/wasm/worker.js" > "$work/utf16.js"
expect utf16-length "worker: 4 inputs, 4 mismatches" 1 --worker "$work/utf16.js"
printf 'self.onmessage = () => { throw new Error("broken"); };\n' > "$work/broken.js"
expect broken-worker "worker: 4 inputs, 4 mismatches" 1 --worker "$work/broken.js"

echo "test-worker-test: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
