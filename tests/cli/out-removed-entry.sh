#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/aemono.ozen aemono
recipe recipes/butaJiru.ozen butaJiru
"$ozen" build > /dev/null 2>&1
[ -d out/recipes/butaJiru ] || fail "butaJiru not built"
rm recipes/butaJiru.ozen
"$ozen" build > /dev/null 2>&1
[ ! -e out/recipes/butaJiru ] || fail "stale output of butaJiru remains"
grep -q '"entry":"butaJiru"' out/index.json && fail "index still lists butaJiru"
exit 0
