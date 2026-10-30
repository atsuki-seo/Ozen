#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

kind() {
  local name=$1 supers=$2 props=$3
  printf '{"kind":"kind","name":"%s","module":"std.prelude","supers":[%s],"members":[{"section":"props","members":[%s]}]}' \
    "$name" "$supers" "$props"
}

OVEN='{"name":"Place"},{"name":"HeatSource","alts":[[{"bare":"oven"}]]}'
STOVE='{"name":"Place"},{"name":"HeatSource","alts":[[{"bare":"stovetop"}]]}'
TIME='{"field":"preheatTime"}'
WIPE='{"field":"wipeTime"}'

expect() {
  local name=$1 want=$2 want_status=$3 defs=$4 got status
  cases=$((cases + 1))
  printf '#!/bin/sh\ncat <<'"'"'JSON'"'"'\n{"definitions":[%s]}\nJSON\n' "$defs" > "$work/ozen"
  chmod +x "$work/ozen"
  got=$(python3 -B "$here/check-std-preheat.py" --ozen "$work/ozen" 2>/dev/null)
  status=$?
  got=$(tail -n 1 <<< "$got")
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

expect all "oven kinds: 2, with preheatTime: 2" 0 \
  "$(kind Oven "$OVEN" "$TIME"),$(kind OvenRange "$OVEN" "$WIPE,$TIME"),$(kind IHStove "$STOVE" "$WIPE")"
expect missing "oven kinds: 2, with preheatTime: 1" 1 \
  "$(kind Oven "$OVEN" "$TIME"),$(kind OvenRange "$OVEN" "$WIPE")"
expect inherited "oven kinds: 2, with preheatTime: 2" 0 \
  "$(kind Oven "$OVEN" "$TIME"),$(kind ConvectionOven '{"name":"Oven"}' "$WIPE")"
expect none "oven kinds: 0, with preheatTime: 0" 1 "$(kind IHStove "$STOVE" "$WIPE")"
expect stove-only-time "oven kinds: 1, with preheatTime: 0" 1 \
  "$(kind Oven "$OVEN" "$WIPE"),$(kind IHStove "$STOVE" "$TIME")"

echo "test-check-std-preheat: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
