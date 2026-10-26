#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cat > "$work/defs.json" <<'JSON'
{"definitions": [
  {"kind": "action", "name": "cookRice", "params": [{"name": "x"}], "members": []},
  {"kind": "ingredient", "name": "Pork", "params": [], "members": [{"section": "parts", "members": [{"marker": "belly"}, {"section": "shoulderLoin", "members": []}]}]},
  {"kind": "vessel", "name": "SoupBowl", "params": [], "members": []}
], "diagnostics": [{"code": "NAM001", "name": "undefined"}]}
JSON
full() {
  mkdir -p "$1"
  printf '%s\n' "action-cook-rice = { \$x }を炊く" "ingredient-pork = 豚肉" "part-belly = バラ" "part-shoulder-loin = 肩ロース" \
    "vessel-soup-bowl = 汁椀" "diag-undefined = { \$arg1 } は定義されていません" > "$1/ja.ftl"
  printf '%s\n' "action-cook-rice = Cook { \$x }" "ingredient-pork = pork" "part-belly = belly" \
    "part-shoulder-loin = shoulder loin" "vessel-soup-bowl = soup bowl" "diag-undefined =" "    { \$arg1 } is not defined" > "$1/en.ftl"
}
cases=0
failed=0
expect() {
  local name=$1 want=$2 status=$3 dir=$4 got st
  cases=$((cases + 1))
  got=$(node "$here/phrases-check.mjs" "$work/defs.json" "$dir" 2>/dev/null)
  st=$?
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}
full "$work/ok"
expect complete "phrases: 6 keys, 0 missing in ja, 0 missing in en, 0 unknown args" 0 "$work/ok"
full "$work/missing"
sed -i '/^part-belly/d' "$work/missing/en.ftl"
expect missing-en "phrases: 6 keys, 0 missing in ja, 1 missing in en, 0 unknown args" 1 "$work/missing"
full "$work/unknown"
sed -i 's/^action-cook-rice = .*/action-cook-rice = { $x }を{ $time }炊く/' "$work/unknown/ja.ftl"
expect unknown-arg "phrases: 6 keys, 0 missing in ja, 0 missing in en, 1 unknown args" 1 "$work/unknown"
expect no-files "phrases: 6 keys, 6 missing in ja, 6 missing in en, 0 unknown args" 1 "$work/none"
cases=$((cases + 1))
node "$here/phrases-check.mjs" >/dev/null 2>&1
[ $? -eq 2 ] || { echo "FAIL usage" >&2; failed=$((failed + 1)); }
echo "test-phrases-check: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
