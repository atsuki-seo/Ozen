#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

repo="$work/repo"
mkdir -p "$repo/proto/src"
g() { git -C "$repo" -c user.name=t -c user.email=t@example.invalid "$@"; }
g init -q
printf 'a\nb\n' > "$repo/proto/src/a.c"
g add -A && g commit -qm "Add a"
g tag plan-start
printf 'c\n' > "$repo/proto/src/b.c"
g add -A && g commit -qm "Add b"
add_b=$(g rev-parse --short HEAD)
printf 'c\nd\n' > "$repo/proto/src/b.c"
g add -A && g commit -qm "Fix b"
fix_b=$(g rev-parse --short HEAD)

mkdir -p "$work/build/results" "$work/build/fuzz"
printf 'golden: 9 passed, 1 failed, 0 timed out\n' > "$work/build/results/golden-x.txt"
printf 'abc123\n' > "$work/build/results/digest-x.txt"
printf 'fuzz: 10 execs in 600 s, 0 crashes, 0 leaks, 0 timeouts\n' > "$work/build/fuzz/summary.txt"
printf 'date\tmethod\tcategory\tsummary\tcommit\n2026-10-26\tgolden\tx\ty\t%s\n2026-10-26\tfuzz\tx\ty\t%s\n' \
  "$add_b" "$fix_b" > "$work/defects.tsv"

cat > "$work/report.md" <<'EOF'
# Report

## 1. 書き心地とバグ量

| ファイルの組 | ファイル | 行数 |
| --- | --- | --- |
| core | `proto/src/*.c` | 4 |

| 検出した手段 | 件数 |
| --- | --- |
| golden | 1 |
| fuzz | 1 |
| 合計 | 2 |

## 2. 環境ごとの結果

| 環境 | golden | digest |
| --- | --- | --- |
| `x` | 9/10 | `abc123` |
| `y` | — | — |

## 3. ファズ

`fuzz: 10 execs in 600 s, 0 crashes, 0 leaks, 0 timeouts`

## 4. ユーザーが決めること

none
EOF
cp "$work/report.md" "$work/good.md"
cp "$work/defects.tsv" "$work/good.tsv"

expect() {
  local name=$1 want=$2 got
  cases=$((cases + 1))
  got=$(cd "$repo" && python3 -B "$here/check-report.py" "$work/report.md" "$work/build/results" "$work/defects.tsv" 2>/dev/null | tail -n 1) || true
  if [ "$got" != "$want" ]; then
    echo "FAIL $name: got '$got', want '$want'" >&2
    failed=$((failed + 1))
  fi
  cp "$work/good.md" "$work/report.md"
  cp "$work/good.tsv" "$work/defects.tsv"
}

expect all-match "report: 9 checks, 0 failed"
sed -i '/^## 3\. ファズ/d' "$work/report.md"
expect heading-missing "report: 9 checks, 2 failed"
sed -i 's/| `proto\/src\/\*.c` | 4 |/| `proto\/src\/*.c` | 5 |/' "$work/report.md"
expect line-count "report: 9 checks, 1 failed"
sed -i 's/`abc123`/`abc124`/' "$work/report.md"
expect digest "report: 9 checks, 1 failed"
sed -i 's/| 9\/10 |/| 10\/10 |/' "$work/report.md"
expect golden-count "report: 9 checks, 1 failed"
sed -i 's/| `y` | — | — |/| `y` | 1\/1 | — |/' "$work/report.md"
expect extra-row "report: 9 checks, 1 failed"
sed -i 's/600 s, 0 crashes/600 s, 1 crashes/' "$work/report.md"
expect fuzz "report: 9 checks, 1 failed"
sed -i 's/| fuzz | 1 |/| fuzz | 2 |/' "$work/report.md"
expect defect-count "report: 9 checks, 1 failed"
sed -i '$d' "$work/defects.tsv"
expect fix-row-removed "report: 9 checks, 2 failed"
printf '2026-10-26\tgolden\tx\ty\t0000000\n' >> "$work/defects.tsv"
sed -i 's/| golden | 1 |/| golden | 2 |/; s/| 合計 | 2 |/| 合計 | 3 |/' "$work/report.md"
expect non-ancestor "report: 9 checks, 1 failed"

echo "test-check-report: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
