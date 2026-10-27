#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
file stew.ozen <<'OZEN'
recipe stew() {
  ingredients {
    water: 400 ml
    fish: Buri 300 g
  }
  let s = simmer([water, fish], 10 min)
}
OZEN
grep -q 'ブリ 300 g' <<< "$("$ozen" render --kind recipe --lang ja stew.ozen)" || fail "default stage is not the adult name"
out=$("$ozen" render --kind recipe --lang ja --stage young --dialect kansai stew.ozen)
want "$?" 0 "stage status"
grep -q 'ハマチ 300 g' <<< "$out" || fail "young kansai name missing: $out"
grep -q 'イナダ 300 g' <<< "$("$ozen" render --kind recipe --lang ja --stage young stew.ozen)" || fail "young kanto name missing"
