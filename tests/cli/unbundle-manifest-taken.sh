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

recipe soup() {
  ingredients {
    water: 400 ml
    bonito: Katsuobushi 10 g
  }
  let s = simmer([water, bonito], 5 min)
}
OZEN
"$ozen" bundle -o ../soup.bundle.ozen || fail "bundle"
cd .. || fail "cd"
mkdir manifest && printf 'package other {\n  version: "9.9.9"\n}\n' > manifest/package.ozen
msg=$("$ozen" unbundle soup.bundle.ozen -o manifest 2>&1)
want "$?" 2 "manifest status"
want "$msg" "cli: unbundle would overwrite manifest/package.ozen" "manifest message"
[ -e manifest/soup.ozen ] && fail "wrote soup.ozen next to a manifest"
mkdir -p dep/deps/dashi && printf 'ingredient Konbu : Seasoning\n' > dep/deps/dashi/stock.ozen
msg=$("$ozen" unbundle soup.bundle.ozen -o dep 2>&1)
want "$?" 2 "dependency status"
want "$msg" "cli: unbundle would overwrite dep/deps/dashi/stock.ozen" "dependency message"
[ -e dep/package.ozen ] && fail "wrote package.ozen next to a dependency file"
want "$(cat dep/deps/dashi/stock.ozen)" "ingredient Konbu : Seasoning" "kept dependency file"
