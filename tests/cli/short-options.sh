#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/r.ozen r
file kitchens/home.ozen <<'OZEN'
kitchen home {
  places {
    stove: IHStove
  }
}
OZEN
want "$("$ozen" debug options -c 2 -s 3 -t "60 min")" "$(printf 'kitchen cli : Options {\n  servings: 3\n  cooks: 2\n  serveAt: 60 min\n}')" "options"
want "$("$ozen" check --format json -k home -c 2 2>/dev/null | grep -o '"level":[0-9]')" '"level":2' "-k"
msg=$("$ozen" check -e r 2>&1)
want "$?" 2 "unknown short option status"
want "$msg" "cli: unknown option: -e" "unknown short option"
