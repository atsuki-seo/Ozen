#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
printf 'gcc 13.3.0\nclang 17.0.0\n' > "$work/toolchain.txt"

fake() {
  mkdir -p "$work/b/$1/CMakeFiles/3.28.3"
  printf 'set(CMAKE_C_COMPILER_ID "%s")\nset(CMAKE_C_COMPILER_VERSION "%s")\n' "$2" "$3" \
    > "$work/b/$1/CMakeFiles/3.28.3/CMakeCCompiler.cmake"
}

expect() {
  local name=$1 want=$2 status=$3 got st
  cases=$((cases + 1))
  set +e
  got=$("$here/check-compilers.sh" --build "$work/b" --toolchain "$work/toolchain.txt" --configs "gcc-release clang-release" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

fake gcc-release GNU 13.3.0; fake clang-release Clang 17.0.0
expect matching "compilers: 2 configs, 0 mismatched" 0
fake clang-release GNU 13.3.0
expect clang-built-by-gcc "compilers: 2 configs, 1 mismatched" 1
fake clang-release Clang 17.0.1
expect other-version "compilers: 2 configs, 1 mismatched" 1

echo "test-check-compilers: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
