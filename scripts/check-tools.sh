#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 [--toolchain FILE] [--print]" >&2
  exit 2
}

toolchain=""
print=0
while [ $# -gt 0 ]; do
  case "$1" in
    --toolchain) [ $# -ge 2 ] || usage; toolchain="$2"; shift 2 ;;
    --print) print=1; shift ;;
    *) usage ;;
  esac
done
if [ -z "$toolchain" ]; then
  toolchain="$(git rev-parse --show-toplevel)/toolchain.txt"
fi
[ -f "$toolchain" ] || { echo "no such file: $toolchain" >&2; exit 2; }

emcc_bin() {
  if command -v emcc >/dev/null 2>&1; then
    command -v emcc
  else
    echo "${EMSDK:-$HOME/.local/share/emsdk}/upstream/emscripten/emcc"
  fi
}

pymod() {
  python3 -c "import importlib.metadata as m; print(m.version('$1'))" 2>/dev/null
}

probe() {
  case "$1" in
    gcc) gcc -dumpfullversion 2>/dev/null ;;
    clang) clang --version 2>/dev/null | sed -nE '1s/.*clang version ([0-9][0-9.]*).*/\1/p' ;;
    cmake) cmake --version 2>/dev/null | sed -nE '1s/^cmake version ([0-9][0-9.]*).*/\1/p' ;;
    node) node --version 2>/dev/null | sed -nE 's/^v([0-9][0-9.]*)$/\1/p' ;;
    python3) python3 -c 'import platform; print(platform.python_version())' 2>/dev/null ;;
    pyyaml) pymod PyYAML ;;
    jsonschema) pymod jsonschema ;;
    openpyxl) pymod openpyxl ;;
    emcc) "$(emcc_bin)" --version 2>/dev/null | sed -nE '1s/.*\) ([0-9][0-9.]*) .*/\1/p' ;;
    java) java -version 2>&1 | sed -nE '1s/.*version "([0-9][0-9.]*)".*/\1/p' ;;
    swift) swift --version 2>/dev/null | sed -nE 's/.*Swift version ([0-9][0-9.]*).*/\1/p' | head -n 1 ;;
    google-chrome) google-chrome --version 2>/dev/null | sed -nE 's/.* ([0-9][0-9.]*) *$/\1/p' ;;
    xcode) xcodebuild -version 2>/dev/null | sed -nE '1s/^Xcode ([0-9][0-9.]*)$/\1/p' ;;
    *) return 1 ;;
  esac
}

case "$(uname -s)" in
  Darwin) host=macos ;;
  *) host=linux ;;
esac
checked=0
mismatched=0
while read -r name want on _; do
  case "$name" in ''|'#'*) continue ;; esac
  if [ -n "${on:-}" ] && [ "$on" != "$host" ]; then
    continue
  fi
  got=$(probe "$name")
  if [ "$print" -eq 1 ]; then
    printf '%s %s\n' "$name" "${got:-unknown}"
    continue
  fi
  checked=$((checked + 1))
  if [ -z "$got" ] || [ "$got" != "$want" ]; then
    echo "mismatch: $name ${got:-not found} (want $want)" >&2
    mismatched=$((mismatched + 1))
  fi
done < "$toolchain"

[ "$print" -eq 1 ] && exit 0
echo "tools: $checked checked, $mismatched mismatched"
[ "$checked" -ge 1 ] && [ "$mismatched" -eq 0 ]
