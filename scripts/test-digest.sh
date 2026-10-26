#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

printf '#!/bin/sh\necho "$* in $(basename "$PWD")"\n' > "$work/a"
printf '#!/bin/sh\necho "$* in $(basename "$PWD")"\nexit 1\n' > "$work/b"
printf '#!/bin/sh\necho "$* in $(basename "$PWD")"\necho extra\n' > "$work/c"
chmod +x "$work/a" "$work/b" "$work/c"
mkdir -p "$work/g/x/proj" "$work/s/one"
printf 'x\n' > "$work/g/x/one.ozen"
printf '// args: debug ast\n' > "$work/g/x/two.ozen"
printf 'm\n' > "$work/g/x/proj/main.ozen"
: > "$work/g/x/proj/expected.out"
printf 'r\n' > "$work/s/one/main.ozen"

d() { "$here/digest.sh" "$@"; }
check() {
  cases=$((cases + 1))
  if ! eval "$2"; then
    echo "FAIL $1" >&2
    failed=$((failed + 1))
  fi
}

base=$(d "$work/a" "$work/g" "$work/s")
check stable '[ "$base" = "$(d "$work/a" "$work/g" "$work/s")" ]'
check hex '[[ $base =~ ^[0-9a-f]{64}$ ]]'
check exit-code-counts '[ "$base" != "$(d "$work/b" "$work/g" "$work/s")" ]'
check output-counts '[ "$base" != "$(d "$work/c" "$work/g" "$work/s")" ]'
check second-dir-counts '[ "$base" != "$(d "$work/a" "$work/g")" ]'
printf 'y\n' > "$work/g/x/proj/extra.ozen"
check project-files-not-entries '[ "$base" = "$(d "$work/a" "$work/g" "$work/s")" ]'
mkdir -p "$work/t/curry"
printf 'm\n' > "$work/t/curry/main.ozen"
printf 'k\n' > "$work/t/curry/kitchen.ozen"
: > "$work/t/curry/expect.yaml"
printf 'check --kitchen home\n' > "$work/t/curry/args"
check sample-folder-one-entry '[ "$("$here/golden-entries.sh" "$work/t")" = "$work/t/curry" ]'
check sample-args-used '[ "$(d "$work/a" "$work/t")" = "$(printf "%s\ncheck --kitchen home in curry\nexit 0\n" "$work/t/curry" | sha256sum | cut -d" " -f1)" ]'

echo "test-digest: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
