#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
mkdir src && cd src || fail "mkdir"
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
file phrases/ja.ftl <<'FTL'
dish-soup = すまし汁
FTL
file phrases/en.ftl <<'FTL'
dish-soup = Clear soup
FTL
file deps/dashi/phrases/ja.ftl <<'FTL'
ingredient-katsuobushi = かつお節
FTL
"$ozen" bundle -o ../soup.bundle.ozen
want "$?" 0 "bundle status"
cd .. || fail "cd"
mkdir back
"$ozen" unbundle soup.bundle.ozen -o back
want "$?" 0 "unbundle status"
for f in phrases/ja.ftl phrases/en.ftl deps/dashi/phrases/ja.ftl; do
  cmp -s "src/$f" "back/$f" || fail "$f differs"
done
