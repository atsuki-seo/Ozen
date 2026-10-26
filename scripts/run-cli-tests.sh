#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

[ $# -eq 2 ] || { echo "usage: $0 OZEN DIR" >&2; exit 2; }
ozen=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
dir=${2%/}
cases=0
failed=0
for t in "$dir"/*.sh; do
  [ "$(basename "$t")" = lib.sh ] && continue
  cases=$((cases + 1))
  if ! out=$(bash "$t" "$ozen" 2>&1); then
    failed=$((failed + 1))
    echo "FAIL $(basename "$t" .sh): $(tail -n 1 <<< "$out")" >&2
  fi
done
echo "cli: $cases cases, $failed failed"
[ "$failed" -eq 0 ] && [ "$cases" -gt 0 ]
