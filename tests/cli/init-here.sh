#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
mkdir cookbook
cd cookbook || exit 1
out=$("$ozen" init 2>&1)
want "$?" 0 "init status"
want "$(head -n 1 package.ozen)" 'package cookbook {' "package line"
[ -f AGENTS.md ] || fail "no AGENTS.md"
want "$(ls -A | sort | tr '\n' ' ')" "AGENTS.md package.ozen " "files"
grep -q '^created package.ozen$' <<< "$out" || fail "output: $out"
exit 0
