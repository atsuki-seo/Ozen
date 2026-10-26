#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
emsdk=${EMSDK:-$HOME/.local/share/emsdk}
command -v emcc >/dev/null 2>&1 || source "$emsdk/emsdk_env.sh" >/dev/null 2>&1 ||
  { echo "build-wasm: emsdk not found at $emsdk" >&2; exit 2; }
dir="$root/build/core-wasm"
mkdir -p "$dir"
if emcmake cmake -S "$root/core" -B "$dir" -DCMAKE_BUILD_TYPE=Release >"$dir/configure.log" 2>&1 &&
   cmake --build "$dir" -j "$(nproc)" --target ozen ozen_bridge >"$dir/build.log" 2>&1; then
  echo "build-wasm: 1 configs, 0 failed"
else
  tail -n 20 "$dir/build.log" "$dir/configure.log" >&2 2>/dev/null
  echo "build-wasm: 1 configs, 1 failed"
  exit 1
fi
