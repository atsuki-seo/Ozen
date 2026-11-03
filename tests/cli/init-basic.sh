#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0
root=$(cd "$(dirname "$0")/../.." && pwd)
. "$(dirname "$0")/lib.sh"
version=$("$ozen" --version | sed 's/^ozen //')
out=$("$ozen" init book 2>&1)
want "$?" 0 "init status"
want "$out" "created book/package.ozen
created book/AGENTS.md
If this folder already has a CLAUDE.md, add the line @AGENTS.md to it." "init output"
want "$(cat book/package.ozen)" 'package book {
  version: "0.1.0"
}' "package.ozen"
want "$(cat book/AGENTS.md)" "$(sed "s/@VERSION@/$version/g" "$root/reference/agents-template.md")" "AGENTS.md"
exit 0
