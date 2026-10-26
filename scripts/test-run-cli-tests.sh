#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
out=$("$root/scripts/run-cli-tests.sh" /bin/true "$root/tests/cli-selftest" 2>&1)
status=$?
failed=0
[ "$(tail -n 1 <<< "$out")" = "cli: 2 cases, 1 failed" ] || failed=$((failed + 1))
[ "$status" -ne 0 ] || failed=$((failed + 1))
grep -q '^FAIL fail: FAIL: always$' <<< "$out" || failed=$((failed + 1))
echo "test-run-cli-tests: 3 cases, $failed failed"
[ "$failed" -eq 0 ]
