#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 COMMAND DIR... [--save ENV]" >&2
  exit 2
}

[ $# -ge 2 ] || usage
cmd=$1
shift
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
dirs=()
save=""
while [ $# -gt 0 ]; do
  case "$1" in
    --save) save="$2"; shift 2 ;;
    -*) usage ;;
    *) dirs+=("${1%/}"); shift ;;
  esac
done
[ ${#dirs[@]} -gt 0 ] || usage

read -r -a runner <<< "$cmd"
for i in "${!runner[@]}"; do
  [ -e "${runner[$i]}" ] && runner[$i]=$(cd "$(dirname "${runner[$i]}")" && pwd)/$(basename "${runner[$i]}")
done
digest=$(
  for dir in "${dirs[@]}"; do
    "$here/golden-entries.sh" "$dir" | while IFS= read -r entry; do
      args=(check)
      if [ -d "$entry" ]; then
        [ -f "$entry/args" ] && read -r -a args < "$entry/args"
        rundir=$entry
        target=()
      else
        first=$(head -n 1 "$entry")
        case "$first" in
          "// args: "*) read -r -a args <<< "${first#// args: }" ;;
        esac
        rundir=$(dirname "$entry")
        target=("$(basename "$entry")")
      fi
      printf '%s\n' "$entry"
      (cd "$rundir" && timeout "${OZP_RUN_TIMEOUT:-10}" "${runner[@]}" "${args[@]}" "${target[@]}") 2>/dev/null
      printf 'exit %s\n' "$?"
    done
  done | sha256sum | cut -d' ' -f1
)
if [ -n "$save" ]; then
  mkdir -p "$root/build/results"
  printf '%s\n' "$digest" > "$root/build/results/digest-$save.txt"
fi
echo "$digest"
