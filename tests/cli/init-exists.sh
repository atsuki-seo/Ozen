#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
mkdir book
printf 'mine\n' > book/AGENTS.md
"$ozen" init book > out.txt 2>&1
want "$?" 1 "init status with AGENTS.md"
[ -e book/package.ozen ] && fail "wrote package.ozen"
want "$(cat book/AGENTS.md)" mine "AGENTS.md kept"
rm book/AGENTS.md
printf 'package mine {}\n' > book/package.ozen
"$ozen" init book > out.txt 2>&1
want "$?" 1 "init status with package.ozen"
[ -e book/AGENTS.md ] && fail "wrote AGENTS.md"
exit 0
