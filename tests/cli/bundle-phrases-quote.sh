#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
package
recipe soup.ozen soup
file phrases/ja.ftl <<'FTL'
dish-soup = """汁"""
FTL
msg=$("$ozen" bundle -o out.ozen 2>&1)
want "$?" 2 "quote status"
want "$msg" 'cli: phrases/ja.ftl holds """, which a bundle cannot quote' "quote message"
[ -e out.ozen ] && fail "wrote a bundle"
exit 0
