#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")/.." && pwd)
checker="$here/tools/vocabcheck/priorwork.py"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

setup() {
  cat > "$work/e.yaml" <<'Y'
sources:
  a: {title: A}
  b: {title: B}
elements:
  - {id: a-one, source: a, page: 2}
  - {id: a-two, source: a, page: 3}
  - {id: b-one, source: b, page: 1}
Y
  cat > "$work/m.md" <<'M'
| Element | Ozen | Basis |
| --- | --- | --- |
| `a-one` | Something | concept §4.1 |
| `a-two` | No counterpart: not modeled. | — |
| `b-one` | Other | S44 |
M
}

expect() {
  local name=$1 want=$2 status=$3 got st
  cases=$((cases + 1))
  set +e
  got=$(python3 -B "$checker" "$work/e.yaml" "$work/m.md" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

setup; expect good "prior-work: 3 elements from 2 sources, 0 unmapped, 0 without page" 0
setup; sed -i '/b-one/d' "$work/m.md"; expect row-missing "prior-work: 3 elements from 2 sources, 1 unmapped, 0 without page" 1
setup; sed -i 's/| concept §4.1 |/| see above |/' "$work/m.md"; expect no-basis "prior-work: 3 elements from 2 sources, 1 unmapped, 0 without page" 1
setup; sed -i 's/No counterpart: not modeled./No counterpart/' "$work/m.md"; expect no-reason "prior-work: 3 elements from 2 sources, 1 unmapped, 0 without page" 1
setup; sed -i '/b-one/d' "$work/e.yaml"; expect source-without-elements "prior-work: 2 elements from 1 sources, 0 unmapped, 0 without page" 1
setup; sed -i 's/page: 3/page: x/' "$work/e.yaml"; expect page-missing "prior-work: 3 elements from 2 sources, 0 unmapped, 1 without page" 1

echo "test-priorwork: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
