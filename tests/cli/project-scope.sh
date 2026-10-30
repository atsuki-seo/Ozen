#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
file pantry/defs.ozen <<'OZEN'
action rinseTwice(x: T) -> T {
  uses hands for 1 min
}
OZEN
recipe recipes/aemono.ozen aemono
file menus/dinner.ozen <<'OZEN'
import recipes.aemono
menu dinner {
  let side = make aemono
}
OZEN
want "$("$ozen" build 2>&1 | tail -n 1)" "build: 2 entries, 2 passed" "summary"
[ -d out/recipes/aemono ] && [ -d out/menus/dinner ] || fail "entries not written"
[ ! -e out/pantry ] || fail "definitions-only module was built"
rm -rf out
want "$("$ozen" build menus 2>&1 | tail -n 1)" "build: 1 entries, 1 passed" "scoped summary"
[ -d out/menus/dinner ] && [ ! -e out/recipes ] || fail "scope not respected"
