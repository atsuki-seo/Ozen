#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
ozen=${1:-$root/build/core-gcc-release/ozen}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/samples/one"
cat > "$work/samples/one/main.ozen" <<'OZ'
recipe one {
  ingredients { onion: 1 pcs }
  let peeled = peel(onion)
  let sliced = cut(peeled, slice)
}
OZ
cat > "$work/samples/one/package.ozen" <<'OZ'
package one {
  version: "0.1.0"
}
OZ
write_expect() {
  cat > "$work/samples/one/expect.yaml" <<YAML
entry: one
runs:
  - name: base
    cooks: 1
    expect:
      errors: $1
      level: 1
      ir:
        reaches:
          - {to: cut, from: [onion]}
      schedule:
        tasks: [peel, cut]
        order: [$2]
YAML
}
cat > "$work/garbage" <<'SH'
#!/bin/sh
echo not json
SH
cat > "$work/overlap" <<SH
#!/bin/sh
"$ozen" "\$@" | python3 -c '
import json, sys
d = json.load(sys.stdin)
for s in d["schedules"]:
    for t in s["schedule"]["tasks"]:
        t["start"] = {"num": 0, "den": 1}
print(json.dumps(d))'
SH
chmod +x "$work/garbage" "$work/overlap"
cases=0
failed=0
expect() {
  local name=$1 want=$2 status=$3 bin=$4 got st
  cases=$((cases + 1))
  got=$(python3 -B "$here/check.py" --ozen "$bin" "$work/samples" 2>/dev/null)
  st=$?
  got=$(tail -n 1 <<< "$got")
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}
write_expect 0 "[peel, cut]"
expect pass "samples: 1 samples, 8 expectations, 0 failed" 0 "$ozen"
write_expect 1 "[peel, cut]"
expect wrong-errors "samples: 1 samples, 8 expectations, 1 failed" 1 "$ozen"
write_expect 0 "[cut, peel]"
expect wrong-order "samples: 1 samples, 8 expectations, 1 failed" 1 "$ozen"
expect not-json "samples: 1 samples, 1 expectations, 1 failed" 1 "$work/garbage"
write_expect 0 "[peel, cut]"
expect schedcheck-violation "samples: 1 samples, 8 expectations, 2 failed" 1 "$work/overlap"
write_extended() {
  cat > "$work/samples/one/expect.yaml" <<YAML
entry: one
runs:
  - name: extended
    cooks: 1
    expect:
      diagnostics:
        include: [{name: unlimitedEquipment}]
      ir:
        kinds: {in: 1}
        inputs: {cut: [peel]}
      schedule:
        taskCount: {cut@Onion: $1}
YAML
}
write_extended 1
expect extended "samples: 1 samples, 6 expectations, 0 failed" 0 "$ozen"
write_extended 2
expect wrong-count "samples: 1 samples, 6 expectations, 1 failed" 1 "$ozen"
rm "$work/samples/one/expect.yaml"
expect no-samples "samples: 0 samples, 0 expectations, 0 failed" 1 "$ozen"
echo "test-check: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
