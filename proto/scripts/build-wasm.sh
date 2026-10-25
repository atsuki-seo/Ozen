#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
emsdk=${EMSDK:-$HOME/.local/share/emsdk}
source "$emsdk/emsdk_env.sh" >/dev/null 2>&1 || { echo "build-wasm: emsdk not found at $emsdk" >&2; exit 2; }
failed=0
for env in node web; do
  dir="$root/build/wasm"
  [ "$env" = web ] && dir="$root/build/wasm-web"
  mkdir -p "$dir"
  if emcmake cmake -S "$root/proto" -B "$dir" -DCMAKE_BUILD_TYPE=Release -DOZP_WASM_ENV="$env" >"$dir/configure.log" 2>&1 &&
     cmake --build "$dir" -j "$(nproc)" >"$dir/build.log" 2>&1; then
    :
  else
    echo "build-wasm failed: $env (see ${dir#"$root"/}/build.log)" >&2
    failed=$((failed + 1))
  fi
done
echo "build-wasm: 2 configs, $failed failed"
[ "$failed" -eq 0 ]
