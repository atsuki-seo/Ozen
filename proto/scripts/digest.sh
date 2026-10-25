#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 COMMAND [--save ENV] [--golden DIR] [--corpus DIR]" >&2
  exit 2
}

[ $# -ge 1 ] || usage
cmd=$1
shift
root=$(cd "$(dirname "$0")/../.." && pwd)
golden="$root/proto/tests/golden"
corpus="$root/proto/tests/corpus"
save=""
while [ $# -gt 0 ]; do
  case "$1" in
    --save) save="$2"; shift 2 ;;
    --golden) golden="$2"; shift 2 ;;
    --corpus) corpus="$2"; shift 2 ;;
    *) usage ;;
  esac
done

read -r -a runner <<< "$cmd"
digest=$(
  {
    [ -d "$golden" ] && find "$golden" -name '*.ozen' | LC_ALL=C sort | while IFS= read -r f; do
      args=()
      first=$(head -n 1 "$f")
      case "$first" in
        "// args: "*) read -r -a args <<< "${first#// args: }" ;;
      esac
      printf 'golden/%s\n' "${f#"$golden"/}"
      timeout 10 "${runner[@]}" "${args[@]}" "$f" 2>/dev/null
      printf 'exit %s\n' "$?"
    done
    [ -d "$corpus" ] && find "$corpus" -type f | LC_ALL=C sort | while IFS= read -r f; do
      printf 'corpus/%s\n' "${f#"$corpus"/}"
      timeout 10 "${runner[@]}" "$f" 2>/dev/null
      printf 'exit %s\n' "$?"
    done
  } | sha256sum | cut -d' ' -f1
)
if [ -n "$save" ]; then
  mkdir -p "$root/build/results"
  printf '%s\n' "$digest" > "$root/build/results/digest-$save.txt"
fi
echo "$digest"
