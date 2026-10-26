#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 OZEN GOLDEN_DIR OUT_DIR" >&2
  exit 2
}

[ $# -eq 3 ] || usage
ozen=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
dir=${2%/}
out=$3
mkdir -p "$out"
rm -f "$out"/*.json
count=0
while IFS= read -r f; do
  first=$(head -n 1 "$f")
  case "$first" in
    "// args: debug schedule"*) ;;
    *) continue ;;
  esac
  read -r -a args <<< "${first#// args: debug schedule}"
  name=$(printf '%s' "${f#"$dir"/}" | sed 's#/#-#g; s#\.ozen$##')
  k=0
  while IFS= read -r line; do
    case "$line" in
      "{"*)
        k=$((k + 1))
        printf '%s\n' "$line" > "$out/$name-$k.json"
        count=$((count + 1))
        ;;
    esac
  done < <(cd "$(dirname "$f")" && "$ozen" debug schedule --format json "${args[@]}" "$(basename "$f")" 2>/dev/null)
done < <(find "$dir" -name '*.ozen' | LC_ALL=C sort)
echo "collect-schedules: $count schedules"
