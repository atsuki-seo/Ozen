#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

cat > "$work/ozen" <<'OZ'
#!/bin/sh
limit=""
prev=""
for a; do
  case "$prev" in --max-steps|--max-nodes|--max-depth|--max-input-bytes) limit=$a ;; esac
  prev=$a
  last=$a
done
src=$last
[ -f "$src" ] || src=main.ozen
need=$(sed -n 's/^#need //p' "$src")
code=$(sed -n 's/^#code //p' "$src")
if [ -n "$limit" ] && [ "$limit" -lt "$need" ]; then
  echo "error $code limit $src:1:1-1:1: limit"
  echo "result error errors=1 warnings=0 infos=0"
  exit 1
fi
echo "result ok errors=0 warnings=0 infos=0"
OZ
chmod +x "$work/ozen"

expect() {
  local name=$1 want=$2 want_status=$3 got status
  cases=$((cases + 1))
  set +e
  got=$(python3 -B "$here/check-limit-pairs.py" --ozen "$work/ozen" "$work/golden" 2>/dev/null | tail -n 1)
  status=$?
  set -e
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

reset() {
  rm -rf "$work/golden"
  mkdir -p "$work/golden/limits" "$work/golden/eval" "$work/golden/limits-shared/kitchen-exact"
  printf '// args: check --max-nodes 5\n#need 5\n#code MAC003\n' > "$work/golden/limits/nodes-exact.ozen"
  printf '// args: check --max-steps 9\n#need 9\n#code MAC001\n' > "$work/golden/eval/fuel-exact.ozen"
  printf 'check --max-input-bytes 116 main.ozen\n' > "$work/golden/limits-shared/kitchen-exact/args"
  printf '#need 116\n#code SYN010\n' > "$work/golden/limits-shared/kitchen-exact/main.ozen"
  printf 'result ok errors=0 warnings=0 infos=0\nexit 0\n' > "$work/golden/limits-shared/kitchen-exact/expected.out"
  printf '// args: check --max-nodes 3\n#need 5\n#code MAC003\n' > "$work/golden/limits/nodes-over.ozen"
}

reset
expect exact "limit-pairs: 3 pairs, 0 not exact" 0
printf '// args: check --max-nodes 5\n#need 4\n#code MAC003\n' > "$work/golden/limits/nodes-exact.ozen"
expect not-exact "limit-pairs: 3 pairs, 1 not exact" 1
reset
printf '// args: check --max-steps 9\n#need 9\n#code TYP001\n' > "$work/golden/eval/fuel-exact.ozen"
expect wrong-code "limit-pairs: 3 pairs, 1 not exact" 1
reset
printf '// args: check\n#need 9\n#code MAC001\n' > "$work/golden/eval/fuel-exact.ozen"
expect no-limit "limit-pairs: 3 pairs, 1 not exact" 1
reset
rm "$work/golden/limits-shared/kitchen-exact/main.ozen" "$work/golden/limits-shared/kitchen-exact/args" "$work/golden/limits-shared/kitchen-exact/expected.out"
expect fewer-pairs "limit-pairs: 2 pairs, 0 not exact" 0

echo "test-check-limit-pairs: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
