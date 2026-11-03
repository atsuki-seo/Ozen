#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2

generate=0
check=0
for a in "$@"; do
  case "$a" in
    --delete | --delete=*)
      echo "rulesync.sh: refusing $a: it empties whole output folders, and the copilot target writes into .github/" >&2
      exit 2 ;;
    generate) generate=1 ;;
    --check | --dry-run) check=1 ;;
  esac
done

version=$(awk '$1 == "rulesync" { print $2 }' toolchain.txt 2>/dev/null)
if [ -z "$version" ]; then
  echo "rulesync.sh: toolchain.txt has no rulesync line" >&2
  exit 2
fi

run=(npx --yes "rulesync@$version" "$@")
if [ "$generate" -eq 0 ] || [ "$check" -eq 1 ]; then
  exec "${run[@]}"
fi

out=$("${run[@]}")
status=$?
printf '%s\n' "$out"
[ "$status" -eq 0 ] || exit "$status"
bad=0
while IFS= read -r p; do
  if ! grep -qxF -- "$p" scripts/agent-outputs.txt; then
    echo "rulesync.sh: wrote $p, which scripts/agent-outputs.txt does not list" >&2
    bad=1
  fi
done < <(sed -nE 's/^    ([^ ].*)$/\1/p' <<< "$out" | sort -u)
while IFS= read -r p; do
  [ -n "$p" ] || continue
  if [ ! -f "$p" ]; then
    echo "rulesync.sh: scripts/agent-outputs.txt lists $p, which was not written" >&2
    bad=1
  fi
done < scripts/agent-outputs.txt
exit "$bad"
