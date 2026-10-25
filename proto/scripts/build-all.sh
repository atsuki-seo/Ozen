#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
only=""
while [ $# -gt 0 ]; do
  case "$1" in
    --only) only="$2"; shift 2 ;;
    *) echo "usage: $0 [--only CONFIG]" >&2; exit 2 ;;
  esac
done

configs="gcc-debug-san clang-debug-san gcc-release clang-release"
[ -n "$only" ] && configs="$only"
jobs=$(nproc 2>/dev/null || echo 4)
total=0
failed=0
for cfg in $configs; do
  total=$((total + 1))
  case "$cfg" in
    gcc-*) cc=gcc ;;
    clang-*) cc=clang ;;
    *) echo "unknown config $cfg" >&2; failed=$((failed + 1)); continue ;;
  esac
  case "$cfg" in
    *-debug-san) type=Debug; san=ON ;;
    *) type=Release; san=OFF ;;
  esac
  dir="$root/build/$cfg"
  mkdir -p "$dir"
  if cmake -S "$root/proto" -B "$dir" -DCMAKE_C_COMPILER="$cc" -DCMAKE_BUILD_TYPE="$type" \
       -DOZP_SANITIZE="$san" >"$dir/configure.log" 2>&1 &&
     cmake --build "$dir" -j "$jobs" >"$dir/build.log" 2>&1; then
    :
  else
    echo "build failed: $cfg (see build/$cfg/build.log)" >&2
    tail -n 20 "$dir/build.log" "$dir/configure.log" >&2 2>/dev/null
    failed=$((failed + 1))
  fi
done
echo "build: $total configs, $failed failed"
[ "$failed" -eq 0 ]
