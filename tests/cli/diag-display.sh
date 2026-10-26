#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
file bad.ozen <<'OZEN'
recipe r() {
  ingredients {
    onion: 1 pcs
  }
  let s = cut(onion, style: slice)
}
OZEN
out=$("$ozen" check bad.ozen 2>&1)
want "$?" 1 "status"
want "$out" "$(printf 'error TYP006 stateRequired bad.ozen:5:15-5:20 args=["cut", "x", "onion"]: state required\nresult error errors=1 warnings=0 infos=0')" "display"
