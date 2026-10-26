#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

usage() {
  echo "usage: $0 [--root DIR] [--list]" >&2
  exit 2
}

root=""
list_only=0
while [ $# -gt 0 ]; do
  case "$1" in
    --root) [ $# -ge 2 ] || usage; root="$2"; shift 2 ;;
    --list) list_only=1; shift ;;
    *) usage ;;
  esac
done
if [ -z "$root" ]; then
  root=$(git rev-parse --show-toplevel)
fi
cd "$root"

code_id='MIT OR Apache-2.0'
docs_id='CC-BY-4.0'

expected_id() {
  local path=$1 base
  base=${path##*/}
  # Vendored third-party files keep their upstream license headers, so they are exempt.
  case "$path" in
    */vendor/*) return ;;
  esac
  case "$path" in
    proto/*|tools/*|scripts/*|.github/*|core/*|cli/*|tests/*)
      case "$base" in
        CMakeLists.txt|*.c|*.h|*.def|*.py|*.sh|*.mjs|*.cmake|*.yml|*.ebnf) echo "$code_id" ;;
      esac ;;
    spec/vocab/*|spec/schema/*|spec/coverage/*|spec/fixtures/*)
      echo "$code_id" ;;
    spec/samples/*)
      echo "$docs_id" ;;
  esac
}

checked=0
missing=0
wrong=0
while IFS= read -r -d '' path; do
  want=$(expected_id "$path")
  [ -n "$want" ] || continue
  if [ "$list_only" -eq 1 ]; then
    printf '%s\n' "$path"
    continue
  fi
  checked=$((checked + 1))
  found=$(head -n 5 -- "$path" | grep -oE 'SPDX-License-Identifier: [A-Za-z0-9.+ -]+[A-Za-z0-9.+]' | head -n 1 || true)
  if [ -z "$found" ]; then
    echo "missing: $path" >&2
    missing=$((missing + 1))
  elif [ "$found" != "SPDX-License-Identifier: $want" ]; then
    echo "wrong: $path (${found#SPDX-License-Identifier: }, expected $want)" >&2
    wrong=$((wrong + 1))
  fi
done < <(git ls-files -z --cached --others --exclude-standard)

if [ "$list_only" -eq 1 ]; then
  exit 0
fi
echo "spdx: $checked files checked, $missing missing, $wrong wrong"
[ "$missing" -eq 0 ] && [ "$wrong" -eq 0 ]
