#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
repo=$work/repo

git_q() { git -C "$repo" -c user.name=t -c user.email=t@example.invalid "$@" >/dev/null; }

setup() {
  rm -rf "$repo"
  mkdir -p "$repo/scripts" "$repo/tests/golden/a/pack" "$repo/tests/cli" "$repo/core/tests/unit" "$repo/plans"
  cat > "$repo/scripts/build-core.sh" <<'EOF'
#!/bin/sh
mkdir -p build/core-gcc-release
cat > build/core-gcc-release/ozen <<'OZ'
#!/bin/sh
for last; do :; done
src=$last
[ -d "$src" ] || [ -f "$src" ] || src=main.ozen
[ -f "$src" ] || src=main.ozen
sed -n 's/^#> //p' "$src"
code=$(sed -n 's/^#exit //p' "$src")
exit "${code:-0}"
OZ
chmod +x build/core-gcc-release/ozen
EOF
  chmod +x "$repo/scripts/build-core.sh"
  printf '// args: debug steps\n#> recipe r\n#> %%1 = wash(rice)\n#> result ok errors=0 warnings=0 infos=0\n' \
    > "$repo/tests/golden/a/one.ozen"
  printf '// args: check\n#> error TYP001 dimension two.ozen:1:1-1:2\n#> result error errors=1 warnings=0 infos=0\n#exit 1\n' \
    > "$repo/tests/golden/a/two.ozen"
  printf 'check\n' > "$repo/tests/golden/a/pack/args"
  printf '#> result ok errors=0 warnings=0 infos=0\n' > "$repo/tests/golden/a/pack/main.ozen"
  printf 'result ok errors=0 warnings=0 infos=0\nexit 0\n' > "$repo/tests/golden/a/pack/expected.out"
  printf 'cat <<EOF\nrecipe r {\n}\nEOF\n' > "$repo/tests/cli/run.sh"
  printf 'static const char *span_nesting = "recipe r {}";\n' > "$repo/core/tests/unit/test_parse.c"
  cat > "$repo/plans/roadmap.md" <<'EOF'
### 5.3 網羅表

| S 項目 | 観点 | テストまたはゴール | わざと壊す方法 | 主なタスク |
| --- | --- | --- | --- | --- |
| S1 | 正常系 | `cgolden/a/one` | 洗いを消す | P1 |
| S2 | 効かない条件 | `cgolden/a/two` | 次元を見ない | P1 |
| S3 | 正常系 | `cgolden/a/*`（3 個） | — | P1 |
| S4 | 正常系 | `cgolden/a/pack` | 束ねない | P1 |
| S5 | 正常系 | `cli/run` | 走らせない | P1 |
| S6 | 前提の固定 | `cunit/span_nesting` | 入れ子を見ない | P1 |
| S7 | 境界・上限 | `cgolden/a/p*`（1 個） | 束ねる順を変える | P1 |

## 6. フェーズとタスク
EOF
  printf '## 9. 記録\n\n## 10. リスク\n' > "$repo/plans/plan.md"
  git -C "$repo" init -q
  git -C "$repo" add -A
  git_q commit -q -m base
  git -C "$repo" tag base
}

record() {
  sed -i "/^## 10\./i - 書き直しのわざと壊す確認: $1" "$repo/plans/plan.md"
}

commit_head() {
  git -C "$repo" add -A
  git_q commit -q -m head
}

expect() {
  local name=$1 want=$2 want_status=$3 got status
  cases=$((cases + 1))
  set +e
  got=$(cd "$repo" && python3 -B "$here/check-rewrite-equivalence.py" --base base --head HEAD \
    --records plans/plan.md --roadmap plans/roadmap.md --cache "$work/cache" tests/golden 2>/dev/null | tail -n 1)
  status=$?
  set -e
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

setup
sed -i 's/^\/\/ args: debug steps$/\/\/ args: debug steps\n\/\/ renamed/' "$repo/tests/golden/a/one.ozen"
record 'S1・正常系・`cgolden/a/one`。洗いを消すと落ちた'
commit_head
expect same "equivalence: 1 rewritten, 0 changed results, 0 changed steps; records: 1 roadmap rows, 0 missing" 0

setup
sed -i 's/^\/\/ args: debug steps$/\/\/ args: debug steps\n\/\/ renamed/' "$repo/tests/golden/a/one.ozen"
commit_head
expect missing-record "equivalence: 1 rewritten, 0 changed results, 0 changed steps; records: 1 roadmap rows, 1 missing" 1

setup
sed -i 's/TYP001 dimension/TYP002 other/' "$repo/tests/golden/a/two.ozen"
record 'S2・効かない条件・`cgolden/a/two`。次元を見ないと落ちた'
commit_head
expect changed-code "equivalence: 1 rewritten, 1 changed results, 0 changed steps; records: 1 roadmap rows, 0 missing" 1

setup
sed -i 's/errors=1 warnings=0/errors=1 warnings=1/' "$repo/tests/golden/a/two.ozen"
record 'S2・効かない条件・`cgolden/a/two`。次元を見ないと落ちた'
commit_head
expect changed-counts "equivalence: 1 rewritten, 1 changed results, 0 changed steps; records: 1 roadmap rows, 0 missing" 1

setup
sed -i 's/^#exit 1$/#exit 2/' "$repo/tests/golden/a/two.ozen"
record 'S2・効かない条件・`cgolden/a/two`。次元を見ないと落ちた'
commit_head
expect changed-exit "equivalence: 1 rewritten, 1 changed results, 0 changed steps; records: 1 roadmap rows, 0 missing" 1

setup
sed -i 's/^#> %1 = wash(rice)$/#> %1 = wash(rice)\n#> %2 = soak(%1)/' "$repo/tests/golden/a/one.ozen"
record 'S1・正常系・`cgolden/a/one`。洗いを消すと落ちた'
commit_head
expect changed-steps "equivalence: 1 rewritten, 0 changed results, 1 changed steps; records: 1 roadmap rows, 0 missing" 0

setup
printf '#> result ok errors=0 warnings=0 infos=0\n// renamed\n' > "$repo/tests/golden/a/pack/main.ozen"
record 'S4・正常系・`cgolden/a/pack`。束ねないと落ちた'
commit_head
expect folder-golden-glob "equivalence: 1 rewritten, 0 changed results, 0 changed steps; records: 2 roadmap rows, 1 missing" 1
record 'S7・境界・上限・`cgolden/a/p*`。束ねる順を変えると落ちた'
commit_head
expect folder-golden "equivalence: 1 rewritten, 0 changed results, 0 changed steps; records: 2 roadmap rows, 0 missing" 0

setup
printf 'cat <<EOF\nrecipe rice {\n}\nEOF\n' > "$repo/tests/cli/run.sh"
printf 'static const char *span_nesting = "recipe rice {}";\n' > "$repo/core/tests/unit/test_parse.c"
record 'S5・正常系・`cli/run`。走らせないと落ちた'
commit_head
expect cli-and-unit "equivalence: 0 rewritten, 0 changed results, 0 changed steps; records: 2 roadmap rows, 1 missing" 1

setup
printf 'not a commit\n' > "$repo/tests/golden/a/extra.txt"
commit_head
expect untouched "equivalence: 0 rewritten, 0 changed results, 0 changed steps; records: 0 roadmap rows, 0 missing" 0

echo "test-check-rewrite-equivalence: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
