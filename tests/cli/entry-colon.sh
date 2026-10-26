#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe menus/dinner.ozen dinner
msg=$("$ozen" build menus/dinner.ozen:dinner 2>&1)
want "$?" 2 "status"
want "$msg" "cli: use a module path, not file:name" "message"
[ ! -d out ] || fail "out was written"
