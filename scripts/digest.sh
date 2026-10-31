#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 COMMAND DIR... [--save ENV] [--jobs N] [--reuse DIR]" >&2
  exit 2
}

[ $# -ge 2 ] || usage
cmd=$1
shift
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
dirs=()
save=""
jobs=1
reuse=""
while [ $# -gt 0 ]; do
  case "$1" in
    --save) save="$2"; shift 2 ;;
    --jobs) jobs="$2"; shift 2 ;;
    --reuse) reuse="$2"; shift 2 ;;
    -*) usage ;;
    *) dirs+=("${1%/}"); shift ;;
  esac
done
[ ${#dirs[@]} -gt 0 ] || usage
[[ $jobs =~ ^[1-9][0-9]*$ ]] || usage

read -r -a runner <<< "$cmd"
for i in "${!runner[@]}"; do
  [ -e "${runner[$i]}" ] && runner[$i]=$(cd "$(dirname "${runner[$i]}")" && pwd)/$(basename "${runner[$i]}")
done
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

run_entry() {
  local n=$1 entry=$2 kept
  kept="$reuse/$(printf '%s' "$entry" | sed 's#/#%#g')"
  if [ -n "$reuse" ] && [ -f "$kept" ]; then
    cp "$kept" "$work/$n.out"
    return
  fi
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
  (cd "$rundir" && timeout "${OZP_RUN_TIMEOUT:-10}" "${runner[@]}" "${args[@]}" "${target[@]}") > "$work/$n.out" 2>/dev/null
  printf 'exit %s\n' "$?" >> "$work/$n.out"
}

entries=()
for dir in "${dirs[@]}"; do
  while IFS= read -r e; do entries+=("$e"); done < <("$here/golden-entries.sh" "$dir")
done
for n in "${!entries[@]}"; do
  while [ "$(jobs -pr | wc -l)" -ge "$jobs" ]; do wait -n; done
  run_entry "$n" "${entries[$n]}" &
done
wait
digest=$(
  for n in "${!entries[@]}"; do
    printf '%s\n' "${entries[$n]}"
    cat "$work/$n.out"
  done | sha256sum | cut -d' ' -f1
)
if [ -n "$save" ]; then
  mkdir -p "$root/build/results"
  printf '%s\n' "$digest" > "$root/build/results/digest-$save.txt"
fi
echo "$digest"
