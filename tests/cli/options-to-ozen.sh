#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
file opts.ozen <<'OZEN'
kitchen today : Options {
  servings: 2
}
OZEN
want "$("$ozen" debug options --options opts.ozen --cooks 2 --cleanup afterMeal --prefer fewerWashes,lessWaiting --strict --allow vagueAmount --deny undercooked)" "$(cat opts.ozen; printf 'kitchen cli : Options {\n  cooks: 2\n  cleanup: afterMeal\n  prefer: [fewerWashes, lessWaiting]\n  strict: on\n  allow: ["vagueAmount"]\n  deny: ["undercooked"]\n}')" "options text"
recipe gohan.ozen gohan
"$ozen" check --format json --emit schedule --options opts.ozen --cooks 2 gohan.ozen | grep -q '"cooks":2,' || fail "merged options not used"
