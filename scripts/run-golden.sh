#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 COMMAND DIR [--only NAME] [--save ENV] [--std-version V] [--json OUT] [--sarif OUT] [--jobs N] [--keep OUT]" >&2
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
json=""
sarif=""
jobs=1
keep=""
[ -f "$root/VERSION" ] && version=$(tr -d '\n' < "$root/VERSION")
while [ $# -gt 0 ]; do
  case "$1" in
    --only) only="$2"; shift 2 ;;
    --save) save="$2"; shift 2 ;;
    --std-version) version="$2"; shift 2 ;;
    --json) json="$2"; shift 2 ;;
    --sarif) sarif="$2"; shift 2 ;;
    --jobs) jobs="$2"; shift 2 ;;
    --keep) keep="$2"; shift 2 ;;
    *) usage ;;
  esac
done
[[ $jobs =~ ^[1-9][0-9]*$ ]] || usage

read -r -a runner <<< "$cmd"
for i in "${!runner[@]}"; do
  [ -e "${runner[$i]}" ] && runner[$i]=$(cd "$(dirname "${runner[$i]}")" && pwd)/$(basename "${runner[$i]}")
done
[ -n "$json" ] && mkdir -p "$json" && rm -f "$json"/*.result.json
[ -n "$json" ] && json=$(cd "$json" && pwd)
[ -n "$sarif" ] && mkdir -p "$sarif" && rm -f "$sarif"/*.json
[ -n "$sarif" ] && sarif=$(cd "$sarif" && pwd)
[ -n "$keep" ] && mkdir -p "$keep"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

parse_entry() {
  entry=$1
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
}

run_entry() {
  local n=$1 status
  parse_entry "$2"
  if [ -n "$json" ] || [ -n "$sarif" ]; then
    flags=()
    for ((k = 0; k < ${#args[@]}; k++)); do
      case "${args[$k]}" in
        --kitchen|--options|--entry) flags+=("${args[$k]}" "${args[$((k + 1))]}"); k=$((k + 1)) ;;
      esac
    done
    name=$(printf '%s' "$rel" | sed 's#/#-#g')
    if [ -n "$json" ]; then
      (cd "$rundir" && timeout "${OZP_RUN_TIMEOUT:-10}" "${runner[@]}" check --format json \
        --emit ir,schedule,steps,signatures,definitions,types "${flags[@]}" "${target[@]}") > "$work/$n.json" 2>/dev/null
      if head -c 1 "$work/$n.json" | grep -q '{'; then
        cp "$work/$n.json" "$json/$name.result.json"
      fi
    fi
    if [ -n "$sarif" ]; then
      (cd "$rundir" && timeout "${OZP_RUN_TIMEOUT:-10}" "${runner[@]}" check --format sarif "${flags[@]}" \
        "${target[@]}") > "$work/$n.sarif" 2>/dev/null
      if grep -q '"results":\[{' "$work/$n.sarif"; then
        cp "$work/$n.sarif" "$sarif/$name.json"
      fi
    fi
  fi
  (cd "$rundir" && timeout "${OZP_RUN_TIMEOUT:-10}" "${runner[@]}" "${args[@]}" "${target[@]}") > "$work/$n.out" 2>/dev/null
  status=$?
  printf 'exit %s\n' "$status" >> "$work/$n.out"
  [ -n "$keep" ] && cp "$work/$n.out" "$keep/$(printf '%s' "$entry" | sed 's#/#%#g')"
  echo "$status" > "$work/$n.status"
}

entries=()
while IFS= read -r e; do
  parse_entry "$e"
  if [ -n "$only" ] && [ "$rel" != "$only" ]; then
    continue
  fi
  entries+=("$e")
done < <("$here/golden-entries.sh" "$dir")
for n in "${!entries[@]}"; do
  while [ "$(jobs -pr | wc -l)" -ge "$jobs" ]; do wait -n; done
  run_entry "$n" "${entries[$n]}" &
done
wait

passed=0
failed=0
timedout=0
report=""
for n in "${!entries[@]}"; do
  parse_entry "${entries[$n]}"
  if [ "$(cat "$work/$n.status")" -eq 124 ]; then
    timedout=$((timedout + 1))
    report+="TIMEOUT $rel"$'\n'
    continue
  fi
  sed "s/{{STD_VERSION}}/$version/g" "$expected" > "$work/want" 2>/dev/null
  if cmp -s "$work/$n.out" "$work/want"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    report+="FAIL $rel"$'\n'
    diff "$work/want" "$work/$n.out" | head -n 10 >&2
  fi
done
printf '%s' "$report" >&2
summary="golden: $passed passed, $failed failed, $timedout timed out"
if [ -n "$save" ]; then
  mkdir -p "$root/build/results"
  printf '%s\n' "$summary" > "$root/build/results/golden-$save.txt"
fi
echo "$summary"
[ "$failed" -eq 0 ] && [ "$timedout" -eq 0 ] && [ "$passed" -ge 1 ]
