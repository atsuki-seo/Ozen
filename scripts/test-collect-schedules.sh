#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
ozen=${OZEN:-$root/build/core-gcc-release/ozen}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

mkdir -p "$work/in/a"
printf '// args: debug schedule\nrecipe r() {\n  ingredients { rice: 300 g }\n  let w = wash(rice)\n}\n' > "$work/in/a/one.ozen"
printf '// args: check\nrecipe s() {\n  ingredients { rice: 300 g }\n  let w = wash(rice)\n}\n' > "$work/in/a/other.ozen"
out=$("$root/scripts/collect-schedules.sh" "$ozen" "$work/in" "$work/out")
cases=$((cases + 1))
if [ "$out" != "collect-schedules: 1 schedules" ] || [ ! -f "$work/out/a-one-1.json" ]; then
  echo "FAIL collect: $out" >&2
  failed=$((failed + 1))
fi
cases=$((cases + 1))
if ! python3 -B -c 'import json,sys; d=json.load(open(sys.argv[1])); assert d["tasks"][0]["op"] == "wash"' \
  "$work/out/a-one-1.json" 2>/dev/null; then
  echo "FAIL json content" >&2
  failed=$((failed + 1))
fi
echo "test-collect-schedules: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
