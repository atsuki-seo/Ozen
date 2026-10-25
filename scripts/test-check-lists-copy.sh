#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-lists-copy.py"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cases=0
failed=0

cat > "$work/plan.md" <<'EOF'
### 4.7 lists

| 種類 | 数 | 識別子（concept の語） |
| --- | --- | --- |
| 切り方 | 2 | `slice`（薄切り）`julienne`（せん切り） |
| 部位 | 1 | `Pork`: `loin` |
| 合計 | `ignored` | |

#### 4.7.1 relations

| # | 関係 | concept |
| --- | --- | --- |
| 1 | `notCounted` | x |
EOF

expect() {
  local name=$1 body=$2 want_last=$3 want_status=$4 got got_status
  cases=$((cases + 1))
  printf '%s\n' "$body" > "$work/lists.yaml"
  set +e
  got=$(python3 -B "$checker" "$work/plan.md" "$work/lists.yaml" 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got" != "$want_last" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $got_status), want '$want_last' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
}

good='entries:
  - {kind: cut, id: slice}
  - {kind: cut, id: julienne}
  - {kind: part, owner: Pork, id: loin}'

expect exact "$good" "copy: 4 identifiers, 0 differences" 0
expect misspelled "${good/julienne/julliene}" "copy: 4 identifiers, 2 differences" 1
expect missing-owner "${good/owner: Pork, /}" "copy: 4 identifiers, 1 differences" 1
expect extra "$good
  - {kind: cut, id: mince}" "copy: 4 identifiers, 1 differences" 1
expect empty "entries: []" "copy: 4 identifiers, 4 differences" 1

echo "test-check-lists-copy: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
