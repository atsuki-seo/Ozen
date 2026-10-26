#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/r.ozen r
for k in home:kitchens/home.ozen away:kitchens/other.ozen twin:kitchens/dup1.ozen twin:kitchens/dup2.ozen; do
  file "${k#*:}" <<OZEN
kitchen ${k%%:*} {
  places {
    stove: IHStove
  }
}
OZEN
done
level() {
  "$ozen" check --format json "$@" 2>/dev/null | grep -o '"level":[0-9]'
}
want "$(level -k home)" '"level":2' "by name"
want "$(level -k kitchens/other.ozen)" '"level":2' "by path"
msg=$("$ozen" check -k twin 2>&1)
want "$?" 2 "ambiguous status"
want "$msg" "cli: kitchen is ambiguous: twin (kitchens/dup1.ozen, kitchens/dup2.ozen)" "ambiguous message"
msg=$("$ozen" check -k nowhere 2>&1)
want "$?" 2 "missing status"
want "$msg" "cli: kitchen not found: nowhere" "missing message"
