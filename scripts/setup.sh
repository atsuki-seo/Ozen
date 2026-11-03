#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2

case "$(uname -s)" in
  Darwin) host=macos; required="cmake python3 node" ;;
  *) host=linux; required="gcc cmake python3 node" ;;
esac

found=$(scripts/check-tools.sh --toolchain toolchain.txt --print)
status=0
while read -r name want on _; do
  case "$name" in ''|'#'*) continue ;; esac
  if [ -n "${on:-}" ] && [ "$on" != "$host" ]; then
    echo "skip $name"
    continue
  fi
  got=$(awk -v n="$name" '$1 == n { print $2 }' <<< "$found")
  if [ -z "$got" ] || [ "$got" = unknown ]; then
    line="missing $name want $want"
  elif [ "$got" != "$want" ]; then
    line="differs $name want $want got $got"
  else
    line="ok $name $want"
  fi
  echo "$line"
  case " $required " in
    *" $name "*) [ "${line%% *}" = ok ] || status=1 ;;
  esac
done < toolchain.txt

hooks=$(git config --local core.hooksPath)
if [ -z "$hooks" ] || [ "$hooks" = .githooks ]; then
  git config --local core.hooksPath .githooks
  echo "hooksPath: .githooks"
else
  echo "hooksPath: kept $hooks"
fi

outputs_digest() {
  local p
  while read -r p; do
    [ -f "$p" ] && sha256sum "$p"
  done < scripts/agent-outputs.txt
}
before=$(outputs_digest)
if scripts/rulesync.sh generate >/dev/null 2>&1; then
  if [ "$(outputs_digest)" = "$before" ]; then
    echo "agent files: unchanged"
  else
    echo "agent files: updated"
  fi
else
  echo "agent files: skipped (rulesync failed)"
fi

exit "$status"
