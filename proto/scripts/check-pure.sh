#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail
export LC_ALL=C

if [ $# -lt 2 ]; then
  echo "usage: $0 SRC_DIR LIBRARY..." >&2
  exit 2
fi
src=$1
shift

allowed_headers=" stddef.h stdint.h stdbool.h string.h limits.h "
# Compilers emit calls the core never makes: clang lowers memcmp() used only for equality to bcmp(), and
# gcc turns the core's own length loop into strlen() (loop idiom recognition at -O2 and above).
allowed_symbols=" memcpy memmove memset memcmp __stack_chk_fail bcmp strlen "
bad_includes=0
bad_symbols=0

while IFS= read -r line; do
  file=${line%%:*}
  inc=$(sed -nE 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*([<"])([^>"]+)[>"].*/\1\2/p' <<< "${line#*:}")
  [ -n "$inc" ] || continue
  kind=${inc:0:1}
  name=${inc:1}
  if [ "$kind" = "<" ]; then
    case "$allowed_headers" in *" $name "*) continue ;; esac
  else
    dir=$(dirname "$file")
    if [ -e "$dir/$name" ] || [ -e "$dir/../include/$name" ]; then
      continue
    fi
  fi
  echo "disallowed include: $file: $name" >&2
  bad_includes=$((bad_includes + 1))
done < <(grep -H -E '^[[:space:]]*#[[:space:]]*include' "$src"/*.c "$src"/*.h 2>/dev/null)

for lib in "$@"; do
  defined=$(nm --defined-only "$lib" 2>/dev/null | awk 'NF==3{print $3}' | sort -u)
  while IFS= read -r sym; do
    [ -n "$sym" ] || continue
    if grep -qxF "$sym" <<< "$defined"; then
      continue
    fi
    case "$allowed_symbols" in *" $sym "*) continue ;; esac
    echo "disallowed symbol: $lib: $sym" >&2
    bad_symbols=$((bad_symbols + 1))
  done < <(nm -u "$lib" 2>/dev/null | awk '$1=="U"{print $2}' | sort -u)
done

echo "pure: $bad_symbols disallowed symbols, $bad_includes disallowed includes"
[ "$bad_symbols" -eq 0 ] && [ "$bad_includes" -eq 0 ]
