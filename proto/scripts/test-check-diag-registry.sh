#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0
mkdir -p "$work/out"

setup() {
  printf 'OZPI_DIAG(SYN001, SYN, badEscape)\nOZPI_DIAG(TYP001, TYP, dimension)\n' > "$work/reg.def"
  printf 'SYN:badEscape\nTYP:dimension\n' > "$work/names.txt"
  printf 'error SYN001 badEscape 1:1-1:2\nerror TYP001 dimension 2:1-2:3\nexit 1\n' > "$work/out/a.out"
}

expect() {
  local name=$1 want=$2 status=$3 got st
  cases=$((cases + 1))
  set +e
  got=$("$here/check-diag-registry.sh" --registry "$work/reg.def" --names "$work/names.txt" --outputs "$work/out" 2>/dev/null | tail -n 1)
  st=$?
  set -e
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name: got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}

setup; expect good "diag: 2 codes, 0 duplicate codes, 0 duplicate names, 0 bad codes, 0 unlisted, 0 unused" 0
setup; echo 'OZPI_DIAG(SYN001, SYN, other)' >> "$work/reg.def"
expect duplicate-code "diag: 3 codes, 1 duplicate codes, 0 duplicate names, 0 bad codes, 1 unlisted, 1 unused" 1
setup; echo 'OZPI_DIAG(SYN002, SYN, badEscape)' >> "$work/reg.def"
expect duplicate-name "diag: 3 codes, 0 duplicate codes, 1 duplicate names, 0 bad codes, 0 unlisted, 1 unused" 1
setup; sed -i 's/SYN001, SYN/SY001, SYN/' "$work/reg.def"; sed -i 's/SYN001/SY001/' "$work/out/a.out"
expect bad-code "diag: 2 codes, 0 duplicate codes, 0 duplicate names, 1 bad codes, 0 unlisted, 0 unused" 1
setup; sed -i '/badEscape/d' "$work/reg.def"
expect unlisted "diag: 1 codes, 0 duplicate codes, 0 duplicate names, 0 bad codes, 1 unlisted, 0 unused" 1
setup; sed -i '/TYP001/d' "$work/out/a.out"
expect unused "diag: 2 codes, 0 duplicate codes, 0 duplicate names, 0 bad codes, 0 unlisted, 1 unused" 1

setup; echo 'OZNI_DIAG(PKG001, PKG, importCycle, ERROR)' >> "$work/reg.def"; echo 'PKG:importCycle' >> "$work/names.txt"
printf 'error PKG001 importCycle a.ozen:1:1-1:2\n' > "$work/out/expected.out"
expect core-entry-with-severity "diag: 3 codes, 0 duplicate codes, 0 duplicate names, 0 bad codes, 0 unlisted, 0 unused" 0
rm -f "$work/out/expected.out"
setup; echo 'OZNI_DIAG(XYZ001, XYZ, other, ERROR)' >> "$work/reg.def"; echo 'XYZ:other' >> "$work/names.txt"
printf 'error XYZ001 other a.ozen:1:1-1:2\n' >> "$work/out/a.out"
expect unknown-category "diag: 3 codes, 0 duplicate codes, 0 duplicate names, 1 bad codes, 0 unlisted, 0 unused" 1

echo "test-check-diag-registry: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
