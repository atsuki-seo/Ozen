#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
mkdir src && cd src || fail "mkdir"
package
recipe soup.ozen soup
"$ozen" bundle -o ../soup.bundle.ozen
want "$?" 0 "bundle status"
cd .. || fail "cd"
mkdir taken
printf 'recipe mine {\n}\n' > taken/soup.ozen
msg=$("$ozen" unbundle soup.bundle.ozen -o taken 2>&1)
want "$?" 2 "overwrite status"
want "$msg" "cli: unbundle would overwrite taken/soup.ozen" "overwrite message"
want "$(cat taken/soup.ozen)" "$(printf 'recipe mine {\n}')" "kept file"
[ -e taken/package.ozen ] && fail "wrote package.ozen next to an existing file"
mkdir fresh
"$ozen" unbundle soup.bundle.ozen -o fresh
want "$?" 0 "fresh status"
[ -f fresh/package.ozen ] && [ -f fresh/soup.ozen ] || fail "fresh files missing"
