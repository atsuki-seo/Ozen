#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

[ $# -ge 1 ] || { echo "usage: $0 DIR..." >&2; exit 2; }
for dir in "$@"; do
  [ -d "$dir" ] || continue
  find "$dir" \( -name expected.out -o -name expect.yaml \) | while IFS= read -r f; do dirname "$f"; done
  find "$dir" -name '*.ozen' | while IFS= read -r f; do
    d=$(dirname "$f")
    inside=0
    while [ "$d" != "$dir" ] && [ "$d" != "." ] && [ "$d" != "/" ]; do
      if [ -f "$d/expected.out" ] || [ -f "$d/expect.yaml" ]; then inside=1; break; fi
      d=$(dirname "$d")
    done
    { [ -f "$dir/expected.out" ] || [ -f "$dir/expect.yaml" ]; } && inside=1
    [ "$inside" -eq 0 ] && printf '%s\n' "$f"
  done
done | LC_ALL=C sort -u
