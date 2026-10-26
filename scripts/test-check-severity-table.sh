#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

mkdir -p "$work/std"
printf 'OZNI_DIAG(TYP001, TYP, dimension, ERROR)\nOZNI_DIAG(LIN001, LIN, leftoverPart, INFO)\n' > "$work/reg.def"
printf 'type Options {\n  check SCH101 tooManyCooks: cooks <= 2 else warn\n}\n' > "$work/std/types.ozen"
printf 'severity\tA\tTYP:dimension\terror\tE\nseverity\tB\tLIN:leftoverPart\tinfo\tE\nseverity\tC\tSCH:tooManyCooks\twarning\tE\nseverity\tD\tSCH:washInserted\tinfo\tF\ncategory\tE\tTYP:dimension\t-\tD\n' > "$work/ex.txt"

expect() {
  local name=$1 want=$2 got
  shift 2
  cases=$((cases + 1))
  got=$(python3 -B "$here/check-severity-table.py" "$@" 2>/dev/null | tail -n 1) || true
  if [ "$got" != "$want" ]; then
    echo "FAIL $name: got '$got', want '$want'" >&2
    failed=$((failed + 1))
  fi
}

expect phase-e "severity: 3 examples, 0 mismatched; categories: 1 examples, 0 mismatched" --phase E "$work/ex.txt" "$work/reg.def" "$work/std"
expect all-phases "severity: 4 examples, 1 mismatched; categories: 1 examples, 0 mismatched" "$work/ex.txt" "$work/reg.def" "$work/std"
sed -i 's/LIN001, LIN, leftoverPart, INFO/LIN001, LIN, leftoverPart, WARNING/' "$work/reg.def"
expect wrong-severity "severity: 3 examples, 1 mismatched; categories: 1 examples, 0 mismatched" --phase E "$work/ex.txt" "$work/reg.def" "$work/std"
sed -i 's/TYP001, TYP, dimension/TYP001, TYP, unitMismatch/' "$work/reg.def"
expect unregistered "severity: 3 examples, 2 mismatched; categories: 1 examples, 1 mismatched" --phase E "$work/ex.txt" "$work/reg.def" "$work/std"

echo "test-check-severity-table: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
