#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

usage() {
  echo "usage: $0 linux|macos|wasm|sums [--binary FILE] [--wasm-dir DIR] [--out DIR]" >&2
  exit 2
}

root=$(cd "$(dirname "$0")/.." && pwd)
[ $# -ge 1 ] || usage
kind=$1
shift
binary=""
wasm_dir="$root/build/core-wasm"
out="$root/dist"
while [ $# -gt 0 ]; do
  case "$1" in
    --binary) [ $# -ge 2 ] || usage; binary=$2; shift 2 ;;
    --wasm-dir) [ $# -ge 2 ] || usage; wasm_dir=$2; shift 2 ;;
    --out) [ $# -ge 2 ] || usage; out=$2; shift 2 ;;
    *) usage ;;
  esac
done
v=$(cat "$root/VERSION")
mkdir -p "$out"
out=$(cd "$out" && pwd)
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

common() {
  cp "$root/LICENSE-MIT" "$root/LICENSE-APACHE" "$root/LICENSE-CC-BY-4.0" "$root/NOTICE" "$root/README.md" "$1/"
}

pack_tar() {
  tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$stage" -cf - "ozen-$v" | gzip -n > "$out/$1"
  echo "wrote $out/$1"
}

native() {
  local name=$1 bin=$2 d="$stage/ozen-$v"
  [ -x "$bin" ] || { echo "make-release: no binary at $bin" >&2; exit 1; }
  mkdir -p "$d/bin" "$d/renderers"
  cp "$bin" "$d/bin/ozen"
  (cd "$root/renderers" && find . -path ./test -prune -o -type f -print) | while IFS= read -r f; do
    mkdir -p "$d/renderers/$(dirname "$f")"
    cp "$root/renderers/$f" "$d/renderers/$f"
  done
  cp -RL "$root/phrases" "$root/reference" "$d/"
  common "$d"
  pack_tar "$name"
}

case "$kind" in
  linux) native "ozen-$v-linux-x86_64.tar.gz" "${binary:-$root/build/core-gcc-release/ozen}" ;;
  macos) native "ozen-$v-macos-universal.tar.gz" "${binary:-$root/build/macos/ozen}" ;;
  wasm)
    d="$stage/ozen-$v"
    mkdir -p "$d"
    for f in ozen_bridge.js ozen_bridge.wasm; do
      [ -f "$wasm_dir/$f" ] || { echo "make-release: no $f in $wasm_dir" >&2; exit 1; }
      cp "$wasm_dir/$f" "$d/"
    done
    cp "$root/bindings/wasm/ozen.mjs" "$root/bindings/wasm/worker.js" "$root/bindings/wasm/node-run.mjs" "$d/"
    common "$d"
    rm -f "$out/ozen-$v-wasm.zip"
    (cd "$stage" && find "ozen-$v" -type f | sort | TZ=UTC zip -q -X -D "$out/ozen-$v-wasm.zip" -@)
    echo "wrote $out/ozen-$v-wasm.zip"
    ;;
  sums)
    (cd "$out" && ls "ozen-$v"-*.tar.gz "ozen-$v"-*.zip 2>/dev/null | sort | xargs -r sha256sum > SHA256SUMS)
    echo "wrote $out/SHA256SUMS ($(wc -l < "$out/SHA256SUMS") archives)"
    ;;
  *) usage ;;
esac
