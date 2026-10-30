#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
file package.ozen <<'OZEN'
package book {
  version: "1.0.0"
  requires: {
    dashi: "1.2"
  }
}
OZEN
file deps/dashi/package.ozen <<'OZEN'
package dashi {
  version: "1.2.5"
}
OZEN
file deps/dashi/stock.ozen <<'OZEN'
ingredient Katsuobushi : Seasoning
OZEN
file deps/dashi/phrases/ja.ftl <<'FTL'
ingredient-katsuobushi = かつお節
FTL
file soup.ozen <<'OZEN'
import dashi.stock.{Katsuobushi}

recipe soup {
  ingredients {
    water: 400 ml
    bonito: Katsuobushi 10 g
  }
  let simmered = simmer([water, bonito], 5 min)
}
OZEN
out=$("$ozen" render --kind recipe --lang ja soup)
want "$?" 0 "render status"
grep -q 'かつお節 10 g' <<< "$out" || fail "dependency phrase missing: $out"
