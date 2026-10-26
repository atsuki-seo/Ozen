#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 COMMAND DIR [--only NAME] [--save ENV] [--std-version V]" >&2
  exit 2
}

[ $# -ge 2 ] || usage
cmd=$1
dir=${2%/}
shift 2
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
only=""
save=""
version=""
[ -f "$root/VERSION" ] && version=$(tr -d '\n' < "$root/VERSION")
while [ $# -gt 0 ]; do
  case "$1" in
    --only) only="$2"; shift 2 ;;
    --save) save="$2"; shift 2 ;;
    --std-version) version="$2"; shift 2 ;;
    *) usage ;;
  esac
done

read -r -a runner <<< "$cmd"
for i in "${!runner[@]}"; do
  [ -e "${runner[$i]}" ] && runner[$i]=$(cd "$(dirname "${runner[$i]}")" && pwd)/$(basename "${runner[$i]}")
done
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
passed=0
failed=0
timedout=0
report=""
while IFS= read -r entry; do
  rel=${entry#"$dir"/}
  args=(check)
  if [ -d "$entry" ]; then
    expected="$entry/expected.out"
    [ -f "$entry/args" ] && read -r -a args < "$entry/args"
    rundir=$entry
    target=()
  else
    rel=${rel%.ozen}
    expected="${entry%.ozen}.out"
    first=$(head -n 1 "$entry")
    case "$first" in
      "// args: "*) read -r -a args <<< "${first#// args: }" ;;
    esac
    rundir=$(dirname "$entry")
    target=("$(basename "$entry")")
  fi
  if [ -n "$only" ] && [ "$rel" != "$only" ]; then
    continue
  fi
  (cd "$rundir" && timeout "${OZP_RUN_TIMEOUT:-10}" "${runner[@]}" "${args[@]}" "${target[@]}") > "$work/out" 2>/dev/null
  status=$?
  if [ "$status" -eq 124 ]; then
    timedout=$((timedout + 1))
    report+="TIMEOUT $rel"$'\n'
    continue
  fi
  printf 'exit %s\n' "$status" >> "$work/out"
  sed "s/{{STD_VERSION}}/$version/g" "$expected" > "$work/want" 2>/dev/null
  if cmp -s "$work/out" "$work/want"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    report+="FAIL $rel"$'\n'
    diff "$work/want" "$work/out" | head -n 10 >&2
  fi
done < <("$here/golden-entries.sh" "$dir")
printf '%s' "$report" >&2
summary="golden: $passed passed, $failed failed, $timedout timed out"
if [ -n "$save" ]; then
  mkdir -p "$root/build/results"
  printf '%s\n' "$summary" > "$root/build/results/golden-$save.txt"
fi
echo "$summary"
[ "$failed" -eq 0 ] && [ "$timedout" -eq 0 ] && [ "$passed" -ge 1 ]
