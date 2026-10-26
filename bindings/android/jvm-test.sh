#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

[ $# -eq 1 ] || { echo "usage: $0 BUILD_DIR" >&2; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(cd "$1" && pwd)
java_home=${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mapfile -t inputs < <(grep -v '^#' "${BRIDGE_INPUTS:-$root/bindings/inputs.txt}")
native=${BRIDGE_NATIVE:-$build/ozen}
gcc -std=c17 -O2 -shared -fPIC -Wall -Werror -I "$java_home/include" -I "$java_home/include/linux" \
  -I "$root/core/include" "$root/bindings/android/jni/ozen_jni.c" -L "$build" -lozen -Wl,-rpath,"$build" \
  -o "$work/libozen_jni.so" > "$work/build.log" 2>&1 || { cat "$work/build.log" >&2; echo "jni: build failed"; exit 1; }
javac --release 17 -d "$work/classes" "$root/bindings/android/java/ozen/Ozen.java" \
  "$root/bindings/android/test/JvmTest.java" > "$work/javac.log" 2>&1 ||
  { cat "$work/javac.log" >&2; echo "jni: javac failed"; exit 1; }
mkdir -p "$work/out"
paths=()
for f in "${inputs[@]}"; do
  paths+=("$root/$f")
done
java --enable-native-access=ALL-UNNAMED -Djava.library.path="$work" -cp "$work/classes" JvmTest "$work/out" "${paths[@]}"
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
echo "jni: ${#inputs[@]} inputs, $mismatches mismatches"
[ "$mismatches" -eq 0 ]
