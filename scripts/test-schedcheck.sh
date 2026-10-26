#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
fixtures=$root/tools/schedcheck/fixtures
check=$root/tools/schedcheck/schedcheck.py
cases=0
failed=0

out=$(python3 -B "$check" "$fixtures/ok.json" 2>&1)
cases=$((cases + 1))
if [ "$(tail -n 1 <<< "$out")" != "schedcheck: 1 schedules, 0 violations" ]; then
  echo "FAIL ok: $out" >&2
  failed=$((failed + 1))
fi
for f in "$fixtures"/bad-*.json; do
  name=$(basename "$f" .json)
  name=${name#bad-}
  name=${name%%-*}
  out=$(python3 -B "$check" "$f" 2>&1)
  cases=$((cases + 1))
  kinds=$(sed -nE 's/^violation: [^:]+: ([a-z]+): .*/\1/p' <<< "$out" | sort -u | tr '\n' ' ')
  if [ "$kinds" != "$name " ] || grep -q ', 0 violations$' <<< "$out"; then
    echo "FAIL $name: got kinds '$kinds'" >&2
    failed=$((failed + 1))
  fi
done
echo "test-schedcheck: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
