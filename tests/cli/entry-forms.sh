#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe menus/dinner.ozen dinner
file menus/meals.ozen <<'OZEN'
recipe lunch {
  ingredients {
    rice: 300 g
  }
  let washed = wash(rice)
}
recipe brunch {
  ingredients {
    rice: 200 g
  }
  let washed = wash(rice)
}
OZEN
recipe side.ozen lunch
recipe side/lunch.ozen special
check() {
  out=$("$ozen" build "$@" 2>&1 | tail -n 1)
  want "$out" "build: 1 entries, 1 passed" "build $*"
  [ -f "out/$dir/schedule.json" ] || fail "build $*: no out/$dir/schedule.json"
  rm -rf out
}
dir=menus/dinner check menus.dinner
dir=menus/meals/lunch check menus.meals.lunch
dir=menus/dinner check menus/dinner.ozen
dir=menus/meals/brunch check menus/meals.ozen --entry brunch
dir=side check side.lunch
