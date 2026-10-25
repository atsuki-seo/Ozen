#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

expect() {
  local name=$1 want=$2 status=$3 got st
  shift 3
  cases=$((cases + 1))
  set +e
  got=$("$here/check-pure.sh" "$@" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

mkdir -p "$work/ok" "$work/io" "$work/assert"
printf '#include <string.h>\n#include "own.h"\nint f(char *d, const char *s) { memcpy(d, s, 4); return g(); }\n' > "$work/ok/a.c"
printf 'int g(void);\n' > "$work/ok/own.h"
printf 'int g(void) { return 1; }\n' > "$work/ok/b.c"
gcc -c "$work/ok/a.c" -o "$work/ok/a.o" && gcc -c "$work/ok/b.c" -o "$work/ok/b.o"
ar rcs "$work/ok/lib.a" "$work/ok/a.o" "$work/ok/b.o"
expect clean "pure: 0 disallowed symbols, 0 disallowed includes" 0 "$work/ok" "$work/ok/lib.a"

printf '#include <stdio.h>\nvoid *h(void) { return fopen("x", "r"); }\n' > "$work/io/c.c"
gcc -c "$work/io/c.c" -o "$work/io/c.o" && ar rcs "$work/io/lib.a" "$work/io/c.o"
expect fopen-and-stdio "pure: 1 disallowed symbols, 1 disallowed includes" 1 "$work/io" "$work/io/lib.a"

printf '#include <assert.h>\nint k(int x) { return x; }\n' > "$work/assert/d.c"
expect assert-header "pure: 0 disallowed symbols, 1 disallowed includes" 1 "$work/assert" "$work/ok/lib.a"

echo "test-check-pure: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
