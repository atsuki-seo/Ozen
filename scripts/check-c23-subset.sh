#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
probes="$here/c23-probes"
toolchains="gcc:-std=c2x clang:-std=c23"
dirs=()
while [ $# -gt 0 ]; do
  case "$1" in
    --probes) probes="$2"; shift 2 ;;
    --toolchains) toolchains="$2"; shift 2 ;;
    -*) echo "usage: $0 [--probes DIR] [--toolchains 'CC:STD ...'] DIR..." >&2; exit 2 ;;
    *) dirs+=("$1"); shift ;;
  esac
done
[ ${#dirs[@]} -gt 0 ] || { echo "usage: $0 [--probes DIR] [--toolchains 'CC:STD ...'] DIR..." >&2; exit 2; }

nprobes=0
ok=0
ntool=0
for tc in $toolchains; do
  ntool=$((ntool + 1))
done
for p in "$probes"/*.c; do
  [ -f "$p" ] || continue
  nprobes=$((nprobes + 1))
  pass=1
  for tc in $toolchains; do
    cc=${tc%%:*}
    std=${tc#*:}
    if ! "$cc" "$std" -pedantic-errors -Werror -Wall -Wextra -c "$p" -o /dev/null 2>/dev/null; then
      echo "probe failed: $(basename "$p") with $cc $std" >&2
      pass=0
    fi
  done
  ok=$((ok + pass))
done

banned='#[[:space:]]*embed|_BitInt|<stdckdint\.h>|<stdbit\.h>|\bckd_(add|sub|mul)\b|\bstdc_[a-z_]+\b|\b(memset_explicit|memalignment|strdup|strndup|strfrom[dfl]|free_sized|free_aligned_sized|timegm|gmtime_r|localtime_r)\b|"[^"]*%[-+ #0-9.]*b[^"]*"'
uses=0
for d in "${dirs[@]}"; do
  [ -d "$d" ] || continue
  while IFS= read -r hit; do
    echo "banned: $hit" >&2
    uses=$((uses + 1))
  done < <(grep -rnE --include='*.c' --include='*.h' "$banned" "$d")
done

echo "c23: $ok allowed probes ok on $ntool toolchains, $uses banned uses"
[ "$ok" -eq "$nprobes" ] && [ "$nprobes" -ge 1 ] && [ "$uses" -eq 0 ]
