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
  got=$(python3 -B "$here/check-noglobal.py" "$@" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

printf 'const char *const names[] = {"a", "b"};\nconst char *pick(int i) { return names[i]; }\n' > "$work/ro.c"
gcc -O2 -fPIC -c "$work/ro.c" -o "$work/ro.o" && ar rcs "$work/ro.a" "$work/ro.o"
objdump -h "$work/ro.o" | grep -q 'data.rel.ro' || { echo "FAIL setup: no .data.rel.ro" >&2; exit 1; }
expect relro-only "noglobal: 1 objects, 0 writable sections" 0 "$work/ro.a"

printf 'static int n;\nint next(void) { return ++n; }\n' > "$work/bss.c"
gcc -O2 -c "$work/bss.c" -o "$work/bss.o" && ar rcs "$work/bss.a" "$work/bss.o" "$work/ro.o"
expect static-counter "noglobal: 2 objects, 1 writable sections" 1 "$work/bss.a"

printf 'int counter = 5;\nint get(void) { return counter++; }\n' > "$work/data.c"
gcc -O2 -c "$work/data.c" -o "$work/data.o" && ar rcs "$work/data.a" "$work/data.o"
expect initialized-global "noglobal: 1 objects, 1 writable sections" 1 "$work/data.a"

echo "test-check-noglobal: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
