#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 COMMAND [--only CATEGORY/NAME] [--dir DIR] [--save ENV]" >&2
  exit 2
}

[ $# -ge 1 ] || usage
cmd=$1
shift
root=$(cd "$(dirname "$0")/../.." && pwd)
dir="$root/proto/tests/golden"
only=""
save=""
while [ $# -gt 0 ]; do
  case "$1" in
    --only) only="$2"; shift 2 ;;
    --dir) dir="$2"; shift 2 ;;
    --save) save="$2"; shift 2 ;;
    *) usage ;;
  esac
done

read -r -a runner <<< "$cmd"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
passed=0
failed=0
timedout=0
report=""
while IFS= read -r input; do
  rel=${input#"$dir"/}
  rel=${rel%.ozen}
  if [ -n "$only" ] && [ "$rel" != "$only" ]; then
    continue
  fi
  args=()
  first=$(head -n 1 "$input")
  case "$first" in
    "// args: "*) read -r -a args <<< "${first#// args: }" ;;
  esac
  timeout 10 "${runner[@]}" "${args[@]}" "$input" > "$work/out" 2>/dev/null
  status=$?
  if [ "$status" -eq 124 ]; then
    timedout=$((timedout + 1))
    report+="TIMEOUT $rel"$'\n'
    continue
  fi
  printf 'exit %s\n' "$status" >> "$work/out"
  if cmp -s "$work/out" "${input%.ozen}.out"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    report+="FAIL $rel"$'\n'
    diff "${input%.ozen}.out" "$work/out" | head -n 10 >&2
  fi
done < <(find "$dir" -name '*.ozen' | LC_ALL=C sort)
printf '%s' "$report" >&2
summary="golden: $passed passed, $failed failed, $timedout timed out"
if [ -n "$save" ]; then
  mkdir -p "$root/build/results"
  printf '%s\n' "$summary" > "$root/build/results/golden-$save.txt"
fi
echo "$summary"
[ "$failed" -eq 0 ] && [ "$timedout" -eq 0 ] && [ $((passed)) -ge 1 ]
