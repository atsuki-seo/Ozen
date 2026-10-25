#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-plan-tests.sh"

cases=0
failed=0

new_repo() {
  local dir
  dir=$(mktemp -d)
  git -C "$dir" init -q
  printf '%s\n' "$dir"
}

put() {
  mkdir -p "$(dirname "$1/$2")"
  printf '%s\n' "${3:-}" > "$1/$2"
}

plan() {
  local repo=$1 rows=$2 records=${3:-}
  cat > "$repo/plan.md" <<EOF
## 5. goals
### 5.3 coverage

| S 項目 | 観点 | テストまたはゴール | わざと壊す方法 | 主なタスク |
| --- | --- | --- | --- | --- |
$rows

## 6. phases

## 9. records
$records
## 10. risks
EOF
}

expect() {
  local name=$1 repo=$2 flags=$3 want_last=$4 want_status=$5 got got_status
  cases=$((cases + 1))
  set +e
  got=$("$checker" --root "$repo" $flags "$repo/plan.md" 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got" != "$want_last" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $got_status), want '$want_last' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
  rm -rf "$repo"
}

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `fx/vocab-ng/a`、`G-X` | 壊す | P1 |
| S2 | 正常系 | `py/test_m.test_f` | 壊す | P1 |'
put "$r" spec/fixtures/vocab-ng/a.yaml
put "$r" tools/x/test_m.py 'def test_f():'
expect all-present "$r" "" "plan-tests: 2 names, 0 missing" 0

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `fx/vocab-ng/a` | 壊す | P1 |
| S2 | 正常系 | `golden/syntax/b` | 壊す | P1 |'
put "$r" spec/fixtures/vocab-ng/a.yaml
put "$r" proto/tests/golden/syntax/b.ozen
expect golden-without-out "$r" "" "plan-tests: 2 names, 1 missing" 1

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `cgolden/decl/*`（種類ごとに 2 個） | 壊す | P7 |'
put "$r" tests/golden/decl/a.ozen
put "$r" tests/golden/decl/a.out
put "$r" tests/golden/decl/b/expected.out
expect star-exact "$r" "" "plan-tests: 1 names, 0 missing" 0

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `cgolden/decl/*`（3 個） | 壊す | P7 |'
put "$r" tests/golden/decl/a.ozen
put "$r" tests/golden/decl/a.out
put "$r" tests/golden/decl/b/expected.out
expect star-short "$r" "" "plan-tests: 1 names, 1 missing" 1

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `cgolden/decl/*` | 壊す | P7 |'
put "$r" tests/golden/decl/a.ozen
put "$r" tests/golden/decl/a.out
expect star-without-count "$r" "" "plan-tests: 0 names, 0 missing" 1

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `sh/test-x.sh`、`prop/p`、`cli/c`、`sample/s`、`rtest/t` | 壊す | P1 |'
put "$r" proto/scripts/test-x.sh
put "$r" tests/props/p.py
put "$r" tests/cli/c.sh
put "$r" samples/s/expect.yaml
put "$r" renderers/test/t.test.mjs
expect other-categories "$r" "" "plan-tests: 5 names, 0 missing" 0

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `fx/vocab-ng/gone`、`fx/vocab-ng/a` | 壊す | P1 |'
put "$r" spec/fixtures/vocab-ng/a.yaml
put "$r" scripts/retired-tests.txt 'fx/vocab-ng/gone	retired by D28'
expect retired "$r" "" "plan-tests: 1 names, 0 missing" 0

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `py/test_m.test_f` | 壊す | P1 |'
put "$r" tools/x/test_m.py 'def test_other():'
expect py-def-missing "$r" "" "plan-tests: 1 names, 1 missing" 1

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `G-A` | 壊す | P1 |
| S2 | 正常系 | `G-B` | 壊す | P1 |' '- わざと壊す確認: `G-A` — x
  - わざと壊す確認: `G-B` — y'
expect mutations-equal "$r" --mutations "mutations: 2 rows, 2 recorded" 0

r=$(new_repo)
plan "$r" '| S1 | 正常系 | `G-A` | 壊す | P1 |
| S2 | 正常系 | `G-B` | 壊す | P1 |' '- わざと壊す確認: `G-A` — x'
expect mutations-short "$r" --mutations "mutations: 2 rows, 1 recorded" 1

echo "test-check-plan-tests: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
