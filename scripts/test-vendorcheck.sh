#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
fx=$root/tools/vendor-fixtures
cases=0
failed=0
cases=$((cases + 1))
[ "$(python3 -B "$root/tools/vendorcheck.py" "$fx/ok/README.md" 2>&1 | tail -n 1)" = "vendor: 1 files, 0 mismatched" ] ||
  { echo "FAIL ok" >&2; failed=$((failed + 1)); }
cases=$((cases + 1))
[ "$(python3 -B "$root/tools/vendorcheck.py" "$fx/bad/README.md" 2>&1 | tail -n 1)" = "vendor: 1 files, 1 mismatched" ] ||
  { echo "FAIL bad" >&2; failed=$((failed + 1)); }
echo "test-vendorcheck: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
