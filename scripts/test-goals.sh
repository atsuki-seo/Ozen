#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
goals="$here/goals.sh"

cases=0
failed=0

new_repo() {
  local dir
  dir=$(mktemp -d)
  git -C "$dir" init -q
  mkdir -p "$dir/plans"
  printf '%s\n' "$dir"
}

expect() {
  local name=$1 repo=$2 id=$3 want_last=$4 want_status=$5 got got_status
  cases=$((cases + 1))
  set +e
  got=$("$goals" --root "$repo" "$id" 2>/dev/null | tail -n 1)
  got_status=$?
  set -e
  if [ "$got" != "$want_last" ] || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $got_status), want '$want_last' (status $want_status)" >&2
    failed=$((failed + 1))
  fi
  rm -rf "$repo"
}

licenses() {
  printf 'Permission is hereby granted, free of charge\nCopyright (c) 2026 atsuki.seo\n' > "$1/LICENSE-MIT"
  printf 'Version 2.0, January 2004\n' > "$1/LICENSE-APACHE"
  printf 'Attribution 4.0 International\n' > "$1/LICENSE-CC-BY-4.0"
}

r=$(new_repo); licenses "$r"
expect license-good "$r" G-REPO-LICENSE-TEXT "goal G-REPO-LICENSE-TEXT: pass (5 checks)" 0

r=$(new_repo); licenses "$r"; printf 'Copyright (c) <year> <copyright holders>\n' >> "$r/LICENSE-MIT"
expect license-placeholder "$r" G-REPO-LICENSE-TEXT "goal G-REPO-LICENSE-TEXT: FAIL (1 of 5 checks)" 1

coverage_plan() {
  cat > "$1/plans/ozen-roadmap.md" <<EOF
## 4. spec
- **S1** one
- **S2** two
## 5. goals
### 5.3 coverage
| S 項目 | 観点 |
| --- | --- |
| S1 | 正常系 |
$2
## 6. phases
## 8. handoff
- item — 機械で決められない理由: none
$3
## 9. records
EOF
}

r=$(new_repo); coverage_plan "$r" '| S2 | 正常系 |' ''
expect coverage-good "$r" G-PLAN-COVERAGE "goal G-PLAN-COVERAGE: pass (2 checks)" 0

r=$(new_repo); coverage_plan "$r" '' ''
expect coverage-missing-row "$r" G-PLAN-COVERAGE "goal G-PLAN-COVERAGE: FAIL (1 of 2 checks)" 1

r=$(new_repo); coverage_plan "$r" '| S2 | 正常系 |' '- item without a reason'
expect coverage-handoff-reason "$r" G-PLAN-COVERAGE "goal G-PLAN-COVERAGE: FAIL (1 of 2 checks)" 1

r=$(new_repo)
expect unknown-goal "$r" G-NO-SUCH-GOAL "" 2

cases=$((cases + 1))
if ! "$goals" --list | grep -qx 'G-PLAN-COVERAGE'; then
  echo "FAIL list: G-PLAN-COVERAGE not listed" >&2
  failed=$((failed + 1))
fi

echo "test-goals: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
