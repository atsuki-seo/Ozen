#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
file menus/meals.ozen <<'OZEN'
recipe lunch() {
  ingredients {
    rice: 300 g
  }
  let w = wash(rice)
}
recipe brunch() {
  ingredients {
    rice: 200 g
  }
  let w = wash(rice)
}
OZEN
msg=$("$ozen" build menus.meals 2>&1)
want "$?" 2 "status"
want "$msg" "cli: several entries; choose one of: brunch, lunch" "message"
