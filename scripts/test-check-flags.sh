#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
W="-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror"

db() {
  printf '[{"file": "/x/src/a.c", "command": "%s"}, {"file": "/x/driver/m.c", "command": "gcc"}]\n' "$1" > "$work/cc.json"
}

expect() {
  local name=$1 want=$2 status=$3 got st
  shift 3
  cases=$((cases + 1))
  set +e
  got=$(python3 -B "$here/check-flags.py" --std-gcc c2x --std-clang c23 "$@" "$work/cc.json" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

db "/usr/bin/gcc $W -std=c2x -c a.c"; expect gcc-ok "flags: 1 files, 0 missing" 0
db "/usr/bin/gcc $W -std=gnu2x -c a.c"; expect gnu-extension "flags: 1 files, 1 missing" 1
db "/usr/bin/clang $W -std=c2x -c a.c"; expect clang-wrong-std "flags: 1 files, 1 missing" 1
db "/usr/bin/clang ${W/-Wshadow /} -std=c23 -c a.c"; expect missing-warning "flags: 1 files, 1 missing" 1
db "/usr/bin/gcc $W -std=c2x -c a.c"; expect extra-missing "flags: 1 files, 1 missing" 1 --extra -Wswitch-enum

echo "test-check-flags: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
