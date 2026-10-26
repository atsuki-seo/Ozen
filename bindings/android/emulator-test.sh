#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

[ $# -eq 1 ] || { echo "usage: $0 LIB_DIR" >&2; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
libs=$(cd "$1" && pwd)
native=${BRIDGE_NATIVE:-$root/build/core-gcc-release/ozen}
device=${OZEN_ADB_DIR:-/data/local/tmp/ozen}/jni
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mapfile -t inputs < <(grep -v '^#' "${BRIDGE_INPUTS:-$root/bindings/inputs.txt}")
javac --release 17 -d "$work/classes" "$root/bindings/android/java/ozen/Ozen.java" \
  "$root/bindings/android/test/JvmTest.java" > "$work/javac.log" 2>&1 ||
  { cat "$work/javac.log" >&2; echo "jni-android: javac failed"; exit 1; }
mapfile -t classes < <(find "$work/classes" -name '*.class')
"${D8:-d8}" --min-api 24 --output "$work" "${classes[@]}" > "$work/d8.log" 2>&1 ||
  { cat "$work/d8.log" >&2; echo "jni-android: d8 failed"; exit 1; }
adb shell rm -rf "$device" && adb shell mkdir -p "$device/out" || { echo "jni-android: no device"; exit 1; }
adb push "$work/classes.dex" "$libs/libozen.so" "$libs/libozen_jni.so" "$device/" >/dev/null ||
  { echo "jni-android: push failed"; exit 1; }
args=()
for i in "${!inputs[@]}"; do
  adb shell mkdir -p "$device/in/$i"
  adb push "$root/${inputs[$i]}" "$device/in/$i/" >/dev/null
  args+=("$device/in/$i/$(basename "${inputs[$i]}")")
done
adb shell "cd $device && LD_LIBRARY_PATH=$device dalvikvm -Djava.library.path=$device -cp $device/classes.dex JvmTest $device/out ${args[*]}"
adb pull "$device/out" "$work/" >/dev/null
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
echo "jni-android: ${#inputs[@]} inputs, $mismatches mismatches"
[ "$mismatches" -eq 0 ]
