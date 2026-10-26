#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/a.ozen a
recipe recipes/b.ozen b
"$ozen" build > /dev/null 2>&1
[ -d out/recipes/b ] || fail "b not built"
rm recipes/b.ozen
"$ozen" build > /dev/null 2>&1
[ ! -e out/recipes/b ] || fail "stale output of b remains"
grep -q '"entry":"b"' out/index.json && fail "index still lists b"
exit 0
