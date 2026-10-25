#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

usage() {
  echo "usage: $0 [--root DIR] [--no-run]" >&2
  exit 2
}

root=""
run=1
while [ $# -gt 0 ]; do
  case "$1" in
    --root) [ $# -ge 2 ] || usage; root="$2"; shift 2 ;;
    --no-run) run=0; shift ;;
    *) usage ;;
  esac
done
if [ -z "$root" ]; then
  root=$(git rev-parse --show-toplevel)
fi
cd "$root" || exit 2

list=scripts/selftests.txt
dirs="scripts proto/scripts tools tests/samples tests/props renderers/tools bindings"

declare -A test_of=()
if [ -f "$list" ]; then
  while read -r script cmd; do
    case "$script" in ''|'#'*) continue ;; esac
    test_of[$script]=$cmd
  done < "$list"
fi

is_checker() {
  local base=${1##*/}
  case "$base" in
    test-*|test_*|__init__.py) return 1 ;;
    check-*|run-*|digest.sh|goals.sh|*-test.sh|*check*.py|*check.mjs) return 0 ;;
  esac
  return 1
}

found=()
for d in $dirs; do
  [ -d "$d" ] || continue
  while IFS= read -r f; do
    is_checker "$f" && found+=("$f")
  done < <(git ls-files --cached --others --exclude-standard -- "$d" | sort)
done

declare -A all=()
for f in "${found[@]+"${found[@]}"}"; do all[$f]=1; done
for f in "${!test_of[@]}"; do all[$f]=1; done

scripts=0
without=0
failed=0
for f in $(printf '%s\n' "${!all[@]}" | sort); do
  scripts=$((scripts + 1))
  cmd=${test_of[$f]:-}
  if [ ! -e "$f" ]; then
    echo "listed but missing: $f" >&2
    failed=$((failed + 1))
    continue
  fi
  if [ -z "$cmd" ]; then
    echo "without test: $f" >&2
    without=$((without + 1))
    continue
  fi
  first=${cmd%% *}
  if [ "$first" = "${first%/*}" ] || [ -e "$first" ]; then
    :
  else
    echo "test missing: $f -> $first" >&2
    without=$((without + 1))
    continue
  fi
  [ "$run" -eq 1 ] || continue
  if ! out=$(bash -c "$cmd" 2>&1); then
    echo "failed: $f -> $cmd" >&2
    tail -n 5 <<< "$out" >&2
    failed=$((failed + 1))
  fi
done

echo "selftests: $scripts scripts, $without without test, $failed failed"
[ "$without" -eq 0 ] && [ "$failed" -eq 0 ]
