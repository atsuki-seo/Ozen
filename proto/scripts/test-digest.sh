#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
mkdir -p "$work/golden/a" "$work/corpus"
printf 'one\n' > "$work/golden/a/x.ozen"
printf '// args: --flag\ntwo\n' > "$work/golden/a/y.ozen"
printf 'three\n' > "$work/corpus/c1"
printf '#!/bin/sh\nfor a; do f=$a; done\nprintf "%%s|" "$@"; cat "$f"\n' > "$work/run.sh"
chmod +x "$work/run.sh"

d() { "$here/digest.sh" "$work/run.sh" --golden "$work/golden" --corpus "$work/corpus"; }

base=$(d)
check() {
  local name=$1 cond=$2
  cases=$((cases + 1))
  if ! eval "$cond"; then
    echo "FAIL $name" >&2
    failed=$((failed + 1))
  fi
}
check stable '[ "$(d)" = "$base" ]'
check hex '[[ $base =~ ^[0-9a-f]{64}$ ]]'
printf 'ONE\n' > "$work/golden/a/x.ozen"; check golden-input-changes '[ "$(d)" != "$base" ]'; printf 'one\n' > "$work/golden/a/x.ozen"
printf 'THREE\n' > "$work/corpus/c1"; check corpus-input-changes '[ "$(d)" != "$base" ]'; printf 'three\n' > "$work/corpus/c1"
printf '// args: --other\ntwo\n' > "$work/golden/a/y.ozen"; check args-change '[ "$(d)" != "$base" ]'
printf '// args: --flag\ntwo\n' > "$work/golden/a/y.ozen"; check restored '[ "$(d)" = "$base" ]'

echo "test-digest: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
