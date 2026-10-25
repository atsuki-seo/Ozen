#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
build="$root/build"
toolchain="$root/toolchain.txt"
configs="gcc-debug-san clang-debug-san gcc-release clang-release"
while [ $# -gt 0 ]; do
  case "$1" in
    --build) build="$2"; shift 2 ;;
    --toolchain) toolchain="$2"; shift 2 ;;
    --configs) configs="$2"; shift 2 ;;
    *) echo "usage: $0 [--build DIR] [--toolchain FILE] [--configs LIST]" >&2; exit 2 ;;
  esac
done

checked=0
mismatched=0
for cfg in $configs; do
  checked=$((checked + 1))
  case "$cfg" in
    *gcc-*) id=GNU; tool=gcc ;;
    *clang-*) id=Clang; tool=clang ;;
  esac
  want=$(awk -v t="$tool" '$1 == t { print $2 }' "$toolchain")
  file=$(ls "$build/$cfg"/CMakeFiles/*/CMakeCCompiler.cmake 2>/dev/null | head -n 1)
  got_id=$(sed -nE 's/^set\(CMAKE_C_COMPILER_ID "([^"]*)"\)$/\1/p' "$file" 2>/dev/null)
  got_ver=$(sed -nE 's/^set\(CMAKE_C_COMPILER_VERSION "([^"]*)"\)$/\1/p' "$file" 2>/dev/null)
  if [ "$got_id" != "$id" ] || [ -z "$want" ] || [ "$got_ver" != "$want" ]; then
    echo "mismatch: $cfg uses ${got_id:-?} ${got_ver:-?}, want $id $want" >&2
    mismatched=$((mismatched + 1))
  fi
done
echo "compilers: $checked configs, $mismatched mismatched"
[ "$mismatched" -eq 0 ]
