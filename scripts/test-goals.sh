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
- item — Reason a command cannot decide: none
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

r=$(new_repo); coverage_plan "$r" '| S2 | 正常系 |' ''
cases=$((cases + 1))
set +e
got=$("$goals" --root "$r" --summary G-REPO-LICENSE-TEXT G-PLAN-COVERAGE 2>&1)
got_status=$?
set -e
if [ "$got" != "$(printf 'G-REPO-LICENSE-TEXT fail\nG-PLAN-COVERAGE pass')" ] || [ "$got_status" -ne 1 ]; then
  echo "FAIL summary: got '$got' (status $got_status)" >&2
  failed=$((failed + 1))
fi
rm -rf "$r"

noreply_case() {
  local name=$1 global=$2 local_email=$3 actions=$4 file=$5 want=$6 want_status=$7 r home got got_status
  cases=$((cases + 1))
  r=$(new_repo)
  home=$(mktemp -d)
  printf '%s\n' "$file" > "$r/notes.txt"
  git -C "$r" add notes.txt
  git -C "$r" -c user.name=t -c user.email=146819854+atsuki-seo@users.noreply.github.com commit -q -m base
  [ -z "$local_email" ] || git -C "$r" config --local user.email "$local_email"
  [ -z "$global" ] || HOME="$home" git config --global user.email "$global"
  set +e
  got=$(HOME="$home" GIT_CONFIG_NOSYSTEM=1 GITHUB_ACTIONS="$actions" "$goals" --root "$r" G-REPO-NOREPLY 2>&1)
  got_status=$?
  set -e
  if ! grep -qE "$want" <<< "$got" || [ "$got_status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$(tr '\n' ';' <<< "$got")' (status $got_status)" >&2
    failed=$((failed + 1))
  fi
  rm -rf "$r" "$home"
}

nr=146819854+atsuki-seo@users.noreply.github.com
noreply_case noreply-no-global "" "$nr" "" "notes" '^skip  personal email' 0
noreply_case noreply-global-is-noreply "$nr" "$nr" "" "notes $nr" '^skip  personal email' 0
noreply_case noreply-personal-found "me@example.org" "$nr" "" "write to me@example.org" '^FAIL  personal email in tracked files' 1
noreply_case noreply-actions-no-local "" "" true "notes" '^skip  local user.email' 0

cases=$((cases + 1))
r=$(new_repo)
git -C "$r" -c user.name=t -c user.email=$nr commit -q --allow-empty -m base
set +e
got=$(HOME=$(mktemp -d) GIT_CONFIG_NOSYSTEM=1 GITHUB_ACTIONS= "$goals" --root "$r" G-REPO-NOREPLY 2>&1)
set -e
if ! grep -q '^FAIL  local user.email' <<< "$got"; then
  echo "FAIL noreply-local-missing: got '$(tr '\n' ';' <<< "$got")'" >&2
  failed=$((failed + 1))
fi
rm -rf "$r"

r=$(new_repo)
expect unknown-goal "$r" G-NO-SUCH-GOAL "" 2

cases=$((cases + 1))
if ! "$goals" --list | grep -qx 'G-PLAN-COVERAGE'; then
  echo "FAIL list: G-PLAN-COVERAGE not listed" >&2
  failed=$((failed + 1))
fi

echo "test-goals: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
