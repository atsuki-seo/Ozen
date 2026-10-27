#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
recipe r.ozen r
"$ozen" check r.ozen > out.txt 2>&1
want "$?" 0 "check"
"$ozen" build r.ozen > out.txt 2>&1
want "$?" 0 "build"
"$ozen" debug modules > /dev/null 2>&1
want "$?" 2 "debug without a project"
usage=$("$ozen" 2>&1)
for sub in check build render bundle unbundle debug; do
  grep -q "ozen $sub" <<< "$usage" || fail "usage lacks $sub"
done
