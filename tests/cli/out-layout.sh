#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/curry.ozen curry
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
"$ozen" build --format sarif > /dev/null 2>&1
want "$?" 0 "status"
want "$(cd out && find . -type f | LC_ALL=C sort | tr '\n' ' ')" "./index.json ./menus/meals/brunch/diagnostics.json ./menus/meals/brunch/diagnostics.sarif ./menus/meals/brunch/ir.txt ./menus/meals/brunch/schedule.json ./menus/meals/lunch/diagnostics.json ./menus/meals/lunch/diagnostics.sarif ./menus/meals/lunch/ir.txt ./menus/meals/lunch/schedule.json ./recipes/curry/diagnostics.json ./recipes/curry/diagnostics.sarif ./recipes/curry/ir.txt ./recipes/curry/schedule.json " "tree"
want "$(cat out/index.json)" '{"schemaVersion":1,"entries":[{"module":"menus.meals","entry":"brunch","status":"ok","errors":0,"warnings":0,"infos":2,"dir":"menus/meals/brunch"},{"module":"menus.meals","entry":"lunch","status":"ok","errors":0,"warnings":0,"infos":2,"dir":"menus/meals/lunch"},{"module":"recipes.curry","entry":"curry","status":"ok","errors":0,"warnings":0,"infos":2,"dir":"recipes/curry"}]}' "index"
grep -q '^ir curry$' out/recipes/curry/ir.txt || fail "ir.txt"
grep -q '"schedules":\[{"name":"curry"' out/recipes/curry/schedule.json || fail "schedule.json"
