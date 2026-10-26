#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

[ $# -eq 1 ] || { echo "usage: $0 BUILD_DIR" >&2; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(cd "$1" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mapfile -t inputs < <(grep -v '^#' "${BRIDGE_INPUTS:-$root/bindings/inputs.txt}")
native=${BRIDGE_NATIVE:-$build/ozen}
read -r -a swiftc <<< "${BRIDGE_SWIFTC:-swiftc}"
read -r -a run <<< "${BRIDGE_RUN:-}"
"${swiftc[@]}" -O -I "${BRIDGE_MODULE_DIR:-$root/bindings/swift}" -L "$build" -Xlinker -rpath -Xlinker "$build" \
  "$root/bindings/swift/Ozen.swift" "$root/bindings/linux/main.swift" -o "$work/swift-test" > "$work/build.log" 2>&1 ||
  { cat "$work/build.log" >&2; echo "swift: build failed"; exit 1; }
mkdir -p "$work/out"
paths=()
for f in "${inputs[@]}"; do
  paths+=("$root/$f")
done
"${run[@]}" "$work/swift-test" "$work/out" "${paths[@]}"
mismatches=0
for i in "${!inputs[@]}"; do
  f=$root/${inputs[$i]}
  (cd "$(dirname "$f")" && "$native" check --format json --emit ir,schedule,steps,definitions "$(basename "$f")") \
    > "$work/native.json" 2>/dev/null
  if ! cmp -s "$work/native.json" "$work/out/$i.json"; then
    echo "mismatch: ${inputs[$i]}" >&2
    mismatches=$((mismatches + 1))
  fi
done
echo "swift: ${#inputs[@]} inputs, $mismatches mismatches"
[ "$mismatches" -eq 0 ]
