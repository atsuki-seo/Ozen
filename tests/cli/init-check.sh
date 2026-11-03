#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
"$ozen" init book > out.txt 2>&1 || fail "init failed"
out=$(cd book && "$ozen" check package.ozen 2>&1)
grep -q ' errors=0 ' <<< "$out" || fail "check: $out"
exit 0
