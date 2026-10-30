#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/gohan.ozen gohan
for opt in --kit --cook --serve --ent; do
  msg=$("$ozen" check "$opt" x 2>&1)
  want "$?" 2 "$opt status"
  want "$msg" "cli: unknown option: $opt" "$opt message"
done
