#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
head='<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml"><head><meta charset="utf-8"/><title>t</title><style>p{color:red}</style></head><body>'
tail='</body></html>'
page() {
  mkdir -p "$work/$1"
  printf '%s%s%s\n' "$head" "$2" "$tail" > "$work/$1/a.html"
}
svg() {
  printf '<svg xmlns="http://www.w3.org/2000/svg" data-gantt="true" data-label-width="10" data-per-minute="20" width="%s" height="40">%s</svg>' "${2:-200}" "$1"
}
rect() {
  printf '<rect x="%s" y="4" width="%s" height="16" data-row="%s" data-start="%s" data-end="%s"/>' "$@"
}
page ok "<p>a<br/>b</p>$(svg "$(rect 10 20 cook1 0 60)$(rect 30 20 cook1 60 120)")<form><label for=\"c\">c</label><input id=\"c\" data-type=\"Mass\" data-unit=\"g\"/></form>"
page unclosed '<p>a<br>b</p>'
page overlap "$(svg "$(rect 10 20 cook1 0 60)$(rect 20 20 cook1 30 90)")"
page outside "$(svg "$(rect 190 20 cook1 540 600)")"
page misplaced "$(svg "$(rect 50 20 cook1 0 60)")"
page wide "$(svg "$(rect 10 20 cook1 0 60)" 1000)"
page unlabelled '<form><input id="c"/></form>'
page unitless '<form><label for="c">c</label><input id="c" data-type="Mass" data-quantity="false"/></form>'
page cdn '<link rel="stylesheet" href="https://cdn.example.com/a.css"/><p>x</p>'
printf '{"definitions": [{"kind": "dimension", "name": "Mass"}, {"kind": "dimension", "name": "Count"}]}\n' > "$work/defs.json"
cases=0
failed=0
expect() {
  local name=$1 tool=$2 want=$3 status=$4 got st
  cases=$((cases + 1))
  if [ "$tool" = htmlcheck.py ]; then
    got=$(python3 -B "$root/tools/htmlcheck/$tool" --definitions "$work/defs.json" "$work/$name" 2>/dev/null)
  else
    got=$(python3 -B "$root/tools/htmlcheck/$tool" "$work/$name" 2>/dev/null)
  fi
  st=$?
  got=$(tail -n 1 <<< "$got")
  if [ "$got" != "$want" ] || [ "$st" -ne "$status" ]; then
    echo "FAIL $name ($tool): got '$got' ($st)" >&2
    failed=$((failed + 1))
  fi
}
ok_line='html: 1 files, 0 not well-formed; gantt: 1 charts, 0 overlaps, 0 out of bounds; form: 1 inputs, 0 without label, 0 quantity without unit'
expect ok htmlcheck.py "$ok_line" 0
expect unclosed htmlcheck.py 'html: 1 files, 1 not well-formed; gantt: 0 charts, 0 overlaps, 0 out of bounds; form: 0 inputs, 0 without label, 0 quantity without unit' 1
expect overlap htmlcheck.py 'html: 1 files, 0 not well-formed; gantt: 1 charts, 1 overlaps, 0 out of bounds; form: 0 inputs, 0 without label, 0 quantity without unit' 1
expect outside htmlcheck.py 'html: 1 files, 0 not well-formed; gantt: 1 charts, 0 overlaps, 1 out of bounds; form: 0 inputs, 0 without label, 0 quantity without unit' 1
expect misplaced htmlcheck.py 'html: 1 files, 0 not well-formed; gantt: 1 charts, 0 overlaps, 1 out of bounds; form: 0 inputs, 0 without label, 0 quantity without unit' 1
expect wide htmlcheck.py 'html: 1 files, 0 not well-formed; gantt: 1 charts, 0 overlaps, 1 out of bounds; form: 0 inputs, 0 without label, 0 quantity without unit' 1
expect unlabelled htmlcheck.py 'html: 1 files, 0 not well-formed; gantt: 0 charts, 0 overlaps, 0 out of bounds; form: 1 inputs, 1 without label, 0 quantity without unit' 1
expect unitless htmlcheck.py 'html: 1 files, 0 not well-formed; gantt: 0 charts, 0 overlaps, 0 out of bounds; form: 1 inputs, 0 without label, 1 quantity without unit' 1
expect ok selfcontained.py 'selfcontained: 1 files, 0 external references' 0
expect cdn selfcontained.py 'selfcontained: 1 files, 1 external references' 1
echo "test-htmlcheck: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
