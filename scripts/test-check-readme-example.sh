#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
printf '#!/bin/sh\ntr a-z A-Z < "$1"\n' > "$work/upper.sh"
chmod +x "$work/upper.sh"

expect() {
  local name=$1 want=$2 status=$3 got st
  cases=$((cases + 1))
  set +e
  got=$("$here/check-readme-example.sh" --readme "$work/README.md" --bin "$work/upper.sh" --ozen "$work/upper.sh" \
    2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

printf '# x\n\n```ozen\nabc\n```\n\nprints:\n\n```text\nABC\n```\n' > "$work/README.md"
expect match "readme: 1 examples, 0 mismatches" 0
printf '# x\n\n```ozen\nabc\n```\n\nprints:\n\n```text\nABD\n```\n' > "$work/README.md"
expect mismatch "readme: 1 examples, 1 mismatches" 1
printf '# x\n\n<!-- file: a.ozen -->\n```ozen\nabc\n```\n\n<!-- file: b.ozen -->\n```ozen\ndef\n```\n\n<!-- run: ozen a.ozen -->\n```text\nABC\n```\n' > "$work/README.md"
expect named-files "readme: 1 examples, 0 mismatches" 0
printf '# x\n\n<!-- file: a.ozen -->\n```ozen\nabc\n```\n\n<!-- file: b.ozen -->\n```ozen\ndef\n```\n\n<!-- run: ozen b.ozen -->\n```text\nABC\n```\n' > "$work/README.md"
expect run-other-file "readme: 1 examples, 1 mismatches" 1
printf '# x\n\n<!-- file: a.ozen -->\n```ozen\nabc\n```\n\n```text\nABC\n```\n\n<!-- run: ozen a.ozen -->\n```text\nABC\n```\n' > "$work/README.md"
expect carried-files "readme: 2 examples, 0 mismatches" 0
printf '# x\n\nno examples\n' > "$work/README.md"
expect none "readme: 0 examples, 0 mismatches" 1

echo "test-check-readme-example: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
