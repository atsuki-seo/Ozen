#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-spdx.sh"
code='SPDX-License-Identifier: MIT OR Apache-2.0'
docs='SPDX-License-Identifier: CC-BY-4.0'

cases=0
failed=0

new_repo() {
  local dir
  dir=$(mktemp -d)
  git -C "$dir" init -q
  printf '%s\n' "$dir"
}

put() {
  mkdir -p "$(dirname "$1/$2")"
  printf '%s\n' "$3" > "$1/$2"
}

expect() {
  local name=$1 repo=$2 want_summary=$3 want_status=$4 got_summary got_status
  cases=$((cases + 1))
  set +e
  got_summary=$("$checker" --root "$repo" 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got_summary" != "$want_summary" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got_summary' (status $got_status), want '$want_summary' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
  rm -rf "$repo"
}

r=$(new_repo); put "$r" proto/src/a.c "/* $code */"
expect good-first-line "$r" "spdx: 1 files checked, 0 missing, 0 wrong" 0

r=$(new_repo); put "$r" tools/x.py "$(printf '#\n#\n#\n#\n#\n# %s' "$code")"
expect line-six "$r" "spdx: 1 files checked, 1 missing, 0 wrong" 1

r=$(new_repo); put "$r" tools/y.py "$(printf '#\n#\n#\n#\n# %s' "$code")"
expect line-five "$r" "spdx: 1 files checked, 0 missing, 0 wrong" 0

r=$(new_repo); put "$r" spec/vocab/units.yaml "# $docs"
expect wrong-identifier "$r" "spdx: 1 files checked, 0 missing, 1 wrong" 1

r=$(new_repo); put "$r" spec/schema/vocab.schema.json "$(printf '{\n  "$comment": "%s",\n  "type": "object"\n}' "$code")"
expect json-comment "$r" "spdx: 1 files checked, 0 missing, 0 wrong" 0

r=$(new_repo); put "$r" spec/samples/curryRice.yaml "# $code"
expect sample-wrong "$r" "spdx: 1 files checked, 0 missing, 1 wrong" 1

r=$(new_repo); put "$r" core/src/a.c "no header"; put "$r" cli/src/main.c "/* $code */"; put "$r" core/grammar.ebnf "(* $code *)"
expect core-and-cli "$r" "spdx: 3 files checked, 1 missing, 0 wrong" 1

r=$(new_repo); put "$r" proto/README.md "no header"; put "$r" docs/x.c "no header"; put "$r" tools/vendor/lib.py "no header"
expect not-targets "$r" "spdx: 0 files checked, 0 missing, 0 wrong" 0

r=$(new_repo); put "$r" .gitignore "build/"; put "$r" proto/build/gen.c "no header"; put "$r" proto/src/b.c "no header"
expect ignored-and-untracked "$r" "spdx: 1 files checked, 1 missing, 0 wrong" 1

cases=$((cases + 1))
r=$(new_repo); put "$r" scripts/a.sh "# $code"; put "$r" scripts/notes.md "x"; put "$r" spec/samples/s.yaml "# $docs"
listed=$("$checker" --root "$r" --list | sort | tr '\n' ' ')
if [ "$listed" != "scripts/a.sh spec/samples/s.yaml " ]; then
  echo "FAIL list: got '$listed'" >&2
  failed=$((failed + 1))
fi
rm -rf "$r"

echo "test-check-spdx: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
