#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
printf '#!/bin/sh\nfor a; do f=$a; done\necho "error ABC001 bad $f:1:1-1:2 args=[\\"x\\"]: bad thing"\ntr a-z A-Z < "$f"\n' > "$work/fake.sh"
chmod +x "$work/fake.sh"
mkdir -p "$work/root/tests/golden/g"
printf '// args: debug steps\nrecipe pie {\n  let a = x |> f\n  let b = a |> g\n}\n' > "$work/root/tests/golden/g/pie.ozen"
printf 'line one: here\nerror ABC001 bad pie.ozen:3:1-3:2 args=["a"]: bad thing\nresult error errors=1\nexit 1\n' \
  > "$work/root/tests/golden/g/pie.out"

expect() {
  local name=$1 want=$2 status=$3 got st
  cases=$((cases + 1))
  set +e
  got=$(python3 -B "$here/check-site-examples.py" --ozen "$work/fake.sh" --root "$work/root" "$work/page.html" \
    2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

code() { printf '<span class="file">%s</span>\n<pre>%s</pre>\n' "$1" "$2"; }
out() { printf '<span class="file">%s</span>\n<pre class="out">%s</pre>\n' "$1" "$2"; }
bare() { printf '<pre class="out">%s</pre>\n' "$1"; }

{ code 'pie.ozen（抜粋）' '  <b>let</b> b = a |&gt; g'; bare 'error ABC001 bad
  3:1-3:2 args=["a"]'; } > "$work/page.html"
expect golden-excerpt "site-examples: 2 examples, 0 mismatched, 0 unmapped" 0

{ code 'pie.ozen（抜粋）' '  let b = a |&gt; h'; bare 'result error errors=1'; } > "$work/page.html"
expect golden-code-mismatch "site-examples: 2 examples, 1 mismatched, 0 unmapped" 1

{ code 'pie.ozen' 'let b = a |&gt; g'; bare 'error ABC001 bad
  3:1-3:3 args=["a"]'; } > "$work/page.html"
expect golden-output-mismatch "site-examples: 2 examples, 1 mismatched, 0 unmapped" 1

{ code 'pie.ozen' 'let b = a |&gt; g'; out 'ozen check pie.ozen' 'result error errors=1'; } > "$work/page.html"
expect golden-other-args "site-examples: 2 examples, 1 mismatched, 0 unmapped" 1

{ code 'pie.ozen' 'let b = a |&gt; g'; out 'ozen debug steps pie.ozen' 'line one'; } > "$work/page.html"
expect golden-same-args "site-examples: 2 examples, 0 mismatched, 0 unmapped" 0

{ code 'tart.ozen' 'let c = d
  -&gt; e'; out 'ozen check tart.ozen' 'error ABC001 bad
  1:1-1:2 args=["x"]
LET C = D -&gt; E'; } > "$work/page.html"
expect self-run "site-examples: 2 examples, 0 mismatched, 0 unmapped" 0

{ code 'tart.ozen' 'let c = d'; out 'ozen check tart.ozen' 'LET C = Q'; } > "$work/page.html"
expect self-run-mismatch "site-examples: 2 examples, 1 mismatched, 0 unmapped" 1

{ code 'tart.ozen' 'let c = d'; out 'd |&gt; q にすると' 'LET C = D |&gt; Q'; } > "$work/page.html"
expect variant "site-examples: 2 examples, 0 mismatched, 0 unmapped" 0

{ code 'tart.ozen（抜粋）' 'let c = d'; bare 'LET C = D'; } > "$work/page.html"
expect excerpt-without-source "site-examples: 2 examples, 0 mismatched, 2 unmapped" 1

{ code 'tart.ozen' 'let c = d'; out 'ozen check cake.ozen' 'LET C = D'; } > "$work/page.html"
expect output-of-unknown-file "site-examples: 2 examples, 0 mismatched, 1 unmapped" 1

printf '<p>no examples</p>\n' > "$work/page.html"
expect none "site-examples: 0 examples, 0 mismatched, 0 unmapped" 1

echo "test-check-site-examples: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
