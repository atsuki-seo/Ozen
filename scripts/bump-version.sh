#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
if [ $# -ne 1 ] || ! [[ $1 =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "usage: $0 X.Y.Z" >&2
  exit 2
fi
printf '%s\n' "$1" > "$root/VERSION"
sed -i.bak -E "s/^( *version: *)\"[^\"]*\"/\\1\"$1\"/" "$root/stdlib/package.ozen"
rm -f "$root/stdlib/package.ozen.bak"
echo "version: $1"
