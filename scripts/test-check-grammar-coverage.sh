#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

printf '#!/bin/sh\ncat "$4"\n' > "$work/ozen"
chmod +x "$work/ozen"
printf '(* a := comment ; *)\nfile := item ;\nitem := name | number ;\nname := IDENT ;\nnumber := NUMBER ;\n' > "$work/g.ebnf"
mkdir -p "$work/golden/a/proj"
printf 'file\nitem\nname\n' > "$work/golden/a/one.ozen"
printf 'number\n' > "$work/golden/a/proj/main.ozen"

expect() {
  local name=$1 want=$2 want_status=$3 got status
  cases=$((cases + 1))
  set +e
  got=$(python3 -B "$here/check-grammar-coverage.py" --ozen "$work/ozen" "$work/g.ebnf" "$work/golden" 2>/dev/null | tail -n 1)
  status=$?
  set -e
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

expect covered "grammar: 4 rules, 0 not covered" 0
printf 'extra := STRING ;\n' >> "$work/g.ebnf"
expect rule-without-golden "grammar: 5 rules, 1 not covered" 1
sed -i '$d' "$work/g.ebnf"
printf 'mystery\n' >> "$work/golden/a/one.ozen"
expect unknown-rule "grammar: 4 rules, 0 not covered" 1

echo "test-check-grammar-coverage: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
