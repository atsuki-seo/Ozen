#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
file recipes/aemono.ozen <<'OZEN'
recipe aemono {
  ingredients {
    rice: 300 g
  }
  let washed = wash(rice, 2 min)
}
OZEN
recipe recipes/butaJiru.ozen butaJiru
out=$("$ozen" build 2>&1)
want "$?" 1 "status"
want "$(tail -n 1 <<< "$out")" "build: 2 entries, 1 passed" "summary"
want "$(grep -o '"entry":"[a-zA-Z]*","status":"[a-z]*"' out/index.json | tr '\n' ' ')" '"entry":"aemono","status":"error" "entry":"butaJiru","status":"ok" ' "index order"
[ -f out/recipes/butaJiru/schedule.json ] || fail "butaJiru not built after aemono failed"
