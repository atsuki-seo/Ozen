#!/usr/bin/env bash
# SPDX-License-Identifier: MIT OR Apache-2.0

set -euo pipefail

if [ $# -ne 1 ]; then
  echo "usage: $0 RANGE" >&2
  exit 2
fi
range=$1
root=$(git rev-parse --show-toplevel)
cd "$root"

exceptions=()
if [ -f scripts/red-first-exceptions.txt ]; then
  while read -r sha _; do
    case "$sha" in ''|'#'*) continue ;; esac
    exceptions+=("$(git rev-parse --verify --quiet "$sha^{commit}" || echo "$sha")")
  done < scripts/red-first-exceptions.txt
fi

is_exception() {
  local e
  for e in "${exceptions[@]+"${exceptions[@]}"}"; do
    [ "$e" = "$1" ] && return 0
  done
  return 1
}

expectation_kind() {
  case "$1" in
    spec/fixtures/*) echo fixture ;;
    spec/samples/*.yaml) echo vocab-sample ;;
    proto/tests/golden/*.out) echo proto-golden ;;
    tests/golden/*.out) echo core-golden ;;
    samples/*/expect.yaml) echo sample ;;
    renderers/test/*.test.mjs) echo rtest ;;
    tests/cli/*.sh) echo cli ;;
    tests/props/*.py) echo prop ;;
  esac
}

built=""
build_once() {
  local what=$1
  case " $built " in *" $what "*) return 0 ;; esac
  case "$what" in
    proto) proto/scripts/build-all.sh --only gcc-release >/dev/null 2>&1 || return 1 ;;
    core) scripts/build-core.sh --only core-gcc-release >/dev/null 2>&1 || return 1 ;;
  esac
  built="$built $what"
}

run_one() {
  local kind=$1 path=$2 name id
  case "$kind" in
    fixture)
      name=$(cut -d/ -f4 <<< "$path")
      id=test_fx_$(tr '-' '_' <<< "$(cut -d/ -f3 <<< "$path")_${name%.yaml}")
      python3 -B -m unittest discover -s tools -t . -p 'test_*.py' -k "$id" >/dev/null 2>&1 ;;
    vocab-sample)
      python3 -B tools/vocabcheck/vocabcheck.py sample "$path" >/dev/null 2>&1 ;;
    proto-golden)
      name=${path#proto/tests/golden/}
      build_once proto && proto/scripts/run-golden.sh build/gcc-release/ozenproto --only "${name%.out}" >/dev/null 2>&1 ;;
    core-golden)
      name=${path#tests/golden/}
      name=${name%/expected.out}
      build_once core && scripts/run-golden.sh build/core-gcc-release/ozen tests/golden --only "${name%.out}" >/dev/null 2>&1 ;;
    sample)
      name=${path#samples/}
      build_once core && python3 -B tests/samples/check.py --ozen build/core-gcc-release/ozen samples --only "${name%%/*}" >/dev/null 2>&1 ;;
    rtest)
      node --test "$path" >/dev/null 2>&1 ;;
    cli)
      name=${path#tests/cli/}
      build_once core && scripts/run-cli-tests.sh build/core-gcc-release/ozen tests/cli --only "${name%.sh}" >/dev/null 2>&1 ;;
    prop)
      build_once core && python3 -B "$path" --ozen build/core-gcc-release/ozen >/dev/null 2>&1 ;;
    *) return 1 ;;
  esac
}

commits=0
not_red=0
tmp=$(mktemp -d)
cleanup() {
  git worktree remove --force "$tmp/wt" >/dev/null 2>&1 || true
  rm -rf "$tmp"
}
trap cleanup EXIT

while IFS= read -r sha; do
  is_exception "$sha" && continue
  added=()
  while IFS= read -r f; do
    [ -n "$f" ] || continue
    [ -n "$(expectation_kind "$f")" ] && added+=("$f")
  done < <(git diff-tree --no-commit-id --name-only --diff-filter=A -r --root "$sha")
  [ ${#added[@]} -gt 0 ] || continue
  commits=$((commits + 1))
  git worktree add -q --detach "$tmp/wt" "$sha" >/dev/null 2>&1
  passed=()
  (
    cd "$tmp/wt"
    for f in "${added[@]}"; do
      if run_one "$(expectation_kind "$f")" "$f"; then
        echo "$f"
      fi
    done
  ) > "$tmp/passed" || true
  mapfile -t passed < "$tmp/passed"
  git worktree remove --force "$tmp/wt" >/dev/null 2>&1
  built=""
  if [ ${#passed[@]} -gt 0 ]; then
    not_red=$((not_red + 1))
    echo "not red: $(git log -1 --format='%h %s' "$sha"): ${passed[*]}" >&2
  fi
done < <(git rev-list --reverse "$range")

echo "red-first: $commits commits, $not_red not red"
[ "$commits" -ge 1 ] && [ "$not_red" -eq 0 ]
