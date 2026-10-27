#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
file soup.ozen <<'OZEN'
recipe soup() {
  ingredients {
    onion: 2 pcs
    water: 400 ml
  }
  let c = cut(peel(onion), slice, 3 mm)
  let s = simmer([water, c], 15 min)
}
OZEN
file phrases/ja.ftl <<'FTL'
dish-soup = 玉ねぎのスープ
FTL
html=$("$ozen" render --lang ja soup)
want "$?" 0 "html status"
grep -q '<title>玉ねぎのスープ</title>' <<< "$html" || fail "html lacks the project title"
grep -q '玉ねぎ' <<< "$(sed -n '/<ul>/,$p' <<< "$html")" || fail "html lacks std phrases"
"$ozen" render --kind recipe --lang en -o recipe.txt soup
want "$?" 0 "recipe status"
grep -qi 'onion' recipe.txt || fail "recipe text lacks the onion"
grep -q '<form' <<< "$("$ozen" render --kind settings --lang en soup)" || fail "settings lacks a form"
msg=$("$ozen" render --kind poster soup 2>&1)
want "$?" 2 "unknown kind status"
grep -q 'unknown kind' <<< "$msg" || fail "unknown kind message: $msg"
msg=$(OZEN_RENDERER=/nonexistent/cli.mjs "$ozen" render soup 2>&1)
want "$?" 2 "missing renderer status"
want "$msg" "cli: renderer not found: /nonexistent/cli.mjs" "missing renderer message"
