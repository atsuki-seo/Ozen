#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
checker="$here/check-translation.py"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cases=0
failed=0

git -C "$work" init -q
cat > "$work/doc.md" <<'EOF'
# 計画

## 1. 目的

**S1** 卵を割る。`make 名前` を書く。D2 と P1-2 を参照。

| 列 | 値 |
| --- | --- |
| a | `G-X-ONE` |

### D2. 決め方【決定済み】

- **決定:** 推奨どおり。

```bash
# 数を数える
grep -c '^### D[0-9]*\. .*【決定済み】' $PLAN   # 1 になる
```
EOF
git -C "$work" add doc.md
git -C "$work" -c user.name=t -c user.email=t@example.org commit -q -m base
git -C "$work" tag base

good='# Plan

## 1. Purpose

**S1** Crack the eggs. Write `make 名前`. See D2 and P1-2.

| Column | Value |
| --- | --- |
| a | `G-X-ONE` |

### D2. How to decide [decided]

- **Decision:** As recommended.

```bash
# count them
grep -c '"'"'^### D[0-9]*\. .*\[decided\]'"'"' $PLAN   # becomes 1
```'

expect() {
  local name=$1 body=$2 want=$3 allow=${4:-} got
  cases=$((cases + 1))
  printf '%s\n' "$body" > "$work/doc.md"
  printf '%s' "$allow" > "$work/allow.txt"
  set +e
  got=$(cd "$work" && python3 -B "$checker" --base base --allow allow.txt doc.md 2>/dev/null | tail -n 1)
  set -e
  if ! [[ $got =~ $want ]]; then
    echo "FAIL $name: got '$got', want '$want'" >&2
    failed=$((failed + 1))
  fi
}

ok='^translation: 1 files, 0 problems$'
bad='^translation: 1 files, [1-9][0-9]* problems$'
expect good "$good" "$ok"
expect japanese-prose "${good/Crack the eggs./Crack the eggs. 卵を割る。}" "$bad"
expect japanese-shell-comment "${good/\# count them/\# 数える}" "$bad"
expect allowed-japanese "${good/Crack the eggs./Crack the eggs (卵).}" "$ok" $'卵\tthe name of the food in the source\n'
expect dropped-id "${good/\*\*S1\*\* /}" "$bad"
expect renumbered-heading "${good/\#\# 1. Purpose/\#\# 2. Purpose}" "$bad"
expect changed-flag "${good/grep -c/grep -n}" "$bad"
expect deleted-row "${good/| Column | Value |
/}" "$bad"
expect allow-without-reason "${good/Crack the eggs./Crack the eggs (卵).}" "$bad" $'卵\n'
expect allow-unused "$good" "$bad" $'鶏\tnot used\n'

echo "check-translation: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
