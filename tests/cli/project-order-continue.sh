#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
file recipes/a.ozen <<'OZEN'
recipe a() {
  ingredients {
    rice: 300 g
  }
  let w = wash(rice, 2 min)
}
OZEN
recipe recipes/b.ozen b
out=$("$ozen" build 2>&1)
want "$?" 1 "status"
want "$(tail -n 1 <<< "$out")" "build: 2 entries, 1 passed" "summary"
want "$(grep -o '"entry":"[a-z]*","status":"[a-z]*"' out/index.json | tr '\n' ' ')" '"entry":"a","status":"error" "entry":"b","status":"ok" ' "index order"
[ -f out/recipes/b/schedule.json ] || fail "b not built after a failed"
