#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

expect() {
  local name=$1 want=$2 want_status=$3 got status
  shift 3
  cases=$((cases + 1))
  set +e
  got=$("$here/check-c23-subset.sh" "$@" 2>/dev/null | tail -n 1)
  status=$?
  set -e
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

mkdir -p "$work/clean" "$work/embed" "$work/ckd" "$work/probes"
printf 'int f(void);\nint f(void) { return 0; }\n' > "$work/clean/a.c"
printf 'static const char data[] = {\n#embed "x.bin"\n};\n' > "$work/embed/a.c"
printf '#include <stdckdint.h>\n' > "$work/ckd/a.h"
cp "$here/c23-probes/01-nullptr.c" "$work/probes/"
printf 'unsigned _BitInt(128) x;\n' > "$work/probes/99-bitint.c"

expect clean "c23: 12 allowed probes ok on 2 toolchains, 0 banned uses" 0 "$work/clean"
expect embed "c23: 12 allowed probes ok on 2 toolchains, 1 banned uses" 1 "$work/embed"
expect stdckdint-header "c23: 12 allowed probes ok on 2 toolchains, 1 banned uses" 1 "$work/ckd"
expect failing-probe "c23: 1 allowed probes ok on 2 toolchains, 0 banned uses" 1 --probes "$work/probes" "$work/clean"
expect one-toolchain "c23: 12 allowed probes ok on 1 toolchains, 0 banned uses" 0 --toolchains "gcc:-std=c2x" "$work/clean"

echo "test-check-c23-subset: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
