#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/gohan.ozen gohan
file kitchens/home.ozen <<'OZEN'
kitchen home {
  places {
    stove: IHStove
  }
}
OZEN
want "$("$ozen" check --format json 2>/dev/null | grep -o '"level":[0-9]')" '"level":0' "level without -k"
