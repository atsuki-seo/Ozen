#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cases=0
failed=0

expect() {
  local name=$1 want=$2 want_status=$3 got status
  cases=$((cases + 1))
  set +e
  got=$(cd "$work" && python3 -B "$here/check-test-names.py" --empty-parens --exceptions exceptions.txt \
    tests README.md 2>/dev/null | tail -n 1)
  status=$?
  set -e
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

reset() {
  rm -rf "$work/tests" "$work/README.md" "$work/exceptions.txt"
  mkdir -p "$work/tests/golden" "$work/tests/cli" "$work/tests/unit"
  printf 'recipe soup {\n  let warmed = warm(dashi)\n}\nmenu dinner {\n  let bowl = make soup\n}\n' > "$work/tests/golden/soup.ozen"
  printf 'recipe stew(servings: Count = 2) {\n  let cooked = wash()\n}\n' > "$work/tests/golden/stew.ozen"
  printf '#!/bin/sh\ncat <<EOF\nrecipe tea {\n}\nEOF\n' > "$work/tests/cli/tea.sh"
  printf 'static const char *src = "recipe rice {}";\n' > "$work/tests/unit/rice.c"
  printf '# Ozen\n\n```ozen\nrecipe salad {\n}\n```\n\nWrite recipe salad() in prose.\n' > "$work/README.md"
  printf '# path reason\n' > "$work/exceptions.txt"
}

reset
expect clean "empty-parens: 5 sources, 0 found, 0 unlisted exceptions" 0
printf 'recipe salad() {\n}\n' > "$work/tests/golden/salad.ozen"
expect recipe-parens "empty-parens: 6 sources, 1 found, 0 unlisted exceptions" 1
reset
printf 'menu lunch ( ) {\n}\n' > "$work/tests/golden/lunch.ozen"
expect menu-parens "empty-parens: 6 sources, 1 found, 0 unlisted exceptions" 1
reset
printf 'menu lunch {\n  let bowl = make soup()\n}\n' > "$work/tests/golden/lunch.ozen"
expect make-parens "empty-parens: 6 sources, 1 found, 0 unlisted exceptions" 1
reset
printf '#!/bin/sh\ncat <<EOF\nrecipe tea() {\n}\nEOF\n' > "$work/tests/cli/tea.sh"
expect shell-heredoc "empty-parens: 5 sources, 1 found, 0 unlisted exceptions" 1
reset
printf 'static const char *src = "recipe rice() {}";\n' > "$work/tests/unit/rice.c"
expect c-string "empty-parens: 5 sources, 1 found, 0 unlisted exceptions" 1
reset
printf '# Ozen\n\n```ozen\nrecipe salad() {\n}\n```\n' > "$work/README.md"
expect readme-block "empty-parens: 5 sources, 1 found, 0 unlisted exceptions" 1
reset
printf 'recipe salad() {\n}\n' > "$work/tests/golden/salad.ozen"
printf 'tests/golden/salad.ozen compares the two forms\n' >> "$work/exceptions.txt"
expect listed-exception "empty-parens: 6 sources, 0 found, 0 unlisted exceptions" 0
reset
printf 'recipe salad() {\n}\n' > "$work/tests/golden/salad.ozen"
printf 'tests/golden/salad.ozen\n' >> "$work/exceptions.txt"
expect exception-without-reason "empty-parens: 6 sources, 1 found, 1 unlisted exceptions" 1
reset
printf 'recipe broken() {\n  // \377\n}\n' > "$work/tests/golden/broken.ozen"
expect invalid-utf8 "empty-parens: 6 sources, 1 found, 0 unlisted exceptions" 1
reset
rm "$work/README.md"
expect missing-path "" 2

expect_names() {
  local name=$1 want=$2 want_status=$3 got status
  cases=$((cases + 1))
  set +e
  got=$(cd "$work" && python3 -B "$here/check-test-names.py" --exceptions exceptions.txt tests 2>/dev/null | tail -n 1)
  status=$?
  set -e
  if [ "$got" != "$want" ] || [ "$status" -ne "$want_status" ]; then
    echo "FAIL $name: got '$got' (status $status)" >&2
    failed=$((failed + 1))
  fi
}

reset
expect_names names-clean "names: 5 declarations, 3 bindings, 0 one-letter, 0 unlisted exceptions" 0
printf 'recipe r {\n  let x = wash(rice)\n}\n' > "$work/tests/golden/short.ozen"
expect_names one-letter "names: 6 declarations, 4 bindings, 2 one-letter, 0 unlisted exceptions" 1
printf 'tests/golden/short.ozen keeps one-letter names on purpose\n' >> "$work/exceptions.txt"
expect_names names-listed "names: 5 declarations, 3 bindings, 0 one-letter, 0 unlisted exceptions" 0
reset
printf 'recipe pair {\n  let (a, rest) = split(rice)\n}\n' > "$work/tests/golden/pair.ozen"
expect_names tuple-binding "names: 6 declarations, 5 bindings, 1 one-letter, 0 unlisted exceptions" 1
reset
printf '#!/bin/sh\ncat <<EOF\nmenu m {\n}\nEOF\n' > "$work/tests/cli/tea.sh"
expect_names shell-heredoc "names: 5 declarations, 3 bindings, 1 one-letter, 0 unlisted exceptions" 1
reset
printf 'tests/golden/soup.ozen\n' >> "$work/exceptions.txt"
expect_names names-exception-without-reason "names: 5 declarations, 3 bindings, 0 one-letter, 1 unlisted exceptions" 1

echo "test-check-test-names: $cases cases, $failed failed"
[ "$failed" -eq 0 ]
