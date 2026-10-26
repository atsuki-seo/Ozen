#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$root/build/core-gcc-release}
java_home=${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac)")")")}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/bin" "$work/device" "$work/libs"
cat > "$work/bin/adb" <<'SH'
#!/usr/bin/env bash
[ -d "$FAKE_DEVICE" ] || { echo "error: no devices" >&2; exit 1; }
map() { printf '%s' "${1//\/data\/local\/tmp/$FAKE_DEVICE}"; }
case "$1" in
  shell) shift; sh -c "$(map "$*")" ;;
  push) shift; n=$#; dest=$(map "${!n}"); cp -r "${@:1:n-1}" "$dest" ;;
  pull) cp -r "$(map "$2")" "$3" ;;
esac
SH
cat > "$work/bin/d8" <<'SH'
#!/usr/bin/env bash
out=$4
for c in "${@:5}"; do
  case "$c" in */JvmTest.class) jar cf "$out/classes.dex" -C "$(dirname "$c")" . ;; esac
done
SH
cat > "$work/bin/dalvikvm" <<'SH'
#!/bin/sh
exec java --enable-native-access=ALL-UNNAMED "$@"
SH
chmod +x "$work/bin/"*
cp "$build/libozen.so" "$work/libs/"
gcc -std=c17 -O2 -shared -fPIC -I "$java_home/include" -I "$java_home/include/linux" -I "$root/core/include" \
  "$root/bindings/android/jni/ozen_jni.c" -L "$build" -lozen -o "$work/libs/libozen_jni.so"
echo "bindings/inputs/utf8.ozen" > "$work/inputs.txt"
printf '#!/bin/sh\necho different\n' > "$work/native"
chmod +x "$work/native"
cases=0
failed=0
expect() {
  local name=$1 want=$2 got
  shift 2
  cases=$((cases + 1))
  got=$(env PATH="$work/bin:$PATH" FAKE_DEVICE="$work/device" BRIDGE_INPUTS="$work/inputs.txt" \
    BRIDGE_NATIVE="$build/ozen" "$@" "$root/bindings/android/emulator-test.sh" "$work/libs" 2>/dev/null | tail -n 1)
  [ "$got" = "$want" ] || { echo "FAIL $name: $got" >&2; failed=$((failed + 1)); }
}
expect same "jni-android: 1 inputs, 0 mismatches"
expect native-differs "jni-android: 1 inputs, 1 mismatches" BRIDGE_NATIVE="$work/native"
expect no-device "jni-android: no device" FAKE_DEVICE="$work/missing"
echo "test-emulator-test: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
