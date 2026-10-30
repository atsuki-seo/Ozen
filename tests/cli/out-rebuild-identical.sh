#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe recipes/aemono.ozen aemono
recipe recipes/butaJiru.ozen butaJiru
"$ozen" build > /dev/null 2>&1
cp -r out first
"$ozen" build > /dev/null 2>&1
diff -r first out > /dev/null || fail "second build differs"
