#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
. "$(dirname "$0")/lib.sh"
version=$("$ozen" --version | sed 's/^ozen //')
"$ozen" init book > out.txt 2>&1 || fail "init failed"
grep -qF "https://github.com/atsuki-seo/Ozen/blob/v$version/reference/ai-guide.md" book/AGENTS.md || fail "no link to v$version"
exit 0
