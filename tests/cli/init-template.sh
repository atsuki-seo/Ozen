#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
"$ozen" init book > out.txt 2>&1 || fail "init failed"
grep -qF 'ozen check --format json' book/AGENTS.md || fail "no check command"
grep -qF 'ozen render --kind recipe' book/AGENTS.md || fail "no render command"
exit 0
