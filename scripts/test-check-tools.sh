#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-tools.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cases=0
failed=0

expect() {
  local name=$1 file=$2 want_last=$3 want_status=$4 got got_status
  cases=$((cases + 1))
  set +e
  got=$("$checker" --toolchain "$file" 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got" != "$want_last" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $got_status), want '$want_last' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
}

printf 'gcc 0\npython3 0\n' > "$work/names.txt"
"$checker" --toolchain "$work/names.txt" --print > "$work/current.txt"

expect current "$work/current.txt" "tools: 2 checked, 0 mismatched" 0

awk '$1 == "gcc" { n = split($2, v, "."); v[n] = v[n] + 1; s = v[1]; for (i = 2; i <= n; i++) s = s "." v[i]; $2 = s } { print }' \
  "$work/current.txt" > "$work/patch.txt"
expect patch-differs "$work/patch.txt" "tools: 2 checked, 1 mismatched" 1

awk '$1 == "python3" { $2 = $2 ".0" } { print }' "$work/current.txt" > "$work/longer.txt"
expect longer-version "$work/longer.txt" "tools: 2 checked, 1 mismatched" 1

printf '# comment\n\nno-such-tool 1.0\n' > "$work/unknown.txt"
expect unknown-tool "$work/unknown.txt" "tools: 1 checked, 1 mismatched" 1

echo "test-check-tools: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
