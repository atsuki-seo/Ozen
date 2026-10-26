#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
fixtures=$root/tools/jsoncheck/fixtures
check=$root/tools/jsoncheck/validate.py
cases=0
failed=0
for f in "$fixtures"/ok.json "$fixtures"/bad-*.json; do
  cases=$((cases + 1))
  out=$(python3 -B "$check" --schema "$fixtures/test.schema.json" "$f" 2>&1 | tail -n 1)
  case "$(basename "$f")" in
    ok.json) want="sarif: 1 files, 0 invalid" ;;
    *) want="sarif: 1 files, 1 invalid" ;;
  esac
  if [ "$out" != "$want" ]; then
    echo "FAIL $(basename "$f"): got '$out'" >&2
    failed=$((failed + 1))
  fi
done
cases=$((cases + 1))
if [ "$(python3 -B "$check" "$root/schemas" "$fixtures" 2>&1 | tail -n 1)" != "json: 0 files, 0 invalid" ]; then
  echo "FAIL suffix: files without the result suffix were checked" >&2
  failed=$((failed + 1))
fi
echo "test-validate: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
